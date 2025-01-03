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

  void setDrive(float drive) { driveParam = drive; };

  // Adjust gains/coefficients dynamically (e.g. driven by a “drive” or “EQ” parameter)
  void updateCoefficients(float sampleRate);

  // Process an entire buffer (in-place)
  void processAudioBlock(float sampleRate, juce::dsp::AudioBlock<float>& oversampledBlock);

private:
  float lastSmoothedDrive = 0;
  float driveParam = 0;

  juce::dsp::IIR::Filter<float> midPeakFilter;
  juce::dsp::IIR::Filter<float> lowShelfFilter;
  juce::dsp::IIR::Filter<float> highShelfFilter;
};

