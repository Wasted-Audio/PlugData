/*
 // Copyright (c) 2021-2022 Timothy Schoen
 // For information on usage and redistribution, and for a DISCLAIMER OF ALL
 // WARRANTIES, see the file, "LICENSE.txt," in this distribution.
 */

class ToggleObject final : public ObjectBase {
    std::atomic<bool> toggleState = false;
    bool alreadyToggled = false;
    Value nonZero = SynchronousValue();
    Value sizeProperty = SynchronousValue();

    float value = 0.0f;

    IEMHelper iemHelper;

public:
    ToggleObject(pd::WeakReference ptr, Object* object)
        : ObjectBase(ptr, object)
        , iemHelper(ptr, object, this)
    {
        onConstrainerCreate = [this]() {
            constrainer->setFixedAspectRatio(1);
        };

        objectParameters.addParamFloat("Non-zero value", cGeneral, &nonZero, 1.0f);
        objectParameters.addParamSize(&sizeProperty, true);

        iemHelper.addIemParameters(objectParameters, true, true, 17, 7);
    }

    bool hideInlets() override
    {
        return iemHelper.hasReceiveSymbol();
    }

    bool hideOutlets() override
    {
        return iemHelper.hasSendSymbol();
    }

    void updateLabel() override
    {
        iemHelper.updateLabel(label);
    }

    Rectangle<int> getPdBounds() override
    {
        return iemHelper.getPdBounds();
    }

    void setPdBounds(Rectangle<int> b) override
    {
        iemHelper.setPdBounds(b);
    }

    void update() override
    {
        if (auto toggle = ptr.get<t_toggle>()) {
            sizeProperty = toggle->x_gui.x_w;
            nonZero = toggle->x_nonzero;
        }

        iemHelper.update();

        value = getValue();
        setToggleStateFromFloat(value);
    }
    
    
    void render(NVGcontext* nvg) override
    {
        auto b = getLocalBounds().toFloat();
        
        auto backgroundColour = convertColour(iemHelper.getBackgroundColour());
        auto toggledColour = convertColour(iemHelper.getForegroundColour()); // TODO: this is some bad threading practice!
        auto untoggledColour = convertColour(iemHelper.getForegroundColour().interpolatedWith(iemHelper.getBackgroundColour(), 0.8f));
        auto selectedOutlineColour = convertColour(object->findColour(PlugDataColour::objectSelectedOutlineColourId));
        auto outlineColour = convertColour(object->findColour(PlugDataColour::objectOutlineColourId));
        auto internalLineColour = convertColour(object->findColour(PlugDataColour::guiObjectInternalOutlineColour));

        nvgBeginPath(nvg);
        NVGpaint rectPaint = nvgRoundedRectPaint(nvg, b.getX(), b.getY(), b.getWidth(), b.getHeight(), backgroundColour, object->isSelected() ? selectedOutlineColour : outlineColour, Corners::objectCornerRadius);
        nvgFillPaint(nvg, rectPaint);
        nvgRect(nvg, b.getX() - 0.5f, b.getY() - 0.5f, b.getWidth() + 1.0f, b.getHeight() + 1.0f);
        nvgFill(nvg);
        
        auto const sizeReduction = std::min(1.0f, getWidth() / 20.0f);
        float margin = (getWidth() * 0.08f + 4.5f) * sizeReduction;
        auto crossBounds = getLocalBounds().toFloat().reduced(margin);
        
        auto const max = std::max(crossBounds.getWidth(), crossBounds.getHeight());
        auto strokeWidth = std::max(max * 0.15f, 2.0f) * sizeReduction;
        
        nvgBeginPath(nvg);
        nvgMoveTo(nvg, crossBounds.getX(), crossBounds.getY());
        nvgLineTo(nvg, crossBounds.getRight(), crossBounds.getBottom());
        nvgMoveTo(nvg, crossBounds.getRight(), crossBounds.getY());
        nvgLineTo(nvg, crossBounds.getX(), crossBounds.getBottom());
        nvgStrokeColor(nvg, toggleState ? toggledColour : untoggledColour);
        nvgStrokeWidth(nvg, strokeWidth);
        nvgStroke(nvg);
    }

    void paint(Graphics& g) override
    {
        g.setColour(iemHelper.getBackgroundColour());
        g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), Corners::objectCornerRadius);

        bool selected = object->isSelected() && !cnv->isGraph;
        auto outlineColour = object->findColour(selected ? PlugDataColour::objectSelectedOutlineColourId : objectOutlineColourId);

        g.setColour(outlineColour);
        g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), Corners::objectCornerRadius, 1.0f);

        auto toggledColour = iemHelper.getForegroundColour();
        auto untoggledColour = toggledColour.interpolatedWith(iemHelper.getBackgroundColour(), 0.8f);
        g.setColour(toggleState ? toggledColour : untoggledColour);

        auto const sizeReduction = std::min(1.0f, getWidth() / 20.0f);
        float margin = (getWidth() * 0.08f + 4.5f) * sizeReduction;
        auto crossBounds = getLocalBounds().toFloat().reduced(margin);
        
        auto const max = std::max(crossBounds.getWidth(), crossBounds.getHeight());
        auto strokeWidth = std::max(max * 0.15f, 2.0f) * sizeReduction;
        
        if (getWidth() < 18) {
            //crossBounds = getLocalBounds().toFloat().reduced(1.5f);
        }
        
        g.drawLine({ crossBounds.getTopLeft(), crossBounds.getBottomRight() }, strokeWidth);
        g.drawLine({ crossBounds.getBottomLeft(), crossBounds.getTopRight() }, strokeWidth);
    }

    void toggleObject(Point<int> position) override
    {
        ignoreUnused(position);

        if (!alreadyToggled) {
            startEdition();
            auto newValue = value != 0 ? 0 : ::getValue<float>(nonZero);
            sendToggleValue(newValue);
            setToggleStateFromFloat(newValue);
            stopEdition();
            alreadyToggled = true;
        }
    }

    void sendToggleValue(float newValue)
    {
        if (auto iem = ptr.get<t_iemgui>()) {
            t_atom atom;
            SETFLOAT(&atom, newValue);
            pd_typedmess(iem.cast<t_pd>(), pd->generateSymbol("set"), 1, &atom);

            outlet_float(iem->x_obj.ob_outlet, newValue);
            if (iem->x_fsf.x_snd_able && iem->x_snd->s_thing)
                pd_float(iem->x_snd->s_thing, newValue);
        }
    }

    void untoggleObject() override
    {
        alreadyToggled = false;
        repaint();
    }

    void mouseDown(MouseEvent const& e) override
    {
        if (!e.mods.isLeftButtonDown())
            return;

        startEdition();
        auto newValue = value != 0 ? 0 : ::getValue<float>(nonZero);
        sendToggleValue(newValue);
        setToggleStateFromFloat(newValue);
        stopEdition();

        // Make sure we don't re-toggle with an accidental drag
        alreadyToggled = true;
    }

    void setToggleStateFromFloat(float newValue)
    {
        value = newValue;
        toggleState = std::abs(newValue) > std::numeric_limits<float>::epsilon();
        repaint();
    }

    void receiveObjectMessage(hash32 symbol, pd::Atom const atoms[8], int numAtoms) override
    {
        switch (symbol) {
        case hash("bang"): {
            value = !value;
            setToggleStateFromFloat(value);
            break;
        }
        case hash("float"):
        case hash("list"):
        case hash("set"): {
            value = atoms[0].getFloat();
            setToggleStateFromFloat(value);
            break;
        }
        case hash("nonzero"): {
            if (numAtoms >= 1)
                setParameterExcludingListener(nonZero, atoms[0].getFloat());
            break;
        }
        default: {
            iemHelper.receiveObjectMessage(symbol, atoms, numAtoms);
            break;
        }
        }
    }

    void updateSizeProperty() override
    {
        setPdBounds(object->getObjectBounds());

        if (auto iem = ptr.get<t_iemgui>()) {
            setParameterExcludingListener(sizeProperty, var(iem->x_w));
        }
    }

    void valueChanged(Value& value) override
    {
        if (value.refersToSameSourceAs(sizeProperty)) {
            auto* constrainer = getConstrainer();
            auto size = std::max(::getValue<int>(sizeProperty), constrainer->getMinimumWidth());
            setParameterExcludingListener(sizeProperty, size);

            if (auto tgl = ptr.get<t_toggle>()) {
                tgl->x_gui.x_w = size;
                tgl->x_gui.x_h = size;
            }

            object->updateBounds();
        } else if (value.refersToSameSourceAs(nonZero)) {
            float val = nonZero.getValue();
            if (auto toggle = ptr.get<t_toggle>()) {
                toggle->x_nonzero = val;
            }
        } else {
            iemHelper.valueChanged(value);
        }
    }

    float getValue()
    {
        if (auto toggle = ptr.get<t_toggle>())
            return toggle->x_on;

        return 0.0f;
    }
};
