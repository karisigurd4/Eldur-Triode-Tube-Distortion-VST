// korenTriodeModel.h

#pragma once

#include <JuceHeader.h>

// Holds the logic for computing the triode distortion via the Koren model.
class KorenTriodeModel
{
public:
  KorenTriodeModel() = default;
  ~KorenTriodeModel() = default;

  // Solve for Vp given an input, using Koren’s equations
  static float solveForVp(float Vgk, float B_plus, float Rp, float G, float mu, float C, float P,
    int maxIter = 5, float tol = 1e-7, float Vp_init = 200.0f);

  // Applies the full Koren triode algorithm on an audio block (oversampled).
  //    in/out: audio block pointers
  static void processAudioBlock(const juce::dsp::AudioBlock<float>& block,
    float gainVal, float bias, float drive,
    float G, float mu, float C, float P,
    float B_plus, float Rp,
    int maxIter = 8, float tol = 1e-5);

private:
};

