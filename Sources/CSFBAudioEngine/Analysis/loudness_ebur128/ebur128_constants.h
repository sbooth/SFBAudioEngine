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

#ifndef LOUDNESS_EBUR128_SRC_EBUR128_CONSTANTS_H_
#define LOUDNESS_EBUR128_SRC_EBUR128_CONSTANTS_H_

#include <cmath>
#include <cstdint>
#include <limits>

namespace loudness {

// Smallest sample rate in Hz for which this code supports proper K-weighted
// filtering. Sample rates lower than this will not produce a compliant
// loudness measurement.
inline constexpr int kMinimumSupportedSampleRate = 3378;

// Maximum number of channels that will be measured by the loudness library,
// even if the input audio has more channels than this.
inline constexpr int kMaxNumChannelsMeasured = 32;

// Number of 100-millisecond steps in a 400 millisecond "momentary block".
inline constexpr int kStepsPerMomentaryBlock = 4;

// Number of 100-millisecond steps in a 3 second "short-term block".
inline constexpr int kStepsPerShortTermBlock = 30;

// A biquad filter technically has 6 coefficients, but first coefficient is
// always 1.
inline constexpr int kNumBiquadCoeffs = 5;
inline constexpr int kNumBiquadStages = 2;

// Minimum dBFS / LKFS value for clamping to avoid reporting -inf.
// Note that -10000 LKFS is well below the theoretical smallest amplitude
// representable by double-precision (i.e. 1e-308, corresponding to
// approximately -6160 dBFS).
inline constexpr float kMinLKFS = -10000.0f;
inline constexpr float kMinDBFS = -10000.0f;

// Absolute gating threshold. Momentary loudness measurements below -70 LUFS are
// not included when computing gated integrated loudness.
inline constexpr float kAbsoluteThresholdLKFS = -70.0f;
inline const float kPowerAbsoluteThreshold =
    pow(10.0f, 0.1f * (kAbsoluteThresholdLKFS + 0.691f));

// ITU 1770, i.e. for computing relative-gated integrated loudness, uses a
// relative threshold that is 10 LU (dB) below the absolute-gated integrated
// loudness.
inline constexpr float k1770RelativeThresholdLU = -10.0f;

// EBU 3342, for computing LRA (loudness range), uses a relative threshold that
// is 20 LU (dB) below the absolute-gated integrated loudness.
inline constexpr float k3342RelativeThresholdLU = -20.0f;

// "Momentary" refers to the duration of measurement for "momentary loudness" as
// defined by EBU 3341. This is the same duration as a single "gating block"
// described in ITU 1770-4, which is 400 milliseconds.
inline constexpr float kMomentaryBlockSizeSeconds = 0.4f;

// "Short term" refers to the duration of measurement for "short-term loudness"
// as defined by EBU 3341.
inline constexpr float kShortTermBlockSizeSeconds = 3.0f;

// Rms block length set to match step length - i.e., 0% overlap between blocks.
inline constexpr float kRmsBlockSizeSeconds = 0.1f;

// ITU 1770 and EBU 3341 specify that momentary and short-term block sizes
// should be updated at a minimum of 10 Hz. This corresponds to stepping forward
// by 100 milliseconds for each next measurement.
inline constexpr float kStepLengthSeconds = 0.1f;

// EBU 3341 defines that LRA, while it could be computed, should be annotated as
// "not stable" until at least 60 seconds of audio have been processed.
inline constexpr float k3341StableLRASeconds = 60.0f;

// ITU 1770 specifies the following four upsampling FIR filter phases, used
// for measuring true peaks.
inline constexpr int kTruePeakFilterLength = 12;
inline constexpr float kTruePeakFilterPhase0[] = {
    0.0017089843750f,  0.0109863281250f,  -0.0196533203125f, 0.0332031250000f,
    -0.0594482421875f, 0.1373291015625f,  0.9721679687500f,  -0.1022949218750f,
    0.0476074218750f,  -0.0266113281250f, 0.0148925781250f,  -0.0083007812500f};

inline constexpr float kTruePeakFilterPhase1[] = {
    -0.0291748046875f, 0.0292968750000f,  -0.0517578125000f, 0.0891113281250f,
    -0.1665039062500f, 0.4650878906250f,  0.7797851562500f,  -0.2003173828125f,
    0.1015625000000f,  -0.0582275390625f, 0.0330810546875f,  -0.0189208984375f};

inline constexpr float kTruePeakFilterPhase2[] = {
    -0.0189208984375f, 0.0330810546875f,  -0.0582275390625f, 0.1015625000000f,
    -0.2003173828125f, 0.7797851562500f,  0.4650878906250f,  -0.1665039062500f,
    0.0891113281250f,  -0.0517578125000f, 0.0292968750000f,  -0.0291748046875f};

inline constexpr float kTruePeakFilterPhase3[] = {
    -0.0083007812500f, 0.0148925781250f,  -0.0266113281250f, 0.0476074218750f,
    -0.1022949218750f, 0.9721679687500f,  0.1373291015625f,  -0.0594482421875f,
    0.0332031250000f,  -0.0196533203125f, 0.0109863281250f,  0.0017089843750f};

// Constants for converting the canonical 16-bit or 32-bit integer audio
// sample formats into canonical floating point audio format.
inline constexpr float kNorm16 = 1.0f / std::numeric_limits<int16_t>::max();
inline constexpr float kNorm32 = 1.0f / std::numeric_limits<int32_t>::max();

}  // namespace loudness
#endif  // LOUDNESS_EBUR128_SRC_EBUR128_CONSTANTS_H_
