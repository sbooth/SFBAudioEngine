/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

@import os.log;

@import AudioToolbox;

#import "SFBCoreAudioDecoder.h"

#import "NSError+SFBURLPresentation.h"
#import "SFBCStringForOSType.h"

// ========================================
// Callbacks
static OSStatus read_callback(void *inClientData, SInt64 inPosition, UInt32 requestCount, void *buffer, UInt32 *actualCount)
{
	NSCParameterAssert(inClientData != NULL);

	SFBCoreAudioDecoder *decoder = (__bridge SFBCoreAudioDecoder *)inClientData;

	NSInteger offset;
	if(![decoder->_inputSource getOffset:&offset error:nil])
		return kAudioFileUnspecifiedError;

	if(inPosition != offset) {
		if(!decoder->_inputSource.supportsSeeking || ![decoder->_inputSource seekToOffset:inPosition error:nil])
			return kAudioFileOperationNotSupportedError;
	}

	NSInteger bytesRead;
	if(![decoder->_inputSource readBytes:buffer length:requestCount bytesRead:&bytesRead error:nil])
		return kAudioFileUnspecifiedError;

	*actualCount = (UInt32)bytesRead;

	if(decoder->_inputSource.atEOF)
		return kAudioFileEndOfFileError;

	return noErr;
}

static SInt64 get_size_callback(void *inClientData)
{
	NSCParameterAssert(inClientData != NULL);

	SFBCoreAudioDecoder *decoder = (__bridge SFBCoreAudioDecoder *)inClientData;

	NSInteger length;
	if(![decoder->_inputSource getLength:&length error:nil])
		return -1;
	return length;
}

@interface SFBCoreAudioDecoder ()
{
@private
	AudioFileID _af;
	ExtAudioFileRef _eaf;
}
@end

@implementation SFBCoreAudioDecoder

+ (void)load
{
	[SFBAudioDecoder registerSubclass:[self class] priority:-75];
}

+ (NSSet *)supportedPathExtensions
{
	CFArrayRef supportedExtensions = nil;
	UInt32 size = sizeof(supportedExtensions);
	OSStatus result = AudioFileGetGlobalInfo(kAudioFileGlobalInfo_AllExtensions, 0, NULL, &size, &supportedExtensions);

	if(result != noErr) {
		os_log_error(gSFBAudioDecoderLog, "AudioFileGetGlobalInfo (kAudioFileGlobalInfo_AllExtensions) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
		return [NSSet set];
	}

	return [NSSet setWithArray:(__bridge_transfer NSArray *)supportedExtensions];
}

+ (NSSet *)supportedMIMETypes
{
	CFArrayRef supportedMIMETypes = nil;
	UInt32 size = sizeof(supportedMIMETypes);
	OSStatus result = AudioFileGetGlobalInfo(kAudioFileGlobalInfo_AllMIMETypes, 0, NULL, &size, &supportedMIMETypes);

	if(result != noErr) {
		os_log_error(gSFBAudioDecoderLog, "AudioFileGetGlobalInfo (kAudioFileGlobalInfo_AllMIMETypes) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
		return [NSSet set];
	}

	return [NSSet setWithArray:(__bridge_transfer NSArray *)supportedMIMETypes];
}

- (BOOL)openReturningError:(NSError **)error
{
	if(![super openReturningError:error])
		return NO;

	// Open the input file
	OSStatus result = AudioFileOpenWithCallbacks((__bridge void *)self, read_callback, NULL, get_size_callback, NULL, 0, &_af);
	if(result != noErr) {
		os_log_error(gSFBAudioDecoderLog, "AudioFileOpenWithCallbacks failed: %d", result);

		if(error)
			*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
											 code:SFBAudioDecoderErrorCodeInputOutput
					descriptionFormatStringForURL:NSLocalizedString(@"The format of the file “%@” was not recognized.", @"")
											  url:_inputSource.url
									failureReason:NSLocalizedString(@"File Format Not Recognized", @"")
							   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];

		return NO;
	}

	result = ExtAudioFileWrapAudioFileID(_af, false, &_eaf);
	if(result != noErr) {
		os_log_error(gSFBAudioDecoderLog, "ExtAudioFileWrapAudioFileID failed: %d", result);

		result = AudioFileClose(_af);
		if(result != noErr)
			os_log_error(gSFBAudioDecoderLog, "AudioFileClose failed: %d", result);

		_af = NULL;

		if(error)
			*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
											 code:SFBAudioDecoderErrorCodeInputOutput
					descriptionFormatStringForURL:NSLocalizedString(@"The format of the file “%@” was not recognized.", @"")
											  url:_inputSource.url
									failureReason:NSLocalizedString(@"File Format Not Recognized", @"")
							   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];

		return NO;
	}

	// Query file format
	AudioStreamBasicDescription format = {0};
	UInt32 dataSize = sizeof(format);
	result = ExtAudioFileGetProperty(_eaf, kExtAudioFileProperty_FileDataFormat, &dataSize, &format);
	if(result != noErr) {
		os_log_error(gSFBAudioDecoderLog, "ExtAudioFileGetProperty (kExtAudioFileProperty_FileDataFormat) failed: %d", result);

		result = ExtAudioFileDispose(_eaf);
		if(result != noErr)
			os_log_error(gSFBAudioDecoderLog, "ExtAudioFileDispose failed: %d", result);

		result = AudioFileClose(_af);
		if(result != noErr)
			os_log_error(gSFBAudioDecoderLog, "AudioFileClose failed: %d", result);

		_af = NULL;
		_eaf = NULL;

		return NO;
	}

	// Query channel layout
	AVAudioChannelLayout *channelLayout = nil;
	result = ExtAudioFileGetPropertyInfo(_eaf, kExtAudioFileProperty_FileChannelLayout, &dataSize, NULL);
	if(result == noErr) {
		AudioChannelLayout *layout = (AudioChannelLayout *)malloc(dataSize);
		result = ExtAudioFileGetProperty(_eaf, kExtAudioFileProperty_FileChannelLayout, &dataSize, layout);
		if(result != noErr) {
			os_log_error(gSFBAudioDecoderLog, "ExtAudioFileGetProperty (kExtAudioFileProperty_FileChannelLayout) failed: %d", result);

			free(layout);

			result = ExtAudioFileDispose(_eaf);
			if(result != noErr)
				os_log_error(gSFBAudioDecoderLog, "ExtAudioFileDispose failed: %d", result);

			result = AudioFileClose(_af);
			if(result != noErr)
				os_log_error(gSFBAudioDecoderLog, "AudioFileClose failed: %d", result);

			_af = NULL;
			_eaf = NULL;

			if(error)
				*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
												 code:SFBAudioDecoderErrorCodeInputOutput
						descriptionFormatStringForURL:NSLocalizedString(@"The format of the file “%@” was not recognized.", @"")
												  url:_inputSource.url
										failureReason:NSLocalizedString(@"File Format Not Recognized", @"")
								   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];

			return NO;
		}

		channelLayout = [[AVAudioChannelLayout alloc] initWithLayout:layout];
		free(layout);

		// ExtAudioFile occasionally returns empty channel layouts; ignore them
		if(channelLayout.channelCount != format.mChannelsPerFrame) {
			os_log_error(gSFBAudioDecoderLog, "Channel count mismatch between AudioStreamBasicDescription (%u) and AVAudioChannelLayout (%u)", format.mChannelsPerFrame, channelLayout.channelCount);
			channelLayout = nil;
		}
	}
	else
		os_log_error(gSFBAudioDecoderLog, "ExtAudioFileGetPropertyInfo (kExtAudioFileProperty_FileChannelLayout) failed: %d", result);

	// Tell the ExtAudioFile the format in which we'd like our data

	// For Linear PCM formats, leave the data untouched
	if(format.mFormatID == kAudioFormatLinearPCM)
		_processingFormat = [[AVAudioFormat alloc] initWithStreamDescription:&format channelLayout:channelLayout];
	// For Apple Lossless, convert to high-aligned signed ints in 32 bits
	else if(format.mFormatID == kAudioFormatAppleLossless) {
		AudioStreamBasicDescription asbd = {0};

		asbd.mFormatID			= kAudioFormatLinearPCM;
		asbd.mFormatFlags		= kAudioFormatFlagsNativeEndian | kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsAlignedHigh;

		asbd.mSampleRate		= format.mSampleRate;
		asbd.mChannelsPerFrame	= format.mChannelsPerFrame;

		if(format.mFormatFlags == kAppleLosslessFormatFlag_16BitSourceData)
			asbd.mBitsPerChannel = 16;
		else if(format.mFormatFlags == kAppleLosslessFormatFlag_20BitSourceData)
			asbd.mBitsPerChannel = 20;
		else if(format.mFormatFlags == kAppleLosslessFormatFlag_24BitSourceData)
			asbd.mBitsPerChannel = 24;
		else if(format.mFormatFlags == kAppleLosslessFormatFlag_32BitSourceData)
			asbd.mBitsPerChannel = 32;

		asbd.mBytesPerPacket	= 4 * asbd.mChannelsPerFrame;
		asbd.mFramesPerPacket	= 1;
		asbd.mBytesPerFrame		= asbd.mBytesPerPacket * asbd.mFramesPerPacket;

		_processingFormat = [[AVAudioFormat alloc] initWithStreamDescription:&asbd channelLayout:channelLayout];
	}
	// For all other formats convert to the canonical Core Audio format
	else
		_processingFormat = [[AVAudioFormat alloc] initWithCommonFormat:AVAudioPCMFormatFloat32 sampleRate:format.mSampleRate interleaved:NO channelLayout:channelLayout];

	// For audio with more than 2 channels AVAudioFormat requires a channel map. Since ExtAudioFile doesn't always return one, there is a
	// chance that the initialization of _processingFormat failed. If that happened then attempting to set kExtAudioFileProperty_ClientDataFormat
	// will segfault
	if(!_processingFormat) {
		result = ExtAudioFileDispose(_eaf);
		if(result != noErr)
			os_log_error(gSFBAudioDecoderLog, "ExtAudioFileDispose failed: %d", result);

		result = AudioFileClose(_af);
		if(result != noErr)
			os_log_error(gSFBAudioDecoderLog, "AudioFileClose failed: %d", result);

		_af = NULL;
		_eaf = NULL;

		if(error)
			*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
											 code:SFBAudioDecoderErrorCodeInputOutput
					descriptionFormatStringForURL:NSLocalizedString(@"The format of the file “%@” was not recognized.", @"")
											  url:_inputSource.url
									failureReason:NSLocalizedString(@"File Format Not Recognized", @"")
							   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];

		return NO;
	}

	result = ExtAudioFileSetProperty(_eaf, kExtAudioFileProperty_ClientDataFormat, sizeof(AudioStreamBasicDescription), _processingFormat.streamDescription);
	if(result != noErr) {
		os_log_error(gSFBAudioDecoderLog, "ExtAudioFileSetProperty (kExtAudioFileProperty_ClientDataFormat) failed: %d", result);

		result = ExtAudioFileDispose(_eaf);
		if(result != noErr)
			os_log_error(gSFBAudioDecoderLog, "ExtAudioFileDispose failed: %d", result);

		result = AudioFileClose(_af);
		if(result != noErr)
			os_log_error(gSFBAudioDecoderLog, "AudioFileClose failed: %d", result);

		_af = NULL;
		_eaf = NULL;

		if(error)
			*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
											 code:SFBAudioDecoderErrorCodeInputOutput
					descriptionFormatStringForURL:NSLocalizedString(@"The format of the file “%@” was not recognized.", @"")
											  url:_inputSource.url
									failureReason:NSLocalizedString(@"File Format Not Recognized", @"")
							   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];

		return NO;
	}

	return YES;
}

- (BOOL)closeReturningError:(NSError **)error
{
	if(_eaf) {
		OSStatus result = ExtAudioFileDispose(_eaf);
		if(result != noErr)
			os_log_error(gSFBAudioDecoderLog, "ExtAudioFileDispose failed: %d", result);
		_eaf = NULL;
	}

	if(_af) {
		OSStatus result = AudioFileClose(_af);
		if(result != noErr)
			os_log_error(gSFBAudioDecoderLog, "AudioFileClose failed: %d", result);
		_af = NULL;
	}

	return [super closeReturningError:error];
}

- (BOOL)isOpen
{
	return _eaf != NULL;
}

- (AVAudioFramePosition)framePosition
{
	SInt64 currentFrame;
	OSStatus result = ExtAudioFileTell(_eaf, &currentFrame);
	if(result != noErr) {
		os_log_error(gSFBAudioDecoderLog, "ExtAudioFileTell failed: %d", result);
		return -1;
	}
	return currentFrame;
}

- (AVAudioFramePosition)frameLength
{
	SInt64 frameLength;
	UInt32 dataSize = sizeof(frameLength);
	OSStatus result = ExtAudioFileGetProperty(_eaf, kExtAudioFileProperty_FileLengthFrames, &dataSize, &frameLength);
	if(result != noErr) {
		os_log_error(gSFBAudioDecoderLog, "ExtAudioFileGetProperty (kExtAudioFileProperty_FileLengthFrames) failed: %d", result);
		return -1;
	}
	return frameLength;
}

- (BOOL)decodeIntoBuffer:(AVAudioPCMBuffer *)buffer frameLength:(AVAudioFrameCount)frameLength error:(NSError **)error
{
	NSParameterAssert(buffer != nil);

	if(![buffer.format isEqual:_processingFormat]) {
		os_log_debug(gSFBAudioDecoderLog, "-decodeAudio:frameLength:error: called with invalid parameters");
		return NO;
	}

	if(frameLength > buffer.frameCapacity)
		frameLength = buffer.frameCapacity;

	buffer.frameLength = buffer.frameCapacity;

	OSStatus result = ExtAudioFileRead(_eaf, &frameLength, buffer.mutableAudioBufferList);
	if(result != noErr) {
		os_log_error(gSFBAudioDecoderLog, "ExtAudioFileRead failed: %d", result);
		return NO;
	}

	buffer.frameLength = frameLength;

	return YES;
}

- (BOOL)seekToFrame:(AVAudioFramePosition)frame error:(NSError **)error
{
	OSStatus result = ExtAudioFileSeek(_eaf, frame);
	if(result != noErr) {
		os_log_error(gSFBAudioDecoderLog, "ExtAudioFileSeek failed: %d", result);
		return NO;
	}
	return YES;
}

@end
