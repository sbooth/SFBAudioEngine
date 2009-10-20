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

#include <AudioToolbox/ExtendedAudioFile.h>
#include "AudioDecoder.h"


// ========================================
//
// ========================================
class CoreAudioDecoder : public AudioDecoder
{
	
public:
	
	// ========================================
	// The data types handled by this class
	static bool HandlesFilesWithExtension(CFStringRef extension);
	static bool HandlesMIMEType(CFStringRef mimeType);

	// ========================================
	// Creation
	CoreAudioDecoder(CFURLRef url, CFErrorRef *error = NULL);
	
	// ========================================
	// Destruction
	virtual ~CoreAudioDecoder();

	// ========================================
	// Attempt to read frameCount frames of audio, returning the actual number of frames read
	virtual UInt32 ReadAudio(AudioBufferList *bufferList, UInt32 frameCount);
	
	// ========================================
	// Source audio information
	virtual SInt64 TotalFrames();
	virtual SInt64 CurrentFrame();
	
	// ========================================
	// Seeking support
	virtual inline bool SupportsSeeking()					{ return true; }
	virtual SInt64 SeekToFrame(SInt64 frame);
	
private:
	
	ExtAudioFileRef mExtAudioFile;
	
};
