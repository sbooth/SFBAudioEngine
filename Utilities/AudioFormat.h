/*
 * Copyright (c) 2014 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#pragma once

#include <CoreAudio/CoreAudioTypes.h>

#include "CFWrapper.h"

/*! @file AudioFormat.h @brief A Core %Audio \c AudioStreamBasicDescription wrapper */

/*! @brief \c SFBAudioEngine's encompassing namespace */
namespace SFB {

	/*! @brief %Audio functionality */
	namespace Audio {

		/*! @brief Additional audio format IDs */
		CF_ENUM(AudioFormatID) {
			kAudioFormatDirectStreamDigital 	= 'DSD ',	/*!< Direct Stream Digital (DSD) */
			kAudioFormatDoP 					= 'DoP ',	/*!< DSD over PCM (DoP) */
			kAudioFormatModule 					= 'MOD ',	/*!< Module */
			kAudioFormatMonkeysAudio 			= 'APE ',	/*!< Monkey's Audio (APE) */
			kAudioFormatMusepack 				= 'MPC ',	/*!< Musepack */
			kAudioFormatShorten 				= 'SHN ',	/*!< Shorten */
			kAudioFormatSpeex 					= 'SPX ',	/*!< Ogg Speex */
			kAudioFormatTrueAudio 				= 'TTA ',	/*!< True Audio */
			kAudioFormatVorbis 					= 'VORB',	/*!< Ogg Vorbis */
			kAudioFormatWavPack 				= 'WV  '	/*!< WavPack */
		};

		/*! @brief Common PCM audio formats */
		typedef CF_ENUM(uint32_t, CommonPCMFormat) {
			kCommonPCMFormatFloat32 			= 1, 		/*!< Native-endian \c float */
			kCommonPCMFormatFloat64 			= 2, 		/*!< Native-endian \c double */
			kCommonPCMFormatInt16 				= 3, 		/*!< Native-endian signed 16-bit integers */
			kCommonPCMFormatInt32 				= 4, 		/*!< Native-endian signed 32-bit integers */
		};

		/*! @brief A class extending the functionality of a Core %Audio \c AudioStreamBasicDescription for DSD */
		class Format : public AudioStreamBasicDescription
		{
		public:
			// ========================================
			/*! @name Creation and Destruction */
			//@{

			/*! @brief Create a new, empty \c Format */
			inline Format() noexcept										{ memset(this, 0, sizeof(AudioStreamBasicDescription)); }

			/*! @brief Create a new \c Format for the specified \c AudioStreamBasicDescription */
			inline Format(const AudioStreamBasicDescription& asbd) noexcept	{ memcpy(this, &asbd, sizeof(AudioStreamBasicDescription)); }

			/*! @brief Create a new \c Format for the specified \c AudioStreamBasicDescription */
			inline Format(const AudioStreamBasicDescription *asbd) noexcept { assert(asbd != nullptr); memcpy(this, asbd, sizeof(AudioStreamBasicDescription)); }

			/*! @brief Create a new \c Format for the speciifed \c CommonPCMFormat */
			Format(CommonPCMFormat format, Float32 sampleRate, UInt32 channelsPerFrame, bool isInterleaved) noexcept;

			/*! @brief Copy constructor */
			inline Format(const Format& rhs) noexcept						{ *this = rhs; }

			/*! @brief Assignment operator */
			inline Format& operator=(const Format& rhs) noexcept			{ memcpy(this, &rhs, sizeof(AudioStreamBasicDescription)); return *this; }

			/*! @brief Compare two \c Format objects for equality*/
			inline bool operator==(const Format& rhs) const noexcept		{ return !memcmp(this, &rhs, sizeof(AudioStreamBasicDescription)); }

			/*! @brief Compare two \c Format objects for inequality*/
			inline bool operator!=(const Format& rhs) const noexcept		{ return !operator==(rhs); }

			//@}


			// ========================================
			/*! @name Format information */
			//@{

			/*! @brief Query whether this format represents interleaved data */
			inline bool IsInterleaved() const noexcept						{ return !(kAudioFormatFlagIsNonInterleaved & mFormatFlags); }

			/*! @brief Returns the number of interleaved channels */
			inline UInt32 InterleavedChannelCount() const noexcept			{ return IsInterleaved() ? mChannelsPerFrame : 1; }

			/*! @brief Query whether this format represents PCM audio data */
			inline bool IsPCM() const noexcept								{ return kAudioFormatLinearPCM == mFormatID; }

			/*! @brief Query whether this format represents DSD audio data */
			inline bool IsDSD() const noexcept								{ return kAudioFormatDirectStreamDigital == mFormatID; }

			/*! @brief Query whether this format represents DoP audio data */
			inline bool IsDoP() const noexcept								{ return kAudioFormatDoP == mFormatID; }

			/*! @brief Query whether this format represents big-endian ordered data */
			inline bool IsBigEndian() const noexcept						{ return kAudioFormatFlagIsBigEndian & mFormatFlags; }

			/*! @brief Query whether this format represents little-endian ordered data */
			inline bool IsLittleEndian() const noexcept						{ return !IsBigEndian(); }

			/*! @brief Query whether this format represents native-endian ordered data */
			inline bool IsNativeEndian() const noexcept						{ return kAudioFormatFlagsNativeEndian == (kAudioFormatFlagIsBigEndian & mFormatFlags); }

			/*! @brief Query whether this format represents floating-point data */
			inline bool IsFloat() const noexcept							{ return kAudioFormatFlagIsFloat & mFormatFlags; }

			/*! @brief Query whether this format represents signed integer data */
			inline bool IsSignedInteger() const noexcept					{ return kAudioFormatFlagIsSignedInteger & mFormatFlags; }

			/*! @brief Query whether this format represents packed data */
			inline bool IsPacked() const noexcept							{ return kAudioFormatFlagIsPacked & mFormatFlags; }

			/*! @brief Query whether this format is high-aligned */
			inline bool IsAlignedHigh() const noexcept						{ return kAudioFormatFlagIsAlignedHigh & mFormatFlags; }

			/*! @brief Convert a frame count to byte count */
			size_t FrameCountToByteCount(size_t frameCount) const noexcept;

			/*! @brief Convert a byte count to frame count */
			size_t ByteCountToFrameCount(size_t byteCount) const noexcept;

			//@}


			// ========================================
			/*! @name Format transformation */
			//@{

			/*! @brief Sets \c format to the equivalent non-interleaved format of \c this. Fails for non-PCM formats. */
			bool GetNonInterleavedEquivalent(Format& format) const noexcept;

			/*! @brief Sets \c format to the equivalent interleaved format of \c this. Fails for non-PCM formats. */
			bool GetInterleavedEquivalent(Format& format) const noexcept;

			/*! @brief Sets \c format to the equivalent standard format of \c this. Fails for non-PCM formats. */
			bool GetStandardEquivalent(Format& format) const noexcept;

			//@}


			/*! @brief Returns a string representation of this format suitable for logging */
			CFString Description() const noexcept;

		};

	}
}
