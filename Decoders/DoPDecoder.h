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

#include "AudioDecoder.h"
#include "AudioBufferList.h"

/*! @file DoPDecoder.h @brief Support for DoP decoding */

/*! @brief \c SFBAudioEngine's encompassing namespace */
namespace SFB {

	/*! @brief %Audio functionality */
	namespace Audio {

		/*!
		 * @brief A wrapper around a Decoder supporting DoP (DSD over PCM)
		 *
		 * See http://dsd-guide.com/sites/default/files/white-papers/DoP_openStandard_1v1.pdf
		 */
		class DoPDecoder : public Decoder
		{

		public:

			// ========================================
			/*! @name Factory Methods */
			//@{

			/*!
			 * @brief Create a \c DoPDecoder object for the specified URL
			 * @param url The URL
			 * @param error An optional pointer to a \c CFErrorRef to receive error information
			 * @return A \c DoPDecoder object, or \c nullptr on failure
			 */
			static unique_ptr CreateForURL(CFURLRef url, CFErrorRef *error = nullptr);

			/*!
			 * @brief Create a \c DoPDecoder object for the specified \c InputSource
			 * @param inputSource The input source
			 * @param error An optional pointer to a \c CFErrorRef to receive error information
			 * @return A \c DoPDecoder object, or \c nullptr on failure
			 */
			static unique_ptr CreateForInputSource(InputSource::unique_ptr inputSource, CFErrorRef *error = nullptr);

			/*!
			 * @brief Create a \c DoPDecoder object for the specified \c Decoder
			 * @param decoder The decoder
			 * @param error An optional pointer to a \c CFErrorRef to receive error information
			 * @return A \c DoPDecoder object, or \c nullptr on failure
			 */
			static unique_ptr CreateForDecoder(unique_ptr decoder, CFErrorRef *error = nullptr);
			
			//@}


			// ========================================
			/*! @name Creation and Destruction */
			//@{

			/*! @brief Destroy this \c DoPDecoder */
			virtual ~DoPDecoder() = default;

			/*! @cond */

			/*! @internal This class is non-copyable */
			DoPDecoder(const DoPDecoder& rhs) = delete;

			/*! @internal This class is non-assignable */
			DoPDecoder& operator=(const DoPDecoder& rhs) = delete;

			/*! @endcond */
			//@}

			
		private:

			DoPDecoder() = delete;
			DoPDecoder(Decoder::unique_ptr decoder);

			// Source access
			inline virtual CFURLRef _GetURL() const					{ return mDecoder->GetURL(); }
			inline virtual InputSource& _GetInputSource() const		{ return mDecoder->GetInputSource(); }

			// Audio access
			virtual bool _Open(CFErrorRef *error);
			virtual bool _Close(CFErrorRef *error);

			// The native format of the source audio
			virtual SFB::CFString _GetSourceFormatDescription() const;

			// Attempt to read frameCount frames of audio, returning the actual number of frames read
			virtual UInt32 _ReadAudio(AudioBufferList *bufferList, UInt32 frameCount);

			// Source audio information
			virtual SInt64 _GetTotalFrames() const;
			virtual SInt64 _GetCurrentFrame() const;

			// Seeking support
			inline virtual bool _SupportsSeeking() const			{ return mDecoder->SupportsSeeking(); }
			virtual SInt64 _SeekToFrame(SInt64 frame);


			// Data members
			Decoder::unique_ptr		mDecoder;
			BufferList				mBufferList;
			uint8_t					mMarker;
			bool					mReverseBits;
		};
		
	}
}
