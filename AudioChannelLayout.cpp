/*
 * Copyright (c) 2013 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#include <cstdlib>
#include <functional>
#include <stdexcept>

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

	/*! @brief Get the string representation of an \c AudioChannelLayoutTag */
	const char * GetChannelLayoutTagName(AudioChannelLayoutTag layoutTag)
	{
		switch(layoutTag) {
			case kAudioChannelLayoutTag_Mono:					return "kAudioChannelLayoutTag_Mono";
			case kAudioChannelLayoutTag_Stereo:					return "kAudioChannelLayoutTag_Stereo";
			case kAudioChannelLayoutTag_StereoHeadphones:		return "kAudioChannelLayoutTag_StereoHeadphones";
			case kAudioChannelLayoutTag_MatrixStereo:			return "kAudioChannelLayoutTag_MatrixStereo";
			case kAudioChannelLayoutTag_MidSide:				return "kAudioChannelLayoutTag_MidSide";
			case kAudioChannelLayoutTag_XY:						return "kAudioChannelLayoutTag_XY";
			case kAudioChannelLayoutTag_Binaural:				return "kAudioChannelLayoutTag_Binaural";
			case kAudioChannelLayoutTag_Ambisonic_B_Format:		return "kAudioChannelLayoutTag_Ambisonic_B_Format";
			case kAudioChannelLayoutTag_Quadraphonic:			return "kAudioChannelLayoutTag_Quadraphonic";
			case kAudioChannelLayoutTag_Pentagonal:				return "kAudioChannelLayoutTag_Pentagonal";
			case kAudioChannelLayoutTag_Hexagonal:				return "kAudioChannelLayoutTag_Hexagonal";
			case kAudioChannelLayoutTag_Octagonal:				return "kAudioChannelLayoutTag_Octagonal";
			case kAudioChannelLayoutTag_Cube:					return "kAudioChannelLayoutTag_Cube";
			case kAudioChannelLayoutTag_MPEG_3_0_A:				return "kAudioChannelLayoutTag_MPEG_3_0_A";
			case kAudioChannelLayoutTag_MPEG_3_0_B:				return "kAudioChannelLayoutTag_MPEG_3_0_B";
			case kAudioChannelLayoutTag_MPEG_4_0_A:				return "kAudioChannelLayoutTag_MPEG_4_0_A";
			case kAudioChannelLayoutTag_MPEG_4_0_B:				return "kAudioChannelLayoutTag_MPEG_4_0_B";
			case kAudioChannelLayoutTag_MPEG_5_0_A:				return "kAudioChannelLayoutTag_MPEG_5_0_A";
			case kAudioChannelLayoutTag_MPEG_5_0_B:				return "kAudioChannelLayoutTag_MPEG_5_0_B";
			case kAudioChannelLayoutTag_MPEG_5_0_C:				return "kAudioChannelLayoutTag_MPEG_5_0_C";
			case kAudioChannelLayoutTag_MPEG_5_0_D:				return "kAudioChannelLayoutTag_MPEG_5_0_D";
			case kAudioChannelLayoutTag_MPEG_5_1_A:				return "kAudioChannelLayoutTag_MPEG_5_1_A";
			case kAudioChannelLayoutTag_MPEG_5_1_B:				return "kAudioChannelLayoutTag_MPEG_5_1_B";
			case kAudioChannelLayoutTag_MPEG_5_1_C:				return "kAudioChannelLayoutTag_MPEG_5_1_C";
			case kAudioChannelLayoutTag_MPEG_5_1_D:				return "kAudioChannelLayoutTag_MPEG_5_1_D";
			case kAudioChannelLayoutTag_MPEG_6_1_A:				return "kAudioChannelLayoutTag_MPEG_6_1_A";
			case kAudioChannelLayoutTag_MPEG_7_1_A:				return "kAudioChannelLayoutTag_MPEG_7_1_A";
			case kAudioChannelLayoutTag_MPEG_7_1_B:				return "kAudioChannelLayoutTag_MPEG_7_1_B";
			case kAudioChannelLayoutTag_MPEG_7_1_C:				return "kAudioChannelLayoutTag_MPEG_7_1_C";
			case kAudioChannelLayoutTag_Emagic_Default_7_1:		return "kAudioChannelLayoutTag_Emagic_Default_7_1";
			case kAudioChannelLayoutTag_SMPTE_DTV:				return "kAudioChannelLayoutTag_SMPTE_DTV";
			case kAudioChannelLayoutTag_ITU_2_1:				return "kAudioChannelLayoutTag_ITU_2_1";
			case kAudioChannelLayoutTag_ITU_2_2:				return "kAudioChannelLayoutTag_ITU_2_2";
			case kAudioChannelLayoutTag_DVD_4:					return "kAudioChannelLayoutTag_DVD_4";
			case kAudioChannelLayoutTag_DVD_5:					return "kAudioChannelLayoutTag_DVD_5";
			case kAudioChannelLayoutTag_DVD_6:					return "kAudioChannelLayoutTag_DVD_6";
			case kAudioChannelLayoutTag_DVD_10:					return "kAudioChannelLayoutTag_DVD_10";
			case kAudioChannelLayoutTag_DVD_11:					return "kAudioChannelLayoutTag_DVD_11";
			case kAudioChannelLayoutTag_DVD_18:					return "kAudioChannelLayoutTag_DVD_18";
			case kAudioChannelLayoutTag_AudioUnit_6_0:			return "kAudioChannelLayoutTag_AudioUnit_6_0";
			case kAudioChannelLayoutTag_AudioUnit_7_0:			return "kAudioChannelLayoutTag_AudioUnit_7_0";
			case kAudioChannelLayoutTag_AudioUnit_7_0_Front:	return "kAudioChannelLayoutTag_AudioUnit_7_0_Front";
			case kAudioChannelLayoutTag_AAC_6_0:				return "kAudioChannelLayoutTag_AAC_6_0";
			case kAudioChannelLayoutTag_AAC_6_1:				return "kAudioChannelLayoutTag_AAC_6_1";
			case kAudioChannelLayoutTag_AAC_7_0:				return "kAudioChannelLayoutTag_AAC_7_0";
			case kAudioChannelLayoutTag_AAC_Octagonal:			return "kAudioChannelLayoutTag_AAC_Octagonal";
			case kAudioChannelLayoutTag_TMH_10_2_std:			return "kAudioChannelLayoutTag_TMH_10_2_std";
			case kAudioChannelLayoutTag_TMH_10_2_full:			return "kAudioChannelLayoutTag_TMH_10_2_full";
			case kAudioChannelLayoutTag_AC3_1_0_1:				return "kAudioChannelLayoutTag_AC3_1_0_1";
			case kAudioChannelLayoutTag_AC3_3_0:				return "kAudioChannelLayoutTag_AC3_3_0";
			case kAudioChannelLayoutTag_AC3_3_1:				return "kAudioChannelLayoutTag_AC3_3_1";
			case kAudioChannelLayoutTag_AC3_3_0_1:				return "kAudioChannelLayoutTag_AC3_3_0_1";
			case kAudioChannelLayoutTag_AC3_2_1_1:				return "kAudioChannelLayoutTag_AC3_2_1_1";
			case kAudioChannelLayoutTag_AC3_3_1_1:				return "kAudioChannelLayoutTag_AC3_3_1_1";
			case kAudioChannelLayoutTag_DiscreteInOrder:		return "kAudioChannelLayoutTag_DiscreteInOrder";
			case kAudioChannelLayoutTag_Unknown:				return "kAudioChannelLayoutTag_Unknown";

			default:											return nullptr;
		}
	}

	/*! @brief Get the string representation of an \c AudioChannelLabel */
	const char * GetChannelLabelName(AudioChannelLabel label)
	{
		switch(label) {
			case kAudioChannelLabel_Unknown:					return "kAudioChannelLabel_Unknown";
			case kAudioChannelLabel_Unused:						return "kAudioChannelLabel_Unused";
			case kAudioChannelLabel_UseCoordinates:				return "kAudioChannelLabel_UseCoordinates";
			case kAudioChannelLabel_Left:						return "kAudioChannelLabel_Left";
			case kAudioChannelLabel_Right:						return "kAudioChannelLabel_Right";
			case kAudioChannelLabel_Center:						return "kAudioChannelLabel_Center";
			case kAudioChannelLabel_LFEScreen:					return "kAudioChannelLabel_LFEScreen";
			case kAudioChannelLabel_LeftSurround:				return "kAudioChannelLabel_LeftSurround";
			case kAudioChannelLabel_RightSurround:				return "kAudioChannelLabel_RightSurround";
			case kAudioChannelLabel_LeftCenter:					return "kAudioChannelLabel_LeftCenter";
			case kAudioChannelLabel_RightCenter:				return "kAudioChannelLabel_RightCenter";
			case kAudioChannelLabel_CenterSurround:				return "kAudioChannelLabel_CenterSurround";
			case kAudioChannelLabel_LeftSurroundDirect:			return "kAudioChannelLabel_LeftSurroundDirect";
			case kAudioChannelLabel_RightSurroundDirect:		return "kAudioChannelLabel_RightSurroundDirect";
			case kAudioChannelLabel_TopCenterSurround:			return "kAudioChannelLabel_TopCenterSurround";
			case kAudioChannelLabel_VerticalHeightLeft:			return "kAudioChannelLabel_VerticalHeightLeft";
			case kAudioChannelLabel_VerticalHeightCenter:		return "kAudioChannelLabel_VerticalHeightCenter";
			case kAudioChannelLabel_VerticalHeightRight:		return "kAudioChannelLabel_VerticalHeightRight";
			case kAudioChannelLabel_TopBackLeft:				return "kAudioChannelLabel_TopBackLeft";
			case kAudioChannelLabel_TopBackCenter:				return "kAudioChannelLabel_TopBackCenter";
			case kAudioChannelLabel_TopBackRight:				return "kAudioChannelLabel_TopBackRight";
			case kAudioChannelLabel_RearSurroundLeft:			return "kAudioChannelLabel_RearSurroundLeft";
			case kAudioChannelLabel_RearSurroundRight:			return "kAudioChannelLabel_RearSurroundRight";
			case kAudioChannelLabel_LeftWide:					return "kAudioChannelLabel_LeftWide";
			case kAudioChannelLabel_RightWide:					return "kAudioChannelLabel_RightWide";
			case kAudioChannelLabel_LFE2:						return "kAudioChannelLabel_LFE2";
			case kAudioChannelLabel_LeftTotal:					return "kAudioChannelLabel_LeftTotal";
			case kAudioChannelLabel_RightTotal:					return "kAudioChannelLabel_RightTotal";
			case kAudioChannelLabel_HearingImpaired:			return "kAudioChannelLabel_HearingImpaired";
			case kAudioChannelLabel_Narration:					return "kAudioChannelLabel_Narration";
			case kAudioChannelLabel_Mono:						return "kAudioChannelLabel_Mono";
			case kAudioChannelLabel_DialogCentricMix:			return "kAudioChannelLabel_DialogCentricMix";
			case kAudioChannelLabel_CenterSurroundDirect:		return "kAudioChannelLabel_CenterSurroundDirect";
			case kAudioChannelLabel_Haptic:						return "kAudioChannelLabel_Haptic";
			case kAudioChannelLabel_Ambisonic_W:				return "kAudioChannelLabel_Ambisonic_W";
			case kAudioChannelLabel_Ambisonic_X:				return "kAudioChannelLabel_Ambisonic_X";
			case kAudioChannelLabel_Ambisonic_Y:				return "kAudioChannelLabel_Ambisonic_Y";
			case kAudioChannelLabel_Ambisonic_Z:				return "kAudioChannelLabel_Ambisonic_Z";
			case kAudioChannelLabel_MS_Mid:						return "kAudioChannelLabel_MS_Mid";
			case kAudioChannelLabel_MS_Side:					return "kAudioChannelLabel_MS_Side";
			case kAudioChannelLabel_XY_X:						return "kAudioChannelLabel_XY_X";
			case kAudioChannelLabel_XY_Y:						return "kAudioChannelLabel_XY_Y";
			case kAudioChannelLabel_HeadphonesLeft:				return "kAudioChannelLabel_HeadphonesLeft";
			case kAudioChannelLabel_HeadphonesRight:			return "kAudioChannelLabel_HeadphonesRight";
			case kAudioChannelLabel_ClickTrack:					return "kAudioChannelLabel_ClickTrack";
			case kAudioChannelLabel_ForeignLanguage:			return "kAudioChannelLabel_ForeignLanguage";
			case kAudioChannelLabel_Discrete:					return "kAudioChannelLabel_Discrete";
			case kAudioChannelLabel_Discrete_0:					return "kAudioChannelLabel_Discrete_0";
			case kAudioChannelLabel_Discrete_1:					return "kAudioChannelLabel_Discrete_1";
			case kAudioChannelLabel_Discrete_2:					return "kAudioChannelLabel_Discrete_2";
			case kAudioChannelLabel_Discrete_3:					return "kAudioChannelLabel_Discrete_3";
			case kAudioChannelLabel_Discrete_4:					return "kAudioChannelLabel_Discrete_4";
			case kAudioChannelLabel_Discrete_5:					return "kAudioChannelLabel_Discrete_5";
			case kAudioChannelLabel_Discrete_6:					return "kAudioChannelLabel_Discrete_6";
			case kAudioChannelLabel_Discrete_7:					return "kAudioChannelLabel_Discrete_7";
			case kAudioChannelLabel_Discrete_8:					return "kAudioChannelLabel_Discrete_8";
			case kAudioChannelLabel_Discrete_9:					return "kAudioChannelLabel_Discrete_9";
			case kAudioChannelLabel_Discrete_10:				return "kAudioChannelLabel_Discrete_10";
			case kAudioChannelLabel_Discrete_11:				return "kAudioChannelLabel_Discrete_11";
			case kAudioChannelLabel_Discrete_12:				return "kAudioChannelLabel_Discrete_12";
			case kAudioChannelLabel_Discrete_13:				return "kAudioChannelLabel_Discrete_13";
			case kAudioChannelLabel_Discrete_14:				return "kAudioChannelLabel_Discrete_14";
			case kAudioChannelLabel_Discrete_15:				return "kAudioChannelLabel_Discrete_15";
			case kAudioChannelLabel_Discrete_65535:				return "kAudioChannelLabel_Discrete_65535";

			default:											return nullptr;
		}
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
	//os_log_error(OS_LOG_DEFAULT, "AudioFormatGetProperty (kAudioFormatProperty_NumberOfChannelsForLayout) failed: %d", result);

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
	UInt32 propertySize = (UInt32)sizeof(rawChannelMap);
	OSStatus result = AudioFormatGetProperty(kAudioFormatProperty_ChannelMap, sizeof(layouts), (void *)layouts, &propertySize, &rawChannelMap);

	if(noErr != result)
		return false;
	//os_log_error(OS_LOG_DEFAULT, "AudioFormatGetProperty (kAudioFormatProperty_ChannelMap) failed: %d", result);

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
		//os_log_error(OS_LOG_DEFAULT, "AudioFormatGetProperty (kAudioFormatProperty_AreChannelLayoutsEquivalent) failed: %d", result);

	return layoutsEqual;
}


SFB::CFString SFB::Audio::ChannelLayout::Description() const
{
	if(!mChannelLayout) {
		return CFString("(null)", kCFStringEncodingUTF8);
	}

	CFMutableString result{ CFStringCreateMutable(kCFAllocatorDefault, 0) };

	if(kAudioChannelLayoutTag_UseChannelBitmap == mChannelLayout->mChannelLayoutTag)
		CFStringAppendFormat(result, NULL, CFSTR("Channel bitmap: 0x%0.8x"), mChannelLayout->mChannelBitmap);
	else if(kAudioChannelLayoutTag_UseChannelDescriptions == mChannelLayout->mChannelLayoutTag){
		CFStringAppendFormat(result, NULL, CFSTR("%u channel descriptions: ["), mChannelLayout->mNumberChannelDescriptions);

		const AudioChannelDescription *desc = mChannelLayout->mChannelDescriptions;
		for(UInt32 i = 0; i < mChannelLayout->mNumberChannelDescriptions; ++i, ++desc) {
			if(kAudioChannelLabel_UseCoordinates == desc->mChannelLabel)
				CFStringAppendFormat(result, NULL, CFSTR("%u. Coordinates = (%f, %f, %f), flags = 0x%0.8x"), i, desc->mCoordinates[0], desc->mCoordinates[1], desc->mCoordinates[2], desc->mChannelFlags);
			else
				CFStringAppendFormat(result, NULL, CFSTR("%u. Label = %s (0x%0.8x)"), i, GetChannelLabelName(desc->mChannelLabel), desc->mChannelLabel);
			if(i < mChannelLayout->mNumberChannelDescriptions - 1)
				CFStringAppend(result, CFSTR(", "));
		}

		CFStringAppend(result, CFSTR("]"));
	}
	else
		CFStringAppendFormat(result, NULL, CFSTR("%s (0x%0.8x)"), GetChannelLayoutTagName(mChannelLayout->mChannelLayoutTag), mChannelLayout->mChannelLayoutTag);

	return CFString((CFStringRef)result.Relinquish());
}
