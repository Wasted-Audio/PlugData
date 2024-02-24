/*
 // Copyright (c) 2021-2022 Timothy Schoen
 // For information on usage and redistribution, and for a DISCLAIMER OF ALL
 // WARRANTIES, see the file, "LICENSE.txt," in this distribution.
 */

class BangObject final : public ObjectBase {
    uint32_t lastBang = 0;

    Value bangInterrupt = SynchronousValue(100.0f);
    Value bangHold = SynchronousValue(40.0f);
    Value sizeProperty = SynchronousValue();

    bool bangState = false;
    bool alreadyBanged = false;

    IEMHelper iemHelper;

public:
    BangObject(pd::WeakReference obj, Object* parent)
        : ObjectBase(obj, parent)
        , iemHelper(obj, parent, this)
    {
        onConstrainerCreate = [this]() {
            constrainer->setFixedAspectRatio(1);
        };

        objectParameters.addParamSize(&sizeProperty, true);
        objectParameters.addParamInt("Min. flash time", cGeneral, &bangInterrupt, 50);
        objectParameters.addParamInt("Max. flash time", cGeneral, &bangHold, 250);

        iemHelper.addIemParameters(objectParameters, true, true, 17, 7);
    }

    void update() override
    {
        if (auto bng = ptr.get<t_bng>()) {
            sizeProperty = bng->x_gui.x_w;
            bangInterrupt = bng->x_flashtime_break;
            bangHold = bng->x_flashtime_hold;
        }

        iemHelper.update();
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

    void toggleObject(Point<int> position) override
    {
        if (!alreadyBanged) {

            startEdition();
            if (auto bng = ptr.get<t_pd>())
                pd_bang(bng.get());
            stopEdition();

            trigger();
            alreadyBanged = true;
        }
    }

    void untoggleObject() override
    {
        alreadyBanged = false;
    }

    void mouseDown(MouseEvent const& e) override
    {
        if (!e.mods.isLeftButtonDown())
            return;

        //startEdition();
        pd->enqueueFunctionAsync<t_pd>(ptr, [](t_pd* bng){
            pd_bang(bng);
        });
        //stopEdition();

        // Make sure we don't re-click with an accidental drag
        alreadyBanged = true;
        trigger();
    }

    void paint(Graphics& g) override
    {
        g.setColour(iemHelper.getBackgroundColour());
        g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), Corners::objectCornerRadius);

        bool selected = object->isSelected() && !cnv->isGraph;
        auto outlineColour = object->findColour(selected ? PlugDataColour::objectSelectedOutlineColourId : objectOutlineColourId);

        g.setColour(outlineColour);
        g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), Corners::objectCornerRadius, 1.0f);

        auto const bounds = getLocalBounds().reduced(1).toFloat();
        auto const width = std::max(bounds.getWidth(), bounds.getHeight());

        auto const sizeReduction = std::min(1.0f, getWidth() / 20.0f);
        
        float const circleOuter = 80.f * (width * 0.01f);
        float const circleThickness = std::max(width * 0.06f, 1.5f) * sizeReduction;

        g.setColour(object->findColour(PlugDataColour::guiObjectInternalOutlineColour));
        g.drawEllipse(bounds.reduced((width - circleOuter) * sizeReduction), circleThickness);

        if (bangState) {
            g.setColour(iemHelper.getForegroundColour());
            g.fillEllipse(bounds.reduced((width - circleOuter + circleThickness) * sizeReduction));
        }
    }

    void trigger()
    {
        if (bangState)
            return;

        bangState = true;
        repaint();

        auto currentTime = Time::getCurrentTime().getMillisecondCounter();
        auto timeSinceLast = currentTime - lastBang;

        int holdTime = bangHold.getValue();

        if (timeSinceLast < getValue<int>(bangHold) * 2) {
            holdTime = timeSinceLast / 2;
        }
        if (holdTime < bangInterrupt) {
            holdTime = bangInterrupt.getValue();
        }

        lastBang = currentTime;

        auto deletionChecker = SafePointer(this);
        Timer::callAfterDelay(holdTime,
            [deletionChecker, this]() mutable {
                // First check if this object still exists
                if (!deletionChecker)
                    return;

                if (bangState) {
                    bangState = false;
                    repaint();
                }
            });
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
            auto size = std::max(getValue<int>(sizeProperty), constrainer->getMinimumWidth());
            setParameterExcludingListener(sizeProperty, size);
            if (auto bng = ptr.get<t_bng>()) {
                bng->x_gui.x_w = size;
                bng->x_gui.x_h = size;
            }
            object->updateBounds();
        } else if (value.refersToSameSourceAs(bangInterrupt)) {
            if (auto bng = ptr.get<t_bng>())
                bng->x_flashtime_break = bangInterrupt.getValue();
        } else if (value.refersToSameSourceAs(bangHold)) {
            if (auto bng = ptr.get<t_bng>())
                bng->x_flashtime_hold = bangHold.getValue();
        } else {
            iemHelper.valueChanged(value);
        }
    }

    void receiveObjectMessage(hash32 symbol, pd::Atom const atoms[8], int numAtoms) override
    {
        switch (symbol) {
        case hash("float"):
        case hash("bang"):
        case hash("list"):
            trigger();
            break;
        case hash("flashtime"): {
            if (numAtoms > 0)
                setParameterExcludingListener(bangInterrupt, atoms[0].getFloat());
            if (numAtoms > 1)
                setParameterExcludingListener(bangHold, atoms[1].getFloat());
            break;
        }
        case hash("pos"):
        case hash("size"):
        case hash("loadbang"):
            break;
        default: {
            bool wasIemMessage = iemHelper.receiveObjectMessage(symbol, atoms, numAtoms);
            if (!wasIemMessage) {
                trigger();
            }
            break;
        }
        }
    }
};
