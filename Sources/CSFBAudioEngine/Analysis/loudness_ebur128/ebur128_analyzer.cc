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

#include "ebur128_analyzer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iterator>
#include <numeric>
#include <optional>
#include <vector>

#include "audio_data_access_patterns.h"
#include "ebur128_constants.h"
#include "k_weighting.h"

namespace loudness {

namespace {

// Helper function to clamp from below and sanitize NaNs.
//
// Note: Is it possible that sanitizing NaNs here is obscuring some errors in
// the code? We should investigate how NaN might occur and see if it's
// reasonable that we are correcting it here.
float ClampAndSanitizeDBFS(float x) {
  return (x < kMinDBFS || std::isnan(x)) ? kMinDBFS : x;
}

// Helper function to convert an amplitude to DB full-scale.
float SanitizedConvertToDBFS(float amplitude) {
  return ClampAndSanitizeDBFS(20.0f * log10f(std::fabs(amplitude)));
}

// Helper function to increment an index to a circular buffer
inline int32_t IncrementCircularIndex(int32_t index, int64_t mod) {
  int32_t next_index = index + 1;
  if (next_index == mod) {
    next_index = 0;
  }
  return next_index;
}

}  // namespace

std::vector<float> DefaultChannelWeights() {
  constexpr std::array<float, 6> kDefaultChannelWeights = {1.0f, 1.0f,  1.0f,
                                                           0.0f, 1.41f, 1.41f};
  return std::vector<float>(kDefaultChannelWeights.begin(),
                            kDefaultChannelWeights.end());
}

EbuR128Analyzer::EbuR128Analyzer(int32_t num_input_channels,
                                 std::vector<float> input_channel_weights,
                                 int32_t sample_rate,
                                 bool enable_true_peak_measurement)
    : interleaved_stride_(num_input_channels),
      num_channels_being_measured_(std::min<int32_t>(
          {kMaxNumChannelsMeasured, num_input_channels,
           static_cast<int32_t>(input_channel_weights.size())})),
      momentary_block_size_samples_(sample_rate * kMomentaryBlockSizeSeconds),
      one_over_momentary_block_size_samples_(
          1.0f / static_cast<float>(momentary_block_size_samples_)),
      short_term_block_size_samples_(sample_rate * kShortTermBlockSizeSeconds),
      one_over_short_term_block_size_samples_(
          1.0f / static_cast<float>(short_term_block_size_samples_)),
      rms_block_size_samples_(sample_rate * kRmsBlockSizeSeconds),
      one_over_rms_block_size_samples_(
          1.0f / static_cast<float>(rms_block_size_samples_)),
      lra_stability_duration_samples_(sample_rate * k3341StableLRASeconds),
      num_samples_per_step_(kStepLengthSeconds * sample_rate),
      enable_true_peak_measurement_(enable_true_peak_measurement) {
  // Set up channel weights. Note that channel_weights_ array may have many
  // unused entries which should be zeroed out.
  channel_weights_.fill(0.0);
  for (int i = 0; i < num_channels_being_measured_; ++i) {
    channel_weights_[i] = input_channel_weights[i];
  }

  // Precompute k-weighting filter coefficients and initialize filter state.
  InitKWeightingFilter(sample_rate, stage1_filter_, stage2_filter_);

  // Initialize filter state
  for (int i = 0; i < kMaxNumChannelsMeasured; ++i) {
    filter_memory_all_channels_[i].fill(0.0);
  }

  // Set up momentary, short-term, and rms accumulators to track stats.
  channel_analysis_.fill(ChannelAnalysis());
}

float EbuR128Analyzer::MaxTruePeakFIR(
    const std::array<float, kTruePeakFilterLength>& input_audio,
    int32_t input_audio_circular_index) const {
  // Simultaneously compute four filtered outputs, i.e. for 4x upsampling.
  float upsampled_phase0 = 0.0f;
  float upsampled_phase1 = 0.0f;
  float upsampled_phase2 = 0.0f;
  float upsampled_phase3 = 0.0f;

  int i = kTruePeakFilterLength - 1;
  // Loop from circular index to end of the circular buffer
  for (int j = input_audio_circular_index; j < kTruePeakFilterLength; ++j) {
    upsampled_phase0 += input_audio[j] * kTruePeakFilterPhase0[i];
    upsampled_phase1 += input_audio[j] * kTruePeakFilterPhase1[i];
    upsampled_phase2 += input_audio[j] * kTruePeakFilterPhase2[i];
    upsampled_phase3 += input_audio[j] * kTruePeakFilterPhase3[i];
    --i;
  }
  // Wraparound, loop from beginning of circular buffer to the circular index.
  for (int j = 0; j < input_audio_circular_index; ++j) {
    upsampled_phase0 += input_audio[j] * kTruePeakFilterPhase0[i];
    upsampled_phase1 += input_audio[j] * kTruePeakFilterPhase1[i];
    upsampled_phase2 += input_audio[j] * kTruePeakFilterPhase2[i];
    upsampled_phase3 += input_audio[j] * kTruePeakFilterPhase3[i];
    --i;
  }

  return std::max({std::fabs(upsampled_phase0), std::fabs(upsampled_phase1),
                   std::fabs(upsampled_phase2), std::fabs(upsampled_phase3)});
}

void EbuR128Analyzer::UpdateAnalysisPerStep() {
  for (int channel_index = 0; channel_index < num_channels_being_measured_;
       ++channel_index) {
    // Note: mutable reference.
    ChannelAnalysis& analysis = channel_analysis_[channel_index];

    // Update rms sst.
    analysis.rms_sst = analysis.rms_sst_accumulator;

    // Update momentary sst and partial sums.
    {
      const float old_partial_sum =
          analysis.momentary_partial_sums[analysis.momentary_index];
      const float new_partial_sum = analysis.momentary_sst_accumulator;
      analysis.momentary_sst -= old_partial_sum;
      analysis.momentary_sst += new_partial_sum;
      analysis.momentary_partial_sums[analysis.momentary_index] =
          new_partial_sum;
    }
    analysis.momentary_index = IncrementCircularIndex(analysis.momentary_index,
                                                      kStepsPerMomentaryBlock);

    // Update short term sst and partial sums.
    {
      const float old_partial_sum =
          analysis.short_term_partial_sums[analysis.short_term_index];
      const float new_partial_sum = analysis.short_term_sst_accumulator;
      analysis.short_term_sst -= old_partial_sum;
      analysis.short_term_sst += new_partial_sum;
      analysis.short_term_partial_sums[analysis.short_term_index] =
          new_partial_sum;
    }

    // Update short term peaks
    {
      analysis.short_term_partial_peaks[analysis.short_term_index] =
          analysis.partial_peak;

      // Brute force the short term peak. Since this happens only per step (as
      // opposed to per sample), it should be negligible overhead. If needed
      // in the future, perhaps this can be optimized.
      analysis.short_term_block_peak = 0.0;
      for (int i = 0; i < kStepsPerShortTermBlock; ++i) {
        analysis.short_term_block_peak =
            std::fmax(analysis.short_term_block_peak,
                      analysis.short_term_partial_peaks[i]);
      }
    }

    analysis.short_term_index = IncrementCircularIndex(
        analysis.short_term_index, kStepsPerShortTermBlock);

    // Reset the accumulators to compute the next partial sum.
    analysis.rms_sst_accumulator = 0.0;
    analysis.momentary_sst_accumulator = 0.0;
    analysis.short_term_sst_accumulator = 0.0;
    analysis.partial_peak = 0.0;
  }
}

float EbuR128Analyzer::GetLoudnessForPower(float power) {
  // ITU-R 1770 calls for a bias of -0.691dB to make a -3.0dB output from a
  // 1kHz full-scale sine wave input on one non-surround channel.
  return -0.691f + 10.0f * log10f(power);
}

float EbuR128Analyzer::GetPowerForLoudness(float loudness_lkfs) {
  return powf(10.0f, 0.1f * (loudness_lkfs + 0.691f));
}

inline void EbuR128Analyzer::UpdateStatsForCurrentMomentaryBlock() {
  float channel_weighted_momentary_sum = 0.0f;
  for (int j = 0; j < num_channels_being_measured_; ++j) {
    channel_weighted_momentary_sum +=
        channel_weights_[j] * channel_analysis_[j].momentary_sst;
  }
  const float momentary_power =
      channel_weighted_momentary_sum * one_over_momentary_block_size_samples_;

  // Store all momentary measurements, ungated.
  ungated_momentary_powers_.push_back(momentary_power);
  ungated_momentary_lkfs_.push_back(GetLoudnessForPower(momentary_power));

  // Accumulate this momentary power to compute absolute gated measurement.
  if (momentary_power > kPowerAbsoluteThreshold) {
    sum_of_abs_gated_momentary_powers_ += momentary_power;
    ++num_abs_gated_momentary_powers_;
  }
}

inline void EbuR128Analyzer::UpdateStatsForCurrentShortTermBlock() {
  float channel_weighted_short_term_sum = 0.0f;
  float short_term_block_peak_across_channels = 0.0f;
  for (int j = 0; j < num_channels_being_measured_; ++j) {
    // Aggregate across channels for short-term LKFS
    channel_weighted_short_term_sum +=
        channel_weights_[j] * channel_analysis_[j].short_term_sst;

    // Aggregate across channels for short-term peaks
    short_term_block_peak_across_channels =
        std::fmax(short_term_block_peak_across_channels,
                  channel_analysis_[j].short_term_block_peak);
  }
  const float short_term_power =
      channel_weighted_short_term_sum * one_over_short_term_block_size_samples_;
  const float short_term_lkfs = GetLoudnessForPower(short_term_power);
  const float short_term_peak_dbfs =
      SanitizedConvertToDBFS(short_term_block_peak_across_channels);
  const float short_term_psr = short_term_peak_dbfs - short_term_lkfs;

  // Store all short-term measurements, ungated.
  ungated_short_term_lkfs_.push_back(short_term_lkfs);
  short_term_peaks_.push_back(short_term_block_peak_across_channels);
  short_term_psr_.push_back(short_term_psr);
}

inline void EbuR128Analyzer::UpdateStatsForCurrentRmsBlock() {
  float channel_weighted_rms_sum = 0.0f;
  for (int j = 0; j < num_channels_being_measured_; ++j) {
    channel_weighted_rms_sum +=
        channel_weights_[j] * channel_analysis_[j].rms_sst;
  }
  const float rms_power =
      (channel_weighted_rms_sum * one_over_rms_block_size_samples_) /
      num_channels_being_measured_;
  const float rms_linear = std::sqrt(rms_power);

  rms_dbfs_.push_back(SanitizedConvertToDBFS(rms_linear));
}

void EbuR128Analyzer::UpdatePerSample(const float unfiltered_sample,
                                      const int channel_index) {
  // Update digital-peak
  abs_digital_peak_ =
      std::fmax(abs_digital_peak_, std::fabs(unfiltered_sample));

  float* filter_state = filter_memory_all_channels_[channel_index].data();
  const float s1_wn_minus_1 = filter_state[0];
  const float s1_wn_minus_2 = filter_state[1];
  const float s2_wn_minus_1 = filter_state[2];
  const float s2_wn_minus_2 = filter_state[3];

  // K-weighting Stage 1, "head effect compensation"
  const float s1_wn_minus_0 = /*  1.0  *  */ unfiltered_sample -
                              stage1_filter_[0] * s1_wn_minus_1 -
                              stage1_filter_[1] * s1_wn_minus_2;
  const float s1_yn_minus_0 = stage1_filter_[2] * s1_wn_minus_0 +
                              stage1_filter_[3] * s1_wn_minus_1 +
                              stage1_filter_[4] * s1_wn_minus_2;

  // K-weighting Stage 2, RLB weighting
  const float s2_wn_minus_0 = /*  1.0  *  */ s1_yn_minus_0 -
                              stage2_filter_[0] * s2_wn_minus_1 -
                              stage2_filter_[1] * s2_wn_minus_2;
  // Optimization: the last 3 coefficients of stage2 biquad are
  // always (1, -2, 1) when the filter is present.
  const float s2_yn_minus_0 = /*  1.0  *  */ s2_wn_minus_0 +
                              -2 * s2_wn_minus_1 +
                              /*  1.0  *  */ s2_wn_minus_2;

  // Update filter state to work with next sample
  filter_state[0] = s1_wn_minus_0;
  filter_state[1] = s1_wn_minus_1;
  filter_state[2] = s2_wn_minus_0;
  filter_state[3] = s2_wn_minus_1;

  const float k_weighted_sample = s2_yn_minus_0;

  // Update per-channel filter state and accumulators
  const float unfiltered_squared = unfiltered_sample * unfiltered_sample;
  const float k_weighted_squared = k_weighted_sample * k_weighted_sample;
  ChannelAnalysis& analysis =
      channel_analysis_[channel_index];  // mutable alias
  analysis.rms_sst_accumulator += unfiltered_squared;
  analysis.momentary_sst_accumulator += k_weighted_squared;
  analysis.short_term_sst_accumulator += k_weighted_squared;
  analysis.partial_peak =
      std::fmax(analysis.partial_peak, std::fabs(unfiltered_sample));

  // Update true peak if measurement is enabled.
  if (enable_true_peak_measurement_) {
    analysis.true_peak_input_audio[analysis.true_peak_index] =
        unfiltered_sample;
    analysis.true_peak_index =
        IncrementCircularIndex(analysis.true_peak_index, kTruePeakFilterLength);
    abs_true_peak_ = std::max({abs_true_peak_, abs_digital_peak_,
                               MaxTruePeakFIR(analysis.true_peak_input_audio,
                                              analysis.true_peak_index)});
  }
}

void EbuR128Analyzer::UpdatePerStep() {
  num_samples_processed_past_steps_ += num_samples_per_step_;
  UpdateAnalysisPerStep();
  if (num_samples_processed_past_steps_ >= momentary_block_size_samples_) {
    UpdateStatsForCurrentMomentaryBlock();
  }
  if (num_samples_processed_past_steps_ >= short_term_block_size_samples_) {
    UpdateStatsForCurrentShortTermBlock();
  }
  if (num_samples_processed_past_steps_ >= rms_block_size_samples_) {
    UpdateStatsForCurrentRmsBlock();
  }
}

template <typename T, EbuR128Analyzer::SampleLayout LAYOUT>
void EbuR128Analyzer::ProcessImpl(const void* audio_data,
                                  const int64_t num_samples_per_channel) {
  for (int i = 0; i < num_samples_per_channel; ++i) {
    for (int channel_index = 0; channel_index < num_channels_being_measured_;
         ++channel_index) {
      float unfiltered_sample = GetSampleFromOrigin<T, LAYOUT>(
          audio_data, i, channel_index, interleaved_stride_,
          num_samples_per_channel);
      UpdatePerSample(unfiltered_sample, channel_index);
    }

    // Once we have reached a full block size, and thereafter every step size,
    // we should run the once-per-block update.
    ++num_samples_processed_this_step_;
    if (num_samples_processed_this_step_ == num_samples_per_step_) {
      num_samples_processed_this_step_ = 0;
      UpdatePerStep();
    }
  }
}

void EbuR128Analyzer::Process(const void* audio_data,
                              const int64_t num_samples_per_channel,
                              const SampleFormat sample_fmt,
                              const SampleLayout sample_layout) {
  if (sample_fmt == FLOAT) {
    if (sample_layout == PLANAR_NON_CONTIGUOUS) {
      ProcessImpl<float, PLANAR_NON_CONTIGUOUS>(audio_data,
                                                num_samples_per_channel);
      return;
    } else if (sample_layout == INTERLEAVED) {
      ProcessImpl<float, INTERLEAVED>(audio_data, num_samples_per_channel);
      return;
    } else if (sample_layout == PLANAR_CONTIGUOUS) {
      ProcessImpl<float, PLANAR_CONTIGUOUS>(audio_data,
                                            num_samples_per_channel);
      return;
    }
  }
  if (sample_fmt == S16) {
    if (sample_layout == PLANAR_NON_CONTIGUOUS) {
      ProcessImpl<int16_t, PLANAR_NON_CONTIGUOUS>(audio_data,
                                                  num_samples_per_channel);
      return;
    } else if (sample_layout == INTERLEAVED) {
      ProcessImpl<int16_t, INTERLEAVED>(audio_data, num_samples_per_channel);
      return;
    } else if (sample_layout == PLANAR_CONTIGUOUS) {
      ProcessImpl<int16_t, PLANAR_CONTIGUOUS>(audio_data,
                                              num_samples_per_channel);
      return;
    }
  }
  if (sample_fmt == S32) {
    if (sample_layout == PLANAR_NON_CONTIGUOUS) {
      ProcessImpl<int32_t, PLANAR_NON_CONTIGUOUS>(audio_data,
                                                  num_samples_per_channel);
      return;
    } else if (sample_layout == INTERLEAVED) {
      ProcessImpl<int32_t, INTERLEAVED>(audio_data, num_samples_per_channel);
      return;
    } else if (sample_layout == PLANAR_CONTIGUOUS) {
      ProcessImpl<int32_t, PLANAR_CONTIGUOUS>(audio_data,
                                              num_samples_per_channel);
      return;
    }
  }
  if (sample_fmt == DOUBLE) {
    if (sample_layout == PLANAR_NON_CONTIGUOUS) {
      ProcessImpl<double, PLANAR_NON_CONTIGUOUS>(audio_data,
                                                 num_samples_per_channel);
      return;
    } else if (sample_layout == INTERLEAVED) {
      ProcessImpl<double, INTERLEAVED>(audio_data, num_samples_per_channel);
      return;
    } else if (sample_layout == PLANAR_CONTIGUOUS) {
      ProcessImpl<double, PLANAR_CONTIGUOUS>(audio_data,
                                             num_samples_per_channel);
      return;
    }
  }
}

std::optional<float> EbuR128Analyzer::GetRelativeGatedIntegratedLoudness()
    const {
  // If audio is too short, we cannot meaningfully measure loudness.
  if (ungated_momentary_powers_.empty()) {
    return std::nullopt;
  }

  // If we get here, audio is long enough to produce a loudness measurement. But
  // if everything is quieter than the absolute gating threshold, integrated
  // loudness still technically cannot be measured. Instead, indicate that the
  // audio is virtually silent.
  if (num_abs_gated_momentary_powers_ == 0) {
    return kMinLKFS;
  }

  // Compute absolute-gated loudness
  const float abs_gated_avg_power =
      sum_of_abs_gated_momentary_powers_ / num_abs_gated_momentary_powers_;
  const float abs_gated_loudness = GetLoudnessForPower(abs_gated_avg_power);

  // Compute relative-gated loudness
  const float rel_threshold = abs_gated_loudness + k1770RelativeThresholdLU;
  const float rel_power_threshold = GetPowerForLoudness(rel_threshold);

  float sum_of_rel_gated_momentary_powers = 0.0f;
  int64_t num_rel_gated_momentary_powers = 0;
  for (float ungated_power : ungated_momentary_powers_) {
    // For quiet signals, relative threshold could potentially be less than
    // absolute threshold, and so the requirement is that power must be larger
    // than both thresholds for relative loudness.
    if (ungated_power > kPowerAbsoluteThreshold &&
        ungated_power > rel_power_threshold) {
      sum_of_rel_gated_momentary_powers += ungated_power;
      ++num_rel_gated_momentary_powers;
    }
  }

  if (num_rel_gated_momentary_powers == 0) {
    // Note: We should never get here. If all blocks are pruned by the relative
    // gate, it would be an internal error. If all values could have been below
    // the relative gate, the abs-gated average would have been quieter than the
    // relative gate, too, which by definition can't happen.
    return kMinLKFS;
  }

  const float rel_gated_avg_power =
      sum_of_rel_gated_momentary_powers / num_rel_gated_momentary_powers;
  const float rel_gated_loudness = GetLoudnessForPower(rel_gated_avg_power);
  return ClampAndSanitizeDBFS(rel_gated_loudness);
}

// Added by sfb 20260203
std::optional<float> EbuR128Analyzer::GetRelativeGatedIntegratedLoudness(std::vector<EbuR128Analyzer *> analyzers) {
  const auto all_empty = std::all_of(std::begin(analyzers), std::end(analyzers), [](const auto *analyzer) { return analyzer->ungated_momentary_powers_.empty(); });
  if (all_empty) {
    return std::nullopt;
  }

  const auto all_zero = std::all_of(std::begin(analyzers), std::end(analyzers), [](const auto *analyzer) { return analyzer->num_abs_gated_momentary_powers_ == 0; });
  if (all_zero) {
      return kMinLKFS;
  }

  const float sum_of_abs_gated_momentary_powers = std::accumulate(std::begin(analyzers), std::end(analyzers), 0.0f, [](float f, const auto *analyzer) { return f + analyzer->sum_of_abs_gated_momentary_powers_; });
  const int64_t num_abs_gated_momentary_powers = std::accumulate(std::begin(analyzers), std::end(analyzers), 0LL, [](int64_t i, const auto *analyzer) { return i + analyzer->num_abs_gated_momentary_powers_; });

  const float abs_gated_avg_power = sum_of_abs_gated_momentary_powers / num_abs_gated_momentary_powers;
  const float abs_gated_loudness = GetLoudnessForPower(abs_gated_avg_power);

  const float rel_threshold = abs_gated_loudness + k1770RelativeThresholdLU;
  const float rel_power_threshold = GetPowerForLoudness(rel_threshold);

  float sum_of_rel_gated_momentary_powers = 0.0f;
  int64_t num_rel_gated_momentary_powers = 0;
  for (const auto *analyzer : analyzers) {
    for (float ungated_power : analyzer->ungated_momentary_powers_) {
      if (ungated_power > kPowerAbsoluteThreshold && ungated_power > rel_power_threshold) {
        sum_of_rel_gated_momentary_powers += ungated_power;
        ++num_rel_gated_momentary_powers;
      }
    }
  }

  if (num_rel_gated_momentary_powers == 0) {
    return kMinLKFS;
  }

  const float rel_gated_avg_power = sum_of_rel_gated_momentary_powers / num_rel_gated_momentary_powers;
  const float rel_gated_loudness = GetLoudnessForPower(rel_gated_avg_power);
  return ClampAndSanitizeDBFS(rel_gated_loudness);
}

std::optional<EbuR128Analyzer::LRAStats> EbuR128Analyzer::GetLoudnessRangeStats(
    bool* is_stable) const {
  // Cannot compute any LRA stats if there are no momentary measurements.
  if (num_abs_gated_momentary_powers_ == 0) {
    *is_stable = false;
    return std::nullopt;
  }

  // Compute absolute-gated integrated loudness
  const float abs_gated_avg_power =
      sum_of_abs_gated_momentary_powers_ / num_abs_gated_momentary_powers_;
  const float abs_gated_loudness = GetLoudnessForPower(abs_gated_avg_power);

  // Note: for computing LRA, relative threshold is different than 1770.
  const float rel_threshold = abs_gated_loudness + k3342RelativeThresholdLU;

  EbuR128Analyzer::LRAStats lra_stats;
  lra_stats.short_term_max_lkfs = kMinLKFS;

  // Make a sorted list of relative-gated short-term loudness measurements, so
  // that we can compute percentile. NOTE: if we are OK with some bounded error,
  // then we should consider maintaining a histogram that will allow us to find
  // percentiles more asymptotically efficiently.
  std::vector<float> gated_short_term_values;
  gated_short_term_values.reserve(ungated_short_term_lkfs_.size());
  for (float short_term_loudness : ungated_short_term_lkfs_) {
    lra_stats.short_term_max_lkfs =
        std::fmax(lra_stats.short_term_max_lkfs, short_term_loudness);
    if (short_term_loudness > kAbsoluteThresholdLKFS &&
        short_term_loudness > rel_threshold) {
      gated_short_term_values.push_back(short_term_loudness);
    }
  }
  // Cannot compute any LRA stats if there are no gated short term measurements.
  if (gated_short_term_values.empty()) {
    *is_stable = false;
    return std::nullopt;
  }
  std::sort(gated_short_term_values.begin(), gated_short_term_values.end());

  // Determine the array index for 10th percentile and 95th percentile. The
  // rounding mechanism for computing the index is taken from the Matlab
  // implementation described in EBU 3342.
  const std::vector<float>::size_type length_minus_one = gated_short_term_values.size() - 1;
  const long index_10th = std::lround(length_minus_one * 0.1f);
  const long index_95th = std::lround(length_minus_one * 0.95f);

  lra_stats.short_term_10th_percentile_lkfs =
      ClampAndSanitizeDBFS(gated_short_term_values[index_10th]);
  lra_stats.short_term_95th_percentile_lkfs =
      ClampAndSanitizeDBFS(gated_short_term_values[index_95th]);
  lra_stats.loudness_range_lu = lra_stats.short_term_95th_percentile_lkfs -
                                lra_stats.short_term_10th_percentile_lkfs;

  // EBU TECH 3341 states that the loudness range measurement should be
  // considered "not stable" until at least 60 seconds of audio have been
  // processed.
  *is_stable =
      num_samples_processed_past_steps_ >= lra_stability_duration_samples_;
  return lra_stats;
}

std::optional<EbuR128Analyzer::Rms100msStats>
EbuR128Analyzer::GetRms100msStats() const {
  // Cannot compute RMS stats if there are no complete steps.
  if (rms_dbfs_.empty()) {
    return std::nullopt;
  }
  EbuR128Analyzer::Rms100msStats rms_stats;

  // Make a sorted list of rms output so we can compute percentile.
  std::vector<float> sorted_rms_values;
  std::copy(rms_dbfs_.begin(), rms_dbfs_.end(),
            std::back_inserter(sorted_rms_values));
  std::sort(sorted_rms_values.begin(), sorted_rms_values.end());

  // Determine the array index for 10th percentile and 95th percentile. The
  // rounding mechanism for computing the index is taken from the Matlab
  // implementation described in EBU 3342.
  const std::vector<float>::size_type length_minus_one = sorted_rms_values.size() - 1;
  const long index_10th = std::lround(length_minus_one * 0.1f);
  const long index_95th = std::lround(length_minus_one * 0.95f);

  rms_stats.rms_10th_percentile_dbfs =
      ClampAndSanitizeDBFS(sorted_rms_values[index_10th]);
  rms_stats.rms_95th_percentile_dbfs =
      ClampAndSanitizeDBFS(sorted_rms_values[index_95th]);
  rms_stats.rms_max_dbfs = ClampAndSanitizeDBFS(sorted_rms_values.back());

  return rms_stats;
}

float EbuR128Analyzer::digital_peak_dbfs() const {
  return SanitizedConvertToDBFS(abs_digital_peak_);
}

float EbuR128Analyzer::true_peak_dbfs() const {
  return SanitizedConvertToDBFS(abs_true_peak_);
}

}  // namespace loudness
