/*
 *  Copyright (C) 2011, 2012, 2013, 2014, 2015 Stephen F. Booth <me@sbooth.org>
 *  All Rights Reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are
 *  met:
 *
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <CoreFoundation/CoreFoundation.h>
#include <memory>

/*! @file ReplayGainAnalyzer.h @brief Support for replay gain calculation */

/*! @brief \c SFBAudioEngine's encompassing namespace */
namespace SFB {

	/*! @brief %Audio functionality */
	namespace Audio {

		/*!
		 * @brief A class that calculates replay gain
		 * @see http://wiki.hydrogenaudio.org/index.php?title=ReplayGain_specification
		 *
		 * To calculate an album's replay gain, create a \c ReplayGainAnalyzer and all
		 * \c ReplayGainAnalyzer::AnalyzeURL()
		 */
		class ReplayGainAnalyzer
		{
		public:

			/*! @brief The \c CFErrorRef error domain used by \c ReplayGainAnalyzer */
			static const CFStringRef ErrorDomain;

			/*! @brief Possible \c CFErrorRef error codes used by \c ReplayGainAnalyzer */
			enum ErrorCode {
				FileFormatNotSupportedError			= 0,	/*!< File format not supported */
			};
			

			/*! @brief Get the reference loudness in dB SPL, defined as 89.0 dB */
			static float GetReferenceLoudness();


			/*! @brief Get the maximum supported sample rate for replay gain calculation, currently 48.0 KHz */
			static int32_t GetMaximumSupportedSampleRate();

			/*! @brief Get the minimum supported sample rate for replay gain calculation, currently 8.0 KHz */
			static int32_t GetMinimumSupportedSampleRate();

			/*!
			 * @brief Query whether a sample rate is supported
			 * @note The current supported sample rates are 48.0, 44.1, 32.0, 24.0, 22.05, 16.0, 12.0, 11.025, and 8.0 KHz
			 */
			static bool SampleRateIsSupported(int32_t sampleRate);


			/*! @brief Query whether an even multiple of a sample rate is supported */
			static bool EvenMultipleSampleRateIsSupported(int32_t sampleRate);

			/*! @brief Returns the best sample rate to use for replay gain calculation for the given sample rate */
			static int32_t GetBestReplayGainSampleRateForSampleRate(int32_t sampleRate);


			// ========================================
			/*! @name Creation/Destruction */
			//@{

			/*! @brief Create a new \c ReplayGainAnalyzer */
			ReplayGainAnalyzer();

			/*! @brief Destroy this \c ReplayGainAnalyzer */
			~ReplayGainAnalyzer();

			/*! @cond */

			/*! @internal This class is non-copyable */
			ReplayGainAnalyzer(const ReplayGainAnalyzer& rhs) = delete;

			/*! @internal This class is non-assignable */
			ReplayGainAnalyzer& operator=(const ReplayGainAnalyzer& rhs) = delete;

			/*! @endcond */
			//@}


			// ========================================
			/*! @name Audio analysis */
			//@{

			/*!
			 * @brief Analyze the given URL's replay gain
			 *
			 * If the URL's sample rate is not natively supported, the replay gain adjustment will be calculated using audio
			 * resampled to the sample rate returned by \c ReplayGainAnalyzer::GetBestReplayGainSampleRateForSampleRate()
			 * @param url The URL
			 * @param error An optional pointer to a \c CFErrorRef to receive error information
			 * @return \c true on success, false otherwise
			 */
			bool AnalyzeURL(CFURLRef url, CFErrorRef *error = nullptr);

			//@}


			// ========================================
			/*!
			 * @name Replay gain values
			 * The \c Get() methods return \c true on success, \c false otherwise
			 */
			//@{

			/*! @brief Get the track gain in dB */
			bool GetTrackGain(float& trackGain);

			/*! @brief Get the track peak sample value normalized to [-1, 1) */
			bool GetTrackPeak(float& trackPeak);


			/*! @brief Get the album gain in dB */
			bool GetAlbumGain(float& albumGain);

			/*! @brief Get the album peak sample value normalized to [-1, 1) */
			bool GetAlbumPeak(float& albumPeak);

			//@}


		private:
			bool SetSampleRate(int32_t sampleRate);
			bool AnalyzeSamples(const float *left_samples, const float *right_samples, size_t num_samples, bool stereo);
			
			// The replay gain internal state
			class ReplayGainAnalyzerPrivate;
			std::unique_ptr<ReplayGainAnalyzerPrivate> priv;
		};
		
	}
}
