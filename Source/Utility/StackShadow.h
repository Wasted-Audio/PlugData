#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "NVGSurface.h"

// This needs to be defined before using namespace JUCE
namespace melatonin {
class DropShadow;
}

struct StackShadow : public juce::DeletedAtShutdown {
    StackShadow();

    ~StackShadow() override;

    static void renderDropShadow(hash32 id, juce::Graphics& g, juce::Path const& path, juce::Colour color, int radius = 1, juce::Point<int> offset = { 0, 0 }, int spread = 0);

    std::unordered_map<hash32, std::unique_ptr<melatonin::DropShadow>> dropShadow;

    JUCE_DECLARE_SINGLETON(StackShadow, false)
};
