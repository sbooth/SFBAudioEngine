/*
 *  Copyright (C) 2006 - 2009 Stephen F. Booth <me@sbooth.org>
 *  All Rights Reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *      * Redistributions of source code must retain the above copyright
 *        notice, this list of conditions and the following disclaimer.
 *      * Redistributions in binary form must reproduce the above copyright
 *        notice, this list of conditions and the following disclaimer in the
 *        documentation and/or other materials provided with the distribution.
 *      * Neither the name of Stephen F. Booth nor the
 *        names of its contributors may be used to endorse or promote products
 *        derived from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY STEPHEN F. BOOTH ''AS IS'' AND ANY
 *  EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 *  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *  DISCLAIMED. IN NO EVENT SHALL STEPHEN F. BOOTH BE LIABLE FOR ANY
 *  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 *  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 *  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 *  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <CoreFoundation/CoreFoundation.h>
#include <CoreAudio/CoreAudioTypes.h>


// ========================================
// CFError domain and codes
// ========================================
extern CFStringRef const AudioDecoderErrorDomain;

enum {
	AudioDecoderFileNotFoundError				= 0,
	AudioDecoderFileFormatNotRecognizedError	= 1,
	AudioDecoderFileFormatNotSupportedError		= 2,
	AudioDecoderInputOutputError				= 3
};


// ========================================
// Abstract superclass for an audio decoder
// A decoder is responsible for reading audio data in some format and providing
// it as 32-bit float non-interleaved PCM (canonical Core Audio format)
// ========================================
class AudioDecoder
{
	
public:
	
	// ========================================
	// The data types handled by this class
	static bool HandlesFilesWithExtension(CFStringRef)		{ return false; }
	static bool HandlesMIMEType(CFStringRef)				{ return false; }
	
	// ========================================
	// Return an AudioDecoder of the appropriate class
	static AudioDecoder * CreateDecoderForURL(CFURLRef url, CFErrorRef *error = NULL);
	static AudioDecoder * CreateDecoderForMIMEType(CFStringRef mimeType, CFErrorRef *error = NULL);
	
	// ========================================
	// Creation
	AudioDecoder(CFURLRef url, CFErrorRef *error = NULL);
	
	// ========================================
	// Destruction
	virtual ~AudioDecoder();
	
	// ========================================
	// The stream this decoder will process
	inline CFURLRef GetURL()								{ return mURL; }
	
	// ========================================
	// The native (PCM) format of the source
	inline AudioStreamBasicDescription GetSourceFormat()	{ return mSourceFormat; }
	CFStringRef GetSourceFormatDescription();
	
	// ========================================
	// The type of PCM data provided by this decoder
	inline AudioStreamBasicDescription GetFormat()			{ return mFormat; }
	CFStringRef GetFormatDescription();
	
	// ========================================
	// The layout of the channels this decoder provides
	inline AudioChannelLayout ChannelLayout()				{ return mChannelLayout; }
	CFStringRef GetChannelLayoutDescription();
	
	// ========================================
	// Attempt to read frameCount frames of audio, returning the actual number of frames read
	virtual UInt32 ReadAudio(AudioBufferList *bufferList, UInt32 frameCount) = 0;
	
	// ========================================
	// Source audio information
	virtual SInt64 TotalFrames() = 0;
	virtual SInt64 CurrentFrame() = 0;
	inline SInt64 FramesRemaining()							{ return TotalFrames() - CurrentFrame(); }
	
	// ========================================
	// Seeking support
	virtual bool SupportsSeeking()							{ return false; }
	virtual SInt64 SeekToFrame(SInt64 /*frame*/)			{ return -1; }

protected:

	CFURLRef						mURL;				// The location of the stream to be decoded
	
	AudioStreamBasicDescription		mFormat;			// The type of PCM data provided by this decoder
	AudioChannelLayout				mChannelLayout;		// The channel layout for the PCM data	
	
	AudioStreamBasicDescription		mSourceFormat;		// The native (PCM) format of the source file

	// ========================================
	// For subclass use only
	AudioDecoder();
	AudioDecoder(const AudioDecoder& rhs);
	AudioDecoder& operator=(const AudioDecoder& rhs);

private:

	// Cached values
	CFStringRef						mFormatDescription;
	CFStringRef						mChannelLayoutDescription;
	CFStringRef						mSourceFormatDescription;
	
};
