#pragma once

#include <JuceHeader.h>
#include "DistortionEngine.h"

/**
    The main audio processor class for the Eldur plugin.

    Responsibilities:
    - Manages parameters (drive, bias, mix, bypass, etc.)
    - Coordinates the DistortionEngine (which handles oversampling and triode distortion).
    - Coordinates the ToneStack (which handles EQ/filtering).
    - Performs auto-gain calculations and brickwall limiting.
    - Handles state serialization/deserialization.
*/
class ImperialTriodeOverlordAudioProcessor : public juce::AudioProcessor
{
public:
  //==============================================================================
  ImperialTriodeOverlordAudioProcessor();
  ~ImperialTriodeOverlordAudioProcessor() override;

  //==============================================================================
  void prepareToPlay(double sampleRate, int samplesPerBlock) override;
  void releaseResources() override;

#ifndef JucePlugin_PreferredChannelConfigurations
  bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
#endif

  void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;

  //==============================================================================
  juce::AudioProcessorEditor* createEditor() override;
  bool hasEditor() const override { return true; }

  //==============================================================================
  const juce::String getName() const override { return JucePlugin_Name; }
  bool acceptsMidi() const override { return false; }
  bool producesMidi() const override { return false; }
  bool isMidiEffect() const override { return false; }
  double getTailLengthSeconds() const override { return 0.0; }

  //==============================================================================
  int getNumPrograms() override { return 1; }
  int getCurrentProgram() override { return 0; }
  void setCurrentProgram(int) override {}
  const juce::String getProgramName(int) override { return {}; }
  void changeProgramName(int, const juce::String&) override {}

  //==============================================================================
  void getStateInformation(juce::MemoryBlock& destData) override;
  void setStateInformation(const void* data, int sizeInBytes) override;

  //==============================================================================
  /** Returns a reference to the parameter state for external control (e.g. by the GUI). */
  juce::AudioProcessorValueTreeState& getValueTreeState() { return parameters; }

  /** Toggles bypass on/off. */
  void setBypass(bool shouldBypass) { bypass = shouldBypass; }
  bool getBypass() const { return bypass; }

  /** For debug or meter usage. */
  float getRmsLevel() const noexcept { return lastOutputRms; }

#if DEBUG
  // Debug methods for file playback
  void loadFile(const juce::File& audioFile);
  void startPlayback();
  void stopPlayback();
#endif

private:
  //==============================================================================
  /** Helper to apply a hard limiter at ±1.0f. */
  void brickwallLimit(juce::AudioBuffer<float>& buffer);

  /** Measure RMS pre-distortion for auto-gain reference. */
  void PreCalcAutoGainRms(juce::AudioBuffer<float>& buffer);

  /** Measure RMS post-distortion, apply auto-gain correction. */
  void PostCalcAutoGainRms(juce::AudioBuffer<float>& buffer);

  //==============================================================================
  bool bypass = false;

  /** Holds drive, bias, mix parameters, etc. */
  juce::AudioProcessorValueTreeState parameters;

  /** Our higher-level distortion engine (oversampling, triode distortion, M/S, etc.). */
  DistortionEngine distortionEngine;

  /** Auto-gain smoothing in decibels. */
  juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> autoGainDb;

  /** Keep track of input + output RMS for auto-gain calculations. */
  float lastInputRms = 0.0f;
  float lastOutputRms = 0.0f;

  /** Sample rate cache. */
  float currentSampleRate = 44100.0f;

  // Debug stuff
#if DEBUG
  juce::AudioFormatManager formatManager;
  std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
  juce::AudioTransportSource transportSource;
#endif

  //==============================================================================
  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ImperialTriodeOverlordAudioProcessor)
};
