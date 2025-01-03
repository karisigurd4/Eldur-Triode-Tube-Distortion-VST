// distortionEngine.h
#pragma once

#include <JuceHeader.h>
#include "korenTriodeModel.h"
#include "ToneStack.h"

class DistortionEngine
{
public:
  DistortionEngine() = default;
  ~DistortionEngine() = default;

  void prepare(const juce::dsp::ProcessSpec& spec, int oversamplingFactor = 1);
  void reset();

  void setDrive(float drive) { driveParam = drive; }
  void setBias(float bias) { biasParam = bias; }
  void setMix(float mix) { mixParam = mix; }

  // The main entry point
  void processBlock(float sampleRate, juce::AudioBuffer<float>& buffer);

private:
  void encodeToMS(juce::AudioBuffer<float>& buffer);
  void decodeFromMS(juce::AudioBuffer<float>& buffer);

  // triode stages to call sequentially
  void applyTriodeStages(float sampleRate, juce::dsp::AudioBlock<float>& oversampledBlock, float drive, float bias);

  /** Our tone stack (HP, shelves, peak). */
  ToneStack toneStack;
  juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>> highPassFilter;

  // e.g. 2x oversampling
  std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;

  juce::AudioBuffer<float> dryBuffer;

  float driveParam = 0.2f;
  float biasParam = 0.5f;
  float mixParam = 1.0f;
};

