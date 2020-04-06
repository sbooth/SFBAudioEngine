/*
 * Copyright (c) 2014 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <os/log.h>

#import <AudioToolbox/AudioToolbox.h>

#import "SFBAudioFormat.h"
#import "SFBAudioFormat+Internal.h"

#import "SFBCStringForOSType.h"

// Bob Jenkins' one-at-a-time hash
static NSUInteger OneAtATimeHash(const void *key, size_t len)
{
	const unsigned char *p = key;
	NSUInteger h = 0;

	for(size_t i = 0; i < len; i++) {
		h += p[i];
		h += (h << 10);
		h ^= (h >> 6);
	}

	h += (h << 3);
	h ^= (h >> 11);
	h += (h << 15);

	return h;
}

static AudioFormatFlags CalculateLPCMFlags(UInt32 validBitsPerChannel, UInt32 totalBitsPerChannel, BOOL isFloat, BOOL isBigEndian, BOOL isNonInterleaved)
{
	return (isFloat ? kAudioFormatFlagIsFloat : kAudioFormatFlagIsSignedInteger) | (isBigEndian ? ((UInt32)kAudioFormatFlagIsBigEndian) : 0) | ((validBitsPerChannel == totalBitsPerChannel) ? kAudioFormatFlagIsPacked : kAudioFormatFlagIsAlignedHigh) | (isNonInterleaved ? ((UInt32)kAudioFormatFlagIsNonInterleaved) : 0);
}

static void FillOutASBDForLPCM(AudioStreamBasicDescription *asbd, Float64 sampleRate, UInt32 channelsPerFrame, UInt32 validBitsPerChannel, UInt32 totalBitsPerChannel, BOOL isFloat, BOOL isBigEndian, BOOL isNonInterleaved)
{
	asbd->mFormatID = kAudioFormatLinearPCM;
	asbd->mFormatFlags = CalculateLPCMFlags(validBitsPerChannel, totalBitsPerChannel, isFloat, isBigEndian, isNonInterleaved);

	asbd->mSampleRate = sampleRate;
	asbd->mChannelsPerFrame = channelsPerFrame;
	asbd->mBitsPerChannel = validBitsPerChannel;

	asbd->mBytesPerPacket = (isNonInterleaved ? 1 : channelsPerFrame) * (totalBitsPerChannel / 8);
	asbd->mFramesPerPacket = 1;
	asbd->mBytesPerFrame = (isNonInterleaved ? 1 : channelsPerFrame) * (totalBitsPerChannel / 8);
}

@implementation SFBAudioFormat

- (instancetype)initWithCommonPCMFormat:(SFBAudioFormatCommonPCMFormat)format sampleRate:(double)sampleRate channels:(NSInteger)channels interleaved:(BOOL)interleaved
{
	NSParameterAssert(sampleRate > 0);
	NSParameterAssert(channels > 0);

	AudioStreamBasicDescription asbd = {0};
	switch(format) {
		case SFBAudioFormatCommonPCMFormatFloat32:
			FillOutASBDForLPCM(&asbd, sampleRate, (UInt32)channels, 32, 32, YES, kAudioFormatFlagIsBigEndian == kAudioFormatFlagsNativeEndian, !interleaved);
			break;
		case SFBAudioFormatCommonPCMFormatFloat64:
			FillOutASBDForLPCM(&asbd, sampleRate, (UInt32)channels, 64, 64, YES, kAudioFormatFlagIsBigEndian == kAudioFormatFlagsNativeEndian, !interleaved);
			break;
		case SFBAudioFormatCommonPCMFormatInt16:
			FillOutASBDForLPCM(&asbd, sampleRate, (UInt32)channels, 16, 16, NO, kAudioFormatFlagIsBigEndian == kAudioFormatFlagsNativeEndian, !interleaved);
			break;
		case SFBAudioFormatCommonPCMFormatInt32:
			FillOutASBDForLPCM(&asbd, sampleRate, (UInt32)channels, 32, 32, NO, kAudioFormatFlagIsBigEndian == kAudioFormatFlagsNativeEndian, !interleaved);
			break;
		default:
			return nil;
	}
	return [self initWithStreamDescription:asbd];
}

- (instancetype)initWithStreamDescription:(AudioStreamBasicDescription)streamDescription
{
	return [self initWithStreamDescription:streamDescription channelLayout:nil];
}

- (instancetype)initWithStreamDescription:(AudioStreamBasicDescription)streamDescription channelLayout:(SFBAudioChannelLayout *)channelLayout
{
//	NSAssert(!channelLayout || streamDescription.mChannelsPerFrame == channelLayout.channelCount, @"Channel count mismatch");
//	NSAssert(channelLayout || (!channelLayout && streamDescription.mChannelsPerFrame <= 2), @"Channel map required for > 2 audio channels");

	if(!channelLayout && streamDescription.mChannelsPerFrame == 1)
		channelLayout = SFBAudioChannelLayout.mono;
	else if(!channelLayout && streamDescription.mChannelsPerFrame == 2)
		channelLayout = SFBAudioChannelLayout.stereo;

	if((self = [super init])) {
		_streamDescription = streamDescription;
		_channelLayout = channelLayout;
	}
	return self;
}

- (nonnull id)copyWithZone:(nullable NSZone *)zone
{
#pragma unused(zone)
	return self;
//	SFBAudioFormat *result = [[[self class] alloc] init];
//	result->_format = _format;
//	return result;
}

- (BOOL)isEqual:(id)object
{
	if(![object isKindOfClass:[SFBAudioFormat class]])
		return NO;

	SFBAudioFormat *other = (SFBAudioFormat *)object;
	return memcmp(&_streamDescription, &other->_streamDescription, sizeof(AudioStreamBasicDescription)) == 0 && [_channelLayout isEqual:other->_channelLayout];
}

- (NSUInteger)hash
{
	return OneAtATimeHash(&_streamDescription, sizeof(AudioStreamBasicDescription)) ^ [_channelLayout hash];
}

- (BOOL)isInterleaved
{
	return !(_streamDescription.mFormatFlags & kAudioFormatFlagIsNonInterleaved);
}

- (BOOL)isPCM
{
	return _streamDescription.mFormatID == kAudioFormatLinearPCM;
}

- (BOOL)isDSD
{
	return _streamDescription.mFormatID == SFBAudioFormatIDDirectStreamDigital;
}

- (BOOL)isDoP
{
	return _streamDescription.mFormatID == SFBAudioFormatIDDoP;
}

- (BOOL)isBigEndian
{
	return (_streamDescription.mFormatFlags & kAudioFormatFlagIsBigEndian) == kAudioFormatFlagIsBigEndian;
}

- (BOOL)isNativeEndian
{
	return (_streamDescription.mFormatFlags & kAudioFormatFlagIsBigEndian) == kAudioFormatFlagsNativeEndian;
}

- (NSInteger)channelCount
{
	return _streamDescription.mChannelsPerFrame;
}

- (double)sampleRate
{
	return _streamDescription.mSampleRate;
}

- (const AudioStreamBasicDescription *)streamDescription
{
	return &_streamDescription;
}

- (NSInteger)frameCountToByteCount:(NSInteger)frameCount
{
	switch(_streamDescription.mFormatID) {
		case SFBAudioFormatIDDirectStreamDigital:
			return frameCount / 8;

		case SFBAudioFormatIDDoP:
		case kAudioFormatLinearPCM:
			return frameCount * _streamDescription.mBytesPerFrame;

		default:
			return 0;
	}
}

- (NSInteger)byteCountToFrameCount:(NSInteger)byteCount
{
	switch(_streamDescription.mFormatID) {
		case SFBAudioFormatIDDirectStreamDigital:
			return byteCount * 8;

		case SFBAudioFormatIDDoP:
		case kAudioFormatLinearPCM:
			return byteCount / _streamDescription.mBytesPerFrame;

		default:
			return 0;
	}
}

- (NSString *)description
{
	switch(_streamDescription.mFormatID) {
		case SFBAudioFormatIDDoP:
			return [NSString stringWithFormat:@"DSD over PCM, %u channels, %u Hz", _streamDescription.mChannelsPerFrame, (unsigned int)_streamDescription.mSampleRate];
		case SFBAudioFormatIDDirectStreamDigital:
			return [NSString stringWithFormat:@"Direct Stream Digital, %u channels, %u Hz", _streamDescription.mChannelsPerFrame, (unsigned int)_streamDescription.mSampleRate];
		case SFBAudioFormatIDFLAC:
			return [NSString stringWithFormat:@"FLAC, %u channels, %u Hz", _streamDescription.mChannelsPerFrame, (unsigned int)_streamDescription.mSampleRate];
		case SFBAudioFormatIDModule:
			return [NSString stringWithFormat:@"Module, %u channels, %u Hz", _streamDescription.mChannelsPerFrame, (unsigned int)_streamDescription.mSampleRate];
		case SFBAudioFormatIDMonkeysAudio:
			return [NSString stringWithFormat:@"Monkey's Audio, %u channels, %u Hz", _streamDescription.mChannelsPerFrame, (unsigned int)_streamDescription.mSampleRate];
		case SFBAudioFormatIDMPEG1:
			return [NSString stringWithFormat:@"MPEG-1, %u channels, %u Hz", _streamDescription.mChannelsPerFrame, (unsigned int)_streamDescription.mSampleRate];
		case SFBAudioFormatIDMusepack:
			return [NSString stringWithFormat:@"Musepack, %u channels, %u Hz", _streamDescription.mChannelsPerFrame, (unsigned int)_streamDescription.mSampleRate];
		case SFBAudioFormatIDOpus:
			return [NSString stringWithFormat:@"Ogg Opus, %u channels, %u Hz", _streamDescription.mChannelsPerFrame, (unsigned int)_streamDescription.mSampleRate];
		case SFBAudioFormatIDSpeex:
			return [NSString stringWithFormat:@"Ogg Speex, %u channels, %u Hz", _streamDescription.mChannelsPerFrame, (unsigned int)_streamDescription.mSampleRate];
		case SFBAudioFormatIDTrueAudio:
			return [NSString stringWithFormat:@"True Audio, %u channels, %u Hz", _streamDescription.mChannelsPerFrame, (unsigned int)_streamDescription.mSampleRate];
		case SFBAudioFormatIDVorbis:
			return [NSString stringWithFormat:@"Ogg Vorbis, %u channels, %u Hz", _streamDescription.mChannelsPerFrame, (unsigned int)_streamDescription.mSampleRate];
		case SFBAudioFormatIDWavPack:
			return [NSString stringWithFormat:@"WavPack, %u channels, %u Hz", _streamDescription.mChannelsPerFrame, (unsigned int)_streamDescription.mSampleRate];

		default: {
			CFStringRef description = NULL;
			UInt32 propertySize = sizeof(description);

			OSStatus result = AudioFormatGetProperty(kAudioFormatProperty_FormatName, sizeof(_streamDescription), &_streamDescription, &propertySize, &description);
			if(noErr != result) {
				os_log_error(OS_LOG_DEFAULT, "AudioFormatGetProperty (kAudioFormatProperty_FormatName) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
				return [NSString stringWithFormat:@"%u channels, %u Hz", _streamDescription.mChannelsPerFrame, (unsigned int)_streamDescription.mSampleRate];
			}

			return (__bridge_transfer NSString *)description;
		}
	}
}

// Most of this is stolen from Apple's CAStreamBasicDescription::Print()
- (NSString *)debugDescription
{
	NSMutableString *result = [NSMutableString string];

	unsigned char formatID [5];
	*(UInt32 *)formatID = OSSwapHostToBigInt32(_streamDescription.mFormatID);
	formatID[4] = '\0';

	// General description
	[result appendFormat:@"%u ch, %.2f Hz, '%.4s' (0x%0.8x) ", _streamDescription.mChannelsPerFrame, _streamDescription.mSampleRate, formatID, _streamDescription.mFormatFlags];

	if(_streamDescription.mFormatID == kAudioFormatLinearPCM) {
		// Bit depth
		UInt32 fractionalBits = (_streamDescription.mFormatFlags & kLinearPCMFormatFlagsSampleFractionMask) >> kLinearPCMFormatFlagsSampleFractionShift;
		if(fractionalBits > 0)
			[result appendFormat:@"%d.%d-bit", _streamDescription.mBitsPerChannel - fractionalBits, fractionalBits];
		else
			[result appendFormat:@"%d-bit", _streamDescription.mBitsPerChannel];

		// Endianness
		bool isInterleaved = !(_streamDescription.mFormatFlags & kAudioFormatFlagIsNonInterleaved);
		UInt32 interleavedChannelCount = (isInterleaved ? _streamDescription.mChannelsPerFrame : 1);
		UInt32 sampleSize = (_streamDescription.mBytesPerFrame > 0 && interleavedChannelCount > 0 ? _streamDescription.mBytesPerFrame / interleavedChannelCount : 0);
		if(sampleSize > 1)
			[result appendString:(_streamDescription.mFormatFlags & kLinearPCMFormatFlagIsBigEndian) ? @" big-endian" : @" little-endian"];

		// Sign
		bool isInteger = !(_streamDescription.mFormatFlags & kLinearPCMFormatFlagIsFloat);
		if(isInteger)
			[result appendString:(_streamDescription.mFormatFlags & kLinearPCMFormatFlagIsSignedInteger) ? @" signed" : @" unsigned"];

		// Integer or floating
		[result appendString:isInteger ? @" integer" : @" float"];

		// Packedness
		if(sampleSize > 0 && _streamDescription.mBitsPerChannel != (sampleSize << 3))
			[result appendFormat:(_streamDescription.mFormatFlags & kLinearPCMFormatFlagIsPacked) ? @", packed in %d bytes" : @", unpacked in %d bytes", sampleSize];

		// Alignment
		if((sampleSize > 0 && _streamDescription.mBitsPerChannel != (sampleSize << 3)) || _streamDescription.mBitsPerChannel & 7)
			[result appendString:(_streamDescription.mFormatFlags & kLinearPCMFormatFlagIsAlignedHigh) ? @" high-aligned" : @" low-aligned"];

		if(!isInterleaved)
			[result appendString:@", deinterleaved"];
	}
	else if(_streamDescription.mFormatID == kAudioFormatAppleLossless) {
		UInt32 sourceBitDepth = 0;
		switch(_streamDescription.mFormatFlags) {
			case kAppleLosslessFormatFlag_16BitSourceData:		sourceBitDepth = 16;	break;
			case kAppleLosslessFormatFlag_20BitSourceData:		sourceBitDepth = 20;	break;
			case kAppleLosslessFormatFlag_24BitSourceData:		sourceBitDepth = 24;	break;
			case kAppleLosslessFormatFlag_32BitSourceData:		sourceBitDepth = 32;	break;
		}

		if(sourceBitDepth != 0)
			[result appendFormat:@"from %d-bit source, ", sourceBitDepth];
		else
			[result appendString:@"from UNKNOWN source bit depth, "];

		[result appendFormat:@" %d frames/packet", _streamDescription.mFramesPerPacket];
	}
	else
		[result appendFormat:@"%u bits/channel, %u bytes/packet, %u frames/packet, %u bytes/frame", _streamDescription.mBitsPerChannel, _streamDescription.mBytesPerPacket, _streamDescription.mFramesPerPacket, _streamDescription.mBytesPerFrame];

	if(_channelLayout)
		[result appendFormat:@", %@", _channelLayout.debugDescription];

	return result;
}

@end
