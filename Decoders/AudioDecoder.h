/*
 *  Copyright (C) 2006, 2007, 2008, 2009, 2010 Stephen F. Booth <me@sbooth.org>
 *  All Rights Reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are
 *  met:
 *
 *    - Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    - Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *    - Neither the name of Stephen F. Booth nor the names of its 
 *      contributors may be used to endorse or promote products derived
 *      from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <CoreFoundation/CoreFoundation.h>
#include <CoreAudio/CoreAudioTypes.h>


// ========================================
// Typedefs
// ========================================
class AudioDecoder;
typedef void
(*AudioDecoderCallback)(void					*context,
						const AudioDecoder		*decoder);


struct AudioDecoderCallbackAndContext
{
	AudioDecoderCallback	mCallback;
	void					*mContext;
};


// ========================================
// Abstract superclass for an audio decoder
// A decoder is responsible for reading audio data in some format and providing
// it as 32-bit float normalized non-interleaved PCM (canonical Core Audio format)
// ========================================
class AudioDecoder
{
	
	friend class AudioPlayer;
	
public:

	// ========================================
	// Information on supported file formats
	static CFArrayRef CreateSupportedFileExtensions();
	static CFArrayRef CreateSupportedMIMETypes();

	static bool HandlesFilesWithExtension(CFStringRef extension);
	static bool HandlesMIMEType(CFStringRef mimeType);
	
	// ========================================
	// Factory methods that return an AudioDecoder for the specified URL, or NULL on failure
	static AudioDecoder * CreateDecoderForURL(CFURLRef url);

	// Limit decoding to a specified file region
	static AudioDecoder * CreateDecoderForURLRegion(CFURLRef url, SInt64 startingFrame);
	static AudioDecoder * CreateDecoderForURLRegion(CFURLRef url, SInt64 startingFrame, UInt32 frameCount);
	static AudioDecoder * CreateDecoderForURLRegion(CFURLRef url, SInt64 startingFrame, UInt32 frameCount, UInt32 repeatCount);
	
	// ========================================
	// Destruction
	virtual ~AudioDecoder();
	
	// ========================================
	// The URL this decoder will process
	inline CFURLRef GetURL()								{ return mURL; }
	
	// ========================================
	// The native format of the source audio
	inline AudioStreamBasicDescription GetSourceFormat()	{ return mSourceFormat; }
	virtual CFStringRef CreateSourceFormatDescription();
	
	// ========================================
	// The type of PCM data provided by this decoder
	inline AudioStreamBasicDescription GetFormat()			{ return mFormat; }
	CFStringRef CreateFormatDescription();
	
	// ========================================
	// The layout of the channels this decoder provides
	inline AudioChannelLayout GetChannelLayout()			{ return mChannelLayout; }
	CFStringRef CreateChannelLayoutDescription();
	
	// ========================================
	// Attempt to read frameCount frames of audio, returning the actual number of frames read
	virtual UInt32 ReadAudio(AudioBufferList *bufferList, UInt32 frameCount) = 0;
	
	// ========================================
	// Source audio information
	virtual SInt64 GetTotalFrames() = 0;
	virtual SInt64 GetCurrentFrame() = 0;
	inline SInt64 GetFramesRemaining()						{ return GetTotalFrames() - GetCurrentFrame(); }
	
	// ========================================
	// Seeking support
	virtual bool SupportsSeeking()							{ return false; }
	virtual SInt64 SeekToFrame(SInt64 /*frame*/)			{ return -1; }

	// ========================================
	// AudioPlayer callback support
	void SetDecodingStartedCallback(AudioDecoderCallback callback, void *context);
	void SetDecodingFinishedCallback(AudioDecoderCallback callback, void *context);
	void SetRenderingStartedCallback(AudioDecoderCallback callback, void *context);
	void SetRenderingFinishedCallback(AudioDecoderCallback callback, void *context);

protected:

	CFURLRef						mURL;				// The location of the stream to be decoded
	
	AudioStreamBasicDescription		mFormat;			// The type of PCM data provided by this decoder
	AudioChannelLayout				mChannelLayout;		// The channel layout for the PCM data	
	
	AudioStreamBasicDescription		mSourceFormat;		// The native (PCM) format of the source file

	// ========================================
	// For subclass use only
	AudioDecoder();
	AudioDecoder(CFURLRef url);
	AudioDecoder(const AudioDecoder& rhs);
	AudioDecoder& operator=(const AudioDecoder& rhs);

private:

	// ========================================
	// Callbacks for AudioPlayer use only
	AudioDecoderCallbackAndContext	mCallbacks [4];
	
	void PerformDecodingStartedCallback();
	void PerformDecodingFinishedCallback();
	void PerformRenderingStartedCallback();
	void PerformRenderingFinishedCallback();
	
};
