/*
 * Copyright (c) 2014 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#pragma once

#include "AudioBufferList.h"
#include "AudioDecoder.h"

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
			explicit DoPDecoder(Decoder::unique_ptr decoder);

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
