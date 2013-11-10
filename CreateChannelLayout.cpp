/*
 *  Copyright (C) 2011, 2012, 2013 Stephen F. Booth <me@sbooth.org>
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

size_t SFB::Audio::GetChannelLayoutSize(UInt32 numberChannelDescriptions)
{
	return offsetof(AudioChannelLayout, mChannelDescriptions) + (numberChannelDescriptions * sizeof(AudioChannelDescription));
}

size_t SFB::Audio::GetChannelLayoutSize(const AudioChannelLayout *layout)
{
	assert(nullptr != layout);
	return GetChannelLayoutSize(layout->mNumberChannelDescriptions);
}

AudioChannelLayout * SFB::Audio::CreateChannelLayout(UInt32 numberChannelDescriptions)
{
	size_t layoutSize = GetChannelLayoutSize(numberChannelDescriptions);
	AudioChannelLayout *channelLayout = (AudioChannelLayout *)malloc(layoutSize);
	memset(channelLayout, 0, layoutSize);

	return channelLayout;
}

AudioChannelLayout * SFB::Audio::CreateChannelLayoutWithTag(AudioChannelLayoutTag layoutTag)
{
	AudioChannelLayout *channelLayout = CreateChannelLayout();
	channelLayout->mChannelLayoutTag = layoutTag;
	return channelLayout;
}

AudioChannelLayout * SFB::Audio::CreateChannelLayoutWithChannelLabels(std::vector<AudioChannelLabel> channelLabels)
{
	AudioChannelLayout *channelLayout = CreateChannelLayout((UInt32)channelLabels.size());

	channelLayout->mChannelLayoutTag = kAudioChannelLayoutTag_UseChannelDescriptions;
	channelLayout->mChannelBitmap = 0;

	channelLayout->mNumberChannelDescriptions = (UInt32)channelLabels.size();

	for(std::vector<AudioChannelLabel>::size_type i = 0; i != channelLabels.size(); ++i)
		channelLayout->mChannelDescriptions[i].mChannelLabel = channelLabels[i];

	return channelLayout;
}

AudioChannelLayout * SFB::Audio::CreateChannelLayoutWithBitmap(UInt32 channelBitmap)
{
	AudioChannelLayout *channelLayout = CreateChannelLayout();
	channelLayout->mChannelBitmap = channelBitmap;
	return channelLayout;
}

AudioChannelLayout * SFB::Audio::CopyChannelLayout(const AudioChannelLayout *rhs)
{
	if(nullptr == rhs)
		return nullptr;

	size_t layoutSize = GetChannelLayoutSize(rhs->mNumberChannelDescriptions);
	AudioChannelLayout *channelLayout = (AudioChannelLayout *)malloc(layoutSize);
	memcpy(channelLayout, rhs, layoutSize);
	
	return channelLayout;
}
