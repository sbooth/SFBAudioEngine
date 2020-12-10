/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <os/log.h>

#import <memory>
#import <vector>

#import <AudioToolbox/AudioToolbox.h>

#import "SFBCoreAudioDecoder.h"

#import "NSError+SFBURLPresentation.h"
#import "SFBCStringForOSType.h"

SFBAudioDecoderName const SFBAudioDecoderNameCoreAudio = @"org.sbooth.AudioEngine.Decoder.CoreAudio";

template <>
struct ::std::default_delete<OpaqueAudioFileID> {
	default_delete() = default;
	template <class U>
	constexpr default_delete(default_delete<U>) noexcept {}
	void operator()(OpaqueAudioFileID *af) const noexcept { /* OSStatus result =*/ AudioFileClose(af); }
};

template <>
struct ::std::default_delete<OpaqueExtAudioFile> {
	default_delete() = default;
	template <class U>
	constexpr default_delete(default_delete<U>) noexcept {}
	void operator()(OpaqueExtAudioFile *eaf) const noexcept { /* OSStatus result =*/ ExtAudioFileDispose(eaf); }
};

namespace {
	// ========================================
	// Callbacks
	OSStatus read_callback(void *inClientData, SInt64 inPosition, UInt32 requestCount, void *buffer, UInt32 *actualCount)
	{
		NSCParameterAssert(inClientData != nullptr);

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

	SInt64 get_size_callback(void *inClientData)
	{
		NSCParameterAssert(inClientData != nullptr);

		SFBCoreAudioDecoder *decoder = (__bridge SFBCoreAudioDecoder *)inClientData;

		NSInteger length;
		if(![decoder->_inputSource getLength:&length error:nil])
			return -1;
		return length;
	}
}

@interface SFBCoreAudioDecoder ()
{
@private
	std::unique_ptr<OpaqueAudioFileID> _af;
	std::unique_ptr<OpaqueExtAudioFile> _eaf;
}
@end

@implementation SFBCoreAudioDecoder

+ (void)load
{
	[SFBAudioDecoder registerSubclass:[self class] priority:-75];
}

+ (NSSet *)supportedPathExtensions
{
	static NSSet *pathExtensions = nil;

	static dispatch_once_t onceToken;
	dispatch_once(&onceToken, ^{
		UInt32 size = 0;
		auto result = AudioFileGetGlobalInfoSize(kAudioFileGlobalInfo_ReadableTypes, 0, nullptr, &size);
		if(result != noErr) {
			os_log_error(gSFBAudioDecoderLog, "AudioFileGetGlobalInfoSize (kAudioFileGlobalInfo_ReadableTypes) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
			pathExtensions = [NSSet set];
			return;
		}

		auto readableTypesCount = size / sizeof(UInt32);
		std::vector<UInt32> readableTypes(readableTypesCount);

		result = AudioFileGetGlobalInfo(kAudioFileGlobalInfo_ReadableTypes, 0, nullptr, &size, &readableTypes[0]);
		if(result != noErr) {
			os_log_error(gSFBAudioDecoderLog, "AudioFileGetGlobalInfo (kAudioFileGlobalInfo_ReadableTypes) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
			pathExtensions = [NSSet set];
			return;
		}

		NSMutableSet *supportedPathExtensions = [NSMutableSet set];
		for(UInt32 type : readableTypes) {
			CFArrayRef extensionsForType = nil;
			size = sizeof(extensionsForType);
			result = AudioFileGetGlobalInfo(kAudioFileGlobalInfo_ExtensionsForType, sizeof(type), &type, &size, &extensionsForType);

			if(result == noErr)
				[supportedPathExtensions addObjectsFromArray:(__bridge_transfer NSArray *)extensionsForType];
			else
				os_log_error(gSFBAudioDecoderLog, "AudioFileGetGlobalInfo (kAudioFileGlobalInfo_ExtensionsForType) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
		}

		pathExtensions = [supportedPathExtensions copy];
	});

	return pathExtensions;
}

+ (NSSet *)supportedMIMETypes
{
	static NSSet *mimeTypes = nil;

	static dispatch_once_t onceToken;
	dispatch_once(&onceToken, ^{
		UInt32 size = 0;
		auto result = AudioFileGetGlobalInfoSize(kAudioFileGlobalInfo_ReadableTypes, 0, nullptr, &size);
		if(result != noErr) {
			os_log_error(gSFBAudioDecoderLog, "AudioFileGetGlobalInfoSize (kAudioFileGlobalInfo_ReadableTypes) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
			mimeTypes = [NSSet set];
			return;
		}

		auto readableTypesCount = size / sizeof(UInt32);
		std::vector<UInt32> readableTypes(readableTypesCount);

		result = AudioFileGetGlobalInfo(kAudioFileGlobalInfo_ReadableTypes, 0, nullptr, &size, &readableTypes[0]);
		if(result != noErr) {
			os_log_error(gSFBAudioDecoderLog, "AudioFileGetGlobalInfo (kAudioFileGlobalInfo_ReadableTypes) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
			mimeTypes = [NSSet set];
			return;
		}

		NSMutableSet *supportedMIMETypes = [NSMutableSet set];
		for(UInt32 type : readableTypes) {
			CFArrayRef mimeTypesForType = nil;
			size = sizeof(mimeTypesForType);
			result = AudioFileGetGlobalInfo(kAudioFileGlobalInfo_MIMETypesForType, sizeof(type), &type, &size, &mimeTypesForType);

			if(result == noErr)
				[supportedMIMETypes addObjectsFromArray:(__bridge_transfer NSArray *)mimeTypesForType];
			else
				os_log_error(gSFBAudioDecoderLog, "AudioFileGetGlobalInfo (kAudioFileGlobalInfo_MIMETypesForType) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
		}

		mimeTypes = [supportedMIMETypes copy];
	});

	return mimeTypes;
}

+ (SFBAudioDecoderName)decoderName
{
	return SFBAudioDecoderNameCoreAudio;
}

- (BOOL)decodingIsLossless
{
	switch(_sourceFormat.streamDescription->mFormatID) {
		case kAudioFormatLinearPCM:
		case kAudioFormatAppleLossless:
		case kAudioFormatFLAC:
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

	// Open the input file
	AudioFileID audioFile;
	auto result = AudioFileOpenWithCallbacks((__bridge void *)self, read_callback, nullptr, get_size_callback, nullptr, 0, &audioFile);
	if(result != noErr) {
		os_log_error(gSFBAudioDecoderLog, "AudioFileOpenWithCallbacks failed: failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));

		if(error)
			*error = [NSError SFB_errorWithDomain:NSOSStatusErrorDomain
											 code:result
					descriptionFormatStringForURL:NSLocalizedString(@"The format of the file “%@” was not recognized.", @"")
											  url:_inputSource.url
									failureReason:NSLocalizedString(@"File Format Not Recognized", @"")
							   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];

		return NO;
	}

	auto af = std::unique_ptr<OpaqueAudioFileID>(audioFile);

	ExtAudioFileRef extAudioFile;
	result = ExtAudioFileWrapAudioFileID(af.get(), false, &extAudioFile);
	if(result != noErr) {
		os_log_error(gSFBAudioDecoderLog, "ExtAudioFileWrapAudioFileID failed: failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));

		if(error)
			*error = [NSError SFB_errorWithDomain:NSOSStatusErrorDomain
											 code:result
					descriptionFormatStringForURL:NSLocalizedString(@"The format of the file “%@” was not recognized.", @"")
											  url:_inputSource.url
									failureReason:NSLocalizedString(@"File Format Not Recognized", @"")
							   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];

		return NO;
	}

	auto eaf = std::unique_ptr<OpaqueExtAudioFile>(extAudioFile);

	// Query file format
	AudioStreamBasicDescription format{};
	UInt32 dataSize = sizeof(format);
	result = ExtAudioFileGetProperty(eaf.get(), kExtAudioFileProperty_FileDataFormat, &dataSize, &format);
	if(result != noErr) {
		os_log_error(gSFBAudioDecoderLog, "ExtAudioFileGetProperty (kExtAudioFileProperty_FileDataFormat) failed: failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
		if(error)
			*error = [NSError errorWithDomain:NSOSStatusErrorDomain code:result userInfo:nil];
		return NO;
	}

	// Query channel layout
	AVAudioChannelLayout *channelLayout = nil;
	result = ExtAudioFileGetPropertyInfo(eaf.get(), kExtAudioFileProperty_FileChannelLayout, &dataSize, nullptr);
	if(result == noErr) {
		AudioChannelLayout *layout = (AudioChannelLayout *)malloc(dataSize);
		result = ExtAudioFileGetProperty(eaf.get(), kExtAudioFileProperty_FileChannelLayout, &dataSize, layout);
		if(result != noErr) {
			os_log_error(gSFBAudioDecoderLog, "ExtAudioFileGetProperty (kExtAudioFileProperty_FileChannelLayout) failed: failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));

			free(layout);

			if(error)
				*error = [NSError SFB_errorWithDomain:NSOSStatusErrorDomain
												 code:result
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
		os_log_error(gSFBAudioDecoderLog, "ExtAudioFileGetPropertyInfo (kExtAudioFileProperty_FileChannelLayout) failed: failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));

	_sourceFormat = [[AVAudioFormat alloc] initWithStreamDescription:&format channelLayout:channelLayout];

	// Tell the ExtAudioFile the format in which we'd like our data

	// For Linear PCM formats leave the data untouched
	if(format.mFormatID == kAudioFormatLinearPCM)
		_processingFormat = [[AVAudioFormat alloc] initWithStreamDescription:&format channelLayout:channelLayout];
	// For Apple Lossless and FLAC convert to packed ints if possible, otherwise high-align
	else if(format.mFormatID == kAudioFormatAppleLossless || format.mFormatID == kAudioFormatFLAC) {
		AudioStreamBasicDescription asbd{};

		asbd.mFormatID			= kAudioFormatLinearPCM;
		asbd.mFormatFlags		= kAudioFormatFlagsNativeEndian | kAudioFormatFlagIsSignedInteger;

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

		asbd.mFormatFlags		|= asbd.mBitsPerChannel % 8 ? kAudioFormatFlagIsAlignedHigh : kAudioFormatFlagIsPacked;

		asbd.mBytesPerPacket	= ((asbd.mBitsPerChannel + 7) / 8) * asbd.mChannelsPerFrame;
		asbd.mFramesPerPacket	= 1;
		asbd.mBytesPerFrame		= asbd.mBytesPerPacket / asbd.mFramesPerPacket;

		_processingFormat = [[AVAudioFormat alloc] initWithStreamDescription:&asbd channelLayout:channelLayout];
	}
	// For all other formats convert to the canonical Core Audio format
	else {
		if(channelLayout)
			_processingFormat = [[AVAudioFormat alloc] initWithCommonFormat:AVAudioPCMFormatFloat32 sampleRate:format.mSampleRate interleaved:NO channelLayout:channelLayout];
		else
			_processingFormat = [[AVAudioFormat alloc] initWithCommonFormat:AVAudioPCMFormatFloat32 sampleRate:format.mSampleRate channels:format.mChannelsPerFrame interleaved:NO];
	}

	// For audio with more than 2 channels AVAudioFormat requires a channel map. Since ExtAudioFile doesn't always return one, there is a
	// chance that the initialization of _processingFormat failed. If that happened then attempting to set kExtAudioFileProperty_ClientDataFormat
	// will segfault
	if(!_processingFormat) {
		if(error)
			*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
											 code:SFBAudioDecoderErrorCodeInvalidFormat
					descriptionFormatStringForURL:NSLocalizedString(@"The format of the file “%@” was not recognized.", @"")
											  url:_inputSource.url
									failureReason:NSLocalizedString(@"File Format Not Recognized", @"")
							   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];

		return NO;
	}

	result = ExtAudioFileSetProperty(eaf.get(), kExtAudioFileProperty_ClientDataFormat, sizeof(AudioStreamBasicDescription), _processingFormat.streamDescription);
	if(result != noErr) {
		os_log_error(gSFBAudioDecoderLog, "ExtAudioFileSetProperty (kExtAudioFileProperty_ClientDataFormat) failed: failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));

		if(error)
			*error = [NSError SFB_errorWithDomain:NSOSStatusErrorDomain
											 code:result
					descriptionFormatStringForURL:NSLocalizedString(@"The format of the file “%@” was not recognized.", @"")
											  url:_inputSource.url
									failureReason:NSLocalizedString(@"File Format Not Recognized", @"")
							   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];

		return NO;
	}

	_af = std::move(af);
	_eaf = std::move(eaf);

	return YES;
}

- (BOOL)closeReturningError:(NSError **)error
{
	_eaf.reset();
	_af.reset();

	return [super closeReturningError:error];
}

- (BOOL)isOpen
{
	return _eaf != nullptr;
}

- (AVAudioFramePosition)framePosition
{
	SInt64 currentFrame;
	auto result = ExtAudioFileTell(_eaf.get(), &currentFrame);
	if(result != noErr) {
		os_log_error(gSFBAudioDecoderLog, "ExtAudioFileTell failed: failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
		return SFBUnknownFramePosition;
	}
	return currentFrame;
}

- (AVAudioFramePosition)frameLength
{
	SInt64 frameLength;
	UInt32 dataSize = sizeof(frameLength);
	auto result = ExtAudioFileGetProperty(_eaf.get(), kExtAudioFileProperty_FileLengthFrames, &dataSize, &frameLength);
	if(result != noErr) {
		os_log_error(gSFBAudioDecoderLog, "ExtAudioFileGetProperty (kExtAudioFileProperty_FileLengthFrames) failed: failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
		return SFBUnknownFrameLength;
	}
	return frameLength;
}

- (BOOL)decodeIntoBuffer:(AVAudioPCMBuffer *)buffer frameLength:(AVAudioFrameCount)frameLength error:(NSError **)error
{
	NSParameterAssert(buffer != nil);
	NSParameterAssert([buffer.format isEqual:_processingFormat]);

	if(frameLength > buffer.frameCapacity)
		frameLength = buffer.frameCapacity;

	if(frameLength == 0) {
		buffer.frameLength = 0;
		return YES;
	}

	buffer.frameLength = buffer.frameCapacity;

	auto result = ExtAudioFileRead(_eaf.get(), &frameLength, buffer.mutableAudioBufferList);
	if(result != noErr) {
		os_log_error(gSFBAudioDecoderLog, "ExtAudioFileRead failed: failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
		buffer.frameLength = 0;
		if(error)
			*error = [NSError errorWithDomain:NSOSStatusErrorDomain code:result userInfo:nil];
		return NO;
	}

	buffer.frameLength = frameLength;

	return YES;
}

- (BOOL)seekToFrame:(AVAudioFramePosition)frame error:(NSError **)error
{
	NSParameterAssert(frame >= 0);
	auto result = ExtAudioFileSeek(_eaf.get(), frame);
	if(result != noErr) {
		os_log_error(gSFBAudioDecoderLog, "ExtAudioFileSeek failed: failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
		if(error)
			*error = [NSError errorWithDomain:NSOSStatusErrorDomain code:result userInfo:nil];
		return NO;
	}
	return YES;
}

@end
