#include "MotionEditor.h"
#include "../source/PluginEditor.h"
#include <juce_audio_utils/juce_audio_utils.h>

class MotionAuthor final : public juce::AudioAppComponent
{
public:
    explicit MotionAuthor (bool audioDevice = true) : theme { layout }
    {
        setSize (590, 558);
        source.addItem ("New movement", 1);
        for (int i = 0; i < trench::kNumFuncGenPatterns; ++i)
            source.addItem (trench::kFuncGenPatterns[i].name, i + 2);
        source.setSelectedId (5, juce::dontSendNotification);
        source.onChange = [this]
        {
            auto* custom = processor.apvts.getParameter (ParamID::moveCustom);
            custom->setValueNotifyingHost (0);
            auto* preset = processor.apvts.getParameter (ParamID::movePreset);
            preset->setValueNotifyingHost (preset->convertTo0to1 ((float) (source.getSelectedId() - 1)));
            auto* morph = processor.apvts.getParameter (ParamID::morph);
            morph->setValueNotifyingHost (0);
            resetEditor();
        };
        addAndMakeVisible (source);
        play.setButtonText ("Audition noise");
        play.onClick = [this]
        {
            audition.store (! audition.load());
            play.setButtonText (audition.load() ? "Stop" : sample.getNumSamples() ? "Play audio" : "Audition noise");
            if (audition.load()) processor.restartMovement();
        };
        addAndMakeVisible (play);
        load.setButtonText ("Open audio...");
        load.onClick = [this]
        {
            chooser = std::make_unique<juce::FileChooser> ("Audio to audition through TRENCH", juce::File {}, "*.wav;*.aif;*.aiff;*.flac");
            chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                [safe = juce::Component::SafePointer<MotionAuthor> (this)] (const juce::FileChooser& dialog)
                {
                    if (safe != nullptr && dialog.getResult().existsAsFile()) safe->loadAudio (dialog.getResult());
                });
        };
        addAndMakeVisible (load);
        preview.reset (processor.createEditor());
        addAndMakeVisible (*preview);
        source.onChange();
        if (audioDevice) setAudioChannels (0, 2);
        else prepareToPlay (512, 48000);
        resized();
    }
    ~MotionAuthor() override
    {
        shutdownAudio();
        preview.reset();
        author.reset();
    }
    void prepareToPlay (int blockSize, double rate) override
    {
        sampleRate = rate;
        buffer.setSize (2, blockSize);
        processor.setPlayConfigDetails (2, 2, rate, blockSize);
        processor.prepareToPlay (rate, blockSize);
    }
    void releaseResources() override { processor.releaseResources(); }
    bool smoke()
    {
        juce::AudioBuffer<float> output (2, 512);
        audition.store (true);
        getNextAudioBlock ({ &output, 0, 512 });
        bool valid = output.getMagnitude (0, 512) > 0;
        for (int channel = 0; channel < 2; ++channel)
            for (int i = 0; i < 512; ++i) valid = valid && std::isfinite (output.getSample (channel, i));
        audition.store (false);
        getNextAudioBlock ({ &output, 0, 512 });
        valid = valid && output.getMagnitude (0, 512) == 0;
        auto file = juce::File::getCurrentWorkingDirectory().getChildFile ("motion_author.png");
        juce::FileOutputStream stream (file);
        valid = valid && stream.openedOk() && juce::PNGImageFormat().writeImageToStream (
            createComponentSnapshot (getLocalBounds()), stream);
        return valid;
    }
    void getNextAudioBlock (const juce::AudioSourceChannelInfo& block) override
    {
        block.clearActiveBufferRegion();
        if (! audition.load() || block.numSamples > buffer.getNumSamples()) return;
        juce::AudioBuffer<float> work (buffer.getArrayOfWritePointers(), 2, block.numSamples);
        const int count = sample.getNumSamples();
        for (int i = 0; i < block.numSamples; ++i)
        {
            if (count > 1)
            {
                const int first = (int) position;
                const int next = (first + 1) % count;
                const float fraction = (float) (position - first);
                for (int channel = 0; channel < 2; ++channel)
                {
                    const float a = sample.getSample (channel, first), b = sample.getSample (channel, next);
                    work.setSample (channel, i, a + (b - a) * fraction);
                }
                position = std::fmod (position + fileRate / sampleRate, (double) count);
            }
            else
            {
                const float value = (random.nextFloat() * 2 - 1) * 0.1f;
                work.setSample (0, i, value); work.setSample (1, i, value);
            }
        }
        midi.clear();
        processor.processBlock (work, midi);
        for (int channel = 0; channel < block.buffer->getNumChannels(); ++channel)
            block.buffer->copyFrom (channel, block.startSample, work, channel % 2, 0, block.numSamples);
    }
    void resized() override
    {
        source.setBounds (12, 10, 266, 24);
        play.setBounds (12, 42, 122, 26);
        load.setBounds (142, 42, 136, 26);
        if (author) author->setBounds (6, 78, 278, 442);
        if (preview) preview->setBounds (294, 10, 290, 476);
    }
    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff292b2d));
        g.setColour (juce::Colours::lightgrey);
        g.setFont (12.0f);
        g.drawText (status, 12, 527, 585, 22, juce::Justification::centredLeft);
    }
private:
    void resetEditor()
    {
        author = std::make_unique<trench::ui::MotionEditor> (processor, theme);
        author->onDone = [] { juce::JUCEApplicationBase::quit(); };
        addAndMakeVisible (*author);
        resized();
    }
    void loadAudio (const juce::File& file)
    {
        juce::AudioFormatManager formats;
        formats.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
        if (! reader || reader->lengthInSamples < 2 || reader->lengthInSamples > 30000000)
        {
            status = "Choose a readable audio file under 30 million samples."; repaint(); return;
        }
        audition.store (false);
        shutdownAudio();
        sample.setSize (2, (int) reader->lengthInSamples);
        if (reader->read (&sample, 0, sample.getNumSamples(), 0, true, true))
        {
            fileRate = reader->sampleRate;
            position = 0;
            status = file.getFileName();
            play.setButtonText ("Play audio");
        }
        else { sample.setSize (2, 0); status = "Couldn't read that audio file."; }
        setAudioChannels (0, 2);
        repaint();
    }
    PluginProcessor processor;
    trench::UiLayout layout;
    trench::ui::Theme theme;
    std::unique_ptr<juce::AudioProcessorEditor> preview;
    std::unique_ptr<trench::ui::MotionEditor> author;
    juce::ComboBox source;
    juce::TextButton play, load;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::AudioBuffer<float> buffer, sample;
    juce::MidiBuffer midi;
    juce::Random random;
    std::atomic<bool> audition { false };
    double sampleRate = 48000, fileRate = 48000, position = 0;
    juce::String status { "Internal authoring tool. Saved movements appear in TRENCH's MOVE list." };
};

class MotionAuthorApplication final : public juce::JUCEApplication
{
    class Window final : public juce::DocumentWindow
    {
    public:
        Window() : DocumentWindow ("TRENCH Motion Author", juce::Colour (0xff292b2d), closeButton)
        {
            setUsingNativeTitleBar (true);
            setContentOwned (new MotionAuthor(), true);
            centreWithSize (getWidth(), getHeight());
            setVisible (true);
        }
        void closeButtonPressed() override { juce::JUCEApplicationBase::quit(); }
    };
public:
    const juce::String getApplicationName() override { return "TRENCH Motion Author"; }
    const juce::String getApplicationVersion() override { return "0.1"; }
    void initialise (const juce::String& args) override
    {
        if (args.contains ("--smoke"))
        {
            MotionAuthor author (false);
            setApplicationReturnValue (author.smoke() ? 0 : 1);
            quit();
        }
        else window = std::make_unique<Window>();
    }
    void shutdown() override { window.reset(); }
private:
    std::unique_ptr<Window> window;
};

START_JUCE_APPLICATION (MotionAuthorApplication)
