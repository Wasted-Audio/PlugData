/*
 // Copyright (c) 2021-2022 Timothy Schoen
 // For information on usage and redistribution, and for a DISCLAIMER OF ALL
 // WARRANTIES, see the file, "LICENSE.txt," in this distribution.
 */

#include "Components/DraggableNumber.h"
#include "ObjectBase.h"
#include "IEMHelper.h"

class NumberObject final : public ObjectBase {
    DraggableNumber input;
    IEMHelper iemHelper;

    Value widthProperty = SynchronousValue();
    Value heightProperty = SynchronousValue();
    Value min = SynchronousValue(-std::numeric_limits<float>::infinity());
    Value max = SynchronousValue(std::numeric_limits<float>::infinity());
    Value logHeight = SynchronousValue();
    Value logMode = SynchronousValue();

    float preFocusValue;
    float value = 0.0f;

    NVGcolor backgroundCol;
    NVGcolor foregroundCol;
    NVGcolor flagCol;

public:
    NumberObject(pd::WeakReference ptr, Object* object)
        : ObjectBase(ptr, object)
        , input(false)
        , iemHelper(ptr, object, this)

    {
        iemHelper.iemColourChangedCallback = [this]() {
            // We use this callback to be informed when the IEM colour has changed.
            // As getBackgroundColour() will lock audio thread!
            backgroundCol = convertColour(iemHelper.getBackgroundColour());

            foregroundCol = convertColour(iemHelper.getForegroundColour());
            flagCol = convertColour(iemHelper.getForegroundColour());

            input.setColour(Label::textColourId, iemHelper.getForegroundColour());
            input.setColour(Label::textWhenEditingColourId, iemHelper.getBackgroundColour().contrasting());
        };

        input.onEditorShow = [this]() {
            auto* editor = input.getCurrentTextEditor();
            startEdition();

            editor->setColour(TextEditor::focusedOutlineColourId, Colours::transparentBlack);
            editor->setBorder({ 0, 8, 4, 1 });
            editor->setInputRestrictions(0, "e.-0123456789");
        };

        input.onInteraction = [this](bool isFocused) {
            if (isFocused)
                input.setColour(Label::textColourId, convertColour(backgroundCol).contrasting());
            else
                input.setColour(Label::textColourId, convertColour(foregroundCol));
        };

        input.onEditorHide = [this]() {
            stopEdition();
        };

        input.setBorderSize({ 1, 12, 2, 2 });

        addAndMakeVisible(input);

        addMouseListener(this, true);

        input.dragStart = [this]() {
            startEdition();
        };

        input.onValueChange = [this](float newValue) {
            sendFloatValue(newValue);
        };

        input.dragEnd = [this]() {
            stopEdition();
        };

        objectParameters.addParamInt("Width (chars)", cDimensions, &widthProperty);
        objectParameters.addParamInt("Height", cDimensions, &heightProperty);
        objectParameters.addParamInt("Text/Label Height", cDimensions, &iemHelper.labelHeight, 10);
        objectParameters.addParamFloat("Minimum", cGeneral, &min, -9.999999933815813e36);
        objectParameters.addParamFloat("Maximum", cGeneral, &max, 9.999999933815813e36);
        objectParameters.addParamBool("Logarithmic mode", cGeneral, &logMode, { "Off", "On" }, var(false));
        objectParameters.addParamInt("Logarithmic height", cGeneral, &logHeight, var(256));
        objectParameters.addParamColourFG(&iemHelper.primaryColour);
        objectParameters.addParamColourBG(&iemHelper.secondaryColour);
        objectParameters.addParamReceiveSymbol(&iemHelper.receiveSymbol);
        objectParameters.addParamSendSymbol(&iemHelper.sendSymbol);
        objectParameters.addParamString("Label", cLabel, &iemHelper.labelText, "");
        objectParameters.addParamColourLabel(&iemHelper.labelColour);
        objectParameters.addParamInt("Label X", cLabel, &iemHelper.labelX, 0);
        objectParameters.addParamInt("Label Y", cLabel, &iemHelper.labelY, -8);
        objectParameters.addParamBool("Initialise", cGeneral, &iemHelper.initialise, { "No", "Yes" }, 0);

        input.setResetValue(0.0f);
    }

    void update() override
    {
        if (input.isShowing())
            return;

        value = getValue();
        input.setValue(value, dontSendNotification);

        min = getMinimum();
        max = getMaximum();

        input.setMinimum(::getValue<float>(min));
        input.setMaximum(::getValue<float>(max));

        if (auto nbx = ptr.get<t_my_numbox>()) {
            widthProperty = var(nbx->x_numwidth);
            heightProperty = var(nbx->x_gui.x_h);
            logMode = nbx->x_lin0_log1;
            logHeight = nbx->x_log_height;
        }

        iemHelper.update();

        auto fontHeight = ::getValue<int>(iemHelper.labelHeight) + 3.0f;
        input.setFont(Fonts::getTabularNumbersFont().withHeight(fontHeight));
    }

    bool inletIsSymbol() override
    {
        return iemHelper.hasReceiveSymbol();
    }

    bool outletIsSymbol() override
    {
        return iemHelper.hasSendSymbol();
    }

    void updateLabel() override
    {
        iemHelper.updateLabel(labels);
    }

    Rectangle<int> getPdBounds() override
    {
        if (auto nbx = ptr.get<t_my_numbox>()) {
            auto* patch = cnv->patch.getPointer().get();
            if (!patch)
                return {};

            int x = 0, y = 0, w = 0, h = 0;
            pd::Interface::getObjectBounds(patch, nbx.cast<t_gobj>(), &x, &y, &w, &h);

            return { x, y, calcFontWidth(std::max(nbx->x_numwidth, 1)) + 1, h + 1 };
        }

        return {};
    }

    int getFontWidth()
    {
        if (auto nbx = ptr.get<t_my_numbox>()) {
            return nbx->x_gui.x_fontsize;
        }
        return 10;
    }

    void setPdBounds(Rectangle<int> b) override
    {
        if (auto nbx = ptr.get<t_my_numbox>()) {
            auto* patchPtr = cnv->patch.getPointer().get();
            if (!patchPtr)
                return;

            pd::Interface::moveObject(patchPtr, nbx.cast<t_gobj>(), b.getX(), b.getY());

            nbx->x_numwidth = calcNumWidth(b.getWidth() - 1);
            nbx->x_gui.x_w = b.getWidth() - 1;
            nbx->x_gui.x_h = b.getHeight() - 1;
        }
    }

    void updateSizeProperty() override
    {
        setPdBounds(object->getObjectBounds());

        if (auto nbx = ptr.get<t_my_numbox>()) {
            setParameterExcludingListener(widthProperty, var(nbx->x_numwidth));
            setParameterExcludingListener(heightProperty, var(nbx->x_gui.x_h));
        }
    }

    void resized() override
    {
        input.setBounds(getLocalBounds());
    }

    void focusGained(FocusChangeType cause) override
    {
        preFocusValue = value;
        repaint();
    }

    bool keyPressed(KeyPress const& key) override
    {
        if (key.getKeyCode() == KeyPress::returnKey) {
            auto inputValue = input.getText().getFloatValue();
            preFocusValue = value;
            sendFloatValue(inputValue);
            cnv->grabKeyboardFocus();
            return true;
        }

        return false;
    }

    void focusLost(FocusChangeType cause) override
    {
        auto inputValue = input.getText().getFloatValue();
        if (!approximatelyEqual(inputValue, preFocusValue)) {
            sendFloatValue(inputValue);
        }
        repaint();
    }

    void focusOfChildComponentChanged(FocusChangeType cause) override
    {
        repaint();
    }

    void lock(bool isLocked) override
    {
        input.setResetEnabled(::getValue<bool>(cnv->locked));
        setInterceptsMouseClicks(isLocked, isLocked);
        repaint();
    }

    void receiveObjectMessage(hash32 symbol, StackArray<pd::Atom, 8> const& atoms, int numAtoms) override
    {
        switch (symbol) {
        case hash("float"):
        case hash("list"):
        case hash("set"): {
            if (numAtoms > 0 && atoms[0].isFloat()) {
                value = std::clamp(atoms[0].getFloat(), ::getValue<float>(min), ::getValue<float>(max));
                input.setValue(value, dontSendNotification);
            }
            break;
        }
        case hash("range"): {
            if (numAtoms >= 2 && atoms[0].isFloat() && atoms[1].isFloat()) {
                min = getMinimum();
                max = getMaximum();
            }
            break;
        }
        case hash("log"): {
            setParameterExcludingListener(logMode, true);
            input.setDragMode(DraggableNumber::Logarithmic);
            break;
        }
        case hash("lin"): {
            setParameterExcludingListener(logMode, false);
            input.setDragMode(DraggableNumber::Regular);
            break;
        }
        case hash("log_height"): {
            auto height = static_cast<int>(atoms[0].getFloat());
            setParameterExcludingListener(logHeight, height);
            input.setLogarithmicHeight(height);
        }
        default: {
            iemHelper.receiveObjectMessage(symbol, atoms, numAtoms);
            break;
        }
        }
    }

    int calcFontWidth(int numWidth) const
    {
        if (auto nbx = ptr.get<t_my_numbox>()) {
            int w, f = 31;

            if (nbx->x_gui.x_fsf.x_font_style == 1)
                f = 27;
            else if (nbx->x_gui.x_fsf.x_font_style == 2)
                f = 25;

            w = nbx->x_gui.x_fontsize * f * numWidth;
            w /= 36;
            return (w + (nbx->x_gui.x_h / 2) + 4);
        }
        return 14;
    }

    int calcNumWidth(int width) const
    {
        if (auto nbx = ptr.get<t_my_numbox>()) {
            int f = 31;
            if (nbx->x_gui.x_fsf.x_font_style == 1)
                f = 27;
            else if (nbx->x_gui.x_fsf.x_font_style == 2)
                f = 25;

            return -(18.0f * (8.0f + nbx->x_gui.x_h - 2 * width)) / (nbx->x_gui.x_fontsize * f) + 1;
        }
        return 1;
    }

    void propertyChanged(Value& value) override
    {

        if (value.refersToSameSourceAs(widthProperty)) {
            auto numWidth = std::max(::getValue<int>(widthProperty), 1);

            auto width = calcFontWidth(numWidth) + 1;

            setParameterExcludingListener(widthProperty, var(numWidth));

            if (auto nbx = ptr.get<t_my_numbox>()) {
                nbx->x_numwidth = numWidth;
                nbx->x_gui.x_w = width;
            }

            object->updateBounds();
        } else if (value.refersToSameSourceAs(heightProperty)) {
            auto height = std::max(::getValue<int>(heightProperty), constrainer->getMinimumHeight());
            setParameterExcludingListener(heightProperty, var(height));
            if (auto nbx = ptr.get<t_my_numbox>()) {
                nbx->x_gui.x_h = height;
            }
            object->updateBounds();
        } else if (value.refersToSameSourceAs(min)) {
            setMinimum(::getValue<float>(min));
        } else if (value.refersToSameSourceAs(max)) {
            setMaximum(::getValue<float>(max));
        } else if (value.refersToSameSourceAs(logHeight)) {
            auto height = ::getValue<int>(logHeight);
            if (auto nbx = ptr.get<t_my_numbox>()) {
                nbx->x_log_height = height;
            }

            input.setLogarithmicHeight(height);
        } else if (value.refersToSameSourceAs(logMode)) {
            auto logarithmicDrag = ::getValue<bool>(logMode);
            if (auto nbx = ptr.get<t_my_numbox>()) {
                nbx->x_lin0_log1 = logarithmicDrag;
            }
            input.setDragMode(logarithmicDrag ? DraggableNumber::Logarithmic : DraggableNumber::Regular);
        } else if (value.refersToSameSourceAs(iemHelper.labelHeight)) {
            limitValueMin(iemHelper.labelHeight, 4.f);
            iemHelper.setFontHeight(::getValue<int>(iemHelper.labelHeight));
            updateLabel();

            auto fontHeight = ::getValue<int>(iemHelper.labelHeight) + 3.0f;
            input.setFont(Fonts::getTabularNumbersFont().withHeight(fontHeight));
            object->updateBounds();
        } else {
            iemHelper.valueChanged(value);
        }
    }

    void render(NVGcontext* nvg) override
    {
        auto b = getLocalBounds().toFloat();

        bool selected = object->isSelected() && !cnv->isGraph;

        nvgDrawRoundedRect(nvg, b.getX(), b.getY(), b.getWidth(), b.getHeight(), backgroundCol, selected ? cnv->selectedOutlineCol : cnv->objectOutlineCol, Corners::objectCornerRadius);

        int const indent = 9;
        Rectangle<float> iconBounds = { static_cast<float>(b.getX() + 4), static_cast<float>(b.getY() + 4), static_cast<float>(indent - 4), static_cast<float>(b.getHeight() - 8) };

        auto centreY = iconBounds.getCentreY();
        auto leftX = iconBounds.getX();
        nvgBeginPath(nvg);
        nvgMoveTo(nvg, leftX, centreY + 5.0f);
        nvgLineTo(nvg, iconBounds.getRight(), centreY);
        nvgLineTo(nvg, leftX, centreY - 5.0f);
        nvgClosePath(nvg);

        bool highlighted = hasKeyboardFocus(true) && ::getValue<bool>(object->locked);
        auto flagCol = highlighted ? cnv->selectedOutlineCol : cnv->guiObjectInternalOutlineCol;

        nvgFillColor(nvg, flagCol);
        nvgFill(nvg);

        input.render(nvg);
    }

    float getValue()
    {
        if (auto numbox = ptr.get<t_my_numbox>()) {
            return numbox->x_val;
        }
        return 0.0f;
    }

    float getMinimum()
    {
        if (auto numbox = ptr.get<t_my_numbox>()) {
            return numbox->x_min;
        }
        return -std::numeric_limits<float>::infinity();
    }

    float getMaximum()
    {
        if (auto numbox = ptr.get<t_my_numbox>()) {
            return numbox->x_max;
        }
        return std::numeric_limits<float>::infinity();
    }

    void setMinimum(float value)
    {
        input.setMinimum(value);
        if (auto numbox = ptr.get<t_my_numbox>()) {
            numbox->x_min = value;
        }
    }

    void setMaximum(float value)
    {
        input.setMaximum(value);
        if (auto numbox = ptr.get<t_my_numbox>()) {
            numbox->x_max = value;
        }
    }

    std::unique_ptr<ComponentBoundsConstrainer> createConstrainer() override
    {
        class NumboxBoundsConstrainer : public ComponentBoundsConstrainer {
        public:
            Object* object;
            NumberObject* numbox;

            NumboxBoundsConstrainer(Object* parent, NumberObject* nbx)
                : object(parent)
                , numbox(nbx)
            {
            }

            void checkBounds(Rectangle<int>& bounds,
                Rectangle<int> const& old,
                Rectangle<int> const& limits,
                bool isStretchingTop,
                bool isStretchingLeft,
                bool isStretchingBottom,
                bool isStretchingRight) override
            {
                auto oldBounds = old.reduced(Object::margin);
                auto newBounds = bounds.reduced(Object::margin);

                auto* nbx = reinterpret_cast<t_my_numbox*>(object->getPointer());
                auto* patch = object->cnv->patch.getPointer().get();

                if (!nbx || !patch)
                    return;

                // Calculate the width in text characters for both
                auto newCharWidth = numbox->calcNumWidth(newBounds.getWidth() - 1);

                // Set new width
                if (auto nbx = numbox->ptr.get<t_my_numbox>()) {
                    nbx->x_numwidth = newCharWidth;
                    nbx->x_gui.x_h = std::max(newBounds.getHeight(), 8);
                }

                bounds = object->gui->getPdBounds().expanded(Object::margin) + object->cnv->canvasOrigin;

                // If we're resizing the left edge, move the object left
                if (isStretchingLeft) {
                    auto x = oldBounds.getRight() - (bounds.getWidth() - Object::doubleMargin);
                    auto y = oldBounds.getY(); // don't allow y resize

                    if (auto nbx = numbox->ptr.get<t_my_numbox>()) {
                        pd::Interface::moveObject(static_cast<t_glist*>(patch), nbx.cast<t_gobj>(), x - object->cnv->canvasOrigin.x, y - object->cnv->canvasOrigin.y);
                    }
                    bounds = object->gui->getPdBounds().expanded(Object::margin) + object->cnv->canvasOrigin;
                }
            }
        };

        return std::make_unique<NumboxBoundsConstrainer>(object, this);
    }
};
