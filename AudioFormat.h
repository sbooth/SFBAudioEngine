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
			kAudioFormatFLAC 					= 'FLAC',	/*!< Free Lossless Audio Codec (FLAC) */
			kAudioFormatMOD 					= 'MOD ',	/*!< MOD */
			kAudioFormatMonkeysAudio 			= 'APE ',	/*!< Monkey's Audio (APE) */
			kAudioFormatMPEG1 					= 'MPG1',	/*!< MPEG-1 (Layer I, II, or III) */
			kAudioFormatMusepack 				= 'MPC ',	/*!< Musepack */
			kAudioFormatOpus 					= 'OPUS',	/*!< Ogg Opus */
			kAudioFormatSpeex 					= 'SPX ',	/*!< Ogg Speex */
			kAudioFormatTrueAudio 				= 'TTA ',	/*!< True Audio */
			kAudioFormatVorbis 					= 'OGG ',	/*!< Ogg Vorbis */
			kAudioFormatWavpack 				= 'WV  '	/*!< Wavpack */
		};

		/*! @brief Common PCM audio formats */
		typedef CF_ENUM(uint32_t, CommonPCMFormat) {
			kCommonPCMFormatFloat32 			= 1, 		/*!< Native-endian \c float */
			kCommonPCMFormatFloat64 			= 2, 		/*!< Native-endian \c double */
			kCommonPCMFormatInt16 				= 3, 		/*!< Native-endian signed 16-bit integers */
			kCommonPCMFormatInt32 				= 4, 		/*!< Native-endian signed 32-bit integers */
		};

		/*! @brief A class extending the functionality of a Core %Audio \c AudioStreamBasicDescription for DSD */
		class AudioFormat : public AudioStreamBasicDescription
		{
		public:
			// ========================================
			/*! @name Creation and Destruction */
			//@{

			/*! @brief Create a new, empty \c AudioFormat */
			AudioFormat();

			/*! @brief Create a new \c AudioFormat for the specified \c AudioStreamBasicDescription */
			AudioFormat(const AudioStreamBasicDescription& format);

			/*! @brief Create a new \c AudioFormat for the speciifed \c CommonPCMFormat */
			AudioFormat(CommonPCMFormat format, Float32 sampleRate, UInt32 channelsPerFrame, bool isInterleaved);

			/*! @brief Copy constructor */
			AudioFormat(const AudioFormat& rhs);

			/*! @brief Assignment operator */
			AudioFormat& operator=(const AudioFormat& rhs);

			/*! @brief Compare two \c AudioFormat objects for equality*/
			bool operator==(const AudioFormat& rhs) const;

			/*! @brief Compare two \c AudioFormat objects for inequality*/
			inline bool operator!=(const AudioFormat& rhs) const { return !operator==(rhs); }

			//@}


			// ========================================
			/*! @name Format information */
			//@{

			/*! @brief Query whether this format represents interleaved data */
			inline bool IsInterleaved() const 		{ return !(kAudioFormatFlagIsNonInterleaved & mFormatFlags); }

			/*! @brief Query whether this format represents PCM audio data */
			inline bool IsPCM() const 				{ return kAudioFormatLinearPCM == mFormatID; }

			/*! @brief Query whether this format represents DSD audio data */
			inline bool IsDSD() const 				{ return kAudioFormatDirectStreamDigital == mFormatID; }

			/*! @brief Query whether this format represents DoP audio data */
			inline bool IsDoP() const 				{ return kAudioFormatDoP == mFormatID; }

			/*! @brief Query whether this format represents big-endian ordered daa */
			inline bool IsBigEndian() const 		{ return kAudioFormatFlagIsBigEndian & mFormatFlags; }

			/*! @brief Query whether this format represents native-endian ordered daa */
			inline bool IsNativeEndian() const 		{ return kAudioFormatFlagsNativeEndian == (kAudioFormatFlagIsBigEndian & mFormatFlags); }

			/*! @brief Convert a frame count to byte count */
			size_t FrameCountToByteCount(size_t frameCount) const;

			/*! @brief Convert a byte count to frame count */
			size_t ByteCountToFrameCount(size_t byteCount) const;

			//@}

			/*! @brief Returns a string representation of this format suitable for logging */
			CFString Description() const;

		};

	}
}
