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
			kAudioFormatMOD 					= 'MOD ',	/*!< MOD */
			kAudioFormatMonkeysAudio 			= 'APE ',	/*!< Monkey's Audio (APE) */
			kAudioFormatMPEG1 					= 'MPG1',	/*!< MPEG-1 (Layer I, II, or III) */
			kAudioFormatMusepack 				= 'MPC ',	/*!< Musepack */
			kAudioFormatSpeex 					= 'SPX ',	/*!< Ogg Speex */
			kAudioFormatTrueAudio 				= 'TTA ',	/*!< True Audio */
			kAudioFormatVorbis 					= 'OGG ',	/*!< Ogg Vorbis */
			kAudioFormatWavpack 				= 'WV  ',	/*!< Wavpack */
			kAudioFormatShorten 				= 'SHN '	/*!< Shorten */
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
			inline Format() 												{ memset(this, 0, sizeof(AudioStreamBasicDescription)); }

			/*! @brief Create a new \c Format for the specified \c AudioStreamBasicDescription */
			inline Format(const AudioStreamBasicDescription& format) 		{ memcpy(this, &format, sizeof(AudioStreamBasicDescription)); }

			/*! @brief Create a new \c Format for the specified \c AudioStreamBasicDescription */
			inline Format(const AudioStreamBasicDescription *format) 		{ memcpy(this, format, sizeof(AudioStreamBasicDescription)); }

			/*! @brief Create a new \c Format for the speciifed \c CommonPCMFormat */
			Format(CommonPCMFormat format, Float32 sampleRate, UInt32 channelsPerFrame, bool isInterleaved);

			/*! @brief Copy constructor */
			inline Format(const Format& rhs) 								{ *this = rhs; }

			/*! @brief Assignment operator */
			inline Format& operator=(const Format& rhs) 					{ memcpy(this, &rhs, sizeof(AudioStreamBasicDescription)); return *this; }

			/*! @brief Compare two \c Format objects for equality*/
			inline bool operator==(const Format& rhs) const 				{ return !memcmp(this, &rhs, sizeof(AudioStreamBasicDescription)); }

			/*! @brief Compare two \c Format objects for inequality*/
			inline bool operator!=(const Format& rhs) const 				{ return !operator==(rhs); }

			//@}


			// ========================================
			/*! @name Format information */
			//@{

			/*! @brief Query whether this format represents interleaved data */
			inline bool IsInterleaved() const 								{ return !(kAudioFormatFlagIsNonInterleaved & mFormatFlags); }

			/*! @brief Returns the number of interleaved channels */
			inline UInt32 InterleavedChannelCount() const 					{ return IsInterleaved() ? mChannelsPerFrame : 1; }

			/*! @brief Query whether this format represents PCM audio data */
			inline bool IsPCM() const 										{ return kAudioFormatLinearPCM == mFormatID; }

			/*! @brief Query whether this format represents DSD audio data */
			inline bool IsDSD() const 										{ return kAudioFormatDirectStreamDigital == mFormatID; }

			/*! @brief Query whether this format represents DoP audio data */
			inline bool IsDoP() const 										{ return kAudioFormatDoP == mFormatID; }

			/*! @brief Query whether this format represents big-endian ordered data */
			inline bool IsBigEndian() const 								{ return kAudioFormatFlagIsBigEndian & mFormatFlags; }

			/*! @brief Query whether this format represents little-endian ordered data */
			inline bool IsLittleEndian() const 								{ return !IsBigEndian(); }

			/*! @brief Query whether this format represents native-endian ordered data */
			inline bool IsNativeEndian() const 								{ return kAudioFormatFlagsNativeEndian == (kAudioFormatFlagIsBigEndian & mFormatFlags); }

			/*! @brief Query whether this format represents floating-point data */
			inline bool IsFloat() const 									{ return kAudioFormatFlagIsFloat & mFormatFlags; }

			/*! @brief Query whether this format represents signed integer data */
			inline bool IsSignedInteger() const 							{ return kAudioFormatFlagIsSignedInteger & mFormatFlags; }

			/*! @brief Query whether this format represents packed data */
			inline bool IsPacked() const 									{ return kAudioFormatFlagIsPacked & mFormatFlags; }

			/*! @brief Query whether this format is high-aligned */
			inline bool IsAlignedHigh() const 								{ return kAudioFormatFlagIsAlignedHigh & mFormatFlags; }

			/*! @brief Convert a frame count to byte count */
			size_t FrameCountToByteCount(size_t frameCount) const;

			/*! @brief Convert a byte count to frame count */
			size_t ByteCountToFrameCount(size_t byteCount) const;

			//@}


			// ========================================
			/*! @name Format transformation */
			//@{

			/*! @brief Sets \c format to the equivalent non-interleaved format of \c this. Fails for non-PCM formats. */
			bool GetNonInterleavedEquivalent(Format& format) const;

			/*! @brief Sets \c format to the equivalent interleaved format of \c this. Fails for non-PCM formats. */
			bool GetInterleavedEquivalent(Format& format) const;

			/*! @brief Sets \c format to the equivalent standard format of \c this. Fails for non-PCM formats. */
			bool GetStandardEquivalent(Format& format) const;

			//@}


			/*! @brief Returns a string representation of this format suitable for logging */
			CFString Description() const;

		};

	}
}
