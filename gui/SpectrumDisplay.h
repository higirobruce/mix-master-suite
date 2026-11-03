#pragma once
#include "core/metering/Spectrum.h"
#include "gui/Theme.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>

namespace mm::gui
{
// Log-frequency spectrum behind an EQ curve: pulls frames from up to two analysers (pre dim, post bright).
class SpectrumDisplay : public juce::Component
{
public:
    SpectrumDisplay(mm::SpectrumAnalyser* pre, mm::SpectrumAnalyser* post, double sampleRateHint = 48000.0,
                    double minHz = 20.0, double maxHz = 20000.0);
    void setSampleRate(double sr) noexcept { sampleRate = sr; }
    void setTheme(const Theme& t);
    void setVisiblePrePost(bool showPre, bool showPost) noexcept;
    void pull(); // editor timer: fetch new frames
    void setMarkerHz(float hz); // vertical accent line (0 = none): the imager's mono-below frequency
    float markerHz() const noexcept { return marker; }
    void paint(juce::Graphics&) override;

private:
    void drawSpectrum(juce::Graphics&, const std::vector<float>& magDb, juce::Colour colour, bool fill);
    mm::SpectrumAnalyser *preAnalyser, *postAnalyser;
    std::vector<float> preMag, postMag;
    double sampleRate, minHz, maxHz;
    bool showPreFlag = true, showPostFlag = true;
    float marker = 0.0f;
    Theme theme = Theme::dark(accents::eq);
};
} // namespace mm::gui
