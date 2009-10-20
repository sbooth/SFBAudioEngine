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

#include <AudioToolbox/AudioToolbox.h>


// ========================================
// Forward declarations
// ========================================
class CARingBuffer;
class AudioDecoder;


// ========================================
// CFError domain and codes
// ========================================
extern CFStringRef const AudioPlayerErrorDomain;

enum {
	AudioPlayerInternalError							= 0,
	AudioPlayerFileFormatNotSupportedError				= 1,
	AudioPlayerInputOutputError							= 2
};


// ========================================
//
// ========================================
class AudioPlayer
{
	
public:
	
	// ========================================
	// Creation/Destruction
	AudioPlayer();
	~AudioPlayer();
	
	// ========================================
	// Playback Control
	void Play();
	void Pause();
	void PlayPause();
	void Stop();
	
	bool IsPlaying();
	
	// ========================================
	// Seeking
	void SkipForward()									{ SkipForward(3); }
	void SkipBackward()									{ SkipBackward(3); }
	void SkipForward(UInt32 seconds);
	void SkipBackward(UInt32 seconds);
		
	void SkipToEnd();
	void SkipToBeginning();
		
	// ========================================
	// Player Parameters
	Float32 GetVolume();
	bool SetVolume(Float32 volume);

	Float32 GetPreGain();
	bool SetPreGain(Float32 preGain);

	// ========================================
	// Playlist management
	bool Play(AudioDecoder *decoder);
	bool Enqueue(AudioDecoder *decoder);
	
	// ========================================
	// Callbacks- for internal use only
	OSStatus Render(AudioUnitRenderActionFlags		*ioActionFlags,
					const AudioTimeStamp			*inTimeStamp,
					UInt32							inBusNumber,
					UInt32							inNumberFrames,
					AudioBufferList					*ioData);
	
private:
	
	AUGraph mAUGraph;
	
	AudioStreamBasicDescription mAUGraphFormat;
	AudioChannelLayout mAUGraphChannelLayout;
	
	AUNode mLimiterNode;
	AUNode mOutputNode;
	
	CARingBuffer *mRingBuffer;
	
	SInt64 mFramesDecoded;
	SInt64 mFramesRendered;
	
	AudioDecoder *d;

	// ========================================
	// AUGraph Utilities
	OSStatus CreateAUGraph();
	OSStatus DisposeAUGraph();
	
	OSStatus ResetAUGraph();
	
	Float64 GetAUGraphLatency();
	Float64 GetAUGraphTailTime();
	
	OSStatus SetPropertyOnAUGraphNodes(AudioUnitPropertyID propertyID, const void *propertyData, UInt32 propertyDataSize);

	OSStatus SetAUGraphFormat(AudioStreamBasicDescription format);
	OSStatus SetAUGraphChannelLayout(AudioChannelLayout channelLayout);

	// ========================================
	// PreGain Utilities
	bool EnablePreGain(UInt32 flag);
	bool PreGainIsEnabled();
	
};
