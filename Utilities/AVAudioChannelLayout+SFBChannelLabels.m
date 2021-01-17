/*
 * Copyright (c) 2013 - 2021 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import "AVAudioChannelLayout+SFBChannelLabels.h"

static size_t GetChannelLayoutSize(UInt32 numberChannelDescriptions)
{
	return offsetof(AudioChannelLayout, mChannelDescriptions) + (numberChannelDescriptions * sizeof(AudioChannelDescription));
}

static AudioChannelLayout * CreateChannelLayout(UInt32 numberChannelDescriptions)
{
	size_t layoutSize = GetChannelLayoutSize(numberChannelDescriptions);
	AudioChannelLayout *channelLayout = (AudioChannelLayout *)malloc(layoutSize);
	if(!channelLayout)
		return NULL;

	memset(channelLayout, 0, layoutSize);

	return channelLayout;
}

static AudioChannelLabel ChannelLabelForString(NSString *s)
{
	static NSDictionary *labels;
	static dispatch_once_t onceToken;
	dispatch_once(&onceToken, ^{
		labels = [NSMutableDictionary dictionary];

		[(NSMutableDictionary *)labels setObject:@(kAudioChannelLabel_Left) 					forKey:@"l"];
		[(NSMutableDictionary *)labels setObject:@(kAudioChannelLabel_Right) 					forKey:@"r"];
		[(NSMutableDictionary *)labels setObject:@(kAudioChannelLabel_Center) 					forKey:@"c"];
		[(NSMutableDictionary *)labels setObject:@(kAudioChannelLabel_LFEScreen) 				forKey:@"lfe"];
		[(NSMutableDictionary *)labels setObject:@(kAudioChannelLabel_LeftSurround) 			forKey:@"ls"];
		[(NSMutableDictionary *)labels setObject:@(kAudioChannelLabel_RightSurround) 			forKey:@"rs"];
		[(NSMutableDictionary *)labels setObject:@(kAudioChannelLabel_LeftCenter) 				forKey:@"lc"];
		[(NSMutableDictionary *)labels setObject:@(kAudioChannelLabel_RightCenter) 				forKey:@"rc"];
		[(NSMutableDictionary *)labels setObject:@(kAudioChannelLabel_CenterSurround) 			forKey:@"cs"];
		[(NSMutableDictionary *)labels setObject:@(kAudioChannelLabel_LeftSurroundDirect) 		forKey:@"lsd"];
		[(NSMutableDictionary *)labels setObject:@(kAudioChannelLabel_RightSurroundDirect) 		forKey:@"rsd"];
		[(NSMutableDictionary *)labels setObject:@(kAudioChannelLabel_TopCenterSurround) 		forKey:@"tcs"];
		[(NSMutableDictionary *)labels setObject:@(kAudioChannelLabel_VerticalHeightLeft) 		forKey:@"vhl"];
		[(NSMutableDictionary *)labels setObject:@(kAudioChannelLabel_VerticalHeightCenter) 	forKey:@"vhc"];
		[(NSMutableDictionary *)labels setObject:@(kAudioChannelLabel_VerticalHeightRight) 		forKey:@"vhr"];

		[(NSMutableDictionary *)labels setObject:@(kAudioChannelLabel_TopBackLeft) 				forKey:@"tbl"];
		[(NSMutableDictionary *)labels setObject:@(kAudioChannelLabel_TopBackCenter) 			forKey:@"tbc"];
		[(NSMutableDictionary *)labels setObject:@(kAudioChannelLabel_TopBackRight) 			forKey:@"tbr"];

		[(NSMutableDictionary *)labels setObject:@(kAudioChannelLabel_RearSurroundLeft) 		forKey:@"rls"];
		[(NSMutableDictionary *)labels setObject:@(kAudioChannelLabel_RearSurroundRight) 		forKey:@"rrs"];

		[(NSMutableDictionary *)labels setObject:@(kAudioChannelLabel_LeftWide) 				forKey:@"lw"];
		[(NSMutableDictionary *)labels setObject:@(kAudioChannelLabel_RightWide) 				forKey:@"rw"];
	});

	NSNumber *label = [labels objectForKey:s];
	if(label != nil)
		return (AudioChannelLabel)label.unsignedIntValue;
	return kAudioChannelLabel_Unknown;
}

@implementation AVAudioChannelLayout (SFBChannelLabels)

+ (instancetype)layoutWithChannelLabels:(AVAudioChannelCount)count, ...
{
	va_list ap;
	va_start(ap, count);

	AVAudioChannelLayout *layout = [[AVAudioChannelLayout alloc] initWithChannelLabels:count ap:ap];

	va_end(ap);

	return layout;
}

+ (instancetype)layoutWithChannelLabels:(AudioChannelLabel *)channelLabels count:(AVAudioChannelCount)count
{
	return [[AVAudioChannelLayout alloc] initWithChannelLabels:channelLabels count:count];
}

+ (instancetype)layoutWithChannelLabelString:(NSString *)channelLabelString
{
	return [[AVAudioChannelLayout alloc] initWithChannelLabelString:channelLabelString];
}

- (instancetype)initWithChannelLabels:(AVAudioChannelCount)count, ...
{
	va_list ap;
	va_start(ap, count);

	self = [self initWithChannelLabels:count ap:ap];

	va_end(ap);

	return self;
}

- (instancetype)initWithChannelLabels:(AVAudioChannelCount)count ap:(va_list)ap
{
	NSParameterAssert(count > 0);
	NSParameterAssert(ap != NULL);

	AudioChannelLayout *channelLayout = CreateChannelLayout(count);
	if(!channelLayout)
		return nil;

	channelLayout->mChannelLayoutTag = kAudioChannelLayoutTag_UseChannelDescriptions;
	channelLayout->mNumberChannelDescriptions = count;

	for(AVAudioChannelCount i = 0; i < count; ++i)
		channelLayout->mChannelDescriptions[i].mChannelLabel = va_arg(ap, AudioChannelLabel);

	self = [self initWithLayout:channelLayout];
	free(channelLayout);
	return self;
}

- (instancetype)initWithChannelLabels:(AudioChannelLabel *)channelLabels count:(AVAudioChannelCount)count
{
	NSParameterAssert(channelLabels != NULL);
	NSParameterAssert(count > 0);

	AudioChannelLayout *channelLayout = CreateChannelLayout(count);
	if(!channelLayout)
		return nil;

	channelLayout->mChannelLayoutTag = kAudioChannelLayoutTag_UseChannelDescriptions;
	channelLayout->mNumberChannelDescriptions = count;

	for(AVAudioChannelCount i = 0; i < count; ++i)
		channelLayout->mChannelDescriptions[i].mChannelLabel = channelLabels[i];

	self = [self initWithLayout:channelLayout];
	free(channelLayout);
	return self;
}

- (instancetype)initWithChannelLabelString:(NSString *)channelLabelString
{
	NSParameterAssert(channelLabelString != nil);

	NSArray *channelLabels = [[channelLabelString lowercaseString] componentsSeparatedByString:@" "];

	AVAudioChannelCount count = (AVAudioChannelCount)channelLabels.count;
	AudioChannelLayout *channelLayout = CreateChannelLayout(count);
	if(!channelLayout)
		return nil;

	channelLayout->mChannelLayoutTag = kAudioChannelLayoutTag_UseChannelDescriptions;
	channelLayout->mNumberChannelDescriptions = count;

	for(AVAudioChannelCount i = 0; i < count; ++i)
		channelLayout->mChannelDescriptions[i].mChannelLabel = ChannelLabelForString([channelLabels objectAtIndex:i]);

	self = [self initWithLayout:channelLayout];
	free(channelLayout);
	return self;
}

@end
