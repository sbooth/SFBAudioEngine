/*
 * Copyright (c) 2006 - 2017 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#pragma once

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-variable"

#include <vorbis/vorbisfile.h>

#pragma clang diagnostic pop

#import "AudioDecoder.h"

namespace SFB {

	namespace Audio {

		// ========================================
		// A Decoder subclass supporting Ogg Vorbis
		// ========================================
		class OggVorbisDecoder : public Decoder
		{

		public:

			// Data types handled by this class
			static CFArrayRef CreateSupportedFileExtensions();
			static CFArrayRef CreateSupportedMIMETypes();

			static bool HandlesFilesWithExtension(CFStringRef extension);
			static bool HandlesMIMEType(CFStringRef mimeType);

			static Decoder::unique_ptr CreateDecoder(InputSource::unique_ptr inputSource);

			// Creation and destruction
			explicit OggVorbisDecoder(InputSource::unique_ptr inputSource);
			virtual ~OggVorbisDecoder();

		private:

			// Audio access
			virtual bool _Open(CFErrorRef *error);
			virtual bool _Close(CFErrorRef *error);

			// The native format of the source audio
			virtual SFB::CFString _GetSourceFormatDescription() const;

			// Attempt to read frameCount frames of audio, returning the actual number of frames read
			virtual UInt32 _ReadAudio(AudioBufferList *bufferList, UInt32 frameCount);

			// Source audio information
			inline virtual SInt64 _GetTotalFrames() const			{ return ov_pcm_total(const_cast<OggVorbis_File *>(&mVorbisFile), -1); }
			inline virtual SInt64 _GetCurrentFrame() const			{ return ov_pcm_tell(const_cast<OggVorbis_File *>(&mVorbisFile)); }

			// Seeking support
			inline virtual bool _SupportsSeeking() const			{ return mInputSource->SupportsSeeking(); }
			virtual SInt64 _SeekToFrame(SInt64 frame);

			// Data members
			OggVorbis_File		mVorbisFile;
		};

	}
}
