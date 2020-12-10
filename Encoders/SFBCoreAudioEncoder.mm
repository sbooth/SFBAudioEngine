/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <os/log.h>

#import <memory>
#import <vector>

#import <AudioToolbox/AudioToolbox.h>

#import "SFBCoreAudioEncoder.h"

#import "AudioFormat.h"
#import "SFBCStringForOSType.h"

SFBAudioEncoderName const SFBAudioEncoderNameCoreAudio = @"org.sbooth.AudioEngine.Encoder.CoreAudio";

SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyCoreAudioFileTypeID = @"File Type ID";
SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyCoreAudioFormatID = @"Format ID";
SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyCoreAudioFormatFlags = @"Format Flags";
SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyCoreAudioBitsPerChannel = @"Bits per Channel";
SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyCoreAudioAudioConverterPropertySettings = @"Audio Converter Property Settings";

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

// Abuse std::unique_ptr instead
//	class AudioFileWrapper
//	{
//	public:
//		AudioFileWrapper() noexcept : _af(nullptr) {}
//		AudioFileWrapper(AudioFileID af) noexcept  : _af(af) {}
//		~AudioFileWrapper() noexcept
//		{
//			if(_af) {
//				/* OSStatus result =*/ AudioFileClose(_af);
//				_af = nullptr;
//			}
//		}
//
//		AudioFileWrapper(const AudioFileWrapper &rhs) = delete;
//		AudioFileWrapper& operator=(const AudioFileWrapper& rhs) = delete;
//
//		operator bool() const noexcept { return _af != nullptr; }
//
//	private:
//		AudioFileID _af;
//	};

	template <typename T>
	OSStatus SetAudioConverterProperty(AudioConverterRef audioConverter, AudioConverterPropertyID propertyID, T propertyValue)
	{
		NSCParameterAssert(audioConverter != nullptr);
		return AudioConverterSetProperty(audioConverter, propertyID, sizeof(propertyValue), &propertyValue);
	}

	std::vector<AudioFileTypeID> AudioFileTypeIDsForExtension(NSString *pathExtension)
	{
		NSCParameterAssert(pathExtension != nil);
		CFStringRef extension = (__bridge CFStringRef)pathExtension;

		UInt32 size = 0;
		auto result = AudioFileGetGlobalInfoSize(kAudioFileGlobalInfo_TypesForExtension, sizeof(extension), &extension, &size);
		if(result != noErr) {
			os_log_error(gSFBAudioEncoderLog, "AudioFileGetGlobalInfoSize (kAudioFileGlobalInfo_TypesForExtension) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
			return {};
		}

		auto typesForExtensionCount = size / sizeof(AudioFileTypeID);
		std::vector<AudioFileTypeID> typesForExtension(typesForExtensionCount);

		result = AudioFileGetGlobalInfo(kAudioFileGlobalInfo_TypesForExtension, sizeof(extension), &extension, &size, &typesForExtension[0]);
		if(result != noErr) {
			os_log_error(gSFBAudioEncoderLog, "AudioFileGetGlobalInfo (kAudioFileGlobalInfo_TypesForExtension) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
			return {};
		}

		return typesForExtension;
	}

	std::vector<AudioFormatID> AudioFormatIDsForFileTypeID(AudioFileTypeID fileTypeID, bool forEncoding = false)
	{
		UInt32 size = 0;
		auto result = AudioFileGetGlobalInfoSize(kAudioFileGlobalInfo_AvailableFormatIDs, sizeof(fileTypeID), &fileTypeID, &size);
		if(result != noErr) {
			os_log_error(gSFBAudioEncoderLog, "AudioFileGetGlobalInfoSize (kAudioFileGlobalInfo_AvailableFormatIDs) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
			return {};
		}

		auto availableFormatIDCount = size / sizeof(AudioFormatID);
		std::vector<AudioFormatID> availableFormatIDs(availableFormatIDCount);

		result = AudioFileGetGlobalInfo(kAudioFileGlobalInfo_AvailableFormatIDs, sizeof(fileTypeID), &fileTypeID, &size, &availableFormatIDs[0]);
		if(result != noErr) {
			os_log_error(gSFBAudioEncoderLog, "AudioFileGetGlobalInfo (kAudioFileGlobalInfo_AvailableFormatIDs) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
			return {};
		}

		if(!forEncoding)
			return availableFormatIDs;

		result = AudioFormatGetPropertyInfo(kAudioFormatProperty_EncodeFormatIDs, 0, nullptr, &size);
		if(result != noErr) {
			os_log_error(gSFBAudioEncoderLog, "AudioFormatGetPropertyInfo (kAudioFormatProperty_EncodeFormatIDs) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
			return {};
		}

		auto encodeFormatIDCount = size / sizeof(AudioFormatID);
		std::vector<AudioFormatID> encodeFormatIDs(encodeFormatIDCount);

		result = AudioFormatGetProperty(kAudioFormatProperty_EncodeFormatIDs, 0, nullptr, &size, &encodeFormatIDs[0]);
		if(result != noErr) {
			os_log_error(gSFBAudioEncoderLog, "AudioFormatGetPropertyInfo (kAudioFormatProperty_EncodeFormatIDs) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
			return {};
		}

		std::vector<AudioFormatID> formatIDs;
		std::set_intersection(encodeFormatIDs.begin(), encodeFormatIDs.end(), availableFormatIDs.begin(), availableFormatIDs.end(), std::back_inserter(formatIDs));

		return formatIDs;
	}

	OSStatus my_AudioFile_ReadProc(void *inClientData, SInt64 inPosition, UInt32 requestCount, void *buffer, UInt32 *actualCount)
	{
		NSCParameterAssert(inClientData != nullptr);

		SFBCoreAudioEncoder *encoder = (__bridge SFBCoreAudioEncoder *)inClientData;
		SFBOutputSource *outputSource = encoder->_outputSource;

		if(![outputSource seekToOffset:inPosition error:nil])
			return ioErr;

		NSInteger bytesRead;
		if(![outputSource readBytes:buffer length:(NSInteger)requestCount bytesRead:&bytesRead error:nil])
			return ioErr;

		*actualCount = (UInt32)bytesRead;

		return noErr;
	}

	OSStatus my_AudioFile_WriteProc(void *inClientData, SInt64 inPosition, UInt32 requestCount, const void *buffer, UInt32 *actualCount)
	{
		NSCParameterAssert(inClientData != nullptr);

		SFBCoreAudioEncoder *encoder = (__bridge SFBCoreAudioEncoder *)inClientData;
		SFBOutputSource *outputSource = encoder->_outputSource;

		if(![outputSource seekToOffset:inPosition error:nil])
			return ioErr;

		NSInteger bytesWritten;
		if(![outputSource writeBytes:buffer length:(NSInteger)requestCount bytesWritten:&bytesWritten error:nil])
			return ioErr;

		*actualCount = (UInt32)bytesWritten;

		return noErr;
	}

	SInt64 my_AudioFile_GetSizeProc(void *inClientData)
	{
		NSCParameterAssert(inClientData != nullptr);

		SFBCoreAudioEncoder *encoder = (__bridge SFBCoreAudioEncoder *)inClientData;
		SFBOutputSource *outputSource = encoder->_outputSource;

		NSInteger length;
		if(![outputSource getLength:&length error:nil])
			return -1;

		return length;
	}

	OSStatus my_AudioFile_SetSizeProc(void *inClientData, SInt64 inSize)
	{
		NSCParameterAssert(inClientData != nullptr);

		SFBCoreAudioEncoder *encoder = (__bridge SFBCoreAudioEncoder *)inClientData;
		SFBOutputSource *outputSource = encoder->_outputSource;

		// FIXME: Actually do something here
		(void)outputSource;
		(void)inSize;

		return ioErr;
	}
	
}

@interface SFBCoreAudioEncoder ()
{
@private
	std::unique_ptr<OpaqueAudioFileID> _af;
	std::unique_ptr<OpaqueExtAudioFile> _eaf;
}
@end

@implementation SFBCoreAudioEncoder

+ (void)load
{
	[SFBAudioEncoder registerSubclass:[self class] priority:-75];
}

+ (NSSet *)supportedPathExtensions
{
	static NSSet *pathExtensions = nil;

	static dispatch_once_t onceToken;
	dispatch_once(&onceToken, ^{
		UInt32 size = 0;
		auto result = AudioFileGetGlobalInfoSize(kAudioFileGlobalInfo_WritableTypes, 0, nullptr, &size);
		if(result != noErr) {
			os_log_error(gSFBAudioEncoderLog, "AudioFileGetGlobalInfoSize (kAudioFileGlobalInfo_WritableTypes) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
			pathExtensions = [NSSet set];
			return;
		}

		auto writableTypesCount = size / sizeof(UInt32);
		std::vector<UInt32> writableTypes(writableTypesCount);

		result = AudioFileGetGlobalInfo(kAudioFileGlobalInfo_WritableTypes, 0, nullptr, &size, &writableTypes[0]);
		if(result != noErr) {
			os_log_error(gSFBAudioEncoderLog, "AudioFileGetGlobalInfo (kAudioFileGlobalInfo_WritableTypes) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
			pathExtensions = [NSSet set];
			return;
		}

		NSMutableSet *supportedPathExtensions = [NSMutableSet set];
		for(UInt32 type : writableTypes) {
			CFArrayRef extensionsForType = nil;
			size = sizeof(extensionsForType);
			result = AudioFileGetGlobalInfo(kAudioFileGlobalInfo_ExtensionsForType, sizeof(type), &type, &size, &extensionsForType);

			if(result == noErr)
				[supportedPathExtensions addObjectsFromArray:(__bridge_transfer NSArray *)extensionsForType];
			else
				os_log_error(gSFBAudioEncoderLog, "AudioFileGetGlobalInfo (kAudioFileGlobalInfo_ExtensionsForType) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
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
		auto result = AudioFileGetGlobalInfoSize(kAudioFileGlobalInfo_WritableTypes, 0, nullptr, &size);
		if(result != noErr) {
			os_log_error(gSFBAudioEncoderLog, "AudioFileGetGlobalInfoSize (kAudioFileGlobalInfo_WritableTypes) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
			mimeTypes = [NSSet set];
			return;
		}

		auto writableTypesCount = size / sizeof(UInt32);
		std::vector<UInt32> writableTypes(writableTypesCount);

		result = AudioFileGetGlobalInfo(kAudioFileGlobalInfo_WritableTypes, 0, nullptr, &size, &writableTypes[0]);
		if(result != noErr) {
			os_log_error(gSFBAudioEncoderLog, "AudioFileGetGlobalInfo (kAudioFileGlobalInfo_WritableTypes) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
			mimeTypes = [NSSet set];
			return;
		}

		NSMutableSet *supportedMIMETypes = [NSMutableSet set];
		for(UInt32 type : writableTypes) {
			CFArrayRef mimeTypesForType = nil;
			size = sizeof(mimeTypesForType);
			result = AudioFileGetGlobalInfo(kAudioFileGlobalInfo_MIMETypesForType, sizeof(type), &type, &size, &mimeTypesForType);

			if(result == noErr)
				[supportedMIMETypes addObjectsFromArray:(__bridge_transfer NSArray *)mimeTypesForType];
			else
				os_log_error(gSFBAudioEncoderLog, "AudioFileGetGlobalInfo (kAudioFileGlobalInfo_MIMETypesForType) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
		}

		mimeTypes = [supportedMIMETypes copy];
	});

	return mimeTypes;
}

+ (SFBAudioEncoderName)encoderName
{
	return SFBAudioEncoderNameCoreAudio;
}

- (BOOL)encodingIsLossless
{
	switch(_outputFormat.streamDescription->mFormatID) {
		case kAudioFormatLinearPCM:
		case kAudioFormatAppleLossless:
		case kAudioFormatFLAC:
			return YES;
		default:
			// Be conservative and return NO for formats that aren't known to be lossless
			return NO;
	}
}

- (AVAudioFormat *)processingFormatForSourceFormat:(AVAudioFormat *)sourceFormat
{
	NSParameterAssert(sourceFormat != nil);

	// Validate format
	if(sourceFormat.streamDescription->mFormatID != kAudioFormatLinearPCM)
		return nil;

	return sourceFormat;
}

- (BOOL)openReturningError:(NSError **)error
{
	if(![super openReturningError:error])
		return NO;

	AudioFileTypeID fileType = 0;
	NSNumber *fileTypeSetting = [_settings objectForKey:SFBAudioEncodingSettingsKeyCoreAudioFileTypeID];
	if(fileTypeSetting != nil)
		fileType = (AudioFileTypeID)fileTypeSetting.unsignedIntValue;
	else {
		auto typesForExtension = AudioFileTypeIDsForExtension(_outputSource.url.pathExtension);
		// There is no way to determine caller intent and select the most appropriate type; just use the first one
		if(!typesForExtension.empty())
			fileType = typesForExtension[0];
		os_log_info(gSFBAudioEncoderLog, "SFBAudioEncodingSettingsKeyCoreAudioFileTypeID is not set: guessed '%{public}.4s' based on extension \"%{public}@\"", SFBCStringForOSType(fileType), _outputSource.url.pathExtension);
	}

	AudioFormatID formatID = 0;
	NSNumber *formatIDSetting = [_settings objectForKey:SFBAudioEncodingSettingsKeyCoreAudioFormatID];
	if(formatIDSetting != nil)
		formatID = (AudioFormatID)formatIDSetting.unsignedIntValue;
	else {
		auto availableFormatIDs = AudioFormatIDsForFileTypeID(fileType, true);
		// There is no way to determine caller intent and select the most appropriate format; use PCM if available, otherwise use the first one
		if(!availableFormatIDs.empty()) {
			formatID = availableFormatIDs[0];
			auto result = std::find(std::cbegin(availableFormatIDs), std::cend(availableFormatIDs), kAudioFormatLinearPCM);
			if(result != std::cend(availableFormatIDs))
				formatID = *result;
		}
		os_log_info(gSFBAudioEncoderLog, "SFBAudioEncodingSettingsKeyCoreAudioFormatID is not set: guessed '%{public}.4s' based on format '%{public}.4s'", SFBCStringForOSType(formatID), SFBCStringForOSType(fileType));
	}

	UInt32 formatFlags = 0;
	NSNumber *formatFlagsSetting = [_settings objectForKey:SFBAudioEncodingSettingsKeyCoreAudioFormatFlags];
	if(formatFlagsSetting != nil)
		formatFlags = (UInt32)formatFlagsSetting.unsignedIntValue;
	else
		os_log_info(gSFBAudioEncoderLog, "SFBAudioEncodingSettingsKeyCoreAudioFormatFlags is not set; mFormatFlags will be zero which is probably incorrect");

	UInt32 bitsPerChannel = 0;
	NSNumber *bitsPerChannelSetting = [_settings objectForKey:SFBAudioEncodingSettingsKeyCoreAudioBitsPerChannel];
	if(bitsPerChannelSetting != nil)
		bitsPerChannel = (UInt32)bitsPerChannelSetting.unsignedIntValue;
	else
		os_log_info(gSFBAudioEncoderLog, "SFBAudioEncodingSettingsKeyCoreAudioBitsPerChannel is not set; mBitsPerChannel will be zero which is probably incorrect");

	SFB::Audio::Format format;

	format.mFormatID 			= formatID;
	format.mFormatFlags 		= formatFlags;
	format.mBitsPerChannel 		= bitsPerChannel;
	format.mSampleRate 			= _processingFormat.sampleRate;
	format.mChannelsPerFrame 	= _processingFormat.channelCount;

	// Flesh out output structure for PCM formats
	if(format.IsPCM()) {
		format.mBytesPerPacket	= format.InterleavedChannelCount() * ((format.mBitsPerChannel + 7) / 8);
		format.mFramesPerPacket	= 1;
		format.mBytesPerFrame	= format.mBytesPerPacket / format.mFramesPerPacket;
	}
	// Adjust the flags for Apple Lossless and FLAC
	else if(format.mFormatID == kAudioFormatAppleLossless || format.mFormatFlags == kAudioFormatFLAC) {
		switch(_processingFormat.streamDescription->mBitsPerChannel) {
			case 16:	format.mFormatFlags = kAppleLosslessFormatFlag_16BitSourceData;		break;
			case 20:	format.mFormatFlags = kAppleLosslessFormatFlag_20BitSourceData;		break;
			case 24:	format.mFormatFlags = kAppleLosslessFormatFlag_24BitSourceData;		break;
			case 32:	format.mFormatFlags = kAppleLosslessFormatFlag_32BitSourceData;		break;
			default:	format.mFormatFlags = kAppleLosslessFormatFlag_16BitSourceData;		break;
		}
	}

	AudioFileID audioFile;
	auto result = AudioFileInitializeWithCallbacks((__bridge void *)self, my_AudioFile_ReadProc, my_AudioFile_WriteProc, my_AudioFile_GetSizeProc, my_AudioFile_SetSizeProc, fileType, &format, 0, &audioFile);
	if(result != noErr) {
		os_log_error(gSFBAudioEncoderLog, "AudioFileOpenWithCallbacks failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
		if(error)
			*error = [NSError errorWithDomain:NSOSStatusErrorDomain code:result userInfo:nil];
		return NO;
	}

	auto af = std::unique_ptr<OpaqueAudioFileID>(audioFile);

	ExtAudioFileRef extAudioFile;
	result = ExtAudioFileWrapAudioFileID(af.get(), true, &extAudioFile);
	if(result != noErr) {
		os_log_error(gSFBAudioEncoderLog, "ExtAudioFileWrapAudioFileID failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
		if(error)
			*error = [NSError errorWithDomain:NSOSStatusErrorDomain code:result userInfo:nil];
		return NO;
	}

	auto eaf = std::unique_ptr<OpaqueExtAudioFile>(extAudioFile);

	result = ExtAudioFileSetProperty(eaf.get(), kExtAudioFileProperty_ClientDataFormat, sizeof(AudioStreamBasicDescription), _processingFormat.streamDescription);
	if(result != noErr) {
		os_log_error(gSFBAudioEncoderLog, "ExtAudioFileSetProperty (kExtAudioFileProperty_ClientDataFormat) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
		if(error)
			*error = [NSError errorWithDomain:NSOSStatusErrorDomain code:result userInfo:nil];
		return NO;
	}

	if(_processingFormat.channelLayout) {
		result = ExtAudioFileSetProperty(eaf.get(), kExtAudioFileProperty_ClientChannelLayout, sizeof(_processingFormat.channelLayout.layout), _processingFormat.channelLayout.layout);
		if(result != noErr) {
			os_log_error(gSFBAudioEncoderLog, "ExtAudioFileSetProperty (kExtAudioFileProperty_ClientChannelLayout) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
			if(error)
				*error = [NSError errorWithDomain:NSOSStatusErrorDomain code:result userInfo:nil];
			return NO;
		}
	}

	NSDictionary *audioConverterPropertySettings = [_settings objectForKey:SFBAudioEncodingSettingsKeyCoreAudioAudioConverterPropertySettings];
	if(audioConverterPropertySettings != nil) {
		AudioConverterRef audioConverter = nullptr;
		UInt32 size = sizeof(audioConverter);
		result = ExtAudioFileGetProperty(extAudioFile, kExtAudioFileProperty_AudioConverter, &size, &audioConverter);
		if(result != noErr) {
			os_log_error(gSFBAudioEncoderLog, "ExtAudioFileGetProperty (kExtAudioFileProperty_AudioConverter) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
			if(error)
				*error = [NSError errorWithDomain:NSOSStatusErrorDomain code:result userInfo:nil];
			return NO;
		}

		if(audioConverter) {
			for(NSNumber *key in audioConverterPropertySettings) {
				AudioConverterPropertyID propertyID = (AudioConverterPropertyID)key.unsignedIntValue;
				switch(propertyID) {
					case kAudioConverterSampleRateConverterComplexity:
						result = SetAudioConverterProperty<OSType>(audioConverter, propertyID, [[audioConverterPropertySettings objectForKey:key] unsignedIntValue]);
						break;
					case kAudioConverterSampleRateConverterQuality:
					case kAudioConverterCodecQuality:
					case kAudioConverterEncodeBitRate:
					case kAudioCodecPropertyBitRateControlMode:
					case kAudioCodecPropertySoundQualityForVBR:
					case kAudioCodecPropertyBitRateForVBR:
						result = SetAudioConverterProperty<UInt32>(audioConverter, propertyID, [[audioConverterPropertySettings objectForKey:key] unsignedIntValue]);
						break;
					default:
						os_log_info(gSFBAudioEncoderLog, "Ignoring unknown AudioConverterPropertyID: %d '%{public}.4s'", propertyID, SFBCStringForOSType(propertyID));
						break;
				}

				if(result != noErr) {
					os_log_error(gSFBAudioEncoderLog, "AudioConverterSetProperty ('%{public}.4s') failed: %d '%{public}.4s'", SFBCStringForOSType(propertyID), result, SFBCStringForOSType(result));
					if(error)
						*error = [NSError errorWithDomain:NSOSStatusErrorDomain code:result userInfo:nil];
					return NO;
				}
			}

			// Notify ExtAudioFile about the converter property changes
			CFArrayRef converterConfig = nullptr;
			result = ExtAudioFileSetProperty(eaf.get(), kExtAudioFileProperty_ConverterConfig, sizeof(converterConfig), &converterConfig);
			if(result != noErr) {
				os_log_error(gSFBAudioEncoderLog, "ExtAudioFileSetProperty (kExtAudioFileProperty_ConverterConfig) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
				if(error)
					*error = [NSError errorWithDomain:NSOSStatusErrorDomain code:result userInfo:nil];
				return NO;
			}
		}
		else
			os_log_info(gSFBAudioEncoderLog, "SFBAudioEncodingSettingsKeyCoreAudioAudioConverterPropertySettings is set but kExtAudioFileProperty_AudioConverter is NULL");
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
	OSStatus result = ExtAudioFileTell(_eaf.get(), &currentFrame);
	if(result != noErr) {
		os_log_error(gSFBAudioEncoderLog, "ExtAudioFileTell failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
		return SFBUnknownFramePosition;
	}
	return currentFrame;
}

- (BOOL)encodeFromBuffer:(AVAudioPCMBuffer *)buffer frameLength:(AVAudioFrameCount)frameLength error:(NSError **)error
{
	NSParameterAssert(buffer != nil);
	NSParameterAssert([buffer.format isEqual:_processingFormat]);

	if(frameLength > buffer.frameLength)
		frameLength = buffer.frameLength;

	if(frameLength == 0)
		return YES;

	auto result = ExtAudioFileWrite(_eaf.get(), frameLength, buffer.audioBufferList);
	if(result != noErr) {
		os_log_error(gSFBAudioEncoderLog, "ExtAudioFileWrite failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
		if(error)
			*error = [NSError errorWithDomain:NSOSStatusErrorDomain code:result userInfo:nil];
		return NO;
	}

	return YES;
}

- (BOOL)finishEncodingReturningError:(NSError **)error
{
	return YES;
}

@end
