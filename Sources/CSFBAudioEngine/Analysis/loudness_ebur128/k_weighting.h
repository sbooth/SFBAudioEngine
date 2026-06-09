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

#ifndef LOUDNESS_EBUR128_SRC_K_WEIGHTING_H_
#define LOUDNESS_EBUR128_SRC_K_WEIGHTING_H_

#include <array>
#include <cstdint>

namespace loudness {

using BiquadCoeffs = std::array<float, 5>;

// Computes the filter coefficients for both Stage1 and Stage2 of the
// k-weighting scheme, as defined by ITU 1770.
void InitKWeightingFilter(int32_t sample_rate, BiquadCoeffs& stage1,
                          BiquadCoeffs& stage2);

}  // namespace loudness

#endif  // LOUDNESS_EBUR128_SRC_K_WEIGHTING_H_
