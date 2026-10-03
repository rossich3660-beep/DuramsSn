#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

//==============================================================================
class GlassLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float startAngle, float endAngle, juce::Slider&) override;
};

//==============================================================================
// A rotary knob with its caption underneath. While the mouse is on the knob
// the caption briefly turns into the current value.
class GlassKnob : public juce::Component
{
public:
    GlassKnob (juce::AudioProcessorValueTreeState&, const juce::String& paramId,
               const juce::String& caption, bool bipolar);
    ~GlassKnob() override;
    void resized() override;
    void paint (juce::Graphics&) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override  { repaint(); }
    void mouseDrag (const juce::MouseEvent&) override  { repaint(); }
    void mouseUp (const juce::MouseEvent&) override    { repaint(); }

private:
    juce::Slider slider;
    juce::String caption;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GlassKnob)
};

//==============================================================================
// Centre window: shows the waveform, accepts drag & drop and click-to-load.
class SampleView : public juce::Component,
                   public juce::FileDragAndDropTarget
{
public:
    explicit SampleView (DuramsProcessor&);
    void setSample (std::shared_ptr<const durams::SampleData>);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

    bool isInterestedInFileDrag (const juce::StringArray&) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { dragHover = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override { dragHover = false; repaint(); }
    void filesDropped (const juce::StringArray&, int, int) override;

private:
    juce::Rectangle<int> folderButton() const;
    juce::Rectangle<int> playButton() const;
    void rebuildPeaks();
    void openChooser();
    void loadFile (const juce::File&);
    bool isAudioFile (const juce::String& path) const;

    DuramsProcessor& proc;
    std::shared_ptr<const durams::SampleData> sample;
    std::vector<float> peakMin, peakMax;
    std::unique_ptr<juce::FileChooser> chooser;
    bool dragHover = false;
    int hoverButton = 0; // 0 none, 1 folder, 2 play
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SampleView)
};

//==============================================================================
class DuramsEditor : public juce::AudioProcessorEditor,
                     private juce::ChangeListener
{
public:
    explicit DuramsEditor (DuramsProcessor&);
    ~DuramsEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void buildBackground();

    DuramsProcessor& proc;
    GlassLookAndFeel laf;
    juce::Image bg, blurBg;
    std::unique_ptr<GlassKnob> knobs[12];
    SampleView sampleView;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DuramsEditor)
};
