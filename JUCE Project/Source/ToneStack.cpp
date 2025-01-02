// toneStack.cpp

#include "ToneStack.h"

void ToneStack::prepare(const juce::dsp::ProcessSpec& spec)
{
  midPeakFilter.reset();
  lowShelfFilter.reset();
  highShelfFilter.reset();
  highPassFilter.reset();

  // Prepare the highPassFilter’s internal states
  highPassFilter.prepare(spec);

  // Example: default HP at 20 Hz
  *highPassFilter.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass(spec.sampleRate, 20.0f);
}

void ToneStack::reset()
{
  midPeakFilter.reset();
  lowShelfFilter.reset();
  highShelfFilter.reset();
  highPassFilter.reset();
}

void ToneStack::updateCoefficients(float sampleRate, float lowShelfGainDb, float highShelfGainDb, float midPeakGainDb)
{
  const float shelfGainLinLow = juce::Decibels::decibelsToGain(lowShelfGainDb);
  const float shelfGainLinHigh = juce::Decibels::decibelsToGain(highShelfGainDb);
  const float midPeakGainLin = juce::Decibels::decibelsToGain(midPeakGainDb);

  auto lowCoeffs = juce::dsp::IIR::Coefficients<float>::makeLowShelf(sampleRate, 90.0f, 0.707f, shelfGainLinLow);
  auto highCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighShelf(sampleRate, 14000.f, 0.707f, shelfGainLinHigh);
  auto midPeakCoeffs = juce::dsp::IIR::Coefficients<float>::makePeakFilter(sampleRate, 600.0f, 0.7f, midPeakGainLin);

  lowShelfFilter.coefficients = lowCoeffs;
  highShelfFilter.coefficients = highCoeffs;
  midPeakFilter.coefficients = midPeakCoeffs;
}

void ToneStack::processAudioBlock(juce::dsp::AudioBlock<float>& oversampledBlock)
{
  // 1) Create a DSP context that wraps our incoming block
  juce::dsp::ProcessContextReplacing<float> context(oversampledBlock);

  // 2) High-pass filter first
  highPassFilter.process(context);

  // 3) Then apply the shelf and peak filters sample by sample
  const auto numChannels = oversampledBlock.getNumChannels();
  const auto numSamples = oversampledBlock.getNumSamples();

  for (size_t ch = 0; ch < numChannels; ++ch)
  {
    float* channelData = oversampledBlock.getChannelPointer(ch);
    for (size_t i = 0; i < numSamples; ++i)
    {
      float x = channelData[i];
      x = lowShelfFilter.processSample(x);
      x = highShelfFilter.processSample(x);
      x = midPeakFilter.processSample(x);
      channelData[i] = x;
    }
  }
}
