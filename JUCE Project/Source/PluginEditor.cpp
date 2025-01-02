#include "PluginEditor.h"

//==============================================================================
ImperialTriodeOverlordAudioProcessorEditor::ImperialTriodeOverlordAudioProcessorEditor
(ImperialTriodeOverlordAudioProcessor& p)
  : AudioProcessorEditor(&p), processor(p)
{
  // Set the size of the editor
  const float bgrSizeX = 1130.0f;
  const float bgrSizeY = 561.0f;
  const float scaleUI = 0.6f;
  setSize((int)(bgrSizeX * scaleUI), (int)(bgrSizeY * scaleUI));

  // Setup the sliders
  setupSlider(driveSlider, "drive");
  setupSlider(mixSlider, "mix");
  setupSlider(biasSlider, "bias");

  // For a minimalistic look, hide the textboxes
  driveSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
  mixSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
  biasSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);

  // Bypass toggle
  bypassButton.setClickingTogglesState(true);
  bypassButton.setToggleState(processor.getBypass(),
    juce::NotificationType::dontSendNotification);
  addAndMakeVisible(bypassButton);

  // Bypass onClick
  bypassButton.onClick = [this]
  {
    processor.setBypass(bypassButton.getToggleState());
  };

#if DEBUG
  // File playback buttons + labels
  addAndMakeVisible(loadButton);
  loadButton.addListener(this);
  loadLabel.setText("Load", juce::dontSendNotification);
  loadLabel.setJustificationType(juce::Justification::centred);
  addAndMakeVisible(loadLabel);

  addAndMakeVisible(startButton);
  startButton.addListener(this);
  startLabel.setText("Start", juce::dontSendNotification);
  startLabel.setJustificationType(juce::Justification::centred);
  addAndMakeVisible(startLabel);

  addAndMakeVisible(stopButton);
  stopButton.addListener(this);
  stopLabel.setText("Stop", juce::dontSendNotification);
  stopLabel.setJustificationType(juce::Justification::centred);
  addAndMakeVisible(stopLabel);
#endif

  // Start the timer to refresh UI ~30x/sec
  startTimerHz(30);

  // Load your multi-frame knob images from BinaryData
  // The code below is just an example approach; adjust to your actual resource names.

  for (int i = 1; i <= 100; ++i)
  {
    // Build a resource name. It might be something like "Knob_0001_png" depending on your naming.
    auto resourceName = "_" + juce::String(i).paddedLeft('0', 4) + "_png";
    int dataSize = 0;

    if (auto* dataPtr = BinaryData::getNamedResource(resourceName.toRawUTF8(), dataSize))
    {
      auto image = juce::ImageFileFormat::loadFrom(dataPtr, (size_t)dataSize);
      if (image.isValid())
        knobFrames.push_back(image);
    }
    else
    {
      DBG("Resource not found: " << resourceName);
    }
  }

  // Example for "stepped" knobs:
  steppedKnobFrames.push_back(juce::ImageFileFormat::loadFrom(BinaryData::StepKnob1_png, BinaryData::StepKnob1_pngSize));
  steppedKnobFrames.push_back(juce::ImageFileFormat::loadFrom(BinaryData::StepKnob3_png, BinaryData::StepKnob3_pngSize));
  steppedKnobFrames.push_back(juce::ImageFileFormat::loadFrom(BinaryData::StepKnob2_png, BinaryData::StepKnob2_pngSize));

  // Assign these frames to the LookAndFeels
  knobLookAndFeel.setKnobFrames(knobFrames);
  steppedKnobLookAndFeel.setKnobFrames(steppedKnobFrames);

  // Let our sliders use the custom LNFs
  driveSlider.setLookAndFeel(&knobLookAndFeel);
  mixSlider.setLookAndFeel(&knobLookAndFeel);
  biasSlider.setLookAndFeel(&knobLookAndFeel);

  // Load background from BinaryData
  bgImage = juce::ImageFileFormat::loadFrom(BinaryData::bgr3_png,
    BinaryData::bgr3_pngSize);
}

//==============================================================================
ImperialTriodeOverlordAudioProcessorEditor::~ImperialTriodeOverlordAudioProcessorEditor()
{
  // Important: always clear the slider's LNF before the LNF itself is destroyed
  driveSlider.setLookAndFeel(nullptr);
  mixSlider.setLookAndFeel(nullptr);
  biasSlider.setLookAndFeel(nullptr);

  stopTimer();
}

//==============================================================================
void ImperialTriodeOverlordAudioProcessorEditor::setupSlider(juce::Slider& s,
  const juce::String& paramID)
{
  addAndMakeVisible(s);
  s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
  s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 20);

  if (paramID == "drive")
  {
    driveAttachment.reset(
      new juce::AudioProcessorValueTreeState::SliderAttachment(
        processor.getValueTreeState(), paramID, s));
  }
  else if (paramID == "mix")
  {
    mixAttachment.reset(
      new juce::AudioProcessorValueTreeState::SliderAttachment(
        processor.getValueTreeState(), paramID, s));
  }
  else if (paramID == "bias")
  {
    biasAttachment.reset(
      new juce::AudioProcessorValueTreeState::SliderAttachment(
        processor.getValueTreeState(), paramID, s));
  }
}

//==============================================================================
void ImperialTriodeOverlordAudioProcessorEditor::paint(juce::Graphics& g)
{
  g.fillAll(juce::Colours::darkgrey);

  // If the image is valid, draw it
  if (bgImage.isValid())
  {
    g.drawImage(bgImage, getLocalBounds().toFloat());
  }

  // You could also overlay RMS text or other UI elements here
  // For example:
  // g.setColour(juce::Colours::white);
  // g.drawFittedText("RMS: " + juce::String(currentRms, 2),
  //                  10, 10, 100, 20, juce::Justification::centredLeft, 1);
}

//==============================================================================
void ImperialTriodeOverlordAudioProcessorEditor::resized()
{
  auto area = getLocalBounds().reduced(10);

#if DEBUG
  // Place debug buttons + labels across the top
  {
    auto debugArea = area.removeFromTop(30);
    auto buttonWidth = 80;
    auto labelHeight = 20;

    loadButton.setBounds(debugArea.removeFromLeft(buttonWidth));
    loadLabel.setBounds(loadButton.getX(),
      loadButton.getBottom(),
      loadButton.getWidth(), labelHeight);

    startButton.setBounds(debugArea.removeFromLeft(buttonWidth));
    startLabel.setBounds(startButton.getX(),
      startButton.getBottom(),
      startButton.getWidth(), labelHeight);

    stopButton.setBounds(debugArea.removeFromLeft(buttonWidth));
    stopLabel.setBounds(stopButton.getX(),
      stopButton.getBottom(),
      stopButton.getWidth(), labelHeight);
  }

  // Next row for bypass
  {
    auto bypassArea = area.removeFromTop(40);
    bypassButton.setBounds(bypassArea.removeFromLeft(80));
    // If you want a label for bypass, place it below or to the right
  }
#endif

  // Layout your sliders, e.g. 3 knobs in a row:
  {
    // Just an example coordinate layout
    biasSlider.setBounds(34, 130, 180, 180);
    driveSlider.setBounds(246, 130, 180, 180);
    mixSlider.setBounds(460, 130, 180, 180);
  }
}

//==============================================================================
void ImperialTriodeOverlordAudioProcessorEditor::buttonClicked(juce::Button* button)
{
#if DEBUG
  if (button == &loadButton)
  {
    // Show a file chooser for user to pick an audio file
    auto chooser = std::make_unique<juce::FileChooser>(
      "Select an audio file to load...",
      juce::File{},
      "*.wav;*.aiff;*.mp3");

    constexpr int chooserFlags = juce::FileBrowserComponent::openMode
      | juce::FileBrowserComponent::canSelectFiles;

    chooser->launchAsync(chooserFlags, [this](const juce::FileChooser& fc)
      {
        auto file = fc.getResult();
        if (file.existsAsFile())
          processor.loadFile(file);
      });

    fileChooser = std::move(chooser);
  }
  else if (button == &startButton)
  {
    processor.startPlayback();
  }
  else if (button == &stopButton)
  {
    processor.stopPlayback();
  }
#endif
}

//==============================================================================
void ImperialTriodeOverlordAudioProcessorEditor::timerCallback()
{
  // Poll the processor's RMS level to display in the editor
  currentRms = processor.getRmsLevel();
  repaint();
}
