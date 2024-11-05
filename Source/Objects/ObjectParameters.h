#pragma once

#include <juce_data_structures/juce_data_structures.h>

#include <utility>
#include "LookAndFeel.h"

enum ParameterType {
    tString,
    tInt,
    tFloat,
    tColour,
    tBool,
    tCombo,
    tRangeFloat,
    tRangeInt,
    tFont,
    tCustom
};

enum ParameterCategory {
    cDimensions,
    cGeneral,
    cAppearance,
    cLabel
};

class PropertiesPanelProperty;
using CustomPanelCreateFn = std::function<PropertiesPanelProperty*(void)>;
using InteractionFn = std::function<void(bool)>;

struct ObjectParameter
{
    String name;
    ParameterType type;
    ParameterCategory category;
    Value* valuePtr;
    StringArray options;
    var defaultValue;
    CustomPanelCreateFn createFn;
    InteractionFn interactionFn;

    ObjectParameter(String name, ParameterType type, ParameterCategory category, Value* valuePtr, StringArray options, var defaultValue, CustomPanelCreateFn createFn, InteractionFn interactionFn)
            : name(name), type(type), category(category), valuePtr(valuePtr), options(options), defaultValue(defaultValue), createFn(createFn), interactionFn(interactionFn)
    {}
};

class ObjectParameters {
public:
    ObjectParameters() = default;

    HeapArray<ObjectParameter> getParameters()
    {
        return objectParameters;
    }

    void addParam(ObjectParameter const& param)
    {
        objectParameters.add(param);
    }

    void resetAll()
    {
        auto& lnf = LookAndFeel::getDefaultLookAndFeel();
        for (auto param : objectParameters) {
            if (!param.defaultValue.isVoid()) {
                if (param.type == tColour) {
                    param.valuePtr->setValue(lnf.findColour(param.defaultValue).toString());
                } else if (param.defaultValue.isArray() && param.defaultValue.getArray()->isEmpty()) {
                    return;
                } else {
                    param.valuePtr->setValue(param.defaultValue);
                }
            }
        }
    }

    // ========= overloads for making different types of parameters =========

    void addParamFloat(String const& pString, ParameterCategory pCat, Value* pVal, var const& pDefault = var())
    {
        objectParameters.add(makeParam(pString, tFloat, pCat, pVal, StringArray(), pDefault));
    }

    void addParamInt(String const& pString, ParameterCategory pCat, Value* pVal, var const& pDefault = var(), InteractionFn onInteractionFn = nullptr)
    {
        objectParameters.add(makeParam(pString, tInt, pCat, pVal, StringArray(), pDefault, nullptr, onInteractionFn));
    }

    void addParamBool(String const& pString, ParameterCategory pCat, Value* pVal, StringArray const& pList, var const& pDefault = var())
    {
        objectParameters.add(makeParam(pString, tBool, pCat, pVal, pList, pDefault));
    }

    void addParamString(String const& pString, ParameterCategory pCat, Value* pVal, var const& pDefault = var())
    {
        objectParameters.add(makeParam(pString, tString, pCat, pVal, StringArray(), pDefault));
    }

    void addParamColour(String const& pString, ParameterCategory pCat, Value* pVal, var const& pDefault = var())
    {
        objectParameters.add(makeParam(pString, tColour, pCat, pVal, StringArray(), pDefault));
    }

    void addParamColourFG(Value* pVal)
    {
        objectParameters.add(makeParam("Foreground color", tColour, cAppearance, pVal, StringArray(), PlugDataColour::canvasTextColourId));
    }

    void addParamColourBG(Value* pVal)
    {
        objectParameters.add(makeParam("Background color", tColour, cAppearance, pVal, StringArray(), PlugDataColour::guiObjectBackgroundColourId));
    }

    void addParamColourLabel(Value* pVal)
    {
        objectParameters.add(makeParam("Label color", tColour, cLabel, pVal, StringArray(), PlugDataColour::canvasTextColourId));
    }

    void addParamReceiveSymbol(Value* pVal)
    {
        objectParameters.add(makeParam("Receive Symbol", tString, cGeneral, pVal, StringArray(), ""));
    }

    void addParamSendSymbol(Value* pVal, String const& pDefault = "")
    {
        objectParameters.add(makeParam("Send Symbol", tString, cGeneral, pVal, StringArray(), pDefault));
    }

    void addParamCombo(String const& pString, ParameterCategory pCat, Value* pVal, StringArray const& pStringList, var const& pDefault = var())
    {
        objectParameters.add(makeParam(pString, tCombo, pCat, pVal, pStringList, pDefault));
    }

    void addParamRange(String const& pString, ParameterCategory pCat, Value* pVal, VarArray const& pDefault = VarArray())
    {
        objectParameters.add(makeParam(pString, tRangeFloat, pCat, pVal, StringArray(), pDefault));
    }

    void addParamFont(String const& pString, ParameterCategory pCat, Value* pVal, String const& pDefault = String())
    {
        objectParameters.add(makeParam(pString, tFont, pCat, pVal, StringArray(), pDefault));
    }

    void addParamPosition(Value* positionValue)
    {
        objectParameters.add(makeParam("Position", tRangeInt, cDimensions, positionValue, StringArray(), var()));
    }

    void addParamSize(Value* sizeValue, bool singleDimension = false)
    {
        objectParameters.add(makeParam("Size", singleDimension ? tInt : tRangeInt, cDimensions, sizeValue, StringArray(), var()));
    }

    void addParamCustom(CustomPanelCreateFn customComponentFn)
    {
        objectParameters.add(makeParam("", tCustom, cGeneral, nullptr, StringArray(), var(), customComponentFn));
    }

private:
    HeapArray<ObjectParameter> objectParameters;

    static ObjectParameter makeParam(String const& pString, ParameterType pType, ParameterCategory pCat, Value* pVal, StringArray const& pStringList, var const& pDefault, CustomPanelCreateFn customComponentFn = nullptr, InteractionFn onInteractionFn = nullptr)
    {
        return {pString, pType, pCat, pVal, pStringList, pDefault, customComponentFn, onInteractionFn};
    }
};
