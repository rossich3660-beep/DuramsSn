#include "PluginEditor.h"

namespace
{
    constexpr int kW = 960, kH = 640;
    const juce::Rectangle<int> kLeftPanel  (28, 60, 236, 520);
    const juce::Rectangle<int> kRightPanel (kW - 28 - 236, 60, 236, 520);
    const juce::Rectangle<int> kSampleRect ((kW - 380) / 2, (kH - 260) / 2, 380, 260);

    const juce::Colour kCyan    (0xff4fd8ff);
    const juce::Colour kMagenta (0xffff4fd0);

    struct KnobDef { const char* id; const char* caption; bool bipolar; };
    // 6 on the left panel (2 columns x 3 rows), then 6 on the right panel
    const KnobDef kDefs[12] = {
        { "level",  "LEVEL",  false }, { "curve",  "CURVE",  true  },
        { "range",  "RANGE",  false }, { "tone",   "TONE",   true  },
        { "bright", "BRIGHT", false }, { "human",  "HUMAN",  false },
        { "pitch",  "PITCH",  true  }, { "snap",   "SNAP",   false },
        { "decay",  "DECAY",  false }, { "wires",  "WIRES",  true  },
        { "body",   "BODY",   true  }, { "drive",  "DRIVE",  false }
    };

    void drawGlass (juce::Graphics& g, juce::Rectangle<float> r, float corner, const juce::Image& blurred)
    {
        juce::Path path;
        path.addRoundedRectangle (r, corner);

        juce::DropShadow (juce::Colours::black.withAlpha (0.55f), 26, { 0, 10 }).drawForPath (g, path);

        g.saveState();
        g.reduceClipRegion (path);
        if (blurred.isValid()) g.drawImageAt (blurred, 0, 0);
        g.setColour (juce::Colours::black.withAlpha (0.30f));
        g.fillPath (path);
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.20f), r.getX(), r.getY(),
                                                 juce::Colours::white.withAlpha (0.03f), r.getX(), r.getBottom(), false));
        g.fillPath (path);
        // soft diagonal sheen
        juce::Path sheen;
        sheen.startNewSubPath (r.getX(), r.getY());
        sheen.lineTo (r.getX() + r.getWidth() * 0.55f, r.getY());
        sheen.lineTo (r.getX(), r.getY() + r.getHeight() * 0.35f);
        sheen.closeSubPath();
        g.setColour (juce::Colours::white.withAlpha (0.05f));
        g.fillPath (sheen);
        g.restoreState();

        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.60f), r.getX(), r.getY(),
                                                 juce::Colours::white.withAlpha (0.06f), r.getRight(), r.getBottom(), false));
        g.strokePath (path, juce::PathStrokeType (1.3f));
    }
}

//==============================================================================
void GlassLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                         float pos, float startA, float endA, juce::Slider& s)
{
    auto b = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (4.0f);
    const float d = juce::jmin (b.getWidth(), b.getHeight());
    const auto c = b.getCentre();
    const float R = d * 0.5f, arcR = R - 3.0f, bodyR = R - 13.0f;
    const float angle = startA + pos * (endA - startA);
    const bool bipolar = (bool) s.getProperties()["bipolar"];

    // track
    juce::Path track;
    track.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startA, endA, true);
    g.setColour (juce::Colours::white.withAlpha (0.14f));
    g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // value arc (with glow)
    const float from = bipolar ? (startA + endA) * 0.5f : startA;
    const float a0 = juce::jmin (from, angle), a1 = juce::jmax (from, angle);
    if (a1 - a0 > 0.001f)
    {
        juce::Path val;
        val.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, a0, a1, true);
        g.setGradientFill (juce::ColourGradient (kCyan.withAlpha (0.22f), c.x - arcR, c.y,
                                                 kMagenta.withAlpha (0.22f), c.x + arcR, c.y, false));
        g.strokePath (val, juce::PathStrokeType (9.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setGradientFill (juce::ColourGradient (kCyan, c.x - arcR, c.y, kMagenta, c.x + arcR, c.y, false));
        g.strokePath (val, juce::PathStrokeType (3.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // glass body
    g.setColour (juce::Colours::black.withAlpha (0.40f));
    g.fillEllipse (c.x - bodyR, c.y - bodyR + 3.0f, bodyR * 2.0f, bodyR * 2.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.34f), c.x - bodyR * 0.5f, c.y - bodyR * 0.8f,
                                             juce::Colours::black.withAlpha (0.55f), c.x + bodyR * 0.6f, c.y + bodyR,
                                             false));
    g.fillEllipse (c.x - bodyR, c.y - bodyR, bodyR * 2.0f, bodyR * 2.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.70f), c.x - bodyR, c.y - bodyR,
                                             juce::Colours::white.withAlpha (0.05f), c.x + bodyR, c.y + bodyR, false));
    g.drawEllipse (c.x - bodyR, c.y - bodyR, bodyR * 2.0f, bodyR * 2.0f, 1.2f);
    // top highlight
    const float hw = bodyR * 1.15f, hh = bodyR * 0.62f;
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.30f), c.x, c.y - bodyR * 0.95f,
                                             juce::Colours::white.withAlpha (0.0f), c.x, c.y - bodyR * 0.2f, false));
    g.fillEllipse (c.x - hw * 0.5f, c.y - bodyR * 0.92f, hw, hh);

    // pointer
    juce::Path ptr;
    ptr.addRoundedRectangle (-1.7f, -bodyR + 5.0f, 3.4f, bodyR * 0.45f, 1.7f);
    ptr.applyTransform (juce::AffineTransform::rotation (angle).translated (c.x, c.y));
    g.setColour (juce::Colours::white.withAlpha (0.95f));
    g.fillPath (ptr);
}

//==============================================================================
GlassKnob::GlassKnob (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramId,
                      const juce::String& cap, bool bipolar)
    : caption (cap)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                                juce::MathConstants<float>::pi * 2.75f, true);
    slider.setMouseDragSensitivity (200);
    slider.getProperties().set ("bipolar", bipolar);
    addAndMakeVisible (slider);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, paramId, slider);
    slider.addMouseListener (this, false);
}

GlassKnob::~GlassKnob()
{
    slider.removeMouseListener (this);
}

void GlassKnob::resized()
{
    const int labelH = 24;
    const int size = juce::jmin (getWidth() - 10, getHeight() - labelH);
    slider.setBounds ((getWidth() - size) / 2, 0, size, size);
}

void GlassKnob::paint (juce::Graphics& g)
{
    auto area = juce::Rectangle<int> (0, slider.getBottom() + 2, getWidth(), 22);
    const bool showValue = slider.isMouseOverOrDragging();
    const auto text = showValue ? slider.getTextFromValue (slider.getValue()) : caption;

    g.setFont (juce::Font (juce::FontOptions (showValue ? 13.0f : 12.5f, juce::Font::bold)));
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.drawText (text, area.translated (0, 1), juce::Justification::centred, false);
    g.setColour (showValue ? kCyan.brighter (0.3f) : juce::Colours::white.withAlpha (0.88f));
    g.drawText (text, area, juce::Justification::centred, false);
}

//==============================================================================
SampleView::SampleView (DuramsProcessor& p) : proc (p)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

juce::Rectangle<int> SampleView::folderButton() const
{
    return juce::Rectangle<int> (34, 34).withCentre ({ getWidth() / 2 - 28, getHeight() - 36 });
}

juce::Rectangle<int> SampleView::playButton() const
{
    return juce::Rectangle<int> (34, 34).withCentre ({ getWidth() / 2 + 28, getHeight() - 36 });
}

void SampleView::resized() { rebuildPeaks(); }

void SampleView::setSample (std::shared_ptr<const durams::SampleData> s)
{
    sample = std::move (s);
    rebuildPeaks();
    repaint();
}

void SampleView::rebuildPeaks()
{
    const int cols = juce::jmax (1, getWidth() - 44);
    peakMin.assign ((size_t) cols, 0.0f);
    peakMax.assign ((size_t) cols, 0.0f);
    if (sample == nullptr || sample->length() < 2) return;

    const int len = sample->length();
    float absMax = 1.0e-9f;
    for (int x = 0; x < cols; ++x)
    {
        const int s0 = (int) ((juce::int64) x * len / cols);
        const int s1 = juce::jmax (s0 + 1, (int) ((juce::int64) (x + 1) * len / cols));
        float mn = 0.0f, mx = 0.0f;
        for (int i = s0; i < s1 && i < len; ++i)
        {
            const float v = 0.5f * (sample->left[(size_t) i] + sample->right[(size_t) i]);
            mn = juce::jmin (mn, v); mx = juce::jmax (mx, v);
        }
        peakMin[(size_t) x] = mn; peakMax[(size_t) x] = mx;
        absMax = juce::jmax (absMax, -mn, mx);
    }
    for (int x = 0; x < cols; ++x) { peakMin[(size_t) x] /= absMax; peakMax[(size_t) x] /= absMax; }
}

void SampleView::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();

    if (dragHover)
    {
        g.setColour (kCyan.withAlpha (0.9f));
        g.drawRoundedRectangle (r.reduced (2.0f), 20.0f, 2.2f);
    }

    if (sample == nullptr)
    {
        // empty: dashed drop zone with a plus
        juce::Path zone, dashed;
        zone.addRoundedRectangle (r.reduced (22.0f), 16.0f);
        const float dashes[] = { 7.0f, 6.0f };
        juce::PathStrokeType (1.4f).createDashedStroke (dashed, zone, dashes, 2);
        g.setColour (juce::Colours::white.withAlpha (dragHover ? 0.7f : 0.32f));
        g.fillPath (dashed);

        const auto c = r.getCentre();
        g.setGradientFill (juce::ColourGradient (kCyan, c.x - 36, c.y, kMagenta, c.x + 36, c.y, false));
        g.drawEllipse (c.x - 36, c.y - 36, 72, 72, 2.4f);
        g.fillRoundedRectangle (c.x - 15, c.y - 1.6f, 30, 3.2f, 1.6f);
        g.fillRoundedRectangle (c.x - 1.6f, c.y - 15, 3.2f, 30, 1.6f);
        return;
    }

    // waveform
    const float cx = r.getCentreY() - 14.0f;
    const float half = (r.getHeight() - 96.0f) * 0.5f;
    const float x0 = 22.0f;
    juce::Path wave;
    const int cols = (int) peakMax.size();
    wave.startNewSubPath (x0, cx);
    for (int x = 0; x < cols; ++x) wave.lineTo (x0 + (float) x, cx - peakMax[(size_t) x] * half);
    for (int x = cols - 1; x >= 0; --x) wave.lineTo (x0 + (float) x, cx - peakMin[(size_t) x] * half);
    wave.closeSubPath();

    g.setColour (juce::Colours::white.withAlpha (0.18f));
    g.fillRect (x0, cx - 0.5f, (float) cols, 1.0f);
    g.setGradientFill (juce::ColourGradient (kCyan.withAlpha (0.28f), x0, 0, kMagenta.withAlpha (0.28f), x0 + (float) cols, 0, false));
    g.strokePath (wave, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved));
    g.setGradientFill (juce::ColourGradient (kCyan.withAlpha (0.92f), x0, 0, kMagenta.withAlpha (0.92f), x0 + (float) cols, 0, false));
    g.fillPath (wave);

    // folder + play buttons
    auto drawButton = [&] (juce::Rectangle<int> rc, bool hot)
    {
        auto f = rc.toFloat();
        g.setColour (juce::Colours::white.withAlpha (hot ? 0.28f : 0.12f));
        g.fillEllipse (f);
        g.setColour (juce::Colours::white.withAlpha (hot ? 0.85f : 0.45f));
        g.drawEllipse (f, 1.2f);
    };
    drawButton (folderButton(), hoverButton == 1);
    drawButton (playButton(), hoverButton == 2);

    g.setColour (juce::Colours::white.withAlpha (0.92f));
    {   // folder icon
        auto c = folderButton().toFloat().getCentre();
        juce::Path fp;
        fp.addRoundedRectangle (c.x - 8.0f, c.y - 4.0f, 16.0f, 11.0f, 2.0f);
        fp.addRoundedRectangle (c.x - 8.0f, c.y - 7.0f, 7.0f, 4.0f, 1.2f);
        g.fillPath (fp);
    }
    {   // play icon
        auto c = playButton().toFloat().getCentre();
        juce::Path tp;
        tp.addTriangle (c.x - 4.5f, c.y - 7.0f, c.x - 4.5f, c.y + 7.0f, c.x + 7.5f, c.y);
        g.fillPath (tp);
    }
}

void SampleView::mouseMove (const juce::MouseEvent& e)
{
    int h = 0;
    if (sample != nullptr)
    {
        if (folderButton().contains (e.getPosition())) h = 1;
        else if (playButton().contains (e.getPosition())) h = 2;
    }
    if (h != hoverButton) { hoverButton = h; repaint(); }
}

void SampleView::mouseExit (const juce::MouseEvent&)
{
    if (hoverButton != 0) { hoverButton = 0; repaint(); }
}

void SampleView::mouseDown (const juce::MouseEvent& e)
{
    if (sample != nullptr && playButton().contains (e.getPosition()))
    {
        proc.triggerAudition (0.85f);
        return;
    }
    openChooser();
}

void SampleView::openChooser()
{
    const auto wildcards = proc.getSupportedWildcards().joinIntoString (";");
    chooser = std::make_unique<juce::FileChooser> ("Select a snare one-shot", juce::File(), wildcards, true);
    juce::Component::SafePointer<SampleView> safe (this);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [safe] (const juce::FileChooser& fc)
                          {
                              const auto f = fc.getResult();
                              if (safe != nullptr && f.existsAsFile()) safe->loadFile (f);
                          });
}

void SampleView::loadFile (const juce::File& f)
{
    if (! proc.loadSampleFromFile (f))
        juce::NativeMessageBox::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                     "Durams Sn", "This audio file could not be read.");
}

bool SampleView::isAudioFile (const juce::String& path) const
{
    for (auto pattern : proc.getSupportedWildcards())
        if (pattern.startsWith ("*") && path.endsWithIgnoreCase (pattern.substring (1)))
            return true;
    return false;
}

bool SampleView::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files) if (isAudioFile (f)) return true;
    return false;
}

void SampleView::filesDropped (const juce::StringArray& files, int, int)
{
    dragHover = false;
    for (auto& f : files)
        if (isAudioFile (f)) { loadFile (juce::File (f)); break; }
    repaint();
}

//==============================================================================
DuramsEditor::DuramsEditor (DuramsProcessor& p)
    : juce::AudioProcessorEditor (&p), proc (p), sampleView (p)
{
    setLookAndFeel (&laf);
    setSize (kW, kH);
    setResizable (false, false);
    buildBackground();

    for (int i = 0; i < 12; ++i)
    {
        knobs[i] = std::make_unique<GlassKnob> (proc.apvts, kDefs[i].id, kDefs[i].caption, kDefs[i].bipolar);
        addAndMakeVisible (*knobs[i]);
    }
    addAndMakeVisible (sampleView);
    sampleView.setSample (proc.getCurrentSample());
    proc.addChangeListener (this);
}

DuramsEditor::~DuramsEditor()
{
    proc.removeChangeListener (this);
    setLookAndFeel (nullptr);
}

void DuramsEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    sampleView.setSample (proc.getCurrentSample());
}

void DuramsEditor::buildBackground()
{
    bg = juce::Image (juce::Image::ARGB, kW, kH, true);
    {
        juce::Graphics g (bg);
        g.fillAll (juce::Colour (0xff05060a));
        // A generated dark background keeps the editor self-contained when no artwork is bundled.
        juce::ColourGradient base (juce::Colour (0xff101725), 0.0f, 0.0f,
                                   juce::Colour (0xff05060a), (float) kW, (float) kH, false);
        g.setGradientFill (base);
        g.fillAll();
        g.setColour (juce::Colours::black.withAlpha (0.22f));
        g.fillAll();
    }

    // blurred copy used as the "frosted glass" backdrop (computed once, at 1/4 resolution)
    auto small = bg.rescaled (kW / 4, kH / 4, juce::Graphics::mediumResamplingQuality);
    juce::Image blurred (juce::Image::ARGB, small.getWidth(), small.getHeight(), true);
    juce::ImageConvolutionKernel kernel (9);
    kernel.createGaussianBlur (4.0f);
    kernel.applyToImage (blurred, small, blurred.getBounds());
    blurBg = blurred.rescaled (kW, kH, juce::Graphics::highResamplingQuality);
}

void DuramsEditor::paint (juce::Graphics& g)
{
    g.drawImageAt (bg, 0, 0);
    g.setGradientFill (juce::ColourGradient (juce::Colours::transparentBlack, kW * 0.5f, kH * 0.5f,
                                             juce::Colours::black.withAlpha (0.50f), 0.0f, 0.0f, true));
    g.fillAll();

    drawGlass (g, kLeftPanel.toFloat(),  24.0f, blurBg);
    drawGlass (g, kRightPanel.toFloat(), 24.0f, blurBg);
    drawGlass (g, kSampleRect.toFloat(), 24.0f, blurBg);
}

void DuramsEditor::resized()
{
    sampleView.setBounds (kSampleRect);

    const int cellW = kLeftPanel.getWidth() / 2;
    const int cellH = kLeftPanel.getHeight() / 3;
    for (int i = 0; i < 12; ++i)
    {
        const auto& panel = i < 6 ? kLeftPanel : kRightPanel;
        const int k = i % 6, col = k % 2, row = k / 2;
        knobs[i]->setBounds (panel.getX() + col * cellW + 4, panel.getY() + row * cellH + (cellH - 126) / 2, cellW - 8, 126);
    }
}
