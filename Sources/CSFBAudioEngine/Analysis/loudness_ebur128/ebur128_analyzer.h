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

#ifndef LOUDNESS_EBUR128_INCLUDE_EBUR128_ANALYZER_H_
#define LOUDNESS_EBUR128_INCLUDE_EBUR128_ANALYZER_H_

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "ebur128_constants.h"

namespace loudness {

// Returns a vector of ITU 1770 channel weights, assuming the following audio
// channel ordering:
//    [ L, R, C, LFE, Ls, Rs ]
// This can be used to measure either stereo or 5.1 loudness with the
// EbuR128Analyzer class.
//
// Note Carefully: you must make sure that your audio channel ordering matches
// the ordering of these channel weights.
//
std::vector<float> DefaultChannelWeights();

// EbuR128Analyzer measures loudness statistics according to ITU 1770-4, EBU
// TECH 3341, and EBU TECH 3342. Please refer to those documents for more
// information about loudness measurement standards:
//   https://www.itu.int/rec/R-REC-BS.1770
//   https://tech.ebu.ch/publications/tech3341
//   https://tech.ebu.ch/publications/tech3342
//
// Some terminology:
//
// Momentary Loudness: EBU TECH 3341 defines that loudness measured on a
//    single 400 ms block is a measurement of "momentary loudness", and that
//    it should update at a minimum rate of 10 Hz.
//
// Gating Block: ITU 1770-4 does not explicitly use the term "momentary
//    loudness", but it defines the concept of a "gating block" that is
//    effectively the same; a gating block is a 400 millisecond block. The
//    word "gating" comes from the usage of absolute/relative gating methods
//    which are used to decide whether to use or discard a given momentary
//    loudness.
//
// Absolute Gating: ITU 1770 defines an absolute threshold of -70 LKFS, and
//    any momentary loudness measurement that is less than -70 LKFS will be
//    discarded when computing absolute-gated loudness.
//
// Relative Gating: ITU 1770 also defines a relative threshold of -10 dB,
//    which can only be derived after computing absolute-gated loudness. Any
//    momentary loudness measurement that is less than either the relative or
//    absolute threshold will be discarded when computing relative-gated
//    loudness
//
// Short-term Loudness: refers to ungated loudness measurement performed over
//    3-second blocks. ITU 1770-4 does not specify anything about short-term
//    loudness, this is defined by EBU TECH 3341.
//
// Integrated Loudness: refers to long-running average loudness as specified
//    by ITU 1770-4. In particular, ITU 1770-4 defines integrated loudness as
//    "relative gated integrated loudness".
//
// Loudness Range: EBU TECH 3342 defines loudness range as the interval
//    between 10% - 95% percentile relative-gated short-term loudness
//    measurements. Note carefully: EBU 3341 definition for short-term
//    loudness is defined as ungated, but for EBU 3342 LRA measurements, it is
//    relative-gated. Also note that the relative gate used for LRA
//    measurement uses a different relative threshold (-20 dB) than ITU 1770
//    defines for integrated loudness (-10 dB).
//
// True Peak: In the analog waveform, amplitude peaks might exist between
//    digital samples. ITU 1770 defines how to measure True Peak of a signal
//    by using at least 4x oversampling, with specifically defined upsampling
//    filters.
//
// PLR (Peak to Loudness Ratio): The ratio between a signal's peak amplitude
//     and integrated loudness.
//
// PSR (Peak to Short-term Loudness Ratio): This is a non-standard but useful
//    indicator of how "peaky" an audio signal is compared to its perceived
//    loudness. It is similar to PLR (Peak to Loudness Ratio), but PLR may be
//    less meaningful because it only considers one peak for the entire audio
//    signal. Such a global peak may not be representative of the "peakiness"
//    at any other moment in the audio signal. Instead, PSR only considers the
//    local peak within one short-term block, compared to that block's
//    short-term loudness.
//
class EbuR128Analyzer {
 public:
  struct LRAStats {
    float loudness_range_lu = 0.0f;
    float short_term_10th_percentile_lkfs = kMinDBFS;
    float short_term_95th_percentile_lkfs = kMinDBFS;
    float short_term_max_lkfs = kMinDBFS;
  };

  // Rms stats evaluated in 100 ms blocks, in steps of the same length (100 ms).
  struct Rms100msStats {
    float rms_10th_percentile_dbfs = kMinDBFS;
    float rms_95th_percentile_dbfs = kMinDBFS;
    float rms_max_dbfs = kMinDBFS;
  };

  enum SampleFormat {
    S16 = 0,  // signed 16-bit integer format
    S32 = 1,  // signed 32-bit integer format
    FLOAT = 2,
    DOUBLE = 3,
  };

  enum SampleLayout {
    // Interleaved data layout is a contiguous 1-D array where samples from each
    // channel at one point in time are arranged together.
    // For example in stereo:
    // audio_data = {L1, R1, L2, R2, L3, R3, ..., Ln, Rn}
    INTERLEAVED = 0,

    // Planar data layout is a contiguous 1-D array where all samples of one
    // channel are arranged together before the next channel's data is placed.
    // For example in stereo:
    // audio_data = {L1, L2, L3, ... Ln, R1, R2, R3, ..., Rn}
    PLANAR_CONTIGUOUS = 1,

    // Planar non-contiguous layout is an array of pointers, one pointer for
    // each audio channel. Each pointer refers to a 1-D array that has all the
    // samples for only that channel.
    // For example in stereo:
    // audio data: {<mem_address1>, <mem_address2>}
    // mem_address1:  {L1, L2, L3, ..., Ln}
    // mem_address2:  {R1, R2, R3, ..., Rn}
    PLANAR_NON_CONTIGUOUS = 2,
  };

  // Helper conversions to convert between loudness measured in LKFS and power
  // measurements.
  static float GetLoudnessForPower(float power);
  static float GetPowerForLoudness(float loudness_lkfs);

  // Main constructor to initialize the loudness measurement process.
  //
  // The user must provide correct channel weights with the same ordering that
  // actual data will be provided. Default channel weights for the common case
  // are available via helper function in this library.
  //
  // num_input_channels should match the actual number of channels provided as
  // data when calling Process(), so that the library knows how to walk
  // through the data properly. However, this does NOT necessarily mean that
  // all channels will be used for loudness measurement.
  //
  // The number of channels that will actually be used for measurement is the
  // minimum of (a) num_input_channels, (b) the length of input_channel_weights
  // array, and (c) the internal max number of supported channels.
  EbuR128Analyzer(int32_t num_input_channels,
                  std::vector<float> input_channel_weights, int32_t sample_rate,
                  bool enable_true_peak_measurement = false);

  virtual ~EbuR128Analyzer() = default;

  // Disallow copy or assign
  EbuR128Analyzer(const EbuR128Analyzer&) = delete;
  EbuR128Analyzer& operator=(const EbuR128Analyzer&) = delete;

  // Processes the requested number of audio samples from input buffer
  // audio_data.
  // - audio_data points to the actual audio data to be processed. It will be
  //   reinterpreted based on the specified sample format and sample layout.
  // - num_samples_per_channel is the length of the signal for a single channel.
  void Process(const void* audio_data, int64_t num_samples_per_channel,
               SampleFormat sample_fmt, SampleLayout sample_layout);

  // Version of Process() that receives audio_data bytes as a string. Main use
  // case is to be able to create a Python Clif wrapper.
  void ProcessByteArray(const std::string& audio_bytes,
                        const int64_t num_samples_per_channel,
                        const SampleFormat sample_fmt,
                        const SampleLayout sample_layout) {
    Process(audio_bytes.data(), num_samples_per_channel, sample_fmt,
            sample_layout);
  }

  // Return the relative-gated integrated loudness of the audio signal that has
  // been processed so far. Return value will *not* provide a loudness
  // measurement for very short audio clips, because integrated loudness
  // requires at least one momentary block of loudness to have been processed.
  std::optional<float> GetRelativeGatedIntegratedLoudness() const;

  // Return the loudness range measured by LRA, which is the measurement defined
  // by EBU TECH 3342. Return value will *not* provide a LRA measurement for
  // short audio clips, because LRA requires at least one short term block of
  // loudness to have been processed. Additionally, EBU TECH 3341 states that
  // the LRA measurement should be annotated as "not stable" for the first 60
  // seconds of audio.
  std::optional<LRAStats> GetLoudnessRangeStats(bool* is_stable) const;

  // Not a loudness measurement. This is rms evaluated in 100 ms blocks, in
  // steps of the same length (100 ms).
  std::optional<Rms100msStats> GetRms100msStats() const;

  // Returns the magnitude (absolute value) of the peak amplitude from the audio
  // processed so far.
  float digital_peak() const { return abs_digital_peak_; }

  // Returns the peak level of audio processed so far, in decibels with respect
  // to full scale.
  float digital_peak_dbfs() const;

  // If true peak measurement enabled, returns the magnitude (absolute value)
  // of the true peak amplitude from the upsampled audio processed so far.
  // Otherwise returns 0.
  float true_peak() const { return abs_true_peak_; }

  // If true peak measurement enabled, Returns the true peak level of
  // upsampled audio processed so far, in decibels with respect to full
  // scale.
  float true_peak_dbfs() const;

  // Returns a reference to the full list of ungated momentary power
  // measurements.
  const std::vector<float>& ungated_momentary_powers() const {
    return ungated_momentary_powers_;
  }

  // Returns a reference to the full list of ungated momentary loudness
  // measurements.
  const std::vector<float>& ungated_momentary_lkfs() const {
    return ungated_momentary_lkfs_;
  }

  // Returns a reference to the full list of ungated short-term loudness
  // measurements.
  const std::vector<float>& ungated_short_term_lkfs() const {
    return ungated_short_term_lkfs_;
  }

  // Returns a reference to the full list of short-term peaks.
  const std::vector<float>& short_term_peaks() const {
    return short_term_peaks_;
  }

  // Returns a reference to the full list of short-term peak to short-term
  // loudness ratios.
  const std::vector<float>& short_term_psr() const { return short_term_psr_; }

  // Returns the number of samples processed so far per channel.
  int64_t NumSamplesProcessed() const {
    return num_samples_processed_past_steps_ + num_samples_processed_this_step_;
  }

 protected:
  // ChannelAnalysis tracks the state of analysis per channel. In particular,
  // there are three stages of calculation:
  //   (1) Updated per-audio-sample: accumulators or aggregators are updated
  //       to track stats for each step (i.e. each step is a partial block)
  //   (2) Updated per-step: circular buffers store a history of the partial
  //       blocks so that block-level stats can be computed.
  //   (3) Updated per-step: block-level stats are computed.
  //
  // This partial block strategy allows us to minimize the amount of computation
  // performed per audio sample, while still being able to implement a sliding
  // window that updates per-block stats with fully accurate results.
  struct ChannelAnalysis {
    ChannelAnalysis() {
      true_peak_input_audio.fill(0.0);
      momentary_partial_sums.fill(0.0);
      short_term_partial_sums.fill(0.0);
      short_term_partial_peaks.fill(0.0);
    }

    // Accumulators/stats tracked for each partial block. Updated per sample.
    // SST is Sum of Squares Total, used to compute the RMS-related stats.
    float rms_sst_accumulator = 0.0;
    float momentary_sst_accumulator = 0.0;
    float short_term_sst_accumulator = 0.0;
    float partial_peak = 0.0;

    // Circular buffer of input audio for this channel, specifically for true
    // peak calculation. It requires buffering (a small number of) audio
    // samples in order to calculate FIR filters.
    std::array<float, kTruePeakFilterLength> true_peak_input_audio;
    int32_t true_peak_index = 0;

    // Circular buffer for momentary partial sums. Updated per 100 ms step.
    std::array<float, kStepsPerMomentaryBlock> momentary_partial_sums;
    int32_t momentary_index = 0;

    // Circular buffer for short term partial sums. Updated per 100 ms step.
    std::array<float, kStepsPerShortTermBlock> short_term_partial_sums;
    int32_t short_term_index = 0;

    // Circular buffer for partial peaks. Updated per 100 ms step.
    // Note this uses short_term_index as well.
    std::array<float, kStepsPerShortTermBlock> short_term_partial_peaks;

    // Completed stats for the corresponding momentary /
    // short term / rms block. Updated per 100 ms step, but note that
    // momentary_sst is only valid after processing 4 steps (400 ms) amd
    // short-term is only valid after 30 steps (3 seconds).
    float rms_sst = 0.0;
    float momentary_sst = 0.0;
    float short_term_sst = 0.0;
    float short_term_block_peak = 0.0;
  };

  // Computes the max absolute value of four audio samples computed by
  // applying the ITU 1770 4X upsampling filters.
  inline float MaxTruePeakFIR(
      const std::array<float, kTruePeakFilterLength>& input_audio,
      int32_t input_audio_circular_index) const;

  // Templatized version of Process, allows to avoid unnecessary data type
  // management overhead in the performance-critical per-sample loops.
  template <typename T, EbuR128Analyzer::SampleLayout LAYOUT>
  void ProcessImpl(const void* audio_data, int64_t num_samples_per_channel);

  // Updates peaks, k-weighting filter, and sum-square accumulators. This is a
  // critical path for good performance of the code, so it attempts to do
  // minimal processing per-sample, and leave as much computation as possible to
  // the per-step update instead.
  /* __attribute__((always_inline)) */ inline void UpdatePerSample(
      float unfiltered_sample, int channel_index);

  // Updates block-level stats for RMS, momentary, and short-term blocks.
  /* __attribute__((always_inline)) */ inline void UpdatePerStep();

  inline void UpdateAccumulatorsPerSample(float unfiltered_sample,
                                          float k_weighted_sample,
                                          ChannelAnalysis& sst);

  // Incrementally updates tracking stats. These update functions are called
  // once for every "step" i.e. a rate of 10 Hz.  The momentary block size is
  // 400 ms (i.e. 75% overlap between steps at 10 Hz), the short-term Block
  // is 3 seconds (i.e. 96% overlap between steps at 10 Hz), the rms block size
  // is 100 ms (i.e. 0% overlap between steps at 10 Hz).
  //
  // **NOTE CAREFULLY** - The first call to update a block happens only after a
  // full block size of data is received, which is different for momentary and
  // short-term and RMS block sizes.
  inline void UpdateAnalysisPerStep();
  inline void UpdateStatsForCurrentMomentaryBlock();
  inline void UpdateStatsForCurrentShortTermBlock();
  inline void UpdateStatsForCurrentRmsBlock();

  // Number of channels of the input audio, which defines the stride required to
  // walk through interleaved data.
  const int32_t interleaved_stride_;

  // Number of channels that are actually used for analysis, limited to
  // kMaxNumChannelsMeasured. At this time, channels beyond this will be
  // ignored.
  const int32_t num_channels_being_measured_;

  // The length (in samples) of a momentary block (400 ms).
  const int64_t momentary_block_size_samples_;
  const float one_over_momentary_block_size_samples_;

  // The length (in samples) of a short-term block (3 seconds).
  const int64_t short_term_block_size_samples_;
  const float one_over_short_term_block_size_samples_;

  // The length (in samples) of an rms block (100 ms).
  const int64_t rms_block_size_samples_;
  const float one_over_rms_block_size_samples_;

  // The length (in samples) of time needed for LRA to be stable (60 seconds).
  const int64_t lra_stability_duration_samples_;

  // Number of samples to be used for each 100 ms step.
  const int64_t num_samples_per_step_;

  // Whether or not to measure the true peak of the signal. This involves
  // upsampling 4x and applying four 12-tap FIR filters.
  const bool enable_true_peak_measurement_;

  // Channel weights
  std::array<float, kMaxNumChannelsMeasured> channel_weights_;

  // Filter coefficients for the desired sample rate
  std::array<float, kNumBiquadCoeffs> stage1_filter_;
  std::array<float, kNumBiquadCoeffs> stage2_filter_;

  // A biquad filter has 2 standard forms. In our case, we are using the "second
  // form", in which:
  //   w[n] = x[n] - a1 w[n-1] - a2 w[n-2]
  //   y[n] = b0 w[n] + b1 w[n-1] + b2 w[n-2]
  // where
  //   x[n] is the input
  //   y[n] is the output of the filter
  //   a1, a2, b0, b1, b2 are the filter coefficients
  //   w[n], w[n-1], w[n-2] is intermediate state
  //
  // In order to compute the output y[n] for the current input x[n], we need to
  // keep the values of w[n-1] and w[n-2] from the previous update.  K-weighting
  // uses two biquad stages, so we need to keep four values:
  //
  //   filter_state_[0] = stage 1 w[n-1]
  //   filter_state_[1] = stage 1 w[n-2]
  //   filter_state_[2] = stage 2 w[n-1]
  //   filter_state_[3] = stage 2 w[n-2]
  //
  // And finally, this much filter state is needed for each channel.
  //
  std::array<std::array<float, 4>, kMaxNumChannelsMeasured>
      filter_memory_all_channels_;

  // Tracks the per-channel intermediate calculations used to compute stats.
  std::array<ChannelAnalysis, kMaxNumChannelsMeasured> channel_analysis_;

  // Accumulators for momentary powers that were gated by the absolute
  // threshold. Used to compute absolute-gated loudness without an extra loop.
  float sum_of_abs_gated_momentary_powers_ = 0.0f;
  int64_t num_abs_gated_momentary_powers_ = 0;

  // Counters of how many audio ticks (number of audio samples for an individual
  // channel) have been processed so far.
  int64_t num_samples_processed_past_steps_ = 0;
  int64_t num_samples_processed_this_step_ = 0;

  // The absolute value of the largest amplitude, measured across all amplitudes
  // of all channels that have been processed so far.  Note, this is NOT the
  // same as true-peak as defined by ITU 1770, which requires up-sampling to at
  // least 192 kHz.
  float abs_digital_peak_ = 0.0f;

  // The true absolute value of the largest amplitude, measured across all
  // amplitudes of all channels that have been processed so far. Only updated
  // if true peak measurement is enabled.
  float abs_true_peak_ = 0.0f;

  // Mean squared amplitudes (i.e. power) of each momentary block (400 ms), in
  // steps of 100 ms (i.e. 10 Hz).
  std::vector<float> ungated_momentary_powers_;

  // Loudness measurement in LKFS of each momentary block (400 ms), in steps of
  // 100 ms (i.e. 10 Hz).  Same information as ungated_momentary_powers_, just
  // converted to LKFS.
  std::vector<float> ungated_momentary_lkfs_;

  // Loudness measurement in LKFS of each short-term block (3 seconds), in steps
  // of 100 ms (i.e. 10 Hz).
  std::vector<float> ungated_short_term_lkfs_;

  // Digital peak (absolute value of largest amplitude) within each short-term
  // block (3 seconds), in steps of 100 ms (i.e. 10 Hz).
  std::vector<float> short_term_peaks_;

  // Peak to short-term loudness ratio (PSR) for each short-term block
  // (3 seconds), in steps of 100 ms (i.e. 10 Hz).
  std::vector<float> short_term_psr_;

  // Rms measurement in dBFS of each rms block (100 ms), in steps of 100 ms
  // (i.e. 10 Hz).
  std::vector<float> rms_dbfs_;
};

}  // namespace loudness
#endif  // LOUDNESS_EBUR128_INCLUDE_EBUR128_ANALYZER_H_
