// toneStack.h

#pragma once
#include <JuceHeader.h>

class ToneStack
{
public:
  ToneStack() = default;
  ~ToneStack() = default;

  void prepare(const juce::dsp::ProcessSpec& spec);
  void reset();

  // Adjust gains/coefficients dynamically (e.g. driven by a “drive” or “EQ” parameter)
  void updateCoefficients(float sampleRate, float lowShelfGainDb, float highShelfGainDb, float midPeakGainDb);

  // Process an entire buffer (in-place)
  void processAudioBlock(juce::dsp::AudioBlock<float>& oversampledBlock);

private:
  juce::dsp::IIR::Filter<float> midPeakFilter;
  juce::dsp::IIR::Filter<float> lowShelfFilter;
  juce::dsp::IIR::Filter<float> highShelfFilter;

  juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
    juce::dsp::IIR::Coefficients<float>> highPassFilter;
};

