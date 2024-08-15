//
// Copyright (c) 2011-2024 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

@import os.log;

@import sndfile;

#import "SFBLibsndfileDecoder.h"

#import "NSError+SFBURLPresentation.h"

SFBAudioDecoderName const SFBAudioDecoderNameLibsndfile = @"org.sbooth.AudioEngine.Decoder.Libsndfile";

static AudioFormatFlags CalculateLPCMFlags(UInt32 validBitsPerChannel, UInt32 totalBitsPerChannel, BOOL isFloat, BOOL isBigEndian, BOOL isNonInterleaved)
{
	return (isFloat ? kAudioFormatFlagIsFloat : kAudioFormatFlagIsSignedInteger) | (isBigEndian ? kAudioFormatFlagIsBigEndian : 0) | ((validBitsPerChannel == totalBitsPerChannel) ? kAudioFormatFlagIsPacked : kAudioFormatFlagIsAlignedHigh) | (isNonInterleaved ? kAudioFormatFlagIsNonInterleaved : 0);
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

enum ReadMethod {
	Unknown,
	Short,
	Int,
	Float,
	Double,
};

static sf_count_t my_sf_vio_get_filelen(void *user_data)
{
	NSCParameterAssert(user_data != NULL);

	SFBLibsndfileDecoder *decoder = (__bridge SFBLibsndfileDecoder *)user_data;
	NSInteger length;
	if(![decoder->_inputSource getLength:&length error:nil])
		return -1;
	return length;
}

static sf_count_t my_sf_vio_seek(sf_count_t offset, int whence, void *user_data)
{
	NSCParameterAssert(user_data != NULL);

	SFBLibsndfileDecoder *decoder = (__bridge SFBLibsndfileDecoder *)user_data;
	if(!decoder->_inputSource.supportsSeeking)
		return -1;

	switch(whence) {
		case SEEK_SET:
			// offset remains unchanged
			break;
		case SEEK_CUR: {
			NSInteger inputSourceOffset;
			if([decoder->_inputSource getOffset:&inputSourceOffset error:nil])
				offset += inputSourceOffset;
			break;
		}
		case SEEK_END: {
			NSInteger inputSourceLength;
			if([decoder->_inputSource getLength:&inputSourceLength error:nil])
				offset += inputSourceLength;
			break;
		}
	}

	if(![decoder->_inputSource seekToOffset:offset error:nil])
		return -1;

	NSInteger inputSourceOffset;
	if(![decoder->_inputSource getOffset:&inputSourceOffset error:nil])
		return -1;

	return inputSourceOffset;
}

static sf_count_t my_sf_vio_read(void *ptr, sf_count_t count, void *user_data)
{
	NSCParameterAssert(user_data != NULL);

	SFBLibsndfileDecoder *decoder = (__bridge SFBLibsndfileDecoder *)user_data;

	NSInteger bytesRead;
	if(![decoder->_inputSource readBytes:ptr length:count bytesRead:&bytesRead error:nil])
		return -1;
	return bytesRead;
}

static sf_count_t my_sf_vio_tell(void *user_data)
{
	NSCParameterAssert(user_data != NULL);

	SFBLibsndfileDecoder *decoder = (__bridge SFBLibsndfileDecoder *)user_data;
	NSInteger offset;
	if(![decoder->_inputSource getOffset:&offset error:nil])
		return -1;
	return offset;
}

@interface SFBLibsndfileDecoder ()
{
@private
	SNDFILE *_sndfile;;
	SF_INFO	_sfinfo;
	enum ReadMethod _readMethod;
}
@end

@implementation SFBLibsndfileDecoder

+ (void)load
{
	[SFBAudioDecoder registerSubclass:[self class] priority:-50];
}

+ (NSSet *)supportedPathExtensions
{
	static NSSet *pathExtensions = nil;

	static dispatch_once_t onceToken;
	dispatch_once(&onceToken, ^{
		int majorCount = 0;
		sf_command(NULL, SFC_GET_FORMAT_MAJOR_COUNT, &majorCount, sizeof(int));

		NSMutableSet *majorModeExtensions = [NSMutableSet setWithCapacity:(NSUInteger)majorCount];

		// Loop through each major mode
		for(int i = 0; i < majorCount; ++i) {
			SF_FORMAT_INFO formatInfo;
			formatInfo.format = i;
			if(!sf_command(NULL, SFC_GET_FORMAT_MAJOR, &formatInfo, sizeof(formatInfo))) {
				NSString *pathExtension = [NSString stringWithUTF8String:formatInfo.extension];
				if(pathExtension)
					[majorModeExtensions addObject:pathExtension];
			}
			else
				os_log_debug(gSFBAudioDecoderLog, "sf_command (SFC_GET_FORMAT_MAJOR) %d failed", i);
		}

		pathExtensions = [majorModeExtensions copy];
	});

	return pathExtensions;
}

+ (NSSet *)supportedMIMETypes
{
	return [NSSet set];
}

+ (SFBAudioDecoderName)decoderName
{
	return SFBAudioDecoderNameLibsndfile;
}

+ (BOOL)testInputSource:(SFBInputSource *)inputSource formatIsSupported:(SFBTernaryTruthValue *)formatIsSupported error:(NSError **)error
{
	NSParameterAssert(inputSource != nil);
	NSParameterAssert(formatIsSupported != NULL);

	*formatIsSupported = SFBTernaryTruthValueUnknown;

	return YES;
}

- (BOOL)decodingIsLossless
{
	switch(_sfinfo.format & SF_FORMAT_SUBMASK) {
		case SF_FORMAT_PCM_U8:
		case SF_FORMAT_PCM_S8:
		case SF_FORMAT_PCM_16:
		case SF_FORMAT_PCM_24:
		case SF_FORMAT_PCM_32:
//		case SF_FORMAT_FLOAT:
//		case SF_FORMAT_DOUBLE:
		case SF_FORMAT_ALAC_16:
		case SF_FORMAT_ALAC_20:
		case SF_FORMAT_ALAC_24:
		case SF_FORMAT_ALAC_32:
			return YES;
		default:
			// Be conservative and return NO for formats that aren't known to be lossless
			return NO;
	}
}

- (BOOL)openReturningError:(NSError **)error
{
	if(![super openReturningError:error])
		return NO;

	// Set up the virtual IO function pointers
	SF_VIRTUAL_IO virtualIO;
	virtualIO.get_filelen	= my_sf_vio_get_filelen;
	virtualIO.seek			= my_sf_vio_seek;
	virtualIO.read			= my_sf_vio_read;
	virtualIO.write			= NULL;
	virtualIO.tell			= my_sf_vio_tell;

	// Open the input file
	_sndfile = sf_open_virtual(&virtualIO, SFM_READ, &_sfinfo, (__bridge void *)self);
	if(!_sndfile) {
		os_log_error(gSFBAudioDecoderLog, "sf_open_virtual failed: %{public}s", sf_error_number(sf_error(NULL)));

		if(error)
			*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
											 code:SFBAudioDecoderErrorCodeInvalidFormat
					descriptionFormatStringForURL:NSLocalizedString(@"The format of the file “%@” was not recognized.", @"")
											  url:_inputSource.url
									failureReason:NSLocalizedString(@"File Format Not Recognized", @"")
							   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];

		return NO;
	}

	// Generate interleaved PCM output
	int subFormat = _sfinfo.format & SF_FORMAT_SUBMASK;

	// 8-bit PCM will be high-aligned in shorts
	if(subFormat == SF_FORMAT_PCM_U8 || subFormat == SF_FORMAT_PCM_S8) {
		AudioStreamBasicDescription asbd = {0};
		FillOutASBDForLPCM(&asbd, _sfinfo.samplerate, (UInt32)_sfinfo.channels, 8, 16, NO, kAudioFormatFlagsNativeEndian == kAudioFormatFlagIsBigEndian, NO);
		_processingFormat = [[AVAudioFormat alloc] initWithStreamDescription:&asbd];
		_readMethod = Short;
	}
	// 16-bit PCM
	else if(subFormat == SF_FORMAT_PCM_16) {
		_processingFormat = [[AVAudioFormat alloc] initWithCommonFormat:AVAudioPCMFormatInt16 sampleRate:_sfinfo.samplerate channels:(AVAudioChannelCount)_sfinfo.channels interleaved:YES];
		_readMethod = Short;
	}
	// 24-bit PCM will be high-aligned in ints
	else if(subFormat == SF_FORMAT_PCM_24) {
		AudioStreamBasicDescription asbd = {0};
		FillOutASBDForLPCM(&asbd, _sfinfo.samplerate, (UInt32)_sfinfo.channels, 24, 32, NO, kAudioFormatFlagsNativeEndian == kAudioFormatFlagIsBigEndian, NO);
		_processingFormat = [[AVAudioFormat alloc] initWithStreamDescription:&asbd];
		_readMethod = Int;
	}
	// 32-bit PCM
	else if(subFormat == SF_FORMAT_PCM_32) {
		_processingFormat = [[AVAudioFormat alloc] initWithCommonFormat:AVAudioPCMFormatInt32 sampleRate:_sfinfo.samplerate channels:(AVAudioChannelCount)_sfinfo.channels interleaved:YES];
		_readMethod = Int;
	}
	// Floating point formats
	else if(subFormat == SF_FORMAT_FLOAT) {
		_processingFormat = [[AVAudioFormat alloc] initWithCommonFormat:AVAudioPCMFormatFloat32 sampleRate:_sfinfo.samplerate channels:(AVAudioChannelCount)_sfinfo.channels interleaved:YES];
		_readMethod = Float;
	}
	else if(subFormat == SF_FORMAT_DOUBLE) {
		_processingFormat = [[AVAudioFormat alloc] initWithCommonFormat:AVAudioPCMFormatFloat64 sampleRate:_sfinfo.samplerate channels:(AVAudioChannelCount)_sfinfo.channels interleaved:YES];
		_readMethod = Double;
	}
	// Everything else will be converted to 32-bit float
	else {
		_processingFormat = [[AVAudioFormat alloc] initWithCommonFormat:AVAudioPCMFormatFloat32 sampleRate:_sfinfo.samplerate channels:(AVAudioChannelCount)_sfinfo.channels interleaved:YES];
		_readMethod = Float;
	}

	// Set up the source format
	AudioStreamBasicDescription sourceStreamDescription = {0};

	sourceStreamDescription.mFormatID			= 'SNDF';

	sourceStreamDescription.mSampleRate			= _sfinfo.samplerate;
	sourceStreamDescription.mChannelsPerFrame	= (UInt32)_sfinfo.channels;

	switch(subFormat) {
		case SF_FORMAT_PCM_U8:
			sourceStreamDescription.mBitsPerChannel = 8;
			break;

		case SF_FORMAT_PCM_S8:
			sourceStreamDescription.mFormatFlags = kAudioFormatFlagIsSignedInteger;
			sourceStreamDescription.mBitsPerChannel = 8;
			break;

		case SF_FORMAT_PCM_16:
			sourceStreamDescription.mFormatFlags = kAudioFormatFlagIsSignedInteger;
			sourceStreamDescription.mBitsPerChannel = 16;
			break;

		case SF_FORMAT_PCM_24:
			sourceStreamDescription.mFormatFlags = kAudioFormatFlagIsSignedInteger;
			sourceStreamDescription.mBitsPerChannel = 24;
			break;

		case SF_FORMAT_PCM_32:
			sourceStreamDescription.mFormatFlags = kAudioFormatFlagIsSignedInteger;
			sourceStreamDescription.mBitsPerChannel = 32;
			break;

		case SF_FORMAT_FLOAT:
			sourceStreamDescription.mFormatFlags = kAudioFormatFlagIsFloat;
			sourceStreamDescription.mBitsPerChannel = 32;
			break;

		case SF_FORMAT_DOUBLE:
			sourceStreamDescription.mFormatFlags = kAudioFormatFlagIsFloat;
			sourceStreamDescription.mBitsPerChannel = 64;
			break;
	}

	_sourceFormat = [[AVAudioFormat alloc] initWithStreamDescription:&sourceStreamDescription];

	return YES;
}

- (BOOL)closeReturningError:(NSError **)error
{
	if(_sndfile) {
		int result = sf_close(_sndfile);
		if(result)
			os_log_error(gSFBAudioDecoderLog, "sf_close failed: %{public}s", sf_error_number(result));
		_sndfile = NULL;
	}

	_readMethod = Unknown;

	return [super closeReturningError:error];
}

- (BOOL)isOpen
{
	return _sndfile != NULL;
}

- (AVAudioFramePosition)framePosition
{
	return sf_seek(_sndfile, 0, SF_SEEK_CUR);
}

- (AVAudioFramePosition)frameLength
{
	return _sfinfo.frames;
}

- (BOOL)decodeIntoBuffer:(AVAudioPCMBuffer *)buffer frameLength:(AVAudioFrameCount)frameLength error:(NSError **)error
{
	NSParameterAssert(buffer != nil);
	NSParameterAssert([buffer.format isEqual:_processingFormat]);

	// Reset output buffer data size
	buffer.frameLength = 0;

	if(frameLength > buffer.frameCapacity)
		frameLength = buffer.frameCapacity;

	if(frameLength == 0)
		return YES;

	sf_count_t framesRead = 0;
	switch(_readMethod) {
		case Short:
			framesRead = sf_readf_short(_sndfile, (short *)buffer.audioBufferList->mBuffers[0].mData, frameLength);
			break;
		case Int:
			framesRead = sf_readf_int(_sndfile, (int *)buffer.audioBufferList->mBuffers[0].mData, frameLength);
			break;
		case Float:
			framesRead = sf_readf_float(_sndfile, (float *)buffer.audioBufferList->mBuffers[0].mData, frameLength);
			break;
		case Double:
			framesRead = sf_readf_double(_sndfile, (double *)buffer.audioBufferList->mBuffers[0].mData, frameLength);
			break;
		default:
			os_log_error(gSFBAudioDecoderLog, "Unknown libsndfile read method: %d", _readMethod);
			if(error)
				*error = [NSError errorWithDomain:SFBAudioDecoderErrorDomain code:SFBAudioDecoderErrorCodeInternalError userInfo:nil];
			return NO;
	}

	buffer.frameLength = (AVAudioFrameCount)framesRead;

	int result = sf_error(_sndfile);
	if(result) {
		os_log_error(gSFBAudioDecoderLog, "sf_readf_XXX failed: %{public}s", sf_error_number(result));
		if(error)
			*error = [NSError errorWithDomain:SFBAudioDecoderErrorDomain code:SFBAudioDecoderErrorCodeInternalError userInfo:@{ NSURLErrorKey: _inputSource.url }];
		return NO;
	}

	return YES;
}

- (BOOL)seekToFrame:(AVAudioFramePosition)frame error:(NSError **)error
{
	NSParameterAssert(frame >= 0);

	sf_count_t result = sf_seek(_sndfile, frame, SF_SEEK_SET);
	if(result == -1) {
		os_log_error(gSFBAudioDecoderLog, "sf_seek failed: %{public}s", sf_error_number(sf_error(_sndfile)));
		return NO;
	}
	return YES;
}

@end
