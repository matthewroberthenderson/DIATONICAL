#include "DIATONICAL/PluginProcessor.h"
#include "DIATONICAL/PluginEditor.h"

ChordAudioProcessor::ChordAudioProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
    generateStaticMap();
}

ChordAudioProcessor::~ChordAudioProcessor() {}

void ChordAudioProcessor::getStateInformation(juce::MemoryBlock &destData)
{
    juce::XmlElement xml("DIATONICAL_STATE");

    xml.setAttribute("rootKey", currentRoot);
    xml.setAttribute("scaleType", currentScale);
    xml.setAttribute("bachEnabled", bachMode);

    copyXmlToBinary(xml, destData);
}

void ChordAudioProcessor::setStateInformation(const void *data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));

    if (xmlState != nullptr)
    {
        if (xmlState->hasTagName("DIATONICAL_STATE"))
        {
            currentRoot = xmlState->getIntAttribute("rootKey", 0);
            currentScale = xmlState->getIntAttribute("scaleType", 0);
            bachMode = xmlState->getBoolAttribute("bachEnabled", false);

            // Rebuild the chord map 
            generateStaticMap();
        }
    }
}
void ChordAudioProcessor::generateStaticMap() {
    noteToChordMap.clear();
    const auto& scale = scaleLibrary[currentScale];
    
    for (int i = 0; i < 128; ++i) {
        int degree = i % 7;
        int octave = i / 12;
        int variation = i % 13;
        
        if (bachMode) {
            if (octave <= 3) {
                noteToChordMap[i] = generatePowerSpread(degree, scale, variation);
            }
            else if (octave > 3 && octave <= 6) {
                noteToChordMap[i] = generateComplexTexture(degree, scale, variation);
            }
            else {
                noteToChordMap[i] = generateClassicBach(degree, scale);
            }
        } else {
            // simple triads
            std::vector<int> chord;
            for (int j : {0, 2, 4}) {
                int idx = (degree + j) % 7;
                chord.push_back(48 + currentRoot + scale[idx]);
            }
            noteToChordMap[i] = chord;
        }
    }
}

std::vector<int> ChordAudioProcessor::generatePowerSpread(int degree, const std::vector<int>& scale, int var) {
    std::vector<int> notes;
    int rootBase = 48 + currentRoot + scale[degree];
    notes.push_back(rootBase - 24); // Deep Sub? Might be wrong, but sounds right..
    notes.push_back(rootBase - 17); // Power Fifth
    notes.push_back(rootBase);      // Root
    notes.push_back(rootBase + 12 + scale[(degree + 2) % 7]); // 10th
    return notes;
}

std::vector<int> ChordAudioProcessor::generateComplexTexture(int degree, const std::vector<int>& scale, int var) {
    std::vector<int> notes;
    int rootBase = 48 + currentRoot + scale[degree];
    notes.push_back(rootBase - 12); 
    
    if (var % 2 == 0) { // Alt friction and cluster
        notes.push_back(rootBase);
        notes.push_back(rootBase + scale[(degree + 1) % 7]);
    } else {
        notes.push_back(rootBase + scale[(degree + 2) % 7]);
        notes.push_back(rootBase + 12 + scale[(degree + 1) % 7]); 
    }
    notes.push_back(rootBase + 12 + scale[(degree + 4) % 7]);
    return notes;
}

std::vector<int> ChordAudioProcessor::generateClassicBach(int degree, const std::vector<int>& scale) {
    std::vector<int> notes;
    int rootBase = 48 + currentRoot + scale[degree];
    notes.push_back(rootBase - 12);                             // Bass
    notes.push_back(rootBase + scale[(degree + 2) % 7]);        // Tenor
    notes.push_back(rootBase + 12);                             // Alto
    notes.push_back(rootBase + 12 + scale[(degree + 4) % 7]);   // Soprano
    return notes;
}


void ChordAudioProcessor::processBlock(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midiMessages)
{
    buffer.clear();
    juce::MidiBuffer processedMidi;

    for (const auto metadata : midiMessages)
    {
        auto msg = metadata.getMessage();
        int triggerNote = msg.getNoteNumber();

        if (msg.isNoteOn())
        {
            if (noteToChordMap.count(triggerNote))
            {
                for (int n : noteToChordMap[triggerNote])
                    processedMidi.addEvent(juce::MidiMessage::noteOn(msg.getChannel(), n, msg.getVelocity()), metadata.samplePosition);
                activeNotes[triggerNote] = noteToChordMap[triggerNote];
            }
        }
        else if (msg.isNoteOff())
        {
            if (activeNotes.count(triggerNote))
            {
                for (int n : activeNotes[triggerNote])
                    processedMidi.addEvent(juce::MidiMessage::noteOff(msg.getChannel(), n, msg.getVelocity()), metadata.samplePosition);
                activeNotes.erase(triggerNote);
            }
        }
    }
    midiMessages.swapWith(processedMidi);
}

juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter() { return new ChordAudioProcessor(); }
juce::AudioProcessorEditor *ChordAudioProcessor::createEditor() { return new ChordAudioProcessorEditor(*this); }