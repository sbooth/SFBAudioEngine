/*
 * Copyright (c) 2011 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#pragma once

#include <ogg/ogg.h>
#include <speex/speex_bits.h>
#include <speex/speex_stereo.h>

#include "AudioBufferList.h"
#include "AudioDecoder.h"

namespace SFB {

	namespace Audio {

		// ========================================
		// A Decoder subclass supporting Speex
		// ========================================
		class OggSpeexDecoder : public Decoder
		{

		public:

			// Data types handled by this class
			static CFArrayRef CreateSupportedFileExtensions();
			static CFArrayRef CreateSupportedMIMETypes();

			static bool HandlesFilesWithExtension(CFStringRef extension);
			static bool HandlesMIMEType(CFStringRef mimeType);

			static Decoder::unique_ptr CreateDecoder(InputSource::unique_ptr inputSource);

			// Creation and destruction
			explicit OggSpeexDecoder(InputSource::unique_ptr inputSource);
			virtual ~OggSpeexDecoder();

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

			// Data members
			BufferList			mBufferList;
			SInt64				mCurrentFrame;
			SInt64				mTotalFrames;

			ogg_sync_state		mOggSyncState;
			ogg_page			mOggPage;
			ogg_stream_state	mOggStreamState;

			void				*mSpeexDecoder;
			SpeexBits			mSpeexBits;
			SpeexStereoState	*mSpeexStereoState;

			long				mSpeexSerialNumber;
			bool				mSpeexEOSReached;
			spx_int32_t			mSpeexFramesPerOggPacket;
			UInt32				mOggPacketCount;
			UInt32				mExtraSpeexHeaderCount;
		};

	}
}
