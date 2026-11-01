// mm_snapshot <out-dir> [scale]  -> <out-dir>/utility-dark.png, utility-light.png
#include "gui/PluginEditorBase.h"
#include "plugins/utility/UtilityProcessor.h"
#include <juce_gui_extra/juce_gui_extra.h>
#include <cstdio>

int main(int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI init;
    juce::MessageManager::getInstance()->setCurrentThreadAsMessageThread();
    const juce::File outDir = argc > 1 ? juce::File::getCurrentWorkingDirectory().getChildFile(argv[1])
                                       : juce::File::getCurrentWorkingDirectory();
    const float scale = argc > 2 ? static_cast<float>(std::atof(argv[2])) : 1.0f;
    outDir.createDirectory();
    UtilityProcessor proc;
    proc.prepareToPlay(48000.0, 64);
    // some signal so the meters show something
    juce::AudioBuffer<float> buf(2, 64);
    juce::MidiBuffer midi;
    for (int b = 0; b < 200; ++b)
    {
        for (int n = 0; n < 64; ++n)
        {
            buf.setSample(0, n, 0.35f * std::sin(0.1f * static_cast<float>(b * 64 + n)));
            buf.setSample(1, n, 0.2f * std::sin(0.13f * static_cast<float>(b * 64 + n)));
        }
        proc.processBlock(buf, midi);
    }
    std::unique_ptr<juce::AudioProcessorEditor> editor(proc.createEditor());
    auto* base = dynamic_cast<mm::gui::PluginEditorBase*>(editor.get());
    if (base == nullptr)
        return 1;
    base->setScale(scale);
    for (bool dark : {true, false})
    {
        base->setTheme(dark ? mm::gui::Theme::dark(mm::gui::accents::utility) : mm::gui::Theme::light(mm::gui::accents::utility));
        // pump the meter timer once
        juce::Timer::callPendingTimersSynchronously();
        auto img = editor->createComponentSnapshot(editor->getLocalBounds(), true, 1.0f);
        juce::PNGImageFormat png;
        auto file = outDir.getChildFile(juce::String("utility-") + (dark ? "dark" : "light") + ".png");
        file.deleteFile();
        juce::FileOutputStream os(file);
        if (!png.writeImageToStream(img, os))
            return 2;
        std::printf("%s %dx%d\n", file.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight());
    }
    return 0;
}
