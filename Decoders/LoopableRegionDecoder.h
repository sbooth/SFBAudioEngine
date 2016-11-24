/*
 * Copyright (c) 2006 - 2016 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#pragma once

#include "AudioDecoder.h"

/*! @file LoopableRegionDecoder.h @brief Support for decoding specific audio regions */

/*! @brief \c SFBAudioEngine's encompassing namespace */
namespace SFB {

	/*! @brief %Audio functionality */
	namespace Audio {

		/*! @brief A wrapper around a Decoder that decodes a specific region */
		class LoopableRegionDecoder : public Decoder
		{

		public:

			// ========================================
			/*! @name Factory Methods */
			//@{

			/*!
			 * @brief Create a \c LoopableRegionDecoder object for a region of the specified URL
			 * @param url The URL
			 * @param startingFrame The first frame to decode
			 * @param error An optional pointer to a \c CFErrorRef to receive error information
			 * @return A \c LoopableRegionDecoder object, or \c nullptr on failure
			 */
			static unique_ptr CreateForURLRegion(CFURLRef url, SInt64 startingFrame, CFErrorRef *error = nullptr);

			/*!
			 * @brief Create a \c LoopableRegionDecoder object for a region of the specified URL
			 * @param url The URL
			 * @param startingFrame The first frame to decode
			 * @param frameCount The number of frames to decode
			 * @param error An optional pointer to a \c CFErrorRef to receive error information
			 * @return A \c Decoder object, or \c nullptr on failure
			 */
			static unique_ptr CreateForURLRegion(CFURLRef url, SInt64 startingFrame, UInt32 frameCount, CFErrorRef *error = nullptr);

			/*!
			 * @brief Create a \c LoopableRegionDecoder object for a region of the specified URL
			 * @param url The URL
			 * @param startingFrame The first frame to decode
			 * @param frameCount The number of frames to decode
			 * @param repeatCount The number of times to repeat
			 * @param error An optional pointer to a \c CFErrorRef to receive error information
			 * @return A \c LoopableRegionDecoder object, or \c nullptr on failure
			 */
			static unique_ptr CreateForURLRegion(CFURLRef url, SInt64 startingFrame, UInt32 frameCount, UInt32 repeatCount, CFErrorRef *error = nullptr);


			/*!
			 * @brief Create a \c LoopableRegionDecoder object for a region of the specified \c InputSource
			 * @param inputSource The input source
			 * @param startingFrame The first frame to decode
			 * @param error An optional pointer to a \c CFErrorRef to receive error information
			 * @return A \c LoopableRegionDecoder object, or \c nullptr on failure
			 */
			static unique_ptr CreateForInputSourceRegion(InputSource::unique_ptr inputSource, SInt64 startingFrame, CFErrorRef *error = nullptr);

			/*!
			 * @brief Create a \c LoopableRegionDecoder object for a region of the specified \c InputSource
			 * @param inputSource The input source
			 * @param startingFrame The first frame to decode
			 * @param frameCount The number of frames to decode
			 * @param error An optional pointer to a \c CFErrorRef to receive error information
			 * @return A \c LoopableRegionDecoder object, or \c nullptr on failure
			 */
			static unique_ptr CreateForInputSourceRegion(InputSource::unique_ptr inputSource, SInt64 startingFrame, UInt32 frameCount, CFErrorRef *error = nullptr);

			/*!
			 * @brief Create a \c LoopableRegionDecoder object for a region of the specified \c InputSource
			 * @param inputSource The input source
			 * @param startingFrame The first frame to decode
			 * @param frameCount The number of frames to decode
			 * @param repeatCount The number of times to repeat
			 * @param error An optional pointer to a \c CFErrorRef to receive error information
			 * @return A \c LoopableRegionDecoder object, or \c nullptr on failure
			 */
			static unique_ptr CreateForInputSourceRegion(InputSource::unique_ptr inputSource, SInt64 startingFrame, UInt32 frameCount, UInt32 repeatCount, CFErrorRef *error = nullptr);


			/*!
			 * @brief Create a \c LoopableRegionDecoder object for a region of the specified \c Decoder
			 * @param decoder The decoder
			 * @param startingFrame The first frame to decode
			 * @param error An optional pointer to a \c CFErrorRef to receive error information
			 * @return A \c LoopableRegionDecoder object, or \c nullptr on failure
			 */
			static unique_ptr CreateForDecoderRegion(unique_ptr decoder, SInt64 startingFrame, CFErrorRef *error = nullptr);

			/*!
			 * @brief Create a \c LoopableRegionDecoder object for a region of the specified \c Decoder
			 * @param decoder The decoder
			 * @param startingFrame The first frame to decode
			 * @param frameCount The number of frames to decode
			 * @param error An optional pointer to a \c CFErrorRef to receive error information
			 * @return A \c LoopableRegionDecoder object, or \c nullptr on failure
			 */
			static unique_ptr CreateForDecoderRegion(unique_ptr decoder, SInt64 startingFrame, UInt32 frameCount, CFErrorRef *error = nullptr);

			/*!
			 * @brief Create a \c LoopableRegionDecoder object for a region of the specified \c Decoder
			 * @param decoder The decoder
			 * @param startingFrame The first frame to decode
			 * @param frameCount The number of frames to decode
			 * @param repeatCount The number of times to repeat
			 * @param error An optional pointer to a \c CFErrorRef to receive error information
			 * @return A \c LoopableRegionDecoder object, or \c nullptr on failure
			 */
			static unique_ptr CreateForDecoderRegion(unique_ptr decoder, SInt64 startingFrame, UInt32 frameCount, UInt32 repeatCount, CFErrorRef *error = nullptr);

			//@}


			// ========================================
			/*! @name Creation and Destruction */
			//@{

			/*! @brief Destroy this \c LoopableRegionDecoder */
			virtual ~LoopableRegionDecoder() = default;

			/*! @cond */

			/*! @internal This class is non-copyable */
			LoopableRegionDecoder(const LoopableRegionDecoder& rhs) = delete;

			/*! @internal This class is non-assignable */
			LoopableRegionDecoder& operator=(const LoopableRegionDecoder& rhs) = delete;

			/*! @endcond */
			//@}

			
		private:

			// Creation
			LoopableRegionDecoder() = delete;
			LoopableRegionDecoder(Decoder::unique_ptr decoder, SInt64 startingFrame);
			LoopableRegionDecoder(Decoder::unique_ptr decoder, SInt64 startingFrame, UInt32 frameCount);
			LoopableRegionDecoder(Decoder::unique_ptr decoder, SInt64 startingFrame, UInt32 frameCount, UInt32 repeatCount);

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
			inline virtual SInt64 _GetTotalFrames() const			{ return ((mRepeatCount + 1) * mFrameCount);}
			inline virtual SInt64 _GetCurrentFrame() const			{ return mTotalFramesRead;}

			// Seeking support
			inline virtual bool _SupportsSeeking() const			{ return mDecoder->SupportsSeeking(); }
			virtual SInt64 _SeekToFrame(SInt64 frame);


			// The starting frame for this audio file region
			inline SInt64 GetStartingFrame() const					{ return mStartingFrame; }
			inline void SetStartingFrame(SInt64 startingFrame)		{ mStartingFrame = startingFrame; }

			// The number of frames to decode
			inline UInt32 GetFrameCount() const						{ return mFrameCount; }
			inline void SetFrameCount(UInt32 frameCount)			{ mFrameCount = frameCount; }

			// The number of times to repeat the audio
			inline UInt32 GetRepeatCount() const					{ return mRepeatCount; }
			inline void SetRepeatCount(UInt32 repeatCount)			{ mRepeatCount = repeatCount; }

			inline UInt32 GetCompletedPasses() const				{ return mCompletedPasses; }

			// Reset to initial state
			bool Reset();

			// Called when mDecoder is open
			bool SetupDecoder(bool forceReset = true);

			// Data members
			Decoder::unique_ptr		mDecoder;

			SInt64					mStartingFrame;
			UInt32					mFrameCount;
			UInt32					mRepeatCount;
			
			UInt32					mFramesReadInCurrentPass;
			SInt64					mTotalFramesRead;
			UInt32					mCompletedPasses;
		};
		
	}
}
