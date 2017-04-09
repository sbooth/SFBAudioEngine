/*
 * Copyright (c) 2014 - 2017 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#pragma once

#include <CoreAudio/CoreAudioTypes.h>

/*! @file AudioFormat.h @brief A Core %Audio \c AudioStreamBasicDescription wrapper */

/*! @brief \c SFBAudioEngine's encompassing namespace */
namespace SFB {

	/*! @brief %Audio functionality */
	namespace Audio {

		/*! @brief Additional audio format IDs */
		enum {
			kAudioFormatDirectStreamDigital = 'DSD ',		/*!< Direct Stream Digital (DSD) */
			kAudioFormatDoP = 'DoP '						/*!< DSD over PCM (DoP) */
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

			/*! @brief Copy constructor */
			AudioFormat(const AudioFormat& rhs);

			/*! @brief Assignment operator */
			AudioFormat& operator=(const AudioFormat& rhs);

			//@}


			// ========================================
			/*! @name Format information */
			//@{

			/*! @brief Query whether this format represents interleaved data */
			bool IsInterleaved() const;

			/*! @brief Query whether this format represents PCM audio data */
			bool IsPCM() const;

			/*! @brief Query whether this format represents DSD audio data */
			bool IsDSD() const;

			/*! @brief Query whether this format represents DoP audio data */
			bool IsDoP() const;

			/*! @brief Query whether this format represents native-endian ordered daa */
			bool IsNativeEndian() const;

			/*! @brief Convert a frame count to byte count */
			size_t FrameCountToByteCount(size_t frameCount) const;

			/*! @brief Convert a byte count to frame count */
			size_t ByteCountToFrameCount(size_t byteCount) const;

			//@}
		};

	}
}
