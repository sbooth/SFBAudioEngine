/*
 * Copyright (c) 2018 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#pragma once

#include <vector>

#include "AudioBufferList.h"
#include "AudioDecoder.h"

/*! @file DSDPCMDecoder.h @brief Support for decoding DSD64 to PCM */

/*! @brief \c SFBAudioEngine's encompassing namespace */
namespace SFB {

	/*! @brief %Audio functionality */
	namespace Audio {

		/*! @brief A wrapper around a Decoder supporting DSD64 to PCM conversion */
		class DSDPCMDecoder : public Decoder
		{

		public:

			// ========================================
			/*! @name Factory Methods */
			//@{

			/*!
			 * @brief Create a \c DSDPCMDecoder object for the specified URL
			 * @param url The URL
			 * @param error An optional pointer to a \c CFErrorRef to receive error information
			 * @return A \c DSDPCMDecoder object, or \c nullptr on failure
			 */
			static unique_ptr CreateForURL(CFURLRef url, CFErrorRef *error = nullptr);

			/*!
			 * @brief Create a \c DSDPCMDecoder object for the specified \c InputSource
			 * @param inputSource The input source
			 * @param error An optional pointer to a \c CFErrorRef to receive error information
			 * @return A \c DSDPCMDecoder object, or \c nullptr on failure
			 */
			static unique_ptr CreateForInputSource(InputSource::unique_ptr inputSource, CFErrorRef *error = nullptr);

			/*!
			 * @brief Create a \c DSDPCMDecoder object for the specified \c Decoder
			 * @param decoder The decoder
			 * @param error An optional pointer to a \c CFErrorRef to receive error information
			 * @return A \c DSDPCMDecoder object, or \c nullptr on failure
			 */
			static unique_ptr CreateForDecoder(unique_ptr decoder, CFErrorRef *error = nullptr);

			//@}


			// ========================================
			/*! @name Creation and Destruction */
			//@{

			/*! @brief Destroy this \c DSDPCMDecoder */
			virtual ~DSDPCMDecoder() = default;

			/*! @cond */

			/*! @internal This class is non-copyable */
			DSDPCMDecoder(const DSDPCMDecoder& rhs) = delete;

			/*! @internal This class is non-assignable */
			DSDPCMDecoder& operator=(const DSDPCMDecoder& rhs) = delete;

			/*! @endcond */
			//@}

			// ========================================
			/*! @name PCM Level Adjustment */
			//@{

			/*! @brief Get the linear gain applied to the converted DSD samples (default is 6 dBFS) */
			inline float GetLinearGain() const 						{ return mLinearGain; }

			/*!
			 * @brief Set the linear gain applied to the converted DSD samples
			 * @param linearGain The linear gain to apply after conversion to PCM
			 */
			inline void SetLinearGain(float linearGain) 			{ mLinearGain = linearGain; }

			//@}

		private:

			class DXD;

			DSDPCMDecoder() = delete;
			explicit DSDPCMDecoder(Decoder::unique_ptr decoder);

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
			std::vector<DXD> 		mContext;
			float 					mLinearGain;
		};

	}
}
