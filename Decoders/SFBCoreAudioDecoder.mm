//
// Copyright (c) 2006 - 2024 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import <vector>

#import <os/log.h>

#import <AudioToolbox/AudioToolbox.h>

#import <SFBCAAudioFile.hpp>
#import <SFBCAExtAudioFile.hpp>

#import "SFBCoreAudioDecoder.h"

#import "NSError+SFBURLPresentation.h"
#import "SFBCStringForOSType.h"

SFBAudioDecoderName const SFBAudioDecoderNameCoreAudio = @"org.sbooth.AudioEngine.Decoder.CoreAudio";

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
		if(!decoder->_inputSource.supportsSeeking)
			return kAudioFileOperationNotSupportedError;
		if(![decoder->_inputSource seekToOffset:inPosition error:nil])
			return kAudioFileUnspecifiedError;
	}

	NSInteger bytesRead;
	if(![decoder->_inputSource readBytes:buffer length:requestCount bytesRead:&bytesRead error:nil])
		return kAudioFileUnspecifiedError;

	*actualCount = static_cast<UInt32>(bytesRead);

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
	SFB::CAAudioFile _af;
	SFB::CAExtAudioFile _eaf;
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
		try {
			NSMutableSet *supportedPathExtensions = [NSMutableSet set];

			auto readableTypes = SFB::CAAudioFile::ReadableTypes();
			for(const auto& type : readableTypes) {
				try {
					auto extensionsForType = SFB::CAAudioFile::ExtensionsForType(type);
					[supportedPathExtensions addObjectsFromArray:(NSArray *)extensionsForType];
				}
				catch(const std::exception& e) {
					os_log_error(gSFBAudioDecoderLog, "SFB::CAAudioFile::ExtensionsForType failed for '%{public}.4s': %s", SFBCStringForOSType(type), e.what());
				}
			}

			pathExtensions = [supportedPathExtensions copy];
		}
		catch(const std::exception& e) {
			os_log_error(gSFBAudioDecoderLog, "SFB::CAAudioFile::ReadableTypes failed: %s", e.what());
			pathExtensions = [NSSet set];
		}
	});

	return pathExtensions;
}

+ (NSSet *)supportedMIMETypes
{
	static NSSet *mimeTypes = nil;

	static dispatch_once_t onceToken;
	dispatch_once(&onceToken, ^{
		try {
			NSMutableSet *supportedMIMETypes = [NSMutableSet set];

			auto readableTypes = SFB::CAAudioFile::ReadableTypes();
			for(const auto& type : readableTypes) {
				try {
					auto mimeTypesForType = SFB::CAAudioFile::MIMETypesForType(type);
					[supportedMIMETypes addObjectsFromArray:(NSArray *)mimeTypesForType];
				}
				catch(const std::exception& e) {
					os_log_error(gSFBAudioDecoderLog, "SFB::CAAudioFile::MIMETypesForType failed for '%{public}.4s': %s", SFBCStringForOSType(type), e.what());
				}
			}

			mimeTypes = [supportedMIMETypes copy];
		}
		catch(const std::exception& e) {
			os_log_error(gSFBAudioDecoderLog, "SFB::CAAudioFile::ReadableTypes failed: %s", e.what());
			mimeTypes = [NSSet set];
		}
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

	try {
		// Open the input file
		_af.OpenWithCallbacks((__bridge void *)self, read_callback, nullptr, get_size_callback, nullptr, 0);
		_eaf.WrapAudioFileID(_af, false);

		// Query file format
		auto format = _eaf.FileDataFormat();

		// Query channel layout
		AVAudioChannelLayout *channelLayout = _eaf.FileChannelLayout();

		// ExtAudioFile occasionally returns empty channel layouts; ignore them
		if(channelLayout.channelCount != format.mChannelsPerFrame) {
			os_log_error(gSFBAudioDecoderLog, "Channel count mismatch between AudioStreamBasicDescription (%u) and AVAudioChannelLayout (%u)", format.mChannelsPerFrame, channelLayout.channelCount);
			channelLayout = nil;
		}

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

		_eaf.SetClientDataFormat(*_processingFormat.streamDescription);

		return YES;
	}
	catch(const std::system_error& e) {
		try {
			_af.Close();
			_eaf.Close();
		}
		catch(...)
		{}

		if(error) {
			os_log_error(gSFBAudioDecoderLog, "Error opening SFBCoreAudioDecoder: %s", e.what());
			*error = [NSError SFB_errorWithDomain:NSOSStatusErrorDomain
											 code:e.code().value()
					descriptionFormatStringForURL:NSLocalizedString(@"The format of the file “%@” was not recognized.", @"")
											  url:_inputSource.url
									failureReason:NSLocalizedString(@"File Format Not Recognized", @"")
							   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
		}

		return NO;
	}
}

- (BOOL)closeReturningError:(NSError **)error
{
	try {
		_eaf.Close();
		_af.Close();
	}
	catch(const std::system_error& e) {
		os_log_error(gSFBAudioDecoderLog, "Error closing SFBCoreAudioDecoder: %s", e.what());
		if(error)
			*error = [NSError errorWithDomain:NSOSStatusErrorDomain code:e.code().value() userInfo:@{ NSURLErrorKey: _inputSource.url }];
		return NO;
	}

	return [super closeReturningError:error];
}

- (BOOL)isOpen
{
	return _eaf.IsValid();
}

- (AVAudioFramePosition)framePosition
{
	try {
		return _eaf.Tell();
	}
	catch(const std::exception& e) {
		os_log_error(gSFBAudioDecoderLog, "SFB::CAExtAudioFile::Tell failed: %s", e.what());
		return SFBUnknownFramePosition;
	}
}

- (AVAudioFramePosition)frameLength
{
	try {
		return _eaf.FrameLength();
	}
	catch(const std::exception& e) {
		os_log_error(gSFBAudioDecoderLog, "SFB::CAExtAudioFile::FrameLength failed: %s", e.what());
		return SFBUnknownFramePosition;
	}
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

	try {
		_eaf.Read(frameLength, buffer.mutableAudioBufferList);
		buffer.frameLength = frameLength;
		return YES;
	}
	catch(const std::system_error& e) {
		os_log_error(gSFBAudioDecoderLog, "SFB::CAExtAudioFile::Read failed: %s", e.what());
		buffer.frameLength = 0;
		if(error)
			*error = [NSError errorWithDomain:NSOSStatusErrorDomain code:e.code().value() userInfo:@{ NSURLErrorKey: _inputSource.url }];
		return NO;
	}
}

- (BOOL)seekToFrame:(AVAudioFramePosition)frame error:(NSError **)error
{
	NSParameterAssert(frame >= 0);
	try {
		_eaf.Seek(frame);
		return YES;
	}
	catch(const std::system_error& e) {
		os_log_error(gSFBAudioDecoderLog, "SFB::CAExtAudioFile::Seek failed: %s", e.what());
		if(error)
			*error = [NSError errorWithDomain:NSOSStatusErrorDomain code:e.code().value() userInfo:@{ NSURLErrorKey: _inputSource.url }];
		return NO;
	}
}

@end
