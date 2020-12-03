/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import "AVAudioPCMBuffer+SFBBufferUtilities.h"

@implementation AVAudioPCMBuffer (SFBBufferUtilities)

- (AVAudioFrameCount)appendContentsOfBuffer:(AVAudioPCMBuffer *)buffer
{
	return [self copyFromBuffer:buffer readOffset:0 frameLength:buffer.frameLength writeOffset:self.frameLength];
}

- (AVAudioFrameCount)appendContentsOfBuffer:(AVAudioPCMBuffer *)buffer readOffset:(AVAudioFrameCount)readOffset
{
	return [self copyFromBuffer:buffer readOffset:readOffset frameLength:(self.frameCapacity - self.frameLength) writeOffset:self.frameLength];
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

	AVAudioFrameCount framesToCopy = MIN(self.frameCapacity - writeOffset, MIN(frameLength, buffer.frameLength - readOffset));

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

	AVAudioFrameCount framesToTrim = MIN(frameLength, self.frameLength - offset);
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

- (AVAudioFrameCount)fillRemainderWithSilence
{
	return [self insertSilenceAtOffset:self.frameLength frameLength:self.frameCapacity - self.frameLength];
}

- (AVAudioFrameCount)appendSilenceOfLength:(AVAudioFrameCount)frameLength
{
	return [self insertSilenceAtOffset:self.frameLength frameLength:frameLength];
}

- (AVAudioFrameCount)insertSilenceAtOffset:(AVAudioFrameCount)offset frameLength:(AVAudioFrameCount)frameLength
{
	if(offset > self.frameCapacity || frameLength == 0)
		return 0;

	AVAudioFrameCount framesToZero = MIN(frameLength, self.frameCapacity - offset);

	const AudioStreamBasicDescription *asbd = self.format.streamDescription;
	const AudioBufferList *abl = self.audioBufferList;

	NSAssert((asbd->mFormatFlags & kAudioFormatFlagIsFloat) || ((asbd->mFormatFlags & kAudioFormatFlagIsSignedInteger) && (asbd->mFormatFlags & kAudioFormatFlagIsPacked)), @"Inserting silence for unsigned integer or unpacked samples not supported");

	for(UInt32 i = 0; i < abl->mNumberBuffers; ++i) {
		uint8_t *dstbuf = (uint8_t *)abl->mBuffers[i].mData + (offset * asbd->mBytesPerFrame);
		memset(dstbuf, 0, framesToZero * asbd->mBytesPerFrame);
	}

	return framesToZero;
}

- (BOOL)isEmpty
{
	return self.frameLength == 0;
}

- (BOOL)isFull
{
	return self.frameLength == self.frameCapacity;
}

- (BOOL)isDigitalSilence
{
	if(self.frameLength == 0)
		return YES;

	const AudioStreamBasicDescription *asbd = self.format.streamDescription;
	const AudioBufferList *abl = self.audioBufferList;

	// Floating point
	if(asbd->mFormatFlags & kAudioFormatFlagIsFloat) {
		NSAssert(asbd->mBitsPerChannel == 32 || asbd->mBitsPerChannel == 64, @"Unsupported mBitsPerChannel %d for kAudioFormatFlagIsFloat", asbd->mBitsPerChannel);
		if(asbd->mBitsPerChannel == 32) {
			for(UInt32 i = 0; i < abl->mNumberBuffers; ++i) {
				const float *buf = (const float *)abl->mBuffers[i].mData;
				for(UInt32 sampleNumber = 0; i < abl->mBuffers[i].mDataByteSize / sizeof(float); ++sampleNumber) {
					if(buf[sampleNumber] != 0)
						return NO;
				}
			}
			return YES;
		}
		else if(asbd->mBitsPerChannel == 64) {
			for(UInt32 i = 0; i < abl->mNumberBuffers; ++i) {
				const double *buf = (const double *)abl->mBuffers[i].mData;
				for(UInt32 sampleNumber = 0; i < abl->mBuffers[i].mDataByteSize / sizeof(double); ++sampleNumber) {
					if(buf[sampleNumber] != 0)
						return NO;
				}
			}
			return YES;
		}
	}
	// Integer
	else {
		const UInt32 interleavedChannelCount = asbd->mFormatFlags & kAudioFormatFlagIsNonInterleaved ? 1 : asbd->mChannelsPerFrame;
		const UInt32 bytesPerSample = asbd->mBytesPerFrame / interleavedChannelCount;

		NSAssert(bytesPerSample == 1 || bytesPerSample == 2 || bytesPerSample == 3 || bytesPerSample == 4 || bytesPerSample == 8, @"Unsupported sample width %d", bytesPerSample);

		// Integer, packed
		if(asbd->mFormatFlags & kAudioFormatFlagIsPacked /*|| ((asbd->mBitsPerChannel / 8) * asbd->mChannelsPerFrame) == asbd->mBytesPerFrame*/) {
			switch(bytesPerSample) {
				case 1: {
					const uint8_t silence = asbd->mFormatFlags & kAudioFormatFlagIsSignedInteger ? 0 : 0x80;
					for(UInt32 i = 0; i < abl->mNumberBuffers; ++i) {
						const uint8_t *buf = (const uint8_t *)abl->mBuffers[i].mData;
						size_t sampleCount = abl->mBuffers[i].mDataByteSize / bytesPerSample;
						while(sampleCount--) {
							if(*buf != silence)
								return NO;
							++buf;
						}
					}
					return YES;
				}
				case 2: {
					const uint16_t silence = asbd->mFormatFlags & kAudioFormatFlagIsSignedInteger ? 0 : 0x8000;
					for(UInt32 i = 0; i < abl->mNumberBuffers; ++i) {
						const uint16_t *buf = (const uint16_t *)abl->mBuffers[i].mData;
						size_t sampleCount = abl->mBuffers[i].mDataByteSize / bytesPerSample;
						while(sampleCount--) {
							if(*buf != silence)
								return NO;
							++buf;
						}
					}
					return YES;
				}
				case 3: {
					const uint32_t silence = asbd->mFormatFlags & kAudioFormatFlagIsSignedInteger ? 0 : 0x800000;
					for(UInt32 i = 0; i < abl->mNumberBuffers; ++i) {
						const uint8_t *buf = (const uint8_t *)abl->mBuffers[i].mData;
						size_t sampleCount = abl->mBuffers[i].mDataByteSize / bytesPerSample;
						while(sampleCount--) {
							uint32_t sample = 0;
							if(asbd->mFormatFlags & kAudioFormatFlagIsBigEndian) {
								sample |= (*buf++ << 16) & 0xff0000;
								sample |= (*buf++ << 8) & 0xff00;
								sample |= *buf++ & 0xff;
							}
							else {
								sample |= *buf++ & 0xff;
								sample |= (*buf++ << 8) & 0xff00;
								sample |= (*buf++ << 16) & 0xff0000;
							}
							if(sample != silence)
								return NO;
						}
					}
					return YES;
				}
				case 4: {
					const uint32_t silence = asbd->mFormatFlags & kAudioFormatFlagIsSignedInteger ? 0 : 0x80000000;
					for(UInt32 i = 0; i < abl->mNumberBuffers; ++i) {
						const uint32_t *buf = (const uint32_t *)abl->mBuffers[i].mData;
						size_t sampleCount = abl->mBuffers[i].mDataByteSize / bytesPerSample;
						while(sampleCount--) {
							if(*buf != silence)
								return NO;
							++buf;
						}
					}
					return YES;
				}
				case 8: {
					const uint64_t silence = asbd->mFormatFlags & kAudioFormatFlagIsSignedInteger ? 0 : 0x8000000000000000;
					for(UInt32 i = 0; i < abl->mNumberBuffers; ++i) {
						const uint64_t *buf = (const uint64_t *)abl->mBuffers[i].mData;
						size_t sampleCount = abl->mBuffers[i].mDataByteSize / bytesPerSample;
						while(sampleCount--) {
							if(*buf != silence)
								return NO;
							++buf;
						}
					}
					return YES;
				}
			}
		}
		// Integer, unpacked
		else {
			const size_t shift = (bytesPerSample * 8) - asbd->mBitsPerChannel;
			switch(bytesPerSample) {
				case 1: {
					const uint8_t silence = asbd->mFormatFlags & kAudioFormatFlagIsSignedInteger ? 0 : (uint8_t)(1 << ((asbd->mBitsPerChannel - 1) + (asbd->mFormatFlags & kAudioFormatFlagIsAlignedHigh ? shift : 0)));
					for(UInt32 i = 0; i < abl->mNumberBuffers; ++i) {
						const uint8_t *buf = (const uint8_t *)abl->mBuffers[i].mData;
						size_t sampleCount = abl->mBuffers[i].mDataByteSize / bytesPerSample;
						while(sampleCount--) {
							if(*buf != silence)
								return NO;
							++buf;
						}
					}
					return YES;
				}
				case 2: {
					const uint16_t silence = asbd->mFormatFlags & kAudioFormatFlagIsSignedInteger ? 0 : (uint16_t)(1 << ((asbd->mBitsPerChannel - 1) + (asbd->mFormatFlags & kAudioFormatFlagIsAlignedHigh ? shift : 0)));
					for(UInt32 i = 0; i < abl->mNumberBuffers; ++i) {
						const uint16_t *buf = (const uint16_t *)abl->mBuffers[i].mData;
						size_t sampleCount = abl->mBuffers[i].mDataByteSize / bytesPerSample;
						while(sampleCount--) {
							if(*buf != silence)
								return NO;
							++buf;
						}
					}
					return YES;
				}
				case 3: {
					const uint32_t silence = asbd->mFormatFlags & kAudioFormatFlagIsSignedInteger ? 0 : (uint32_t)(1 << ((asbd->mBitsPerChannel - 1) + (asbd->mFormatFlags & kAudioFormatFlagIsAlignedHigh ? shift : 0)));
					for(UInt32 i = 0; i < abl->mNumberBuffers; ++i) {
						const uint8_t *buf = (const uint8_t *)abl->mBuffers[i].mData;
						size_t sampleCount = abl->mBuffers[i].mDataByteSize / bytesPerSample;
						while(sampleCount--) {
							uint32_t sample = 0;
							if(asbd->mFormatFlags & kAudioFormatFlagIsBigEndian) {
								sample |= (*buf++ << 16) & 0xff0000;
								sample |= (*buf++ << 8) & 0xff00;
								sample |= *buf++ & 0xff;
							}
							else {
								sample |= *buf++ & 0xff;
								sample |= (*buf++ << 8) & 0xff00;
								sample |= (*buf++ << 16) & 0xff0000;
							}
							if(sample != silence)
								return NO;
						}
					}
					return YES;
				}
				case 4: {
					const uint32_t silence = asbd->mFormatFlags & kAudioFormatFlagIsSignedInteger ? 0 : (uint32_t)(1 << ((asbd->mBitsPerChannel - 1) + (asbd->mFormatFlags & kAudioFormatFlagIsAlignedHigh ? shift : 0)));
					for(UInt32 i = 0; i < abl->mNumberBuffers; ++i) {
						const uint32_t *buf = (const uint32_t *)abl->mBuffers[i].mData;
						size_t sampleCount = abl->mBuffers[i].mDataByteSize / bytesPerSample;
						while(sampleCount--) {
							if(*buf != silence)
								return NO;
							++buf;
						}
					}
					return YES;
				}
				case 8: {
					const uint64_t silence = asbd->mFormatFlags & kAudioFormatFlagIsSignedInteger ? 0 : (uint64_t)(1 << ((asbd->mBitsPerChannel - 1) + (asbd->mFormatFlags & kAudioFormatFlagIsAlignedHigh ? shift : 0)));
					for(UInt32 i = 0; i < abl->mNumberBuffers; ++i) {
						const uint64_t *buf = (const uint64_t *)abl->mBuffers[i].mData;
						size_t sampleCount = abl->mBuffers[i].mDataByteSize / bytesPerSample;
						while(sampleCount--) {
							if(*buf != silence)
								return NO;
							++buf;
						}
					}
					return YES;
				}
			}
		}
	}

	return NO;
}

@end

