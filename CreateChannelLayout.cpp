/*
 *  Copyright (C) 2011, 2012 Stephen F. Booth <me@sbooth.org>
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

#include <stdlib.h>
#include <stddef.h>
#include <assert.h>

#include "CreateChannelLayout.h"

static size_t GetChannelLayoutSize(UInt32 numberChannelDescriptions)
{
	return offsetof(AudioChannelLayout, mChannelDescriptions) + (numberChannelDescriptions * sizeof(AudioChannelDescription));
}

size_t GetChannelLayoutSize(const AudioChannelLayout *layout)
{
	assert(nullptr != layout);
	return GetChannelLayoutSize(layout->mNumberChannelDescriptions);
}

AudioChannelLayout * CreateChannelLayout(UInt32 numberChannelDescriptions)
{
	size_t layoutSize = GetChannelLayoutSize(numberChannelDescriptions);
	AudioChannelLayout *channelLayout = static_cast<AudioChannelLayout *>(malloc(layoutSize));
	memset(channelLayout, 0, layoutSize);

	return channelLayout;
}

AudioChannelLayout * CreateChannelLayoutWithTag(AudioChannelLayoutTag layoutTag)
{
	AudioChannelLayout *channelLayout = CreateChannelLayout();
	channelLayout->mChannelLayoutTag = layoutTag;
	return channelLayout;
}

AudioChannelLayout * CreateChannelLayoutWithBitmap(UInt32 channelBitmap)
{
	AudioChannelLayout *channelLayout = CreateChannelLayout();
	channelLayout->mChannelBitmap = channelBitmap;
	return channelLayout;
}

AudioChannelLayout * CopyChannelLayout(const AudioChannelLayout *rhs)
{
	if(nullptr == rhs)
		return nullptr;

	size_t layoutSize = GetChannelLayoutSize(rhs->mNumberChannelDescriptions);
	AudioChannelLayout *channelLayout = static_cast<AudioChannelLayout *>(malloc(layoutSize));
	memcpy(channelLayout, rhs, layoutSize);
	
	return channelLayout;
}

AudioChannelLayout * CreateDefaultAudioChannelLayout(UInt32 channelsPerFrame)
{
	assert(0 < channelsPerFrame);

	AudioChannelLayout *channelLayout = nullptr;

	switch(channelsPerFrame) {
		case 1:		channelLayout = CreateChannelLayoutWithTag(kAudioChannelLayoutTag_Mono);			break;
		case 2:		channelLayout = CreateChannelLayoutWithTag(kAudioChannelLayoutTag_Stereo);			break;
		case 4:		channelLayout = CreateChannelLayoutWithTag(kAudioChannelLayoutTag_Quadraphonic);	break;
		case 6:		channelLayout = CreateChannelLayoutWithTag(kAudioChannelLayoutTag_MPEG_5_1_A);		break;
		case 8:		channelLayout = CreateChannelLayoutWithTag(kAudioChannelLayoutTag_MPEG_7_1_A);		break;
			
		default:
			channelLayout = CreateChannelLayoutWithTag(kAudioChannelLayoutTag_Unknown | channelsPerFrame);
			break;
	}

	return channelLayout;
}
