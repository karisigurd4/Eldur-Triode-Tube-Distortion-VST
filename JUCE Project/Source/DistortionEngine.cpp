// distortionEngine.cpp

#include "DistortionEngine.h"

void DistortionEngine::prepare(const juce::dsp::ProcessSpec& spec, int oversamplingFactor)
{
  oversampler = std::make_unique<juce::dsp::Oversampling<float>>
    ((int)spec.numChannels,
      (uint32_t)oversamplingFactor,
      juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR);

  oversampler->initProcessing((size_t)spec.maximumBlockSize);

  toneStack.prepare(spec);

  // Prepare the highPassFilter’s internal states
  highPassFilter.prepare(spec);
  highPassFilter.reset();
  *highPassFilter.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass(spec.sampleRate, 20.0f);

  // Pre-allocate the dryBuffer at the max size,
  // so we can re-use it without new allocations:
  dryBuffer.setSize((int)spec.numChannels, (int)spec.maximumBlockSize);
}

void DistortionEngine::reset()
{
  if (oversampler)
    oversampler->reset();

  toneStack.reset();
  highPassFilter.reset();
}

void DistortionEngine::encodeToMS(juce::AudioBuffer<float>& buffer)
{
  const int numChannels = buffer.getNumChannels();
  if (numChannels < 2) return;

  int numSamples = buffer.getNumSamples();
  for (int i = 0; i < numSamples; ++i)
  {
    float L = buffer.getSample(0, i);
    float R = buffer.getSample(1, i);
    float M = 0.5f * (L + R);
    float S = 0.5f * (L - R);
    buffer.setSample(0, i, M);
    buffer.setSample(1, i, S);
  }
}

void DistortionEngine::decodeFromMS(juce::AudioBuffer<float>& buffer)
{
  const int numChannels = buffer.getNumChannels();
  if (numChannels < 2) return;

  int numSamples = buffer.getNumSamples();
  for (int i = 0; i < numSamples; ++i)
  {
    float M = buffer.getSample(0, i);
    float S = buffer.getSample(1, i);

    float L = M + S;
    float R = M - S;

    buffer.setSample(0, i, L);
    buffer.setSample(1, i, R);
  }
}

void DistortionEngine::applyTriodeStages(float sampleRate, juce::dsp::AudioBlock<float>& oversampledBlock,
  float drive, float bias)
{
  // Stage 1 12AX7
  KorenTriodeModel::processAudioBlock(
    oversampledBlock,
    /* gainVal   */ 0.3f,
    /* bias      */ 0.0f,
    /* drive     */ 1.0f + (drive * 60.0f),
    /* G         */ 2.5e-3f,
    /* mu        */ 100.0f,
    /* C         */ 0.5f,
    /* P         */ 1.5f,
    /* B_plus    */ 200.0f,
    /* Rp        */ 130000.0f
    // maxIter + tol using defaults
  );

  // Stage 2 12AX7
  KorenTriodeModel::processAudioBlock(
    oversampledBlock,
    /* gainVal   */ 0.3f,
    /* bias      */  1.25f * bias,
    /* drive     */ 1.0f + (drive * 40.0f),
    /* G         */ 2.5e-3f,
    /* mu        */ 100.0f,
    /* C         */ 0.5f,
    /* P         */ 1.5f,
    /* B_plus    */ 300.0f,
    /* Rp        */ 200000.0f
  );

  // Stage 3 12AT7
  KorenTriodeModel::processAudioBlock(
    oversampledBlock,
    /* gainVal   */ drive * 0.65f,
    /* bias      */ -1.35f * bias,
    /* drive     */ 1.0f + (drive * 30.0f),
    /* G         */ 3.5e-3f,
    /* mu        */ 60.0f,
    /* C         */ 0.5f,
    /* P         */ 1.5f,
    /* B_plus    */ 350.0f,
    /* Rp        */ 160000.0f
  );

  // Stage 4 12AT7
  KorenTriodeModel::processAudioBlock(
    oversampledBlock,
    /* gainVal   */ drive * 0.55f,
    /* bias      */ 1.5f * bias,
    /* drive     */ 1.0f + (drive * 30.0f),
    /* G         */ 3.5e-3f,
    /* mu        */ 60.0f,
    /* C         */ 0.5f,
    /* P         */ 1.5f,
    /* B_plus    */ 400.0f,
    /* Rp        */ 120000.0f
  );

  // Tone stack in between
  toneStack.setDrive(drive);
  toneStack.processAudioBlock(sampleRate, oversampledBlock);

  // Stage 5 12AU7
  KorenTriodeModel::processAudioBlock(
    oversampledBlock,
    /* gainVal   */ 0.5f,
    /* bias      */ -1.25f * bias,
    /* drive     */ 1.0f + (drive * 20.0f),
    /* G         */ 7.0e-3f,
    /* mu        */ 17.0f,
    /* C         */ 0.5f,
    /* P         */ 1.5f,
    /* B_plus    */ 400.0f,
    /* Rp        */ 150000.0f
  );

  juce::dsp::ProcessContextReplacing<float> context(oversampledBlock);
  highPassFilter.process(context);
}


void DistortionEngine::processBlock(float sampleRate, juce::AudioBuffer<float>& buffer)
{
  // 1) Copy the input (dry) signal into dryBuffer
  const int numChannels = buffer.getNumChannels();
  const int numSamples = buffer.getNumSamples();

  // We'll copy each channel
  for (int ch = 0; ch < numChannels; ++ch)
    dryBuffer.copyFrom(ch, 0, buffer, ch, 0, numSamples);

  // 2) Convert to AudioBlock & oversample
  juce::dsp::AudioBlock<float> block(buffer);
  auto subset = block.getSubsetChannelBlock(0, juce::jmin(2, (int)block.getNumChannels()));
  auto oversampledBlock = oversampler->processSamplesUp(subset);

  // 3) Triode processing
  applyTriodeStages(sampleRate, oversampledBlock, driveParam, biasParam);

  // 4) Downsample
  oversampler->processSamplesDown(subset);

  // 5) Mix the result with the original DRY buffer
  const float wetGain = mixParam;        // e.g. 0.0..1.0
  const float dryGain = 1.0f - wetGain;

  for (int ch = 0; ch < numChannels; ++ch)
  {
    float* wetData = buffer.getWritePointer(ch);
    const float* dryData = dryBuffer.getReadPointer(ch);

    for (int i = 0; i < numSamples; ++i)
    {
      const float wetVal = wetData[i];
      const float dryVal = dryData[i];
      wetData[i] = (wetGain * wetVal) + (dryGain * dryVal);
    }
  }
}

