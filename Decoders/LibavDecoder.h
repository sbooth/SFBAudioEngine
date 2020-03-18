/*
 * Copyright (c) 2013 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#pragma once

#include "AudioBufferList.h"
#include "AudioDecoder.h"

struct AVFrame;
struct AVIOContext;
struct AVFormatContext;
struct AVCodecContext;

namespace SFB {

	namespace Audio {

		// ========================================
		// An AudioDecoder subclass supporting all formats handled by ffmpeg/libav
		// ========================================
		class LibavDecoder : public Decoder
		{

		public:

			// ========================================
			// The data types handled by this class
			static CFArrayRef CreateSupportedFileExtensions();
			static CFArrayRef CreateSupportedMIMETypes();

			static bool HandlesFilesWithExtension(CFStringRef extension);
			static bool HandlesMIMEType(CFStringRef mimeType);

			static Decoder::unique_ptr CreateDecoder(InputSource::unique_ptr inputSource);

			// Creation
			explicit LibavDecoder(InputSource::unique_ptr inputSource);

		private:

			// Audio access
			virtual bool _Open(CFErrorRef *error);
			virtual bool _Close(CFErrorRef *error);

			// The native format of the source audio
			virtual SFB::CFString _GetSourceFormatDescription() const;

			// Attempt to read frameCount frames of audio, returning the actual number of frames read
			virtual UInt32 _ReadAudio(AudioBufferList *bufferList, UInt32 frameCount);

			// Source audio information
			virtual SInt64 _GetTotalFrames() const;
			inline virtual SInt64 _GetCurrentFrame() const			{ return mCurrentFrame; }

			// Seeking support
			inline virtual bool _SupportsSeeking() const			{ return mInputSource->SupportsSeeking(); }
			virtual SInt64 _SeekToFrame(SInt64 frame);

			int ReadFrame();
			int DecodeFrame();

			using unique_AVFrame_ptr = std::unique_ptr<AVFrame, std::function<void (AVFrame *)>>;
			using unique_AVIOContext_ptr = std::unique_ptr<AVIOContext, std::function<void (AVIOContext *)>>;
			using unique_AVFormatContext_ptr = std::unique_ptr<AVFormatContext, std::function<void (AVFormatContext *)>>;
			using unique_AVCodecContext_ptr = std::unique_ptr<AVCodecContext, std::function<void (AVCodecContext *)>>;

			// Data members
			unique_AVFrame_ptr 					mFrame;
			unique_AVIOContext_ptr 				mIOContext;
			unique_AVFormatContext_ptr 			mFormatContext;
			unique_AVCodecContext_ptr 			mCodecContext;

			int 								mStreamIndex;
			SInt64 								mCurrentFrame;

			// For converting push to pull
			BufferList							mBufferList;

		};

	}
}
