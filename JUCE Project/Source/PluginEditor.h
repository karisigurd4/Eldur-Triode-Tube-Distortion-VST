#pragma once

#include <JuceHeader.h>
#include <BinaryData.h>  // If your resource data is compiled into BinaryData
#include "PluginProcessor.h"

/**
    A custom LookAndFeel to handle multi-frame knobs.
    We store a vector of frames and draw one based on the slider’s position.
*/
class KnobLookAndFeel : public juce::LookAndFeel_V4
{
public:
  KnobLookAndFeel() = default;

  void setKnobFrames(const std::vector<juce::Image>& frames)
  {
    knobFrames = frames;
  }

  void drawRotarySlider(juce::Graphics& g,
    int x, int y, int width, int height,
    float sliderPosProportional,
    float rotaryStartAngle, float rotaryEndAngle,
    juce::Slider& slider) override
  {
    if (knobFrames.empty())
      return; // Avoid crashing if no images are loaded

    const int numFrames = (int)knobFrames.size();

    // Scale sliderPosProportional [0..1] to [0..numFrames-1]
    int frameIndex = (int)std::floor(sliderPosProportional * (float)(numFrames - 1) + 0.5f);
    frameIndex = juce::jlimit(0, numFrames - 1, frameIndex);

    const auto& frame = knobFrames[(size_t)frameIndex];
    g.drawImage(frame,
      (float)x, (float)y,
      (float)width, (float)height,
      0, 0,
      frame.getWidth(), frame.getHeight());
  }

private:
  std::vector<juce::Image> knobFrames;
};


/**
    The main editor component for the Imperial Triode Overlord plugin.
    Handles the UI: knobs (drive, mix, bias), bypass button, debug playback controls, background image, etc.
*/
class ImperialTriodeOverlordAudioProcessorEditor
  : public juce::AudioProcessorEditor,
  private juce::Button::Listener,
  private juce::Timer
{
public:
  ImperialTriodeOverlordAudioProcessorEditor(ImperialTriodeOverlordAudioProcessor&);
  ~ImperialTriodeOverlordAudioProcessorEditor() override;

  //==============================================================================
  void paint(juce::Graphics& g) override;
  void resized() override;

private:
  // Reference to our processor
  ImperialTriodeOverlordAudioProcessor& processor;

  // Sliders + attachments
  juce::Slider driveSlider, mixSlider, biasSlider;
  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> driveAttachment;
  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mixAttachment;
  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> biasAttachment;

  void setupSlider(juce::Slider& slider, const juce::String& paramID);

  // Bypass Toggle
  juce::ToggleButton bypassButton{ "Bypass" };

  // Debug file playback (only in Debug mode)
#if DEBUG
  juce::TextButton loadButton{ "Load File" };
  juce::TextButton startButton{ "Start" };
  juce::TextButton stopButton{ "Stop" };

  juce::Label loadLabel, startLabel, stopLabel;
  std::unique_ptr<juce::FileChooser> fileChooser;
#endif

  // RMS display
  float currentRms = 0.0f;

  // Timer + button overrides
  void buttonClicked(juce::Button* button) override;
  void timerCallback() override;

  // Knob frames
  std::vector<juce::Image> knobFrames;
  std::vector<juce::Image> steppedKnobFrames;

  // Custom LookAndFeel instances
  KnobLookAndFeel knobLookAndFeel;
  KnobLookAndFeel steppedKnobLookAndFeel;

  // Background image
  juce::Image bgImage;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ImperialTriodeOverlordAudioProcessorEditor)
};
