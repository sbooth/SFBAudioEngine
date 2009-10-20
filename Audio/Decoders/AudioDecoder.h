/*
 *  Copyright (C) 2006 - 2009 Stephen F. Booth <me@sbooth.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
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
