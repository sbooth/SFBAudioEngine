/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import "AVAudioPCMBuffer+SFBBufferUtilities.h"

static inline AVAudioFrameCount SFB_min(AVAudioFrameCount a, AVAudioFrameCount b) { return a < b ? a : b; }

@implementation AVAudioPCMBuffer (SFBBufferUtilities)

- (AVAudioFrameCount)appendContentsOfBuffer:(AVAudioPCMBuffer *)buffer
{
	return [self copyFromBuffer:buffer readOffset:0 frameLength:buffer.frameLength writeOffset:self.frameLength];
}

- (AVAudioFrameCount)appendContentsOfBuffer:(AVAudioPCMBuffer *)buffer readOffset:(AVAudioFrameCount)readOffset frameLength:(AVAudioFrameCount)frameLength
{
	return [self copyFromBuffer:buffer readOffset:readOffset frameLength:frameLength writeOffset:self.frameLength];
}

- (AVAudioFrameCount)copyFromBuffer:(AVAudioPCMBuffer *)buffer readOffset:(AVAudioFrameCount)readOffset frameLength:(AVAudioFrameCount)frameLength
{
	return [self copyFromBuffer:buffer readOffset:readOffset frameLength:frameLength writeOffset:0];
}

- (AVAudioFrameCount)copyFromBuffer:(AVAudioPCMBuffer *)buffer readOffset:(AVAudioFrameCount)readOffset frameLength:(AVAudioFrameCount)frameLength writeOffset:(AVAudioFrameCount)writeOffset
{
	NSParameterAssert(buffer != nil);
	NSParameterAssert([self.format isEqual:buffer.format]);

	if(readOffset > buffer.frameLength || writeOffset > self.frameLength || frameLength == 0 || buffer.frameLength == 0)
		return 0;

	AVAudioFrameCount framesToCopy = SFB_min(self.frameCapacity - writeOffset, SFB_min(frameLength, buffer.frameLength - readOffset));

	const AudioStreamBasicDescription *asbd = self.format.streamDescription;
	const AudioBufferList *src_abl = buffer.audioBufferList;
	const AudioBufferList *dst_abl = self.audioBufferList;

	for(UInt32 i = 0; i < src_abl->mNumberBuffers; ++i) {
		const unsigned char *srcbuf = (unsigned char *)src_abl->mBuffers[i].mData + (readOffset * asbd->mBytesPerFrame);
		unsigned char *dstbuf = (unsigned char *)dst_abl->mBuffers[i].mData + (writeOffset * asbd->mBytesPerFrame);
		memcpy(dstbuf, srcbuf, framesToCopy * asbd->mBytesPerFrame);
	}

	self.frameLength += framesToCopy;

	return framesToCopy;
}

- (AVAudioFrameCount)trimAtOffset:(AVAudioFrameCount)offset frameLength:(AVAudioFrameCount)frameLength
{
	if(offset > self.frameLength || frameLength == 0)
		return 0;

	AVAudioFrameCount framesToTrim = SFB_min(frameLength, self.frameLength - offset);
	AVAudioFrameCount moveOffset = offset + framesToTrim;
	AVAudioFrameCount framesToMove = self.frameLength - moveOffset;

	const AudioStreamBasicDescription *asbd = self.format.streamDescription;
	const AudioBufferList *abl = self.audioBufferList;

	for(UInt32 i = 0; i < abl->mNumberBuffers; ++i) {
		const unsigned char *srcbuf = (unsigned char *)abl->mBuffers[i].mData + (moveOffset * asbd->mBytesPerFrame);
		unsigned char *dstbuf = (unsigned char *)abl->mBuffers[i].mData + (offset * asbd->mBytesPerFrame);
		memmove(dstbuf, srcbuf, framesToMove * asbd->mBytesPerFrame);
	}

	self.frameLength -= framesToTrim;

	return framesToMove;
}

@end

