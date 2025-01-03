// toneStack.cpp

#include "ToneStack.h"

void ToneStack::prepare(const juce::dsp::ProcessSpec& spec)
{
  midPeakFilter.reset();
  lowShelfFilter.reset();
  highShelfFilter.reset();
}

void ToneStack::reset()
{
  midPeakFilter.reset();
  lowShelfFilter.reset();
  highShelfFilter.reset();
}

void ToneStack::updateCoefficients(float sampleRate)
{
  if (std::abs(driveParam - lastSmoothedDrive) > 0.0001f) {
    float shelfGainDb = 1.0f * driveParam;
    float shelfGainLin = juce::Decibels::decibelsToGain(shelfGainDb);

    auto lowCoeffs = juce::dsp::IIR::Coefficients<float>::makeLowShelf(sampleRate, 90.0, 0.707f, shelfGainLin);
    auto highCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighShelf(sampleRate, 14000.0, 0.707f, shelfGainLin);
    auto midPeakCoeffs = juce::dsp::IIR::Coefficients<float>::makePeakFilter(sampleRate, 600.0f, 0.7f, 1.412f * driveParam);

    midPeakFilter.coefficients = midPeakCoeffs;
    lowShelfFilter.coefficients = lowCoeffs;
    highShelfFilter.coefficients = highCoeffs;

    lastSmoothedDrive = driveParam;
  }
}

void ToneStack::processAudioBlock(float sampleRate, juce::dsp::AudioBlock<float>& oversampledBlock)
{
  // Update coefficients if drive has changed
  updateCoefficients(sampleRate);

  // Create a DSP context that wraps our incoming block
  juce::dsp::ProcessContextReplacing<float> context(oversampledBlock);

  // Then apply the shelf and peak filters sample by sample
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
