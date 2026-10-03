#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    const char* const kIds[12] = { "level", "curve", "range", "tone", "bright", "human",
                                   "pitch", "snap", "decay", "wires", "body", "drive" };
    constexpr int kMaxSeconds = 10;
    constexpr size_t kMaxEmbedBytes = 1500000;
}

juce::AudioProcessorValueTreeState::ParameterLayout DuramsProcessor::createLayout()
{
    using FP = juce::AudioParameterFloat;
    using Attr = juce::AudioParameterFloatAttributes;
    using Range = juce::NormalisableRange<float>;
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;

    auto pct  = [] (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; };
    auto sgn  = [] (float v, int) { return (v > 0.0f ? "+" : "") + juce::String (v, 2); };
    auto bip  = [] (float v, int) { return (v > 0.5f ? "+" : "") + juce::String (juce::roundToInt ((v - 0.5f) * 200.0f)) + " %"; };
    auto db   = [] (float v, int) { return (v > 0.0f ? "+" : "") + juce::String (v, 1) + " dB"; };
    auto st   = [] (float v, int) { return (v > 0.0f ? "+" : "") + juce::String (v, 2) + " st"; };

    p.push_back (std::make_unique<FP> (juce::ParameterID { "level",  1 }, "Level",  Range (-36.0f, 12.0f, 0.01f), 0.0f,  Attr().withStringFromValueFunction (db)));
    p.push_back (std::make_unique<FP> (juce::ParameterID { "curve",  1 }, "Curve",  Range (-1.0f, 1.0f, 0.001f),  0.0f,  Attr().withStringFromValueFunction (sgn)));
    p.push_back (std::make_unique<FP> (juce::ParameterID { "range",  1 }, "Range",  Range (0.0f, 1.0f, 0.001f),   0.5f,  Attr().withStringFromValueFunction (pct)));
    p.push_back (std::make_unique<FP> (juce::ParameterID { "tone",   1 }, "Tone",   Range (-1.0f, 1.0f, 0.001f),  0.0f,  Attr().withStringFromValueFunction (sgn)));
    p.push_back (std::make_unique<FP> (juce::ParameterID { "bright", 1 }, "Bright", Range (0.0f, 1.0f, 0.001f),   0.5f,  Attr().withStringFromValueFunction (pct)));
    p.push_back (std::make_unique<FP> (juce::ParameterID { "human",  1 }, "Human",  Range (0.0f, 1.0f, 0.001f),   0.35f, Attr().withStringFromValueFunction (pct)));
    p.push_back (std::make_unique<FP> (juce::ParameterID { "pitch",  1 }, "Pitch",  Range (-12.0f, 12.0f, 0.01f), 0.0f,  Attr().withStringFromValueFunction (st)));
    p.push_back (std::make_unique<FP> (juce::ParameterID { "snap",   1 }, "Snap",   Range (0.0f, 1.0f, 0.001f),   0.5f,  Attr().withStringFromValueFunction (pct)));
    p.push_back (std::make_unique<FP> (juce::ParameterID { "decay",  1 }, "Decay",  Range (0.0f, 1.0f, 0.001f),   1.0f,  Attr().withStringFromValueFunction (pct)));
    p.push_back (std::make_unique<FP> (juce::ParameterID { "wires",  1 }, "Wires",  Range (0.0f, 1.0f, 0.001f),   0.5f,  Attr().withStringFromValueFunction (bip)));
    p.push_back (std::make_unique<FP> (juce::ParameterID { "body",   1 }, "Body",   Range (0.0f, 1.0f, 0.001f),   0.5f,  Attr().withStringFromValueFunction (bip)));
    p.push_back (std::make_unique<FP> (juce::ParameterID { "drive",  1 }, "Drive",  Range (0.0f, 1.0f, 0.001f),   0.2f,  Attr().withStringFromValueFunction (pct)));
    return { p.begin(), p.end() };
}

DuramsProcessor::DuramsProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "DURAMS_SN", createLayout())
{
    for (int i = 0; i < 12; ++i)
        prm[i] = apvts.getRawParameterValue (kIds[i]);
    formatManager.registerBasicFormats();
}

DuramsProcessor::~DuramsProcessor()
{
    samplePtr.store (nullptr);
}

bool DuramsProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
           && l.getMainInputChannelSet().isDisabled();
}

void DuramsProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate);
    engine.setSample (samplePtr.load());
    engine.setParams (readParams());
}

durams::Params DuramsProcessor::readParams() const
{
    durams::Params p;
    p.level = prm[0]->load();  p.curve = prm[1]->load();  p.range = prm[2]->load();
    p.tone  = prm[3]->load();  p.bright = prm[4]->load(); p.human = prm[5]->load();
    p.pitch = prm[6]->load();  p.snap = prm[7]->load();   p.decay = prm[8]->load();
    p.wires = prm[9]->load();  p.body = prm[10]->load();  p.drive = prm[11]->load();
    return p;
}

void DuramsProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
    const int n = buffer.getNumSamples();
    if (buffer.getNumChannels() < 2 || n <= 0) return;

    engine.setSample (samplePtr.load (std::memory_order_acquire));
    engine.setParams (readParams());

    float* L = buffer.getWritePointer (0);
    float* R = buffer.getWritePointer (1);

    const float a = auditionVel.exchange (-1.0f);
    if (a >= 0.0f) engine.noteOn (a);

    int pos = 0;
    for (const auto meta : midi)
    {
        const int at = juce::jlimit (pos, n, meta.samplePosition);
        if (at > pos) { engine.render (L + pos, R + pos, at - pos); pos = at; }

        const auto m = meta.getMessage();
        if (m.isNoteOn())                                   engine.noteOn (m.getFloatVelocity());
        else if (m.isAllNotesOff() || m.isAllSoundOff())    engine.allNotesOff();
    }
    if (pos < n) engine.render (L + pos, R + pos, n - pos);
    midi.clear();
}

// ------------------------------------------------------------------ samples
juce::StringArray DuramsProcessor::getSupportedWildcards() const
{
    juce::StringArray out;
    out.addTokens (formatManager.getWildcardForAllFormats(), ";", "");
    out.trim();
    return out;
}

std::shared_ptr<const durams::SampleData> DuramsProcessor::getCurrentSample() const
{
    const juce::ScopedLock sl (loadLock);
    return currentSample;
}

bool DuramsProcessor::loadSampleFromFile (const juce::File& file)
{
    if (! file.existsAsFile()) return false;
    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));
    if (reader == nullptr || reader->lengthInSamples < 2 || reader->numChannels < 1 || reader->sampleRate < 1000.0)
        return false;

    const auto maxLen = (juce::int64) (reader->sampleRate * kMaxSeconds);
    const int len = (int) juce::jmin (reader->lengthInSamples, maxLen);
    const int ch = (int) juce::jmin ((unsigned int) 2, reader->numChannels);

    juce::AudioBuffer<float> tmp (ch, len);
    tmp.clear();
    if (! reader->read (&tmp, 0, len, 0, true, ch > 1))
        return false;

    auto d = std::make_shared<durams::SampleData>();
    d->sampleRate = reader->sampleRate;
    d->left.resize ((size_t) len);
    d->right.resize ((size_t) len);
    const float* l = tmp.getReadPointer (0);
    const float* r = tmp.getReadPointer (ch > 1 ? 1 : 0);
    for (int i = 0; i < len; ++i)
    {
        d->left[(size_t) i]  = std::isfinite (l[i]) ? l[i] : 0.0f;
        d->right[(size_t) i] = std::isfinite (r[i]) ? r[i] : 0.0f;
    }
    installSample (d, file.getFileNameWithoutExtension(), file.getFullPathName(), true);
    return true;
}

void DuramsProcessor::installSample (std::shared_ptr<durams::SampleData> s, const juce::String& name,
                                     const juce::String& path, bool embed)
{
    {
        const juce::ScopedLock sl (loadLock);
        const auto now = juce::Time::getMillisecondCounter();
        if (currentSample != nullptr) retired.push_back ({ currentSample, now });
        retired.erase (std::remove_if (retired.begin(), retired.end(),
                                       [now] (const Retired& r) { return now - r.when > 60000u; }),
                       retired.end());
        currentSample = s;
        samplePtr.store (currentSample.get(), std::memory_order_release);

        if (embed)   // new file loaded by the user: refresh what gets saved with the project
        {
            apvts.state.setProperty ("sampleName", name, nullptr);
            apvts.state.setProperty ("samplePath", path, nullptr);
            apvts.state.removeProperty ("sampleData", nullptr);
            apvts.state.removeProperty ("sampleRate", nullptr);
            apvts.state.removeProperty ("sampleFrames", nullptr);
            apvts.state.removeProperty ("sampleChannels", nullptr);
        }

        if (embed && s != nullptr)
        {
            const size_t frames = s->left.size();
            bool mono = true;
            for (size_t i = 0; i < frames && mono; ++i) mono = (s->left[i] == s->right[i]);
            const size_t chans = mono ? 1 : 2;
            if (frames * chans * 2 <= kMaxEmbedBytes)
            {
                std::vector<juce::int16> pcm (frames * chans);
                auto q = [] (float v) { return (juce::int16) juce::roundToInt (juce::jlimit (-1.0f, 1.0f, v) * 32767.0f); };
                for (size_t i = 0; i < frames; ++i)
                {
                    if (mono) pcm[i] = q (s->left[i]);
                    else { pcm[i * 2] = q (s->left[i]); pcm[i * 2 + 1] = q (s->right[i]); }
                }
                apvts.state.setProperty ("sampleData", juce::Base64::toBase64 (pcm.data(), pcm.size() * 2), nullptr);
                apvts.state.setProperty ("sampleRate", s->sampleRate, nullptr);
                apvts.state.setProperty ("sampleFrames", (int) frames, nullptr);
                apvts.state.setProperty ("sampleChannels", (int) chans, nullptr);
            }
        }
    }
    sendChangeMessage();
}

bool DuramsProcessor::loadSampleFromState()
{
    const auto data = apvts.state.getProperty ("sampleData").toString();
    const double sr = (double) apvts.state.getProperty ("sampleRate", 0.0);
    const int frames = (int) apvts.state.getProperty ("sampleFrames", 0);
    const int chans = (int) apvts.state.getProperty ("sampleChannels", 1);

    if (data.isNotEmpty() && sr >= 1000.0 && frames > 1 && (chans == 1 || chans == 2))
    {
        juce::MemoryOutputStream mo;
        if (juce::Base64::convertFromBase64 (mo, data)
            && mo.getDataSize() == (size_t) frames * (size_t) chans * 2)
        {
            const auto* pcm = static_cast<const juce::int16*> (mo.getData());
            auto d = std::make_shared<durams::SampleData>();
            d->sampleRate = sr;
            d->left.resize ((size_t) frames);
            d->right.resize ((size_t) frames);
            for (int i = 0; i < frames; ++i)
            {
                d->left[(size_t) i]  = (float) pcm[i * chans] / 32768.0f;
                d->right[(size_t) i] = (float) pcm[i * chans + (chans - 1)] / 32768.0f;
            }
            installSample (d, apvts.state.getProperty ("sampleName").toString(),
                           apvts.state.getProperty ("samplePath").toString(), false);
            return true;
        }
    }
    const juce::File f (apvts.state.getProperty ("samplePath").toString());
    return f.existsAsFile() && loadSampleFromFile (f);
}

// -------------------------------------------------------------------- state
void DuramsProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    juce::ValueTree copy;
    {
        const juce::ScopedLock sl (loadLock);
        copy = apvts.copyState();
    }
    if (auto xml = copy.createXml())
        copyXmlToBinary (*xml, dest);
}

void DuramsProcessor::setStateInformation (const void* data, int size)
{
    if (data == nullptr || size <= 0) return;
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, size));
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return;

    {
        const juce::ScopedLock sl (loadLock);
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
    }
    loadSampleFromState();
}

juce::AudioProcessorEditor* DuramsProcessor::createEditor()
{
    return new DuramsEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DuramsProcessor();
}
