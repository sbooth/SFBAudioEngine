/*
 * Copyright (c) 2011 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#pragma once

#include <dumb/dumb.h>

#import "AudioDecoder.h"

namespace SFB {

	namespace Audio {

		// ========================================
		// A Decoder subclass supporting MOD files
		// ========================================
		class MODDecoder : public Decoder
		{

		public:

			// Data types handled by this class
			static CFArrayRef CreateSupportedFileExtensions();
			static CFArrayRef CreateSupportedMIMETypes();

			static bool HandlesFilesWithExtension(CFStringRef extension);
			static bool HandlesMIMEType(CFStringRef mimeType);

			static Decoder::unique_ptr CreateDecoder(InputSource::unique_ptr inputSource);

			// Creation
			explicit MODDecoder(InputSource::unique_ptr inputSource);

		private:

			// Audio access
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

			using unique_DUMBFILE_ptr = std::unique_ptr<DUMBFILE, int(*)(DUMBFILE *)>;
			using unique_DUH_ptr = std::unique_ptr<DUH, void(*)(DUH *)>;
			using unique_DUH_SIGRENDERER_ptr = std::unique_ptr<DUH_SIGRENDERER, void(*)(DUH_SIGRENDERER *)>;

			// Data members
			DUMBFILE_SYSTEM						dfs;
			unique_DUMBFILE_ptr					df;
			unique_DUH_ptr						duh;
			unique_DUH_SIGRENDERER_ptr			dsr;

			SInt64								mTotalFrames;
			SInt64								mCurrentFrame;
		};

	}
}
