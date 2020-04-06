/*
 * Copyright (c) 2013 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <os/log.h>

#import <AudioToolbox/AudioToolbox.h>

#import "SFBAudioBufferList.h"
#import "SFBAudioBufferList+Internal.h"

static void DeallocateAudioBufferList(AudioBufferList *bufferList)
{
	if(bufferList) {
		for(UInt32 bufferIndex = 0; bufferIndex < bufferList->mNumberBuffers; ++bufferIndex) {
			if(bufferList->mBuffers[bufferIndex].mData)
				free(bufferList->mBuffers[bufferIndex].mData);
		}

		free(bufferList);
	}
}

static AudioBufferList * AllocateAudioBufferList(SFBAudioFormat *format, NSInteger capacityFrames)
{
	NSCParameterAssert(capacityFrames > 0);

	UInt32 numBuffers = format.isInterleaved ? 1 : format.streamDescription->mChannelsPerFrame;
	UInt32 channelsPerBuffer = format.isInterleaved ? format.streamDescription->mChannelsPerFrame : 1;

	AudioBufferList *bufferList = calloc(1, offsetof(AudioBufferList, mBuffers) + (sizeof(AudioBuffer) * numBuffers));
	if(!bufferList)
		return NULL;

	bufferList->mNumberBuffers = numBuffers;

	for(UInt32 bufferIndex = 0; bufferIndex < bufferList->mNumberBuffers; ++bufferIndex) {
		void *data = calloc(1, (size_t)[format frameCountToByteCount:capacityFrames]);
		if(!data) {
			DeallocateAudioBufferList(bufferList);
			return NULL;
		}

		bufferList->mBuffers[bufferIndex].mData = data;
		bufferList->mBuffers[bufferIndex].mDataByteSize = (UInt32)[format frameCountToByteCount:capacityFrames];
		bufferList->mBuffers[bufferIndex].mNumberChannels = channelsPerBuffer;
	}

	return bufferList;
}

@implementation SFBAudioBufferList

- (instancetype)initWithFormat:(SFBAudioFormat *)format capacityFrames:(NSInteger)capacityFrames
{
	NSCParameterAssert(format != nil);
	NSCParameterAssert(capacityFrames > 0);

	if((self = [super init])) {
		_format = format;
		_capacityFrames = capacityFrames;
		_bufferList = AllocateAudioBufferList(_format, _capacityFrames);
		if(!_bufferList)
			return nil;
	}
	return self;
}

- (void)dealloc
{
	DeallocateAudioBufferList(_bufferList);
}

- (NSInteger)framesInBuffer
{
	return [_format byteCountToFrameCount:_bufferList->mBuffers[0].mDataByteSize];
}

- (void)reset
{
	for(UInt32 bufferIndex = 0; bufferIndex < _bufferList->mNumberBuffers; ++bufferIndex)
		_bufferList->mBuffers[bufferIndex].mDataByteSize = (UInt32)[_format frameCountToByteCount:_capacityFrames];
}

- (void)empty
{
	for(UInt32 bufferIndex = 0; bufferIndex < _bufferList->mNumberBuffers; ++bufferIndex)
		_bufferList->mBuffers[bufferIndex].mDataByteSize = 0;
}

@end
