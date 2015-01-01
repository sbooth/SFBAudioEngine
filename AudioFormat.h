/*
 *  Copyright (C) 2014, 2015 Stephen F. Booth <me@sbooth.org>
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
