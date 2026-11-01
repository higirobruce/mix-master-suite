#include "plugin_base/ParamTable.h"
#include <cmath>

namespace mm
{
const ParamSpec* ParamTable::find(std::string_view id) const noexcept
{
    for (const auto& s : table)
        if (id == s.id)
            return &s;
    return nullptr;
}

int ParamTable::index(std::string_view id) const noexcept
{
    for (size_t i = 0; i < table.size(); ++i)
        if (id == table[i].id)
            return static_cast<int>(i);
    return -1;
}

static juce::String formatValue(const ParamSpec& s, float v)
{
    const juce::String unit = juce::String::fromUTF8(s.unit);
    const float a = std::abs(v);
    const int decimals = a >= 1000.0f ? 0 : (a >= 100.0f ? 1 : 2);
    return juce::String(v, decimals) + (unit.isEmpty() ? juce::String() : (" " + unit));
}

juce::AudioProcessorValueTreeState::ParameterLayout ParamTable::makeLayout() const
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (const auto& s : table)
    {
        const juce::ParameterID pid(s.id, 1);
        switch (s.type)
        {
        case ParamType::Float:
        {
            juce::NormalisableRange<float> range(s.min, s.max, 0.0f, s.skew);
            auto attrs = juce::AudioParameterFloatAttributes()
                             .withLabel(juce::String::fromUTF8(s.unit))
                             .withStringFromValueFunction([spec = s](float v, int) { return formatValue(spec, v); })
                             .withValueFromStringFunction([](const juce::String& t) { return t.getFloatValue(); });
            layout.add(std::make_unique<juce::AudioParameterFloat>(pid, s.name, range, s.def, attrs));
            break;
        }
        case ParamType::Int:
        {
            auto attrs = juce::AudioParameterIntAttributes().withLabel(juce::String::fromUTF8(s.unit));
            layout.add(std::make_unique<juce::AudioParameterInt>(pid, s.name, static_cast<int>(s.min),
                                                                 static_cast<int>(s.max), static_cast<int>(s.def), attrs));
            break;
        }
        case ParamType::Bool:
            layout.add(std::make_unique<juce::AudioParameterBool>(pid, s.name, s.def >= 0.5f));
            break;
        case ParamType::Choice:
        {
            juce::StringArray choices;
            for (int i = 0; i < s.numChoices; ++i)
                choices.add(s.choices[i]);
            layout.add(std::make_unique<juce::AudioParameterChoice>(pid, s.name, choices, static_cast<int>(s.def)));
            break;
        }
        }
    }
    return layout;
}
} // namespace mm
