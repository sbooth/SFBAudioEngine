/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#pragma once

#include <FLAC/stream_decoder.h>

#include "AudioBufferList.h"
#include "AudioDecoder.h"

namespace SFB {

	namespace Audio {

		// ========================================
		// A Decoder subclass supporting the Free Lossless Audio Codec (FLAC)
		// ========================================
		class FLACDecoder : public Decoder
		{

		public:

			// Data types handled by this class
			static CFArrayRef CreateSupportedFileExtensions();
			static CFArrayRef CreateSupportedMIMETypes();

			static bool HandlesFilesWithExtension(CFStringRef extension);
			static bool HandlesMIMEType(CFStringRef mimeType);

			static Decoder::unique_ptr CreateDecoder(InputSource::unique_ptr inputSource);

			// Creation
			explicit FLACDecoder(InputSource::unique_ptr inputSource);

		private:

			// Audio access
			virtual bool _Open(CFErrorRef *error);
			virtual bool _Close(CFErrorRef *error);

			// The native format of the source audio
			virtual SFB::CFString _GetSourceFormatDescription() const;

			// Attempt to read frameCount frames of audio, returning the actual number of frames read
			virtual UInt32 _ReadAudio(AudioBufferList *bufferList, UInt32 frameCount);

			// Source audio information
			inline virtual SInt64 _GetTotalFrames() const			{ return (SInt64)mStreamInfo.total_samples; }
			inline virtual SInt64 _GetCurrentFrame() const			{ return mCurrentFrame; }

			// Seeking support
			inline virtual bool _SupportsSeeking() const			{ return mInputSource->SupportsSeeking(); }
			virtual SInt64 _SeekToFrame(SInt64 frame);

			using unique_FLAC_ptr = std::unique_ptr<FLAC__StreamDecoder, void(*)(FLAC__StreamDecoder *)>;

			// Data members
			unique_FLAC_ptr						mFLAC;
			FLAC__StreamMetadata_StreamInfo		mStreamInfo;
			SInt64								mCurrentFrame;

			// For converting push to pull
			BufferList							mBufferList;

		public:

			// Callbacks- for internal use only
			FLAC__StreamDecoderWriteStatus Write(const FLAC__StreamDecoder *decoder, const FLAC__Frame *frame, const FLAC__int32 * const buffer[]);
			void Metadata(const FLAC__StreamDecoder *decoder, const FLAC__StreamMetadata *metadata);
			void Error(const FLAC__StreamDecoder *decoder, FLAC__StreamDecoderErrorStatus status);

		};

	}
}
