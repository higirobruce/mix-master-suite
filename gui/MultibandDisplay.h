#pragma once
#include "gui/SpectrumDisplay.h"
#include "gui/Theme.h"
#include <array>
#include <functional>

namespace mm::gui
{
// Multiband display: the output spectrum behind draggable crossover lines, each band's gain reduction drawn as a
// shaded region over its frequency range from the top, the focused band outlined. Click a band to focus it; drag a
// crossover line to move it (the processor keeps them a third of an octave apart; the drag is clamped to the same).
class MultibandDisplay : public juce::Component
{
public:
    static constexpr int kMaxBands = 5;
    MultibandDisplay(SpectrumAnalyser* post, double sampleRate);

    std::function<void(int crossover, double hz)> onCrossoverDrag;
    std::function<void(int crossover, bool starting)> onCrossoverGesture;
    std::function<void(int band)> onFocusBand;

    void setBands(int numBands, const double* crossoverHz); // numBands - 1 crossovers
    void setBandReduction(int band, float db, float levelDb);
    void focusBand(int band);
    int focusedBand() const noexcept { return focused; }
    int numCrossovers() const noexcept { return bands - 1; }
    void dragCrossoverTo(int crossover, float x); // programmatic (tests)
    float xForHz(double hz) const noexcept;
    double hzForX(float x) const noexcept;
    void setSampleRate(double sr) noexcept { spectrum.setSampleRate(sr); }
    void pull() { spectrum.pull(); }
    void setTheme(const Theme& t);

    void paintOverChildren(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;

private:
    int crossoverAt(juce::Point<float> p) const;
    int bandAt(float x) const;
    juce::Rectangle<float> plotArea() const noexcept { return getLocalBounds().toFloat().reduced(8.0f, 6.0f); }

    SpectrumDisplay spectrum;
    Theme theme = Theme::dark(accents::mastering);
    int bands = 4, focused = 0, dragging = -1;
    std::array<double, kMaxBands - 1> crossovers{120.0, 600.0, 2500.0, 8000.0};
    std::array<float, kMaxBands> reductionDb{}, levelDb{};
    static constexpr double kMinHz = 20.0, kMaxHz = 20000.0;
    static constexpr float kRangeDb = 24.0f;
};
} // namespace mm::gui
