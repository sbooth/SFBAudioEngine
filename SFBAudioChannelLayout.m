/*
 * Copyright (c) 2013 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <os/log.h>

#import "SFBAudioChannelLayout.h"
#import "SFBAudioChannelLayout+Internal.h"

#import "SFBCStringForOSType.h"

/*! @brief Get the size in bytes of an \c AudioChannelLayout with the specified number of channel descriptions */
static size_t GetChannelLayoutSize(UInt32 numberChannelDescriptions)
{
	return offsetof(AudioChannelLayout, mChannelDescriptions) + (numberChannelDescriptions * sizeof(AudioChannelDescription));
}

/*!
 * @brief Allocate an \c AudioChannelLayout
 * @param numberChannelDescriptions The number of channel descriptions that will be stored in the channel layout
 * @return An \c AudioChannelLayout or \c NULL on error
 */
static AudioChannelLayout * CreateChannelLayout(UInt32 numberChannelDescriptions)
{
	size_t layoutSize = GetChannelLayoutSize(numberChannelDescriptions);
	AudioChannelLayout *channelLayout = (AudioChannelLayout *)malloc(layoutSize);
	if(channelLayout == NULL)
		return NULL;

	memset(channelLayout, 0, layoutSize);

	return channelLayout;
}

/*! @brief Create a copy of \c rhs */
static AudioChannelLayout * CopyChannelLayout(const AudioChannelLayout *rhs)
{
	if(rhs == NULL)
		return NULL;

	size_t layoutSize = GetChannelLayoutSize(rhs->mNumberChannelDescriptions);
	AudioChannelLayout *channelLayout = (AudioChannelLayout *)malloc(layoutSize);
	if(channelLayout == NULL)
		return NULL;

	memcpy(channelLayout, rhs, layoutSize);

	return channelLayout;
}

/*! @brief Get the string representation of an \c AudioChannelLayoutTag */
static const char * GetChannelLayoutTagName(AudioChannelLayoutTag layoutTag)
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

		default:											return NULL;
	}
}

/*! @brief Get the string representation of an \c AudioChannelLabel */
static const char * GetChannelLabelName(AudioChannelLabel label)
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

		default:											return NULL;
	}
}

@implementation SFBAudioChannelLayout

static SFBAudioChannelLayout *_mono;
static SFBAudioChannelLayout *_stereo;

+ (SFBAudioChannelLayout *)mono
{
	static dispatch_once_t onceToken;
	dispatch_once(&onceToken, ^{
		_mono = [[SFBAudioChannelLayout alloc] initWithTag:kAudioChannelLayoutTag_Mono];
	});
	return _mono;
}

+ (SFBAudioChannelLayout *)stereo
{
	static dispatch_once_t onceToken;
	dispatch_once(&onceToken, ^{
		_stereo = [[SFBAudioChannelLayout alloc] initWithTag:kAudioChannelLayoutTag_Stereo];
	});
	return _stereo;
}

- (instancetype)initWithTag:(AudioChannelLayoutTag)tag
{
	if((self = [super init])) {
		_layout = CreateChannelLayout(0);
		_layout->mChannelLayoutTag = tag;
	}
	return self;
}

- (instancetype)initWithBitmap:(AudioChannelBitmap)bitmap
{
	if((self = [super init])) {
		_layout = CreateChannelLayout(0);
		_layout->mChannelBitmap = bitmap;
	}
	return self;
}

- (instancetype)initWithLabels:(NSArray<NSNumber *> *)labels
{
	NSParameterAssert(labels != nil);
	NSParameterAssert(labels.count > 0);

	if((self = [super init])) {
		_layout = CreateChannelLayout((UInt32)labels.count);

		_layout->mChannelLayoutTag = kAudioChannelLayoutTag_UseChannelDescriptions;
		_layout->mChannelBitmap = 0;

		_layout->mNumberChannelDescriptions = (UInt32)labels.count;

		for(NSUInteger i = 0; i < labels.count; ++i)
			_layout->mChannelDescriptions[i].mChannelLabel = [[labels objectAtIndex:i] unsignedIntValue];
	}
	return self;
}

- (instancetype)initWithLayout:(const AudioChannelLayout *)layout
{
	NSParameterAssert(layout != NULL);

	if((self = [super init]))
		_layout = CopyChannelLayout(layout);
	return self;
}

- (void)dealloc
{
	free(_layout);
}

- (nonnull id)copyWithZone:(nullable NSZone *)zone
{
#pragma unused(zone)
	return self;
//	SFBAudioChannelLayout *result = [[[self class] alloc] init];
//	result->_layout = CopyChannelLayout(_layout);
//	return result;
}

- (BOOL)isEqual:(id)object
{
	if(![object isKindOfClass:[SFBAudioChannelLayout class]])
		return NO;

	SFBAudioChannelLayout *other = (SFBAudioChannelLayout *)object;
	return [self isEquivalentToLayout:other->_layout];
}

- (BOOL)isEquivalentToLayout:(const AudioChannelLayout *)layout
{
	// Two empty channel layouts are considered equivalent
	if(!_layout && !layout)
		return YES;

	if(!_layout || !layout)
		return NO;

	const AudioChannelLayout *layouts [] = { _layout, layout };

	UInt32 layoutsEqual = false;
	UInt32 propertySize = sizeof(layoutsEqual);
	OSStatus result = AudioFormatGetProperty(kAudioFormatProperty_AreChannelLayoutsEquivalent, sizeof(layouts), (void *)layouts, &propertySize, &layoutsEqual);

	if(noErr != result) {
		os_log_error(OS_LOG_DEFAULT, "AudioFormatGetProperty (kAudioFormatProperty_AreChannelLayoutsEquivalent) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
		return NO;
	}

	return layoutsEqual != 0;
}

- (NSUInteger)hash
{
	if(!_layout)
		return 0;

	UInt32 hash = 0;
	UInt32 propertySize = sizeof(hash);
	OSStatus result = AudioFormatGetProperty(kAudioFormatProperty_ChannelLayoutHash, (UInt32)GetChannelLayoutSize(_layout->mNumberChannelDescriptions), _layout, &propertySize, &hash);

	if(result != noErr) {
		os_log_error(OS_LOG_DEFAULT, "AudioFormatGetProperty (kAudioFormatProperty_ChannelLayoutHash) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
		return 0;
	}

	return hash;
}

- (NSInteger)channelCount
{
	if(!_layout)
		return 0;

	UInt32 channelCount = 0;
	UInt32 propertySize = sizeof(channelCount);
	OSStatus result = AudioFormatGetProperty(kAudioFormatProperty_NumberOfChannelsForLayout, (UInt32)GetChannelLayoutSize(_layout->mNumberChannelDescriptions), _layout, &propertySize, &channelCount);

	if(result != noErr) {
		os_log_error(OS_LOG_DEFAULT, "AudioFormatGetProperty (kAudioFormatProperty_NumberOfChannelsForLayout) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
		return 0;
	}

	return channelCount;
}

- (BOOL)isMono
{
	AudioChannelLayout layout = {0};
	layout.mChannelLayoutTag = kAudioChannelLayoutTag_Mono;
	return [self isEquivalentToLayout:&layout];
}

- (BOOL)isStereo
{
	AudioChannelLayout layout = {0};
	layout.mChannelLayoutTag = kAudioChannelLayoutTag_Stereo;
	return [self isEquivalentToLayout:&layout];
}

- (NSArray *)mapToLayout:(SFBAudioChannelLayout *)layout
{
	NSParameterAssert(layout != nil);

	// No valid map exists for empty/unknown layouts
	if(!_layout || !layout->_layout)
		return nil;

	const AudioChannelLayout *layouts [] = {
		_layout,
		layout->_layout
	};

	NSInteger outputChannelCount = layout.channelCount;
	if(0 == outputChannelCount)
		return nil;

	SInt32 rawChannelMap [outputChannelCount];
	UInt32 propertySize = (UInt32)sizeof(rawChannelMap);
	OSStatus result = AudioFormatGetProperty(kAudioFormatProperty_ChannelMap, sizeof(layouts), (void *)layouts, &propertySize, &rawChannelMap);

	if(noErr != result) {
		os_log_error(OS_LOG_DEFAULT, "AudioFormatGetProperty (kAudioFormatProperty_ChannelMap) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
		return nil;
	}

	NSMutableArray *channelMap = [NSMutableArray arrayWithCapacity:(NSUInteger)outputChannelCount];
	for(NSInteger i = 0; i < outputChannelCount; ++i)
		[channelMap addObject:@(rawChannelMap[i])];

	return channelMap;
}

- (NSString *)description
{
	if(!_layout)
		return @"";

	CFStringRef simpleName = NULL;
	UInt32 propertySize = sizeof(simpleName);
	OSStatus result = AudioFormatGetProperty(kAudioFormatProperty_ChannelLayoutSimpleName, (UInt32)GetChannelLayoutSize(_layout->mNumberChannelDescriptions), _layout, &propertySize, &simpleName);

	if(result != noErr) {
		os_log_error(OS_LOG_DEFAULT, "AudioFormatGetProperty (kAudioFormatProperty_ChannelLayoutSimpleName) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
		return @"";
	}

	return (__bridge_transfer NSString *)simpleName;
}

- (NSString *)debugDescription
{
	if(!_layout)
		return @"(null)";

	NSMutableString *result = [NSMutableString string];

	if(_layout->mChannelLayoutTag == kAudioChannelLayoutTag_UseChannelBitmap)
		[result appendFormat:@"Channel bitmap: 0x%0.8x", _layout->mChannelBitmap];
	else if(_layout->mChannelLayoutTag == kAudioChannelLayoutTag_UseChannelDescriptions){
		[result appendFormat:@"%u channel descriptions: [", _layout->mNumberChannelDescriptions];

		const AudioChannelDescription *desc = _layout->mChannelDescriptions;
		for(UInt32 i = 0; i < _layout->mNumberChannelDescriptions; ++i, ++desc) {
			if(desc->mChannelLabel == kAudioChannelLabel_UseCoordinates)
				[result appendFormat:@"%u. Coordinates = (%f, %f, %f), flags = 0x%0.8x", i, desc->mCoordinates[0], desc->mCoordinates[1], desc->mCoordinates[2], desc->mChannelFlags];
			else
				[result appendFormat:@"%u. Label = %s (0x%0.8x)", i, GetChannelLabelName(desc->mChannelLabel), desc->mChannelLabel];
			if(i < _layout->mNumberChannelDescriptions - 1)
				[result appendString:@", "];
		}

		[result appendString:@"]"];
	}
	else
		[result appendFormat:@"%s (0x%0.8x)", GetChannelLayoutTagName(_layout->mChannelLayoutTag), _layout->mChannelLayoutTag];

	return result;
}

@end
