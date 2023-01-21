/*
 // Copyright (c) 2021-2022 Timothy Schoen
 // For information on usage and redistribution, and for a DISCLAIMER OF ALL
 // WARRANTIES, see the file, "LICENSE.txt," in this distribution.
 */

#include "../Utility/DraggableNumber.h"

class FloatAtomObject final : public ObjectBase {

    AtomHelper atomHelper;
    DraggableNumber input;

    Value min = Value(0.0f);
    Value max = Value(0.0f);

    float value = 0.0f;

public:
    FloatAtomObject(void* obj, Object* parent)
        : ObjectBase(obj, parent)
        , atomHelper(obj, parent, this)
        , input(false)
    {
        input.onEditorShow = [this]() {
            auto* editor = input.getCurrentTextEditor();
            startEdition();

            editor->setBorder({ 0, 1, 3, 0 });

            if (editor != nullptr) {
                editor->setInputRestrictions(0, ".-0123456789");
            }
        };

        input.onEditorHide = [this]() {
            sendFloatValue(input.getText().getFloatValue());
            stopEdition();
        };

        addAndMakeVisible(input);

        input.setText(input.formatNumber(value), dontSendNotification);

        min = getMinimum();
        max = getMaximum();

        addMouseListener(this, true);

        input.dragStart = [this]() { startEdition(); };

        input.valueChanged = [this](float newValue) {
            sendFloatValue(newValue);
        };

        input.dragEnd = [this]() { stopEdition(); };
    }

    void focusGained(FocusChangeType cause) override
    {
        repaint();
    }

    void focusLost(FocusChangeType cause) override
    {
        repaint();
    }

    void focusOfChildComponentChanged(FocusChangeType cause) override
    {
        repaint();
    }

    void paint(Graphics& g)
    {
        getLookAndFeel().setColour(Label::textWhenEditingColourId, object->findColour(PlugDataColour::canvasTextColourId));
        getLookAndFeel().setColour(Label::textColourId, object->findColour(PlugDataColour::canvasTextColourId));
        getLookAndFeel().setColour(TextEditor::textColourId, object->findColour(PlugDataColour::canvasTextColourId));

        g.setColour(object->findColour(PlugDataColour::defaultObjectBackgroundColourId));
        g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), PlugDataLook::objectCornerRadius);
    }

    void paintOverChildren(Graphics& g) override
    {
        g.setColour(object->findColour(PlugDataColour::outlineColourId));
        Path triangle;
        triangle.addTriangle(Point<float>(getWidth() - 8, 0), Point<float>(getWidth(), 0), Point<float>(getWidth(), 8));
        triangle = triangle.createPathWithRoundedCorners(4.0f);
        g.fillPath(triangle);

        bool selected = cnv->isSelected(object) && !cnv->isGraph;
        auto outlineColour = object->findColour(selected ? PlugDataColour::objectSelectedOutlineColourId : objectOutlineColourId);

        g.setColour(outlineColour);
        g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), PlugDataLook::objectCornerRadius, 1.0f);

        bool highlighed = hasKeyboardFocus(true) && static_cast<bool>(object->locked.getValue());

        if (highlighed) {
            g.setColour(object->findColour(PlugDataColour::objectSelectedOutlineColourId));
            g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(1.0f), PlugDataLook::objectCornerRadius, 2.0f);
        }
    }

    void updateLabel() override
    {
        atomHelper.updateLabel(label);
    }

    void updateBounds() override
    {
        pd->getCallbackLock()->enter();

        auto* atom = static_cast<t_fake_gatom*>(ptr);

        int x, y, w, h;
        libpd_get_object_bounds(cnv->patch.getPointer(), atom, &x, &y, &w, &h);

        w = std::max<int>(4, atom->a_text.te_width) * glist_fontwidth(cnv->patch.getPointer());

        auto bounds = Rectangle<int>(x, y, w, atomHelper.getAtomHeight());

        pd->getCallbackLock()->exit();

        object->setObjectBounds(bounds);
    }

    void checkBounds() override
    {
        // Apply size limits
        int w = jlimit(30, maxSize, object->getWidth());
        int h = atomHelper.getAtomHeight() + Object::doubleMargin;

        if (w != object->getWidth() || h != object->getHeight()) {
            object->setSize(w, h);
        }
    }

    void applyBounds() override
    {
        auto b = object->getObjectBounds();
        libpd_moveobj(cnv->patch.getPointer(), static_cast<t_gobj*>(ptr), b.getX(), b.getY());

        int fontWidth = glist_fontwidth(cnv->patch.getPointer());

        auto* atom = static_cast<t_fake_gatom*>(ptr);
        atom->a_text.te_width = b.getWidth() / fontWidth;
    }

    void resized() override
    {
        int fontWidth = glist_fontwidth(cnv->patch.getPointer());
        int width = jlimit(30, ObjectBase::maxSize, (getWidth() / fontWidth) * fontWidth);
        int height = jlimit(12, ObjectBase::maxSize, getHeight());
        if (getWidth() != width || getHeight() != height) {
            object->setSize(width + Object::doubleMargin, height + Object::doubleMargin);
        }

        input.setBounds(getLocalBounds());
        input.setFont(getHeight() - 6);
    }

    void lock(bool isLocked) override
    {
        setInterceptsMouseClicks(isLocked, isLocked);
    }

    ObjectParameters getParameters() override
    {
        ObjectParameters allParameters = { { "Minimum", tFloat, cGeneral, &min, {} }, { "Maximum", tFloat, cGeneral, &max, {} } };

        auto atomParameters = atomHelper.getParameters();
        allParameters.insert(allParameters.end(), atomParameters.begin(), atomParameters.end());

        return allParameters;
    }

    void valueChanged(Value& value) override
    {
        if (value.refersToSameSourceAs(min)) {
            setMinimum(static_cast<float>(min.getValue()));
        } else if (value.refersToSameSourceAs(max)) {
            setMaximum(static_cast<float>(max.getValue()));
        } else {
            atomHelper.valueChanged(value);
        }
    }

    float getValue()
    {
        return atom_getfloat(fake_gatom_getatom(static_cast<t_fake_gatom*>(ptr)));
    }

    float getMinimum()
    {
        auto const* gatom = static_cast<t_fake_gatom const*>(ptr);
        return gatom->a_draglo;
    }

    float getMaximum()
    {
        auto const* gatom = static_cast<t_fake_gatom const*>(ptr);
        return gatom->a_draghi;
    }

    void setMinimum(float value)
    {
        auto* gatom = static_cast<t_fake_gatom*>(ptr);
        input.setMinimum(value);
        gatom->a_draglo = value;
    }
    void setMaximum(float value)
    {
        auto* gatom = static_cast<t_fake_gatom*>(ptr);
        input.setMaximum(value);
        gatom->a_draghi = value;
    }

    void receiveObjectMessage(String const& symbol, std::vector<pd::Atom>& atoms) override
    {
        if (symbol == "float") {
            auto min = getMinimum();
            auto max = getMaximum();

            if (min != 0 || max != 0) {
                value = std::clamp(atoms[0].getFloat(), min, max);
            } else {
                value = atoms[0].getFloat();
            }

            input.setText(input.formatNumber(value), dontSendNotification);
        } else if (symbol == "send" && atoms.size() >= 1) {
            setParameterExcludingListener(atomHelper.sendSymbol, atoms[0].getSymbol());
        } else if (symbol == "receive" && atoms.size() >= 1) {
            setParameterExcludingListener(atomHelper.receiveSymbol, atoms[0].getSymbol());
        }
    };
};
