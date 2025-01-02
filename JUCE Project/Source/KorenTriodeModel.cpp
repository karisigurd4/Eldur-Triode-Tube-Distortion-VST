#include "KorenTriodeModel.h"
#include <cmath>

// -----------------------------------------------------------------------------
// Directly computes ln(1 + e^x) and logistic(e^x / (1 + e^x)) 
// on each iteration, instead of using a lookup table.

static float newtonSolveVp(float Vgk,
  float B_plus,
  float Rp,
  float G,
  float mu,
  float C,
  float P,
  int   maxIter,
  float tol,
  float Vp_init)
{
  float Vp = Vp_init;
  const float invMu = 1.0f / mu;
  const float invC = 1.0f / C;

  for (int i = 0; i < maxIter; ++i)
  {
    // x = (Vgk + Vp / mu) / C
    float x = (Vgk + (Vp * invMu)) * invC;

    // Compute ln(1 + e^x) and logistic in real-time:
    float ex = std::exp(x);
    float lnpart = std::log1p(ex);           // ln(1 + e^x)
    float logistic = ex / (1.0f + ex);          // e^x / (1 + e^x)

    // Plate current: Ip = G * [ ln(1 + e^x) ]^P
    float lnpartP = std::pow(lnpart, P);      // lnpart^P
    float Ip = G * lnpartP;

    // f(Vp) = (Vp - B_plus) + (Ip * Rp)
    float f = (Vp - B_plus) + (Ip * Rp);

    if (std::fabs(f) < tol)
      break;

    // Compute derivative
    // lnpart^(P-1) = lnpartP / lnpart
    float lnpartPminus1 = (lnpart > 1e-12f) ? (lnpartP / lnpart) : 0.0f;
    float dlnpart_dVp = logistic / (C * mu);  // derivative of ln(1 + e^x) wrt Vp
    float dIp_dVp = G * P * lnpartPminus1 * dlnpart_dVp;
    float df_dVp = 1.0f + (Rp * dIp_dVp);

    // Newton's method update
    float Vp_new = Vp - (f / df_dVp);

    // If it goes off into a weird space, abort
    if (std::isinf(Vp_new) || std::isnan(Vp_new))
      break;

    Vp = Vp_new;
  }

  return Vp;
}

// -----------------------------------------------------------------------------
// KorenTriodeModel

float KorenTriodeModel::solveForVp(float Vgk,
  float B_plus,
  float Rp,
  float G,
  float mu,
  float C,
  float P,
  int   maxIter,
  float tol,
  float Vp_init)
{
  return newtonSolveVp(Vgk, B_plus, Rp, G, mu, C, P, maxIter, tol, Vp_init);
}

void KorenTriodeModel::processAudioBlock(const juce::dsp::AudioBlock<float>& block,
  float gainVal,
  float bias,
  float drive,
  float G,
  float mu,
  float C,
  float P,
  float B_plus,
  float Rp,
  int   maxIter,
  float tol)
{
  // Precompute scale factor
  const float scale = (gainVal / 300.0f);

  // We keep a running 'Vp_guess' for continuity from sample to sample
  float Vp_guess = B_plus;

  const auto numChannels = block.getNumChannels();
  const auto numSamples = block.getNumSamples();

  for (size_t ch = 0; ch < numChannels; ++ch)
  {
    float* chanData = block.getChannelPointer(ch);

    for (size_t i = 0; i < numSamples; ++i)
    {
      float Vgk = (chanData[i] * drive) + bias;
      Vp_guess = solveForVp(Vgk, B_plus, Rp, G, mu, C, P, maxIter, tol, Vp_guess);
      chanData[i] = Vp_guess * scale;
    }
  }
}
