/*
 * Copyright (c) 2011 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#pragma once

#include <tta++/libtta.h>

#import "AudioDecoder.h"

namespace SFB {

	namespace Audio {

		// ========================================
		// A Decoder subclass supporting TrueAudio files
		// ========================================
		class TrueAudioDecoder : public Decoder
		{

		public:

			// Data types handled by this class
			static CFArrayRef CreateSupportedFileExtensions();
			static CFArrayRef CreateSupportedMIMETypes();

			static bool HandlesFilesWithExtension(CFStringRef extension);
			static bool HandlesMIMEType(CFStringRef mimeType);

			static Decoder::unique_ptr CreateDecoder(InputSource::unique_ptr inputSource);

			// Creation
			explicit TrueAudioDecoder(InputSource::unique_ptr inputSource);

		private:

			// File access
			virtual bool _Open(CFErrorRef *error);
			virtual bool _Close(CFErrorRef *error);

			// The native format of the source audio
			virtual SFB::CFString _GetSourceFormatDescription() const;

			// Attempt to read frameCount frames of audio, returning the actual number of frames read
			virtual UInt32 _ReadAudio(AudioBufferList *bufferList, UInt32 frameCount);

			// Source audio information
			inline virtual SInt64 _GetTotalFrames() const			{ return mTotalFrames; }
			inline virtual SInt64 _GetCurrentFrame() const			{ return mCurrentFrame; }

			// Seeking support
			inline virtual bool _SupportsSeeking() const			{ return mInputSource->SupportsSeeking(); }
			virtual SInt64 _SeekToFrame(SInt64 frame);

		public:

			struct TTA_io_callback_wrapper;

		private:

			using unique_tta_ptr = std::unique_ptr<tta::tta_decoder>;
			using unique_callback_wrapper_ptr = std::unique_ptr<TTA_io_callback_wrapper>;

			// Data members
			unique_tta_ptr						mDecoder;
			unique_callback_wrapper_ptr			mCallbacks;
			SInt64								mCurrentFrame;
			SInt64								mTotalFrames;
			UInt32								mFramesToSkip;
		};

	}
}
