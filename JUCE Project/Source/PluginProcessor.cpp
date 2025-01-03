#include "PluginProcessor.h"
#include "PluginEditor.h"

ImperialTriodeOverlordAudioProcessor::ImperialTriodeOverlordAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
  : AudioProcessor(BusesProperties()
#if ! JucePlugin_IsMidiEffect
#if ! JucePlugin_IsSynth
    .withInput("Input", juce::AudioChannelSet::stereo(), true)
#endif
    .withOutput("Output", juce::AudioChannelSet::stereo(), true)
#endif
  ),
  parameters(*this, nullptr, juce::Identifier("params"),
    {
        std::make_unique<juce::AudioParameterFloat>("drive", "Drive",  0.25f, 1.0f, 0.6f),
        std::make_unique<juce::AudioParameterFloat>("mix",   "Mix",    0.0f, 1.0f, 1.0f),
        std::make_unique<juce::AudioParameterFloat>("bias",  "Bias",   0.0f, 2.0f, 0.0f)
    })
#endif
{
  // Initialize smoothing, oversampling, etc. if needed
  autoGainDb.setCurrentAndTargetValue(-12.0f);
#if DEBUG 
  formatManager.registerBasicFormats();
#endif
}

ImperialTriodeOverlordAudioProcessor::~ImperialTriodeOverlordAudioProcessor() {}

void ImperialTriodeOverlordAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
  currentSampleRate = (float)sampleRate;

#if DEBUG
  transportSource.prepareToPlay(samplesPerBlock, sampleRate);
#endif

  // Setup process spec
  juce::dsp::ProcessSpec spec;
  spec.sampleRate = sampleRate;
  spec.maximumBlockSize = (juce::uint32)samplesPerBlock;
  spec.numChannels = (juce::uint32)getTotalNumOutputChannels();

  // Prepare our engine & tone stack
  distortionEngine.prepare(spec, 1); // 2x oversampling or whatever
  distortionEngine.reset();

  autoGainDb.reset(sampleRate, 0.001); // 1ms ramp, adjust as needed
}

void ImperialTriodeOverlordAudioProcessor::releaseResources()
{
  // Release anything needed
}

bool ImperialTriodeOverlordAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
  if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
    return false;

  if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::stereo())
    return false;

  return true;
}

void ImperialTriodeOverlordAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
  juce::MidiBuffer& midiMessages)
{
  juce::ScopedNoDenormals noDenormals;

#if DEBUG
  // Fill the buffer from the transport source
  transportSource.getNextAudioBlock(juce::AudioSourceChannelInfo(buffer));
#endif

  // 1) Check bypass
  if (bypass)
    return;

  // 2) Pre RMS
  PreCalcAutoGainRms(buffer);

  // 3) Update DistortionEngine parameters
  float drive = *parameters.getRawParameterValue("drive");
  float bias = *parameters.getRawParameterValue("bias");
  float mix = *parameters.getRawParameterValue("mix");

  distortionEngine.setDrive(drive);
  distortionEngine.setBias(bias);
  distortionEngine.setMix(mix);

  // 4) Distortion
  distortionEngine.processBlock(getSampleRate(), buffer);

  // 5) Brickwall limit
  brickwallLimit(buffer);

  // 6) Post RMS + autogain
  PostCalcAutoGainRms(buffer);
}

void ImperialTriodeOverlordAudioProcessor::brickwallLimit(juce::AudioBuffer<float>& buffer)
{
  auto numChannels = buffer.getNumChannels();
  auto numSamples = buffer.getNumSamples();

  for (int ch = 0; ch < numChannels; ++ch)
  {
    float* channelData = buffer.getWritePointer(ch);
    for (int i = 0; i < numSamples; ++i)
      channelData[i] = juce::jlimit(-1.0f, 1.0f, channelData[i]);
  }
}

void ImperialTriodeOverlordAudioProcessor::PostCalcAutoGainRms(juce::AudioBuffer<float>& buffer)
{
  float sumOfSquaresOutput = 0.0f;
  for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
  {
    const float* readPtr = buffer.getReadPointer(ch);
    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
      float sampleVal = readPtr[i];
      sumOfSquaresOutput += sampleVal * sampleVal;
    }
  }
  float meanSquareOutput = sumOfSquaresOutput / (buffer.getNumSamples() * buffer.getNumChannels());
  lastOutputRms = std::sqrt(meanSquareOutput);

  float threshold = 0.001f; // ~ -80 dB
  if (lastInputRms > threshold)
  {
    float inputAmp = lastInputRms + 1e-12f;   // avoid /0
    float outputAmp = lastOutputRms + 1e-12f;

    float correctionDb = juce::Decibels::gainToDecibels(inputAmp / outputAmp);

    autoGainDb.setTargetValue(correctionDb);
  }

  for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
  {
    float currentDb = autoGainDb.getNextValue();
    float currentLin = juce::Decibels::decibelsToGain(currentDb);

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
      float* writePtr = buffer.getWritePointer(ch);
      writePtr[sample] *= currentLin;
    }
  }
}

void ImperialTriodeOverlordAudioProcessor::PreCalcAutoGainRms(juce::AudioBuffer<float>& buffer)
{
  float sumOfSquaresInput = 0.0f;

  for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
  {
    const float* readPtr = buffer.getReadPointer(ch);
    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
      float sampleVal = readPtr[i];
      sumOfSquaresInput += sampleVal * sampleVal;
    }
  }
  float meanSquareInput = sumOfSquaresInput / (buffer.getNumSamples() * buffer.getNumChannels());
  lastInputRms = std::sqrt(meanSquareInput);
}

//==============================================================================
juce::AudioProcessorEditor* ImperialTriodeOverlordAudioProcessor::createEditor()
{
  return new ImperialTriodeOverlordAudioProcessorEditor(*this);
}

//==============================================================================
void ImperialTriodeOverlordAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
  auto state = parameters.copyState();
  std::unique_ptr<juce::XmlElement> xml(state.createXml());
  copyXmlToBinary(*xml, destData);
}

void ImperialTriodeOverlordAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
  std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));

  if (xmlState.get() != nullptr)
    if (xmlState->hasTagName(parameters.state.getType()))
      parameters.replaceState(juce::ValueTree::fromXml(*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
  return new ImperialTriodeOverlordAudioProcessor();
}

#if DEBUG
void ImperialTriodeOverlordAudioProcessor::loadFile(const juce::File& audioFile)
{
  // Stop any current playback
  transportSource.stop();
  transportSource.setSource(nullptr);

  // Attempt to create a reader
  if (auto* reader = formatManager.createReaderFor(audioFile))
  {
    // Create new reader source and set it on transport
    readerSource.reset(new juce::AudioFormatReaderSource(reader, true));
    transportSource.setSource(readerSource.get(),
      0,          // No resampling
      nullptr,    // No buffering
      reader->sampleRate);
  }
}

void ImperialTriodeOverlordAudioProcessor::startPlayback()
{
  transportSource.start();
}

void ImperialTriodeOverlordAudioProcessor::stopPlayback()
{
  transportSource.stop();
}
#endif