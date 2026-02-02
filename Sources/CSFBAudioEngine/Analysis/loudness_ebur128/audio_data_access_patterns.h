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

#ifndef LOUDNESS_EBUR12_SRC_AUDIO_DATA_ACCESS_PATTERNS_H_
#define LOUDNESS_EBUR12_SRC_AUDIO_DATA_ACCESS_PATTERNS_H_

//
// EbuR128Analyzer supports several sample formats and sample layouts, but
// each format/layout combination requires a subtly different data access
// pattern.
//
// To avoid having 12 different copies of an optimized walk through the data
// because of format-specific and layout-specific data access patterns, instead
// those patterns are extracted into templatized functions in this file. This
// allows us to maintain one implementation of the optimized code that can still
// provide benefit to all sample formats and sample layouts.
//
// As long as inlining is honored by the compiler, there was essentially no
// performance penalty to extracting data access into helper functions. In fact
// for non-SIMD implementation, this is an 8-9% performance improvement compared
// to the overhead of a runtime if/switch statements.
//
// About the templatization:
// Default templates need to be defined, but will not be useful because every
// possible specialization requires a slightly different implementation. The
// delete syntax enforces this, but older compilers might not support the delete
// mechanism.  If needed, users of this code can resort to runtime assertion.
//

#include <cstdint>

#include "ebur128_analyzer.h"
#include "ebur128_constants.h"

namespace loudness {

using DataPlaneType = const void*;
using loudness::EbuR128Analyzer;

//
// GetDataPosition
//
// Given the multi-channel audio data and info about planar and interleaved
// strides, returns a pointer to the requested channel and sample index.
//
template <typename T, EbuR128Analyzer::SampleLayout SL>
const T* GetDataPosition(const void* audio_data, int64_t sample_index,
                         int channel_index, int interleaved_stride,
                         int64_t planar_stride) = delete;

template <>
/* __attribute__((always_inline)) */ inline const int16_t*
GetDataPosition<int16_t, EbuR128Analyzer::INTERLEAVED>(const void* audio_data,
                                                       int64_t sample_index,
                                                       int channel_index,
                                                       int interleaved_stride,
                                                       int64_t planar_stride) {
  return reinterpret_cast<const int16_t*>(audio_data) +
         (sample_index * interleaved_stride) + channel_index;
}

template <>
/* __attribute__((always_inline)) */ inline const int16_t*
GetDataPosition<int16_t, EbuR128Analyzer::PLANAR_CONTIGUOUS>(
    const void* audio_data, int64_t sample_index, int channel_index,
    int interleaved_stride, int64_t planar_stride) {
  return (reinterpret_cast<const int16_t*>(audio_data)) +
         (channel_index * planar_stride) + sample_index;
}

template <>
/* __attribute__((always_inline)) */ inline const int16_t*
GetDataPosition<int16_t, EbuR128Analyzer::PLANAR_NON_CONTIGUOUS>(
    const void* audio_data, int64_t sample_index, int channel_index,
    int interleaved_stride, int64_t planar_stride) {
  const DataPlaneType* plane_pointers =
      reinterpret_cast<const DataPlaneType*>(audio_data);
  return reinterpret_cast<const int16_t*>(plane_pointers[channel_index]) +
         sample_index;
}

template <>
/* __attribute__((always_inline)) */ inline const int32_t*
GetDataPosition<int32_t, EbuR128Analyzer::INTERLEAVED>(const void* audio_data,
                                                       int64_t sample_index,
                                                       int channel_index,
                                                       int interleaved_stride,
                                                       int64_t planar_stride) {
  return reinterpret_cast<const int32_t*>(audio_data) +
         (sample_index * interleaved_stride) + channel_index;
}

template <>
/* __attribute__((always_inline)) */ inline const int32_t*
GetDataPosition<int32_t, EbuR128Analyzer::PLANAR_CONTIGUOUS>(
    const void* audio_data, int64_t sample_index, int channel_index,
    int interleaved_stride, int64_t planar_stride) {
  return (reinterpret_cast<const int32_t*>(audio_data)) +
         (channel_index * planar_stride) + sample_index;
}

template <>
/* __attribute__((always_inline)) */ inline const int32_t*
GetDataPosition<int32_t, EbuR128Analyzer::PLANAR_NON_CONTIGUOUS>(
    const void* audio_data, int64_t sample_index, int channel_index,
    int interleaved_stride, int64_t planar_stride) {
  const DataPlaneType* plane_pointers =
      reinterpret_cast<const DataPlaneType*>(audio_data);
  return reinterpret_cast<const int32_t*>(plane_pointers[channel_index]) +
         sample_index;
}

template <>
/* __attribute__((always_inline)) */ inline const float*
GetDataPosition<float, EbuR128Analyzer::INTERLEAVED>(const void* audio_data,
                                                     int64_t sample_index,
                                                     int channel_index,
                                                     int interleaved_stride,
                                                     int64_t planar_stride) {
  return reinterpret_cast<const float*>(audio_data) +
         (sample_index * interleaved_stride) + channel_index;
}

template <>
/* __attribute__((always_inline)) */ inline const float*
GetDataPosition<float, EbuR128Analyzer::PLANAR_CONTIGUOUS>(
    const void* audio_data, int64_t sample_index, int channel_index,
    int interleaved_stride, int64_t planar_stride) {
  return (reinterpret_cast<const float*>(audio_data)) +
         (channel_index * planar_stride) + sample_index;
}

template <>
/* __attribute__((always_inline)) */ inline const float*
GetDataPosition<float, EbuR128Analyzer::PLANAR_NON_CONTIGUOUS>(
    const void* audio_data, int64_t sample_index, int channel_index,
    int interleaved_stride, int64_t planar_stride) {
  const DataPlaneType* plane_pointers =
      reinterpret_cast<const DataPlaneType*>(audio_data);
  return reinterpret_cast<const float*>(plane_pointers[channel_index]) +
         sample_index;
}

template <>
/* __attribute__((always_inline)) */ inline const double*
GetDataPosition<double, EbuR128Analyzer::INTERLEAVED>(const void* audio_data,
                                                      int64_t sample_index,
                                                      int channel_index,
                                                      int interleaved_stride,
                                                      int64_t planar_stride) {
  return reinterpret_cast<const double*>(audio_data) +
         (sample_index * interleaved_stride) + channel_index;
}

template <>
/* __attribute__((always_inline)) */ inline const double*
GetDataPosition<double, EbuR128Analyzer::PLANAR_CONTIGUOUS>(
    const void* audio_data, int64_t sample_index, int channel_index,
    int interleaved_stride, int64_t planar_stride) {
  return (reinterpret_cast<const double*>(audio_data)) +
         (channel_index * planar_stride) + sample_index;
}

template <>
/* __attribute__((always_inline)) */ inline const double*
GetDataPosition<double, EbuR128Analyzer::PLANAR_NON_CONTIGUOUS>(
    const void* audio_data, int64_t sample_index, int channel_index,
    int interleaved_stride, int64_t planar_stride) {
  const DataPlaneType* plane_pointers =
      reinterpret_cast<const DataPlaneType*>(audio_data);
  return reinterpret_cast<const double*>(plane_pointers[channel_index]) +
         sample_index;
}

//
// GetSampleFromOrigin
//
// Given the multi-channel audio data and info about planar and interleaved
// strides, returns the audio sample at the channel index and sample index,
// converted to canonical floating-point audio range.
//
template <typename T, EbuR128Analyzer::SampleLayout LAYOUT>
float GetSampleFromOrigin(const void* audio_data, int64_t sample_index,
                          int channel_index, int interleaved_stride,
                          int64_t planar_stride) = delete;

template <>
/* __attribute__((always_inline)) */ inline float
GetSampleFromOrigin<int16_t, EbuR128Analyzer::INTERLEAVED>(
    const void* audio_data, int64_t sample_index, int channel_index,
    int interleaved_stride, int64_t planar_stride) {
  return kNorm16 * (GetDataPosition<int16_t, EbuR128Analyzer::INTERLEAVED>(
                       audio_data, sample_index, channel_index,
                       interleaved_stride, planar_stride))[0];
}

template <>
/* __attribute__((always_inline)) */ inline float
GetSampleFromOrigin<int16_t, EbuR128Analyzer::PLANAR_CONTIGUOUS>(
    const void* audio_data, int64_t sample_index, int channel_index,
    int interleaved_stride, int64_t planar_stride) {
  return kNorm16 *
         (GetDataPosition<int16_t, EbuR128Analyzer::PLANAR_CONTIGUOUS>(
             audio_data, sample_index, channel_index, interleaved_stride,
             planar_stride))[0];
}

template <>
/* __attribute__((always_inline)) */ inline float
GetSampleFromOrigin<int16_t, EbuR128Analyzer::PLANAR_NON_CONTIGUOUS>(
    const void* audio_data, int64_t sample_index, int channel_index,
    int interleaved_stride, int64_t planar_stride) {
  return kNorm16 *
         (GetDataPosition<int16_t, EbuR128Analyzer::PLANAR_NON_CONTIGUOUS>(
             audio_data, sample_index, channel_index, interleaved_stride,
             planar_stride))[0];
}

template <>
/* __attribute__((always_inline)) */ inline float
GetSampleFromOrigin<int32_t, EbuR128Analyzer::INTERLEAVED>(
    const void* audio_data, int64_t sample_index, int channel_index,
    int interleaved_stride, int64_t planar_stride) {
  return kNorm32 * (GetDataPosition<int32_t, EbuR128Analyzer::INTERLEAVED>(
                       audio_data, sample_index, channel_index,
                       interleaved_stride, planar_stride))[0];
}

template <>
/* __attribute__((always_inline)) */ inline float
GetSampleFromOrigin<int32_t, EbuR128Analyzer::PLANAR_CONTIGUOUS>(
    const void* audio_data, int64_t sample_index, int channel_index,
    int interleaved_stride, int64_t planar_stride) {
  return kNorm32 *
         (GetDataPosition<int32_t, EbuR128Analyzer::PLANAR_CONTIGUOUS>(
             audio_data, sample_index, channel_index, interleaved_stride,
             planar_stride))[0];
}

template <>
/* __attribute__((always_inline)) */ inline float
GetSampleFromOrigin<int32_t, EbuR128Analyzer::PLANAR_NON_CONTIGUOUS>(
    const void* audio_data, int64_t sample_index, int channel_index,
    int interleaved_stride, int64_t planar_stride) {
  return kNorm32 *
         (GetDataPosition<int32_t, EbuR128Analyzer::PLANAR_NON_CONTIGUOUS>(
             audio_data, sample_index, channel_index, interleaved_stride,
             planar_stride))[0];
}

template <>
/* __attribute__((always_inline)) */ inline float
GetSampleFromOrigin<float, EbuR128Analyzer::INTERLEAVED>(
    const void* audio_data, int64_t sample_index, int channel_index,
    int interleaved_stride, int64_t planar_stride) {
  return (GetDataPosition<float, EbuR128Analyzer::INTERLEAVED>(
      audio_data, sample_index, channel_index, interleaved_stride,
      planar_stride))[0];
}

template <>
/* __attribute__((always_inline)) */ inline float
GetSampleFromOrigin<float, EbuR128Analyzer::PLANAR_CONTIGUOUS>(
    const void* audio_data, int64_t sample_index, int channel_index,
    int interleaved_stride, int64_t planar_stride) {
  return (GetDataPosition<float, EbuR128Analyzer::PLANAR_CONTIGUOUS>(
      audio_data, sample_index, channel_index, interleaved_stride,
      planar_stride))[0];
}

template <>
/* __attribute__((always_inline)) */ inline float
GetSampleFromOrigin<float, EbuR128Analyzer::PLANAR_NON_CONTIGUOUS>(
    const void* audio_data, int64_t sample_index, int channel_index,
    int interleaved_stride, int64_t planar_stride) {
  return (GetDataPosition<float, EbuR128Analyzer::PLANAR_NON_CONTIGUOUS>(
      audio_data, sample_index, channel_index, interleaved_stride,
      planar_stride))[0];
}

template <>
/* __attribute__((always_inline)) */ inline float
GetSampleFromOrigin<double, EbuR128Analyzer::INTERLEAVED>(
    const void* audio_data, int64_t sample_index, int channel_index,
    int interleaved_stride, int64_t planar_stride) {
  return (GetDataPosition<double, EbuR128Analyzer::INTERLEAVED>(
      audio_data, sample_index, channel_index, interleaved_stride,
      planar_stride))[0];
}

template <>
/* __attribute__((always_inline)) */ inline float
GetSampleFromOrigin<double, EbuR128Analyzer::PLANAR_CONTIGUOUS>(
    const void* audio_data, int64_t sample_index, int channel_index,
    int interleaved_stride, int64_t planar_stride) {
  return (GetDataPosition<double, EbuR128Analyzer::PLANAR_CONTIGUOUS>(
      audio_data, sample_index, channel_index, interleaved_stride,
      planar_stride))[0];
}

template <>
/* __attribute__((always_inline)) */ inline float
GetSampleFromOrigin<double, EbuR128Analyzer::PLANAR_NON_CONTIGUOUS>(
    const void* audio_data, int64_t sample_index, int channel_index,
    int interleaved_stride, int64_t planar_stride) {
  return (GetDataPosition<double, EbuR128Analyzer::PLANAR_NON_CONTIGUOUS>(
      audio_data, sample_index, channel_index, interleaved_stride,
      planar_stride))[0];
}

}  // namespace loudness
#endif  // LOUDNESS_EBUR12_SRC_AUDIO_DATA_ACCESS_PATTERNS_H_
