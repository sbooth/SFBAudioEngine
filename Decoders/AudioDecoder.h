/*
 *  Copyright (C) 2006, 2007, 2008, 2009, 2010, 2011, 2012 Stephen F. Booth <me@sbooth.org>
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

#include "InputSource.h"

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
// Error Codes
// ========================================
extern const CFStringRef		AudioDecoderErrorDomain;

enum {
	AudioDecoderFileFormatNotRecognizedError		= 0,
	AudioDecoderFileFormatNotSupportedError			= 1,
	AudioDecoderInputOutputError					= 2
};

// ========================================
// Abstract superclass for an audio decoder
// An AudioDecoder is responsible for reading audio data in some format and providing
// it in a PCM format that is handled by an AudioConverter
// ========================================
class AudioDecoder
{
	
	friend class AudioPlayer;
	friend class BasicAudioPlayer;
	
public:

	// ========================================
	// Information on supported file formats
	static CFArrayRef CreateSupportedFileExtensions();
	static CFArrayRef CreateSupportedMIMETypes();

	static bool HandlesFilesWithExtension(CFStringRef extension);
	static bool HandlesMIMEType(CFStringRef mimeType);
	
	// ========================================
	// Factory methods that return an AudioDecoder for the specified resource, or nullptr on failure
	// If specified, the MIME type will take precedence over file extension-based type resolution
	static AudioDecoder * CreateDecoderForURL(CFURLRef url, CFErrorRef *error = nullptr);
	static AudioDecoder * CreateDecoderForURL(CFURLRef url, CFStringRef mimeType, CFErrorRef *error = nullptr);

	// If this returns nullptr the caller is responsible for deleting inputSource
	static AudioDecoder * CreateDecoderForInputSource(InputSource *inputSource, CFErrorRef *error = nullptr);
	static AudioDecoder * CreateDecoderForInputSource(InputSource *inputSource, CFStringRef mimeType, CFErrorRef *error = nullptr);

	// Limit decoding to a specified region
	static AudioDecoder * CreateDecoderForURLRegion(CFURLRef url, SInt64 startingFrame, CFErrorRef *error = nullptr);
	static AudioDecoder * CreateDecoderForURLRegion(CFURLRef url, SInt64 startingFrame, UInt32 frameCount, CFErrorRef *error = nullptr);
	static AudioDecoder * CreateDecoderForURLRegion(CFURLRef url, SInt64 startingFrame, UInt32 frameCount, UInt32 repeatCount, CFErrorRef *error = nullptr);
	
	static AudioDecoder * CreateDecoderForInputSourceRegion(InputSource *inputSource, SInt64 startingFrame, CFErrorRef *error = nullptr);
	static AudioDecoder * CreateDecoderForInputSourceRegion(InputSource *inputSource, SInt64 startingFrame, UInt32 frameCount, CFErrorRef *error = nullptr);
	static AudioDecoder * CreateDecoderForInputSourceRegion(InputSource *inputSource, SInt64 startingFrame, UInt32 frameCount, UInt32 repeatCount, CFErrorRef *error = nullptr);

	static AudioDecoder * CreateDecoderForDecoderRegion(AudioDecoder *decoder, SInt64 startingFrame, CFErrorRef *error = nullptr);
	static AudioDecoder * CreateDecoderForDecoderRegion(AudioDecoder *decoder, SInt64 startingFrame, UInt32 frameCount, CFErrorRef *error = nullptr);
	static AudioDecoder * CreateDecoderForDecoderRegion(AudioDecoder *decoder, SInt64 startingFrame, UInt32 frameCount, UInt32 repeatCount, CFErrorRef *error = nullptr);

	// ========================================
	// Flag to specify whether AudioDecoders created with the above methods should be automatically opened (default is false)
	static inline bool AutomaticallyOpenDecoders()				{ return sAutomaticallyOpenDecoders; }
	static inline void SetAutomaticallyOpenDecoders(bool flag)	{ sAutomaticallyOpenDecoders = flag; }

	// ========================================
	// Destruction
	virtual ~AudioDecoder();

	// This class is non-copyable
	AudioDecoder(const AudioDecoder& rhs) = delete;
	AudioDecoder& operator=(const AudioDecoder& rhs) = delete;

	// ========================================
	// The URL this decoder will process
	inline CFURLRef GetURL() const								{ return mInputSource->GetURL(); }
	
	// ========================================
	// The input source feeding the decoder
	inline InputSource * GetInputSource() const					{ return mInputSource; }

	// ========================================
	// Audio access (must be implemented by subclasses)
	virtual bool Open(CFErrorRef *error = nullptr) = 0;
	virtual bool Close(CFErrorRef *error = nullptr) = 0;
	
	inline bool IsOpen() const									{ return mIsOpen; }

	// ========================================
	// The native format of the source audio
	inline AudioStreamBasicDescription GetSourceFormat() const 	{ return mSourceFormat; }
	virtual CFStringRef CreateSourceFormatDescription() const;
	
	// ========================================
	// The type of PCM data provided by this decoder
	inline AudioStreamBasicDescription GetFormat() const		{ return mFormat; }
	CFStringRef CreateFormatDescription() const;
	
	// ========================================
	// The layout of the channels this decoder provides
	inline AudioChannelLayout * GetChannelLayout() const		{ return mChannelLayout; }
	CFStringRef CreateChannelLayoutDescription() const;
	
	// ========================================
	// Attempt to read frameCount frames of audio, returning the actual number of frames read
	virtual UInt32 ReadAudio(AudioBufferList *bufferList, UInt32 frameCount) = 0;
	
	// ========================================
	// Source audio information
	virtual SInt64 GetTotalFrames() const = 0;
	virtual SInt64 GetCurrentFrame() const = 0;
	inline SInt64 GetFramesRemaining() const					{ return GetTotalFrames() - GetCurrentFrame(); }
	
	// ========================================
	// Seeking support
	virtual bool SupportsSeeking() const						{ return false; }
	virtual SInt64 SeekToFrame(SInt64 /*frame*/)				{ return -1; }

	// ========================================
	// AudioPlayer callback support
	void SetDecodingStartedCallback(AudioDecoderCallback callback, void *context);
	void SetDecodingFinishedCallback(AudioDecoderCallback callback, void *context);
	void SetRenderingStartedCallback(AudioDecoderCallback callback, void *context);
	void SetRenderingFinishedCallback(AudioDecoderCallback callback, void *context);

protected:

	InputSource						*mInputSource;		// The input source feeding the decoder
	
	AudioStreamBasicDescription		mFormat;			// The type of PCM data provided by this decoder
	AudioChannelLayout				*mChannelLayout;	// The channel layout for the PCM data, or nullptr if unknown/unspecified
	
	AudioStreamBasicDescription		mSourceFormat;		// The native format of the source file

	bool							mIsOpen;			// Subclasses should set this to true if Open() is successful
														// and false if Close() is successful
	
	// ========================================
	// For subclass use only
	AudioDecoder();
	AudioDecoder(InputSource *inputSource);

private:

	static bool						sAutomaticallyOpenDecoders;

	// ========================================
	// Callbacks for AudioPlayer use only
	AudioDecoderCallbackAndContext	mCallbacks [4];
	
	void PerformDecodingStartedCallback();
	void PerformDecodingFinishedCallback();
	void PerformRenderingStartedCallback();
	void PerformRenderingFinishedCallback();
	
};
