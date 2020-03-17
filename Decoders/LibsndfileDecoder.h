/*
 * Copyright (c) 2011 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#pragma once

#include <sndfile/sndfile.h>

#include "AudioDecoder.h"

namespace SFB {

	namespace Audio {

		// ========================================
		// A Decoder subclass supporting all formats handled by Libsndfile
		// ========================================
		class LibsndfileDecoder : public Decoder
		{

		public:

			// Data types handled by this class
			static CFArrayRef CreateSupportedFileExtensions();
			static CFArrayRef CreateSupportedMIMETypes();

			static bool HandlesFilesWithExtension(CFStringRef extension);
			static bool HandlesMIMEType(CFStringRef mimeType);

			static Decoder::unique_ptr CreateDecoder(InputSource::unique_ptr inputSource);

			// Creation
			explicit LibsndfileDecoder(InputSource::unique_ptr inputSource);

		private:

			// Audio access
			virtual bool _Open(CFErrorRef *error);
			virtual bool _Close(CFErrorRef *error);

			// The native format of the source audio
			virtual SFB::CFString _GetSourceFormatDescription() const;

			// Attempt to read frameCount frames of audio, returning the actual number of frames read
			virtual UInt32 _ReadAudio(AudioBufferList *bufferList, UInt32 frameCount);

			// Source audio information
			inline virtual SInt64 _GetTotalFrames() const			{ return mFileInfo.frames; }
			inline virtual SInt64 _GetCurrentFrame() const			{ return sf_seek(mFile.get(), 0, SEEK_CUR); }

			// Seeking support
			inline virtual bool _SupportsSeeking() const			{ return mInputSource->SupportsSeeking(); }
			inline virtual SInt64 _SeekToFrame(SInt64 frame)		{ return sf_seek(mFile.get(), frame, SEEK_SET); }

			using unique_SNDFILE_ptr = std::unique_ptr<SNDFILE, int(*)(SNDFILE *)>;

			// Data members
			enum class ReadMethod {
				Unknown,
				Short,
				Int,
				Float,
				Double
			};

			unique_SNDFILE_ptr	mFile;
			SF_INFO				mFileInfo;
			ReadMethod			mReadMethod;
		};

	}
}
