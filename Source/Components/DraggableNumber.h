/*
 // Copyright (c) 2021-2022 Timothy Schoen
 // For information on usage and redistribution, and for a DISCLAIMER OF ALL
 // WARRANTIES, see the file, "LICENSE.txt," in this distribution.
 */
#include "Utility/NanoVGGraphicsContext.h"

#pragma once

class DraggableNumber : public Label
    , public Label::Listener {
public:
    enum DragMode {
        Regular,
        Integer,
        Logarithmic
    };

protected:
    int16 decimalDrag = 0;
    int16 hoveredDecimal = -1;
    float dragValue = 0.0;
    Rectangle<float> hoveredDecimalPosition;

    float lastValue = 0.0;
    float logarithmicHeight = 256.0f;
    int16 lastLogarithmicDragPosition = 0;
    float min = 0.0, max = 0.0;

    DragMode dragMode:2 = Regular;
    bool isMinLimited:1 = false;
    bool isMaxLimited:1 = false;
    bool resetOnCommandClick:1 = false;
    bool wasReset:1 = false;
    bool showEllipses:1 = true;
    float valueToResetTo = 0.0;
    float valueToRevertTo = 0.0;
    Colour outlineColour, textColour;

    std::unique_ptr<NanoVGGraphicsContext> nvgCtx;

public:
    std::function<void(float)> onValueChange = [](float) { };
    std::function<void()> dragStart = []() { };
    std::function<void()> dragEnd = []() { };

    std::function<void(bool)> onInteraction = [](bool) { };

    explicit DraggableNumber(bool integerDrag)
        : dragMode(integerDrag ? Integer : Regular)
    {
        setWantsKeyboardFocus(true);
        addListener(this);
        setFont(Fonts::getTabularNumbersFont().withHeight(14.0f));
        lookAndFeelChanged();
    }

    void colourChanged() override
    {
        lookAndFeelChanged();
    }

    void lookAndFeelChanged() override
    {
        outlineColour = findColour(ComboBox::outlineColourId);
        textColour = findColour(Label::textColourId);
    }

    void labelTextChanged(Label* labelThatHasChanged) override { }

    void editorShown(Label* l, TextEditor& editor) override
    {
        onInteraction(true);
        dragStart();
        editor.onTextChange = [this]() {
            if (onTextChange)
                onTextChange();
        };
        editor.setJustification(Justification::centredLeft);
    }

    void editorHidden(Label*, TextEditor& editor) override
    {
        onInteraction(hasKeyboardFocus(false));
        auto newValue = editor.getText().getDoubleValue();
        setValue(newValue, dontSendNotification);
        decimalDrag = 0;
        dragEnd();
    }

    void focusGained(FocusChangeType cause) override
    {
        juce::Label::focusGained(cause);
        onInteraction(true);
    }

    void focusLost(FocusChangeType cause) override
    {
        juce::Label::focusLost(cause);
        onInteraction(false);
    }

    void setEditableOnClick(bool editable)
    {
        setEditable(editable, editable);
        setInterceptsMouseClicks(true, true);
    }

    void setMaximum(float maximum)
    {
        isMaxLimited = true;
        max = maximum;
    }

    void setMinimum(float minimum)
    {
        isMinLimited = true;
        min = minimum;
    }

    void setLogarithmicHeight(double logHeight)
    {
        logarithmicHeight = logHeight;
    }

    // Toggle between showing ellipses or ">" if number is too large to fit
    void setShowEllipsesIfTooLong(bool shouldShowEllipses)
    {
        showEllipses = shouldShowEllipses;
    }

    bool keyPressed(KeyPress const& key) override
    {
        if (isEditable())
            return false;
        // Otherwise it might catch a shortcut
        if (key.getModifiers().isCommandDown())
            return false;

        auto chr = key.getTextCharacter();

        if (!getCurrentTextEditor() && ((chr >= '0' && chr <= '9') || chr == '+' || chr == '-' || chr == '.')) {
            showEditor();
            auto* editor = getCurrentTextEditor();

            auto text = String();
            text += chr;
            editor->setText(text);
            editor->moveCaretToEnd(false);
            return true;
        }

        if (!isEditableOnSingleClick() && !getCurrentTextEditor() && key.isKeyCode(KeyPress::upKey)) {
            setValue(getText().getFloatValue() + 1.0);
            return true;
        }
        if (!isEditableOnSingleClick() && !getCurrentTextEditor() && key.isKeyCode(KeyPress::downKey)) {
            setValue(getText().getFloatValue() - 1.0);
            return true;
        }

        return false;
    }

    void setValue(float newValue, NotificationType notification = sendNotification)
    {
        wasReset = false;

        newValue = limitValue(newValue);

        if (!approximatelyEqual(lastValue, newValue)) {
            lastValue = newValue;

            setText(String(newValue, 8), notification);
            onValueChange(newValue);
        }
    }

    float getValue() const
    {
        return lastValue;
    }

    void setResetEnabled(bool enableReset)
    {
        resetOnCommandClick = enableReset;
    }

    void setResetValue(float resetValue)
    {
        valueToResetTo = resetValue;
    }

    // Make sure mouse cursor gets reset, sometimes this doesn't happen automatically
    void mouseEnter(MouseEvent const& e) override
    {
        if (e.mouseWasDraggedSinceMouseDown())
            return;

        setMouseCursor(MouseCursor::NormalCursor);
        updateMouseCursor();
    }

    void mouseExit(MouseEvent const& e) override
    {
        if (e.mouseWasDraggedSinceMouseDown())
            return;

        setMouseCursor(MouseCursor::NormalCursor);
        updateMouseCursor();

        hoveredDecimal = -1;
        repaint();
    }

    void mouseDown(MouseEvent const& e) override
    {
        if (isBeingEdited())
            return;

        onInteraction(true);

        bool command = e.mods.isCommandDown();

        if (command && resetOnCommandClick) {
            if (wasReset) {
                setValue(valueToRevertTo);
            } else {
                valueToRevertTo = lastValue;
                setValue(valueToResetTo);
                wasReset = true;
            }
        }

        dragValue = getText().getDoubleValue();

        if (dragMode != Regular) {
            decimalDrag = 0;
            lastLogarithmicDragPosition = e.y;
            return;
        }

        decimalDrag = getDecimalAtPosition(e.getMouseDownX());

        dragStart();
    }

    void setDragMode(DragMode newDragMode)
    {
        dragMode = newDragMode;
    }

    int getDecimalAtPosition(int x, Rectangle<float>* position = nullptr)
    {
        auto const textArea = getBorderSize().subtractedFrom(getLocalBounds());

        // For integer or logarithmic drag mode, draw the highlighted area around the whole number
        if (dragMode != Regular) {
            auto text = dragMode == Integer ? getText().upToFirstOccurrenceOf(".", false, false) : String(getText().getFloatValue());

            GlyphArrangement glyphs;
            glyphs.addFittedText(getFont(), text, textArea.getX(), 0., 99999, getHeight(), 1, 1.0f);
            auto glyphsBounds = glyphs.getBoundingBox(0, glyphs.getNumGlyphs(), false);
            if (x < glyphsBounds.getRight() && x < getLocalBounds().getRight()) {
                if (position)
                    *position = glyphsBounds;
                return 0;
            } else {
                if (position)
                    *position = Rectangle<float>();
                return -1;
            }
        }

        GlyphArrangement glyphs;
        auto formattedNumber = formatNumber(getText().getFloatValue());
        auto fullNumber = formattedNumber + String("000000");
        glyphs.addFittedText(getFont(), fullNumber, textArea.getX(), 0., 99999, getHeight(), 1, 1.0f);
        int draggedDecimal = -1;

        int decimalPointPosition = 0;
        bool afterDecimalPoint = false;
        for (int i = 0; i < glyphs.getNumGlyphs(); ++i) {
            auto const& glyph = glyphs.getGlyph(i);

            bool isDecimalPoint = glyph.getCharacter() == '.';
            if (isDecimalPoint) {
                decimalPointPosition = i;
                afterDecimalPoint = true;
            }

            if (x <= glyph.getRight() && glyph.getRight() < getLocalBounds().getRight()) {
                draggedDecimal = isDecimalPoint ? 0 : i - decimalPointPosition;

                auto glyphBounds = glyph.getBounds();
                if (!afterDecimalPoint) {
                    continue;
                }
                if (isDecimalPoint) {
                    glyphBounds = Rectangle<float>();
                    for (int j = 0; j < i; j++) {
                        glyphBounds = glyphBounds.getUnion(glyphs.getGlyph(j).getBounds());
                    }
                }
                if (position)
                    *position = glyphBounds;

                break;
            }
        }

        return draggedDecimal;
    }

    virtual void render(NVGcontext* nvg)
    {
        NVGScopedState scopedState(nvg);
        nvgIntersectScissor(nvg, 0, 0, getWidth(), getHeight());

        if (isBeingEdited()) {
            if (!nvgCtx || nvgCtx->getContext() != nvg)
                nvgCtx = std::make_unique<NanoVGGraphicsContext>(nvg);
            nvgCtx->setPhysicalPixelScaleFactor(2.0f);
            Graphics g(*nvgCtx);
            {
                paintEntireComponent(g, true);
            }
            return;
        }

        if (hoveredDecimal >= 0) {
            // TODO: make this colour Id configurable
            auto highlightColour = outlineColour.withAlpha(isMouseButtonDown() ? 0.5f : 0.3f);
            nvgFillColor(nvg, NVGComponent::convertColour(highlightColour));
            nvgFillRoundedRect(nvg, hoveredDecimalPosition.getX(), hoveredDecimalPosition.getY(), hoveredDecimalPosition.getWidth(), hoveredDecimalPosition.getHeight(), 2.5f);
        }

        auto font = getFont();
        auto textArea = getBorderSize().subtractedFrom(getLocalBounds()).toFloat();
        auto numberText = formatNumber(getText().getDoubleValue(), decimalDrag);
        auto extraNumberText = String();
        auto numDecimals = numberText.fromFirstOccurrenceOf(".", false, false).length();
        auto numberTextLength = CachedFontStringWidth::get()->calculateSingleLineWidth(font, numberText);

        for (int i = 0; i < std::min(hoveredDecimal - numDecimals, 7 - numDecimals); ++i)
            extraNumberText += "0";

        // If show ellipses is false, only show ">" when integers are too large to fit
        if (!showEllipses && numDecimals == 0) {
            int i = 0;
            while (numberTextLength > textArea.getWidth() + 3 && i < 5) {
                numberText = numberText.trimCharactersAtEnd(".>");
                numberText = numberText.dropLastCharacters(1);
                numberText += ">";
                numberTextLength = CachedFontStringWidth::get()->calculateSingleLineWidth(font, numberText);
                i++;
            }
        }

        nvgFontFace(nvg, "Inter-Tabular");
        nvgFontSize(nvg, font.getHeight() * 0.862f);
        nvgTextLetterSpacing(nvg, 0.275f);
        nvgTextAlign(nvg, NVG_ALIGN_MIDDLE | NVG_ALIGN_LEFT);
        nvgFillColor(nvg, NVGComponent::convertColour(textColour));
        nvgText(nvg, textArea.getX(), textArea.getCentreY() + 1.5f, numberText.toRawUTF8(), nullptr);

        if (dragMode == Regular) {
            textArea = textArea.withTrimmedLeft(numberTextLength);
            nvgFillColor(nvg, NVGComponent::convertColour(textColour.withAlpha(0.4f)));
            nvgText(nvg, textArea.getX(), textArea.getCentreY() + 1.5f, extraNumberText.toRawUTF8(), nullptr);
        }
    }

    void paint(Graphics& g) override
    {
        if (hoveredDecimal >= 0) {
            g.setColour(outlineColour.withAlpha(isMouseButtonDown() ? 0.5f : 0.3f));
            g.fillRoundedRectangle(hoveredDecimalPosition, 2.5f);
        }

        auto font = getFont();
        if (!isBeingEdited()) {
            auto textArea = getBorderSize().subtractedFrom(getLocalBounds()).toFloat();
            auto numberText = formatNumber(getText().getFloatValue(), decimalDrag);
            auto extraNumberText = String();
            auto numDecimals = numberText.fromFirstOccurrenceOf(".", false, false).length();
            auto numberTextLength = CachedFontStringWidth::get()->calculateSingleLineWidth(font, numberText);

            for (int i = 0; i < std::min(hoveredDecimal - numDecimals, 7 - numDecimals); ++i)
                extraNumberText += "0";

            // If show ellipses is false, only show ">" when integers are too large to fit
            if (!showEllipses && numDecimals == 0) {
                int i = 0;
                while (numberTextLength > textArea.getWidth() + 3 && i < 5) {
                    numberText = numberText.trimCharactersAtEnd(".>");
                    numberText = numberText.dropLastCharacters(1);
                    numberText += ">";
                    numberTextLength = CachedFontStringWidth::get()->calculateSingleLineWidth(font, numberText);
                    i++;
                }
            }

            g.setFont(font);
            g.setColour(textColour);
            g.drawText(numberText, textArea, Justification::centredLeft, showEllipses);

            if (dragMode == Regular) {
                g.setColour(textColour.withAlpha(0.4f));
                g.drawText(extraNumberText, textArea.withTrimmedLeft(numberTextLength), Justification::centredLeft, false);
            }
        }
    }

    void updateHoverPosition(int x)
    {
        int oldHoverPosition = hoveredDecimal;
        hoveredDecimal = getDecimalAtPosition(x, &hoveredDecimalPosition);

        if (oldHoverPosition != hoveredDecimal) {
            repaint();
        }
    }

    void mouseMove(MouseEvent const& e) override
    {
        updateHoverPosition(e.x);
    }

    void mouseDrag(MouseEvent const& e) override
    {
        if (isBeingEdited() || decimalDrag < 0)
            return;

        updateHoverPosition(e.getMouseDownX());

        // Hide cursor and set unbounded mouse movement
        setMouseCursor(MouseCursor::NoCursor);
        updateMouseCursor();

        auto mouseSource = Desktop::getInstance().getMainMouseSource();
        mouseSource.enableUnboundedMouseMovement(true, true);

        if (dragMode == Logarithmic) {
            float logMin = min;
            float logMax = max;

            if ((logMin == 0.0f) && (logMax == 0.0f))
                logMax = 1.0f;
            if (logMax > 0.0f) {
                if (logMin <= 0.0f)
                    logMin = 0.01f * logMax;
            } else {
                if (logMin > 0.0f)
                    logMax = 0.01f * logMin;
            }

            float dy = lastLogarithmicDragPosition - e.y;
            float k = std::exp(log(logMax / logMin) / std::max(logarithmicHeight, 10.0f));
            float factor = std::pow(k, dy);
            setValue(std::clamp(getValue(), logMin, logMax) * factor);

            lastLogarithmicDragPosition = e.y;
        } else {
            int const decimal = decimalDrag + e.mods.isShiftDown();
            float const increment = (decimal == 0) ? 1. : (1. / std::pow(10.f, decimal));
            float const deltaY = (e.y - e.mouseDownPosition.y) * 0.7f;

            // truncate value and set
            float newValue = dragValue + (increment * -deltaY);

            if (decimal > 0) {
                int const sign = (newValue > 0) ? 1 : -1;
                unsigned long long ui_temp = (newValue * std::pow(10.f, decimal)) * sign;
                newValue = (((long double)ui_temp) / std::pow(10.f, decimal) * sign);
            } else {
                newValue = static_cast<int64_t>(newValue);
            }

            setValue(newValue);
        }
    }

    float limitValue(float valueToLimit) const
    {
        if (min == 0.0 && max == 0.0)
            return valueToLimit;

        if (isMinLimited)
            valueToLimit = std::max(valueToLimit, min);
        if (isMaxLimited)
            valueToLimit = std::min(valueToLimit, max);

        return valueToLimit;
    }

    void mouseUp(MouseEvent const& e) override
    {
        if (isBeingEdited())
            return;

        onInteraction(hasKeyboardFocus(false));

        repaint();

        // Show cursor again
        setMouseCursor(MouseCursor::NormalCursor);
        updateMouseCursor();

        // Reset mouse position to where it was first clicked and disable unbounded movement
        auto mouseSource = Desktop::getInstance().getMainMouseSource();
        mouseSource.setScreenPosition(e.getMouseDownScreenPosition().toFloat());
        mouseSource.enableUnboundedMouseMovement(false);
        dragEnd();

        if (!e.mouseWasDraggedSinceMouseDown()) {
            Label::mouseUp(e);
        }
    }

    String formatNumber(double value, int precision = -1)
    {
        auto text = String(value, precision == -1 ? 8 : precision);

        if (dragMode != Integer) {
            if (!text.containsChar('.'))
                text << '.';
            text = text.trimCharactersAtEnd("0");
        }

        return text;
    }
};

struct DraggableListNumber : public DraggableNumber {
    int numberStartIdx = 0;
    int numberEndIdx = 0;

    bool targetFound = false;

    explicit DraggableListNumber()
        : DraggableNumber(true)
    {
        setEditableOnClick(true);
    }

    void mouseDown(MouseEvent const& e) override
    {
        if (isBeingEdited())
            return;

        repaint();

        auto [numberStart, numberEnd, numberValue] = getListItemAtPosition(e.x);

        numberStartIdx = numberStart;
        numberEndIdx = numberEnd;
        dragValue = numberValue;

        targetFound = numberStart != -1;
        if (targetFound) {
            dragStart();
        }
    }

    void mouseMove(MouseEvent const& e) override
    {
        updateListHoverPosition(e.x);
    }

    void mouseDrag(MouseEvent const& e) override
    {
        if (isBeingEdited() || !targetFound)
            return;

        // Hide cursor and set unbounded mouse movement
        setMouseCursor(MouseCursor::NoCursor);
        updateMouseCursor();

        auto mouseSource = Desktop::getInstance().getMainMouseSource();
        mouseSource.enableUnboundedMouseMovement(true, true);

        float const deltaY = (e.y - e.mouseDownPosition.y) * 0.7;
        float const increment = e.mods.isShiftDown() ? (0.01 * std::floor(-deltaY)) : std::floor(-deltaY);

        float newValue = dragValue + increment;

        newValue = limitValue(newValue);

        int length = numberEndIdx - numberStartIdx;

        auto replacement = String();
        replacement << newValue;

        auto newText = getText().replaceSection(numberStartIdx, length, replacement);

        // In case the length of the number changes
        if (length != replacement.length()) {
            numberEndIdx = replacement.length() + numberStartIdx;
        }

        setText(newText, dontSendNotification);
        onValueChange(0);

        updateListHoverPosition(e.getMouseDownX());
    }

    void mouseUp(MouseEvent const& e) override
    {
        if (isBeingEdited() || !targetFound)
            return;

        // Show cursor again
        setMouseCursor(MouseCursor::NormalCursor);
        updateMouseCursor();

        // Reset mouse position to where it was first clicked and disable unbounded movement
        auto mouseSource = Desktop::getInstance().getMainMouseSource();
        mouseSource.setScreenPosition(e.getMouseDownScreenPosition().toFloat());
        mouseSource.enableUnboundedMouseMovement(false);
        dragEnd();
    }

    void paint(Graphics& g) override
    {
        if (hoveredDecimal >= 0) {
            // TODO: make this colour Id configurable?
            g.setColour(outlineColour.withAlpha(isMouseButtonDown() ? 0.5f : 0.3f));
            g.fillRoundedRectangle(hoveredDecimalPosition, 2.5f);
        }

        if (!isBeingEdited()) {
            g.setColour(textColour);
            g.setFont(getFont());

            auto textArea = getBorderSize().subtractedFrom(getLocalBounds());
            g.drawText(getText(), textArea, Justification::centredLeft, false);
        }
    }

    void render(NVGcontext* nvg) override
    {
        NVGScopedState scopedState(nvg);
        nvgIntersectScissor(nvg, 0.5f, 0.5f, getWidth() - 1, getHeight() - 1);

        if (isBeingEdited()) {
            if (!nvgCtx || nvgCtx->getContext() != nvg)
                nvgCtx = std::make_unique<NanoVGGraphicsContext>(nvg);
            nvgCtx->setPhysicalPixelScaleFactor(2.0f);
            Graphics g(*nvgCtx);
            {
                paintEntireComponent(g, true);
            }
            return;
        }

        if (hoveredDecimal >= 0) {
            // TODO: make this colour Id configurable
            auto const highlightColour = outlineColour.withAlpha(isMouseButtonDown() ? 0.5f : 0.3f);
            nvgFillColor(nvg, NVGComponent::convertColour(highlightColour));
            nvgFillRoundedRect(nvg, hoveredDecimalPosition.getX(), hoveredDecimalPosition.getY() - 1, hoveredDecimalPosition.getWidth(), hoveredDecimalPosition.getHeight(), 2.5f);
        }

        nvgFontFace(nvg, "Inter-Tabular");
        nvgFontSize(nvg, getFont().getHeight() * 0.862f);
        nvgTextLetterSpacing(nvg, 0.15f);
        nvgTextAlign(nvg, NVG_ALIGN_MIDDLE | NVG_ALIGN_LEFT);
        nvgFillColor(nvg, NVGComponent::convertColour(textColour));

        auto listText = getText();
        auto const textArea = getBorderSize().subtractedFrom(getBounds());
        nvgText(nvg, textArea.getX(), textArea.getCentreY() + 1.5f, listText.toRawUTF8(), nullptr);
    }

    void editorHidden(Label* l, TextEditor& editor) override
    {
        setText(editor.getText().trimEnd(), dontSendNotification);
        onValueChange(0);
        dragEnd();
    }

    void updateListHoverPosition(int x)
    {
        int oldHoverPosition = hoveredDecimal;
        auto [numberStart, numberEnd, numberValue] = getListItemAtPosition(x, &hoveredDecimalPosition);

        hoveredDecimal = numberStart;

        if (oldHoverPosition != hoveredDecimal) {
            repaint();
        }
    }

    std::tuple<int, int, double> getListItemAtPosition(int x, Rectangle<float>* position = nullptr)
    {
        auto const textArea = getBorderSize().subtractedFrom(getBounds());

        GlyphArrangement glyphs;
        glyphs.addFittedText(getFont(), getText(), textArea.getX(), 0., 99999, textArea.getHeight(), Justification::centredLeft, 1, getMinimumHorizontalScale());

        auto text = getText();
        // Loop to find start of item
        for (int i = 0; i < glyphs.getNumGlyphs(); i++) {
            auto const& startGlyph = glyphs.getGlyph(i);

            // Don't start at whitespace
            if (startGlyph.isWhitespace())
                continue;

            // Loop from start to find end of item
            for (int j = i; j < glyphs.getNumGlyphs(); j++) {
                auto const& endGlyph = glyphs.getGlyph(j);

                // End of item when we find whitespace or end of message
                if (endGlyph.isWhitespace() || j == glyphs.getNumGlyphs() - 1) {
                    if (j == glyphs.getNumGlyphs() - 1)
                        j++;
                    auto number = text.substring(i, j);

                    // Check if item is a number and if mouse clicked on it
                    if (number.containsOnly("0123456789.-") && x >= startGlyph.getLeft() && x <= endGlyph.getRight()) {

                        if (position)
                            *position = glyphs.getBoundingBox(i, j - i, false).translated(0, 2);
                        return { i, j, number.getFloatValue() };
                    }

                    // Move start to end of current item
                    i = j;
                    break;
                }
            }
        }

        return { -1, -1, 0.0f };
    }
};
