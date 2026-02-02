// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "k_weighting.h"

#include <complex>
#include <cstdint>
#include <cstdlib>

#include "ebur128_constants.h"

namespace loudness {

namespace {

// Biquad filter coefficients are in the following order:
// a1, a2, b0, b1, b2.
// a0 is implicitly assumed to be 1.0
inline constexpr BiquadCoeffs kKWeighting48000Stage1 = {
    -1.69065929318241f, 0.73248077421585f, 1.53512485958697f,
    -2.69169618940638f, 1.19839281085285f};

inline constexpr BiquadCoeffs kKWeighting48000Stage2 = {
    -1.99004745483398f, 0.99007225036621f, 1.0f, -2.0f, 1.0f};

inline constexpr BiquadCoeffs kKWeighting44100Stage1 = {
    -1.66365511325602f, 0.712595428073226f, 1.53084123005035f,
    -2.65097999515473f, 1.16907907992159f};

inline constexpr BiquadCoeffs kKWeighting44100Stage2 = {
    -1.98916967362980f, 0.989199035787039f, 1.0f, -2.0f, 1.0f};

// If sample rate is too low, a second set of coeffecients are not generated.
// These coefficients describe fallback coefficients to be used if bilinear
// transform code does not provide the filter.
inline constexpr BiquadCoeffs kKWeightingFallbackStage1 = {0.0f, 0.0f, 1.0f,
                                                           0.0f, 0.0f};
inline constexpr BiquadCoeffs kKWeightingFallbackStage2 = {0.0f, 0.0f, 1.0f,
                                                           -2.0f, 1.0f};

// The K-weighting filter, described by poles and zeros in the s-plane (analog
// domain)
inline constexpr double kKWeightingGain = 1.585;
inline constexpr double kKWeightingRealPole = -240;  // applied twice
inline constexpr std::complex<double> kKWeightingConjPole = {-7471.63, 7534.19};
inline constexpr double kKWeightingRealZero = 0;  // applied twice
inline constexpr std::complex<double> kKWeightingConjZero = {-5943.129,
                                                             5976.7400};

}  // anonymous namespace

void InitKWeightingFilter(int32_t sample_rate, BiquadCoeffs& stage1,
                          BiquadCoeffs& stage2) {
  // Determine the K-weighting filter coefficients for the input signal's
  // sample rate. 48 kHz and 44.1 kHz rates use hard-coded coefficients
  // because they are more accurate on the compliance tests. Other sample
  // rates use coefficients derived from the analog definition of the filter.
  if (sample_rate == 48000) {
    stage1 = kKWeighting48000Stage1;
    stage2 = kKWeighting48000Stage2;
  } else if (sample_rate == 44100) {
    stage1 = kKWeighting44100Stage1;
    stage2 = kKWeighting44100Stage2;
  } else if (sample_rate >= kMinimumSupportedSampleRate) {
    // Use bilinear transform to compute the discrete (z-plane) poles and
    // zeros for the given sample rate.
    const double K = 2 * sample_rate;
    const double discrete_real_pole =
        (K + kKWeightingRealPole) / (K - kKWeightingRealPole);
    const std::complex<double> discrete_conj_pole =
        (K + kKWeightingConjPole) / (K - kKWeightingConjPole);
    // discrete_real_zero = 1;  // (K + 0) / (K - 0)
    const std::complex<double> discrete_conj_zero =
        (K + kKWeightingConjZero) / (K - kKWeightingConjZero);

    std::complex<double> Y = kKWeightingGain;
    const std::complex<double> complex_K = K;
    // Note: divide-by-zero can only happen in this code if the sample rate is
    // negative, but we would not have entered this code path in that case.
    Y /= (kKWeightingRealPole - complex_K) * (kKWeightingRealPole - complex_K);
    Y /= (kKWeightingConjPole - complex_K) *
         (std::conj(kKWeightingConjPole) - complex_K);
    Y *= (kKWeightingRealZero - complex_K) * (kKWeightingRealZero - complex_K);
    Y *= (kKWeightingConjZero - complex_K) *
         (std::conj(kKWeightingConjZero) - complex_K);
    const double discrete_gain = std::abs(Y);

    // Compute the filter coefficients from the poles and zeros.
    stage1[0] = -(discrete_conj_pole + std::conj(discrete_conj_pole)).real();
    stage1[1] = (discrete_conj_pole * std::conj(discrete_conj_pole)).real();

    stage1[2] = 1.0 * discrete_gain;
    stage1[3] = -(discrete_conj_zero + std::conj(discrete_conj_zero)).real() *
                discrete_gain;
    stage1[4] = (discrete_conj_zero * std::conj(discrete_conj_zero)).real() *
                discrete_gain;

    stage2[0] = -discrete_real_pole - discrete_real_pole;
    stage2[1] = discrete_real_pole * discrete_real_pole;

    stage2[2] = 1.0;
    stage2[3] = -2.0;  // -discrete_real_zero - discrete_real_zero;
    stage2[4] = 1.0;   // discrete_real_zero * discrete_real_zero;
  } else {
    // The sample rate is so low that the K-weighting poles and zeros may not
    // be usable. Or, the sample rate may be negative which is invalid. For
    // now, fall back to no-op filters. In the future it should be possible to
    // compute a proper K-weighting filter for lower sample rates.
    stage1 = kKWeightingFallbackStage1;
    stage2 = kKWeightingFallbackStage2;
  }
}

}  // namespace loudness
