/*
 *  Copyright (C) 2013, 2014 Stephen F. Booth <me@sbooth.org>
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

#include <cstdlib>
#include <stdexcept>
#include <functional>

#include "AudioChannelLayout.h"

namespace {

	/*! @brief Get the size in bytes of an \c AudioChannelLayout with the specified number of channel descriptions */
	size_t GetChannelLayoutSize(UInt32 numberChannelDescriptions)
	{
		return offsetof(AudioChannelLayout, mChannelDescriptions) + (numberChannelDescriptions * sizeof(AudioChannelDescription));
	}

	/*!
	 * @brief Allocate an \c AudioChannelLayout
	 * @param numberChannelDescriptions The number of channel descriptions that will be stored in the channel layout
	 * @return An \c AudioChannelLayout
	 * @throws std::bad_alloc
	 */
	AudioChannelLayout * CreateChannelLayout(UInt32 numberChannelDescriptions)
	{
		size_t layoutSize = GetChannelLayoutSize(numberChannelDescriptions);
		AudioChannelLayout *channelLayout = (AudioChannelLayout *)std::malloc(layoutSize);
		if(nullptr == channelLayout)
			throw std::bad_alloc();

		memset(channelLayout, 0, layoutSize);

		return channelLayout;
	}

	/*! @brief Create a copy of \c rhs */
	AudioChannelLayout * CopyChannelLayout(const AudioChannelLayout *rhs)
	{
		if(nullptr == rhs)
			return nullptr;

		size_t layoutSize = GetChannelLayoutSize(rhs->mNumberChannelDescriptions);
		AudioChannelLayout *channelLayout = (AudioChannelLayout *)std::malloc(layoutSize);
		if(nullptr == channelLayout)
			throw std::bad_alloc();

		memcpy(channelLayout, rhs, layoutSize);

		return channelLayout;
	}

}

// Constants
const SFB::Audio::ChannelLayout SFB::Audio::ChannelLayout::Mono		= SFB::Audio::ChannelLayout::ChannelLayoutWithTag(kAudioChannelLayoutTag_Mono);
const SFB::Audio::ChannelLayout SFB::Audio::ChannelLayout::Stereo	= SFB::Audio::ChannelLayout::ChannelLayoutWithTag(kAudioChannelLayoutTag_Stereo);

SFB::Audio::ChannelLayout SFB::Audio::ChannelLayout::ChannelLayoutWithTag(AudioChannelLayoutTag layoutTag)
{
	auto channelLayout = ChannelLayout((UInt32)0);
	channelLayout.mChannelLayout->mChannelLayoutTag = layoutTag;
	return channelLayout;
}

SFB::Audio::ChannelLayout SFB::Audio::ChannelLayout::ChannelLayoutWithChannelLabels(std::vector<AudioChannelLabel> channelLabels)
{
	auto channelLayout = ChannelLayout((UInt32)channelLabels.size());

	channelLayout.mChannelLayout->mChannelLayoutTag = kAudioChannelLayoutTag_UseChannelDescriptions;
	channelLayout.mChannelLayout->mChannelBitmap = 0;

	channelLayout.mChannelLayout->mNumberChannelDescriptions = (UInt32)channelLabels.size();

	for(std::vector<AudioChannelLabel>::size_type i = 0; i != channelLabels.size(); ++i)
		channelLayout.mChannelLayout->mChannelDescriptions[i].mChannelLabel = channelLabels[i];

	return channelLayout;
}

SFB::Audio::ChannelLayout SFB::Audio::ChannelLayout::ChannelLayoutWithBitmap(UInt32 channelBitmap)
{
	auto channelLayout = ChannelLayout((UInt32)0);
	channelLayout.mChannelLayout->mChannelBitmap = channelBitmap;
	return channelLayout;
}

SFB::Audio::ChannelLayout::ChannelLayout()
	: mChannelLayout(nullptr, nullptr)
{}

SFB::Audio::ChannelLayout::ChannelLayout(UInt32 numberChannelDescriptions)
	: mChannelLayout(CreateChannelLayout(numberChannelDescriptions), std::free)
{}

SFB::Audio::ChannelLayout::ChannelLayout(const AudioChannelLayout *channelLayout)
	: mChannelLayout(CopyChannelLayout(channelLayout), std::free)
{}

SFB::Audio::ChannelLayout::ChannelLayout(ChannelLayout&& rhs)
	: mChannelLayout(std::move(rhs.mChannelLayout))
{}

SFB::Audio::ChannelLayout& SFB::Audio::ChannelLayout::operator=(ChannelLayout&& rhs)
{
	if(this != &rhs)
		mChannelLayout = std::move(rhs.mChannelLayout);
	return *this;
}

SFB::Audio::ChannelLayout::ChannelLayout(const ChannelLayout& rhs)
	: ChannelLayout()
{
	*this = rhs;
}

SFB::Audio::ChannelLayout& SFB::Audio::ChannelLayout::operator=(const ChannelLayout& rhs)
{
	if(this != &rhs) {
		if(!rhs)
			mChannelLayout.reset();
		else
			mChannelLayout = unique_AudioChannelLayout_ptr(CopyChannelLayout(rhs.mChannelLayout.get()), std::free);
	}

	return *this;
}

SFB::Audio::ChannelLayout& SFB::Audio::ChannelLayout::operator=(const AudioChannelLayout *rhs)
{
	if(nullptr == rhs)
		mChannelLayout.reset();
	else
		mChannelLayout = unique_AudioChannelLayout_ptr(CopyChannelLayout(rhs), std::free);

	return *this;
}

size_t SFB::Audio::ChannelLayout::GetChannelCount() const
{
	if(!mChannelLayout)
		return 0;

	UInt32 channelCount = 0;
	UInt32 propertySize = sizeof(channelCount);
	OSStatus result = AudioFormatGetProperty(kAudioFormatProperty_NumberOfChannelsForLayout, (UInt32)GetACLSize(), (void *)GetACL(), &propertySize, &channelCount);

	if(noErr != result)
		return 0;
	//LOGGER_ERR("org.sbooth.AudioEngine.ChannelLayout", "AudioFormatGetProperty (kAudioFormatProperty_NumberOfChannelsForLayout) failed: " << result);

	return channelCount;
}

bool SFB::Audio::ChannelLayout::MapToLayout(const ChannelLayout& outputLayout, std::vector<SInt32>& channelMap) const
{
	// No valid map exists for empty/unknown layouts
	if(!mChannelLayout || !outputLayout.mChannelLayout)
		return false;

	const AudioChannelLayout *layouts [] = {
		GetACL(),
		outputLayout.GetACL()
	};

	auto outputChannelCount = outputLayout.GetChannelCount();
	if(0 == outputChannelCount)
		return false;

	SInt32 rawChannelMap [outputChannelCount];
	UInt32 propertySize = sizeof(rawChannelMap);
	OSStatus result = AudioFormatGetProperty(kAudioFormatProperty_ChannelMap, sizeof(layouts), (void *)layouts, &propertySize, &rawChannelMap);

	if(noErr != result)
		return false;
	//LOGGER_ERR("org.sbooth.AudioEngine.ChannelLayout", "AudioFormatGetProperty (kAudioFormatProperty_ChannelMap) failed: " << result);

	auto start = (SInt32 *)rawChannelMap;
	channelMap.assign(start, start + outputChannelCount);

	return true;
}

size_t SFB::Audio::ChannelLayout::GetACLSize() const
{
	if(!mChannelLayout)
		return 0;

	return GetChannelLayoutSize(mChannelLayout->mNumberChannelDescriptions);
}

bool SFB::Audio::ChannelLayout::operator==(const ChannelLayout& rhs) const
{
	// Two empty channel layouts are considered equivalent
	if(!mChannelLayout && !rhs.mChannelLayout)
		return true;

	if(!mChannelLayout || !rhs.mChannelLayout)
		return false;

	const AudioChannelLayout *layouts [] = {
		rhs.GetACL(),
		GetACL()
	};

	UInt32 layoutsEqual = false;
	UInt32 propertySize = sizeof(layoutsEqual);
	OSStatus result = AudioFormatGetProperty(kAudioFormatProperty_AreChannelLayoutsEquivalent, sizeof(layouts), (void *)layouts, &propertySize, &layoutsEqual);

	if(noErr != result)
		return false;
		//LOGGER_ERR("org.sbooth.AudioEngine.ChannelLayout", "AudioFormatGetProperty (kAudioFormatProperty_AreChannelLayoutsEquivalent) failed: " << result);

	return layoutsEqual;
}
