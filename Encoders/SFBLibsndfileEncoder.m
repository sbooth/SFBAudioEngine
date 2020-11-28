/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

@import os.log;

#import <sndfile/sndfile.h>

#import "SFBLibsndfileEncoder.h"

#import "AVAudioFormat+SFBFormatTransformation.h"
#import "NSError+SFBURLPresentation.h"

SFBAudioEncoderName const SFBAudioEncoderNameLibsndfile = @"org.sbooth.AudioEngine.Encoder.Libsndfile";

SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyLibsndfileMajorFormat = @"Major Format";
SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyLibsndfileSubtype = @"Subtype";
SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyLibsndfileFileEndian = @"File Endian";

@interface AVAudioFormat (SFBCommonFormatTransformation)
/// Returns the specified common format with the same sample rate and channel layout as \c self
- (nullable AVAudioFormat *)transformedToCommonFormat:(AVAudioCommonFormat)commonFormat interleaved:(BOOL)interleaved;
@end

@implementation AVAudioFormat (SFBCommonFormatTransformation)

- (nullable AVAudioFormat *)transformedToCommonFormat:(AVAudioCommonFormat)commonFormat interleaved:(BOOL)interleaved
{
	if(self.channelLayout)
		return [[AVAudioFormat alloc] initWithCommonFormat:commonFormat sampleRate:self.sampleRate interleaved:interleaved channelLayout:self.channelLayout];
	else
		return [[AVAudioFormat alloc] initWithCommonFormat:commonFormat sampleRate:self.sampleRate channels:self.channelCount interleaved:interleaved];
}

@end

/// Returns the major format corresponding to \c pathExtension or \c 0 if none
static int MajorFormatForExtension(NSString *pathExtension)
{
	NSCParameterAssert(pathExtension != nil);

	int majorCount = 0;
	sf_command(NULL, SFC_GET_FORMAT_MAJOR_COUNT, &majorCount, sizeof(int));

	for(int i = 0; i < majorCount; ++i) {
		SF_FORMAT_INFO formatInfo;
		formatInfo.format = i;
		if(!sf_command(NULL, SFC_GET_FORMAT_MAJOR, &formatInfo, sizeof(formatInfo))) {
			if([pathExtension isEqualToString:[NSString stringWithUTF8String:formatInfo.extension]])
				return formatInfo.format;
		}
		else
			os_log_debug(gSFBAudioEncoderLog, "sf_command (SFC_GET_FORMAT_MAJOR) %d failed", i);
	}

	return 0;
}

/// Returns the subtype corresponding to \c format or \c 0 if none
static int InferSubtypeFromFormat(AVAudioFormat *format)
{
	NSCParameterAssert(format != nil);

	const AudioStreamBasicDescription *asbd = format.streamDescription;
	if(asbd->mFormatID != kAudioFormatLinearPCM)
		return 0;

	if(asbd->mFormatFlags & kAudioFormatFlagIsFloat) {
		if(asbd->mBitsPerChannel == 32)
			return SF_FORMAT_FLOAT;
		else if(asbd->mBitsPerChannel == 64)
			return SF_FORMAT_DOUBLE;
	}
	else {
		switch(asbd->mBitsPerChannel) {
			case 8:
				if(asbd->mFormatFlags & kAudioFormatFlagIsSignedInteger)
					return SF_FORMAT_PCM_S8;
				else
					return SF_FORMAT_PCM_U8;
			case 16:
				if(asbd->mFormatFlags & kAudioFormatFlagIsSignedInteger)
					return SF_FORMAT_PCM_16;
				break;
			case 24:
				if(asbd->mFormatFlags & kAudioFormatFlagIsSignedInteger)
					return SF_FORMAT_PCM_24;
				break;
			case 32:
				if(asbd->mFormatFlags & kAudioFormatFlagIsSignedInteger)
					return SF_FORMAT_PCM_32;
				break;
		}
	}

	return 0;
}

enum WriteMethod {
	Unknown,
	Short,
	Int,
	Float,
	Double
};

static sf_count_t my_sf_vio_get_filelen(void *user_data)
{
	NSCParameterAssert(user_data != NULL);

	SFBLibsndfileEncoder *encoder = (__bridge SFBLibsndfileEncoder *)user_data;
	NSInteger length;
	if(![encoder->_outputSource getLength:&length error:nil])
		return -1;
	return length;
}

static sf_count_t my_sf_vio_seek(sf_count_t offset, int whence, void *user_data)
{
	NSCParameterAssert(user_data != NULL);

	SFBLibsndfileEncoder *encoder = (__bridge SFBLibsndfileEncoder *)user_data;
	if(!encoder->_outputSource.supportsSeeking)
		return -1;

	switch(whence) {
		case SEEK_SET:
			// offset remains unchanged
			break;
		case SEEK_CUR: {
			NSInteger inputSourceOffset;
			if([encoder->_outputSource getOffset:&inputSourceOffset error:nil])
				offset += inputSourceOffset;
			break;
		}
		case SEEK_END: {
			NSInteger inputSourceLength;
			if([encoder->_outputSource getLength:&inputSourceLength error:nil])
				offset += inputSourceLength;
			break;
		}
	}

	if(![encoder->_outputSource seekToOffset:offset error:nil])
		return -1;

	NSInteger inputSourceOffset;
	if(![encoder->_outputSource getOffset:&inputSourceOffset error:nil])
		return -1;

	return inputSourceOffset;
}

static sf_count_t my_sf_vio_read(void *ptr, sf_count_t count, void *user_data)
{
	NSCParameterAssert(user_data != NULL);

	SFBLibsndfileEncoder *encoder = (__bridge SFBLibsndfileEncoder *)user_data;

	NSInteger bytesRead;
	if(![encoder->_outputSource readBytes:ptr length:count bytesRead:&bytesRead error:nil])
		return -1;
	return bytesRead;
}

static sf_count_t my_sf_vio_write(const void *ptr, sf_count_t count, void *user_data)
{
	NSCParameterAssert(user_data != NULL);

	SFBLibsndfileEncoder *encoder = (__bridge SFBLibsndfileEncoder *)user_data;

	NSInteger bytesWritten;
	if(![encoder->_outputSource writeBytes:ptr length:(NSInteger)count bytesWritten:&bytesWritten error:nil])
		return 0;

	return bytesWritten;
}

static sf_count_t my_sf_vio_tell(void *user_data)
{
	NSCParameterAssert(user_data != NULL);

	SFBLibsndfileEncoder *encoder = (__bridge SFBLibsndfileEncoder *)user_data;
	NSInteger offset;
	if(![encoder->_outputSource getOffset:&offset error:nil])
		return -1;
	return offset;
}

@interface SFBLibsndfileEncoder ()
{
@private
	SNDFILE *_sndfile;;
	SF_INFO	_sfinfo;
	enum WriteMethod _writeMethod;
}
@end

@implementation SFBLibsndfileEncoder

+ (void)load
{
	[SFBAudioEncoder registerSubclass:[self class] priority:-50];
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
				os_log_debug(gSFBAudioEncoderLog, "sf_command (SFC_GET_FORMAT_MAJOR) %d failed", i);
		}

		pathExtensions = [majorModeExtensions copy];
	});

	return pathExtensions;
}

+ (NSSet *)supportedMIMETypes
{
	return [NSSet set];
}

+ (SFBAudioEncoderName)encoderName
{
	return SFBAudioEncoderNameLibsndfile;
}

- (BOOL)encodingIsLossless
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

- (AVAudioFormat *)processingFormatForSourceFormat:(AVAudioFormat *)sourceFormat
{
	NSParameterAssert(sourceFormat != nil);

	// Validate format
	const AudioStreamBasicDescription *asbd = sourceFormat.streamDescription;

	if(asbd->mFormatID != kAudioFormatLinearPCM)
		return nil;

	// Floating point
	if(asbd->mFormatFlags & kAudioFormatFlagIsFloat) {
		if(asbd->mBitsPerChannel == 32)
			return [sourceFormat transformedToCommonFormat:AVAudioPCMFormatFloat32 interleaved:YES];
		else if(asbd->mBitsPerChannel != 64)
			return [sourceFormat transformedToCommonFormat:AVAudioPCMFormatFloat64 interleaved:YES];
	}
	// Integer
	else {
		AudioStreamBasicDescription streamDescription = {0};

		streamDescription.mFormatID				= kAudioFormatLinearPCM;
		streamDescription.mFormatFlags			= kAudioFormatFlagsNativeEndian | kAudioFormatFlagIsSignedInteger;

		streamDescription.mSampleRate			= asbd->mSampleRate;
		streamDescription.mChannelsPerFrame		= asbd->mChannelsPerFrame;

		streamDescription.mBitsPerChannel		= ((asbd->mBitsPerChannel + 7) / 8) * 8;
		streamDescription.mFormatFlags			|= streamDescription.mBitsPerChannel % 8 ? kAudioFormatFlagIsAlignedHigh : kAudioFormatFlagIsPacked;

		streamDescription.mBytesPerPacket		= ((streamDescription.mBitsPerChannel + 7) / 8) * streamDescription.mChannelsPerFrame;
		streamDescription.mFramesPerPacket		= 1;
		streamDescription.mBytesPerFrame		= streamDescription.mBytesPerPacket / streamDescription.mFramesPerPacket;

		// TODO: what channel layout is appropriate?
		AVAudioChannelLayout *channelLayout = nil;
		return [[AVAudioFormat alloc] initWithStreamDescription:&streamDescription channelLayout:channelLayout];
	}

	return nil;
}

- (BOOL)openReturningError:(NSError **)error
{
	if(![super openReturningError:error])
		return NO;

	int majorFormat = 0;
	NSNumber *majorFormatSetting = [_settings objectForKey:SFBAudioEncodingSettingsKeyLibsndfileMajorFormat];
	if(majorFormatSetting != nil) {
		int intValue = majorFormatSetting.intValue;
		switch(intValue) {
			case SFBAudioEncoderLibsndfileMajorFormatWAV:		majorFormat = SF_FORMAT_WAV;		break;
			case SFBAudioEncoderLibsndfileMajorFormatAIFF:		majorFormat = SF_FORMAT_AIFF;		break;
			case SFBAudioEncoderLibsndfileMajorFormatAU:		majorFormat = SF_FORMAT_AU;			break;
			case SFBAudioEncoderLibsndfileMajorFormatRaw:		majorFormat = SF_FORMAT_RAW;		break;
			case SFBAudioEncoderLibsndfileMajorFormatPAF:		majorFormat = SF_FORMAT_PAF;		break;
			case SFBAudioEncoderLibsndfileMajorFormatSVX:		majorFormat = SF_FORMAT_SVX;		break;
			case SFBAudioEncoderLibsndfileMajorFormatNIST:		majorFormat = SF_FORMAT_NIST;		break;
			case SFBAudioEncoderLibsndfileMajorFormatVOC:		majorFormat = SF_FORMAT_VOC;		break;
			case SFBAudioEncoderLibsndfileMajorFormatIRCAM:		majorFormat = SF_FORMAT_IRCAM;		break;
			case SFBAudioEncoderLibsndfileMajorFormatW64:		majorFormat = SF_FORMAT_W64;		break;
			case SFBAudioEncoderLibsndfileMajorFormatMAT4:		majorFormat = SF_FORMAT_MAT4;		break;
			case SFBAudioEncoderLibsndfileMajorFormatMAT5:		majorFormat = SF_FORMAT_MAT5;		break;
			case SFBAudioEncoderLibsndfileMajorFormatPVF:		majorFormat = SF_FORMAT_PVF;		break;
			case SFBAudioEncoderLibsndfileMajorFormatXI:		majorFormat = SF_FORMAT_XI;			break;
			case SFBAudioEncoderLibsndfileMajorFormatHTK:		majorFormat = SF_FORMAT_HTK;		break;
			case SFBAudioEncoderLibsndfileMajorFormatSDS:		majorFormat = SF_FORMAT_SDS;		break;
			case SFBAudioEncoderLibsndfileMajorFormatAVR:		majorFormat = SF_FORMAT_AVR;		break;
			case SFBAudioEncoderLibsndfileMajorFormatWAVEX:		majorFormat = SF_FORMAT_WAVEX;		break;
			case SFBAudioEncoderLibsndfileMajorFormatSD2:		majorFormat = SF_FORMAT_SD2;		break;
			case SFBAudioEncoderLibsndfileMajorFormatFLAC:		majorFormat = SF_FORMAT_FLAC;		break;
			case SFBAudioEncoderLibsndfileMajorFormatCAF:		majorFormat = SF_FORMAT_CAF;		break;
			case SFBAudioEncoderLibsndfileMajorFormatWVE:		majorFormat = SF_FORMAT_WVE;		break;
			case SFBAudioEncoderLibsndfileMajorFormatOgg:		majorFormat = SF_FORMAT_OGG;		break;
			case SFBAudioEncoderLibsndfileSubtypeOpus:
			case SFBAudioEncoderLibsndfileMajorFormatMPC2K:		majorFormat = SF_FORMAT_MPC2K;		break;
			case SFBAudioEncoderLibsndfileMajorFormatRF64:		majorFormat = SF_FORMAT_RF64;		break;
			default:
				os_log_error(gSFBAudioEncoderLog, "Ignoring unknown Libsndfile major format: 0x%x", intValue);
				break;
		}
	}
	else {
		majorFormat = MajorFormatForExtension(_outputSource.url.pathExtension);
		os_log_info(gSFBAudioEncoderLog, "SFBAudioEncodingSettingsKeyLibsndfileMajorFormat is not set: guessed 0x%x based on extension \"%{public}@\"", majorFormat, _outputSource.url.pathExtension);
	}

	int subtype = 0;
	NSNumber *subtypeSetting = [_settings objectForKey:SFBAudioEncodingSettingsKeyLibsndfileSubtype];
	if(subtypeSetting != nil) {
		int intValue = subtypeSetting.intValue;
		switch(intValue) {
			case SFBAudioEncoderLibsndfileSubtypePCM_S8:		subtype = SF_FORMAT_PCM_S8;			break;
			case SFBAudioEncoderLibsndfileSubtypePCM_16:		subtype = SF_FORMAT_PCM_16;			break;
			case SFBAudioEncoderLibsndfileSubtypePCM_24:		subtype = SF_FORMAT_PCM_24;			break;
			case SFBAudioEncoderLibsndfileSubtypePCM_32:		subtype = SF_FORMAT_PCM_32;			break;
			case SFBAudioEncoderLibsndfileSubtypePCM_U8:		subtype = SF_FORMAT_PCM_U8;			break;
			case SFBAudioEncoderLibsndfileSubtypeFloat:			subtype = SF_FORMAT_FLAC;			break;
			case SFBAudioEncoderLibsndfileSubtypeDouble:		subtype = SF_FORMAT_DOUBLE;			break;
			case SFBAudioEncoderLibsndfileSubtypeµLAW:			subtype = SF_FORMAT_ULAW;			break;
			case SFBAudioEncoderLibsndfileSubtypeALAW:			subtype = SF_FORMAT_ALAW;			break;
			case SFBAudioEncoderLibsndfileSubtypeIMA_ADPCM:		subtype = SF_FORMAT_IMA_ADPCM;		break;
			case SFBAudioEncoderLibsndfileSubtypeMS_ADPCM:		subtype = SF_FORMAT_MS_ADPCM;		break;
			case SFBAudioEncoderLibsndfileSubtypeGSM610:		subtype = SF_FORMAT_GSM610;			break;
			case SFBAudioEncoderLibsndfileSubtypeVOX_ADPCM:		subtype = SF_FORMAT_VOX_ADPCM;		break;
			case SFBAudioEncoderLibsndfileSubtypeNMS_ADPCM_16:	subtype = SF_FORMAT_NMS_ADPCM_16;	break;
			case SFBAudioEncoderLibsndfileSubtypeNMS_ADPCM_24:	subtype = SF_FORMAT_NMS_ADPCM_24;	break;
			case SFBAudioEncoderLibsndfileSubtypeNMS_ADPCM_32:	subtype = SF_FORMAT_NMS_ADPCM_32;	break;
			case SFBAudioEncoderLibsndfileSubtypeG721_32:		subtype = SF_FORMAT_G721_32;		break;
			case SFBAudioEncoderLibsndfileSubtypeG723_24:		subtype = SF_FORMAT_G723_24;		break;
			case SFBAudioEncoderLibsndfileSubtypeG723_40:		subtype = SF_FORMAT_G723_40;		break;
			case SFBAudioEncoderLibsndfileSubtypeDWVW_12:		subtype = SF_FORMAT_DWVW_12;		break;
			case SFBAudioEncoderLibsndfileSubtypeDWVW_16:		subtype = SF_FORMAT_DWVW_16;		break;
			case SFBAudioEncoderLibsndfileSubtypeDWVW_24:		subtype = SF_FORMAT_DWVW_24;		break;
			case SFBAudioEncoderLibsndfileSubtypeDWVW_N:		subtype = SF_FORMAT_DWVW_N;			break;
			case SFBAudioEncoderLibsndfileSubtypeDPCM_8:		subtype = SF_FORMAT_DPCM_8;			break;
			case SFBAudioEncoderLibsndfileSubtypeDPCM_16:		subtype = SF_FORMAT_DPCM_16;		break;
			case SFBAudioEncoderLibsndfileSubtypeVorbis:		subtype = SF_FORMAT_VORBIS;			break;
			case SFBAudioEncoderLibsndfileSubtypeOpus:			subtype = SF_FORMAT_OPUS;			break;
			case SFBAudioEncoderLibsndfileSubtypeALAC_16:		subtype = SF_FORMAT_ALAC_16;		break;
			case SFBAudioEncoderLibsndfileSubtypeALAC_20:		subtype = SF_FORMAT_ALAC_20;		break;
			case SFBAudioEncoderLibsndfileSubtypeALAC_24:		subtype = SF_FORMAT_ALAC_24;		break;
			case SFBAudioEncoderLibsndfileSubtypeALAC_32:		subtype = SF_FORMAT_ALAC_32;		break;
			default:
				os_log_error(gSFBAudioEncoderLog, "Ignoring unknown Libsndfile subtype: 0x%x", intValue);
				break;
		}
	}
	else {
		subtype = InferSubtypeFromFormat(_processingFormat);
		os_log_info(gSFBAudioEncoderLog, "SFBAudioEncodingSettingsKeyLibsndfileSubtype is not set: guessed 0x%x based on format %{public}@", subtype, _processingFormat);
	}

	int endian = 0;
	NSNumber *endianSetting = [_settings objectForKey:SFBAudioEncodingSettingsKeyLibsndfileFileEndian];
	if(endianSetting != nil) {
		int intValue = endianSetting.intValue;
		switch(intValue) {
			case SFBAudioEncoderLibsndfileEndianDefault:	endian = SF_ENDIAN_FILE;	break;
			case SFBAudioEncoderLibsndfileEndianLittle:		endian = SF_ENDIAN_LITTLE;	break;
			case SFBAudioEncoderLibsndfileEndianBig:		endian = SF_ENDIAN_BIG;		break;
			case SFBAudioEncoderLibsndfileEndianCPU:		endian = SF_ENDIAN_CPU;		break;
			default:
				os_log_error(gSFBAudioEncoderLog, "Ignoring unknown Libsndfile endian-ness: 0x%x", intValue);
				break;
		}
	}

	_sfinfo.samplerate 	= (int)_processingFormat.sampleRate;
	_sfinfo.channels 	= (int)_processingFormat.channelCount;
	_sfinfo.format		= majorFormat | subtype | endian;
	_sfinfo.seekable	= 1;

	switch(subtype) {
		case SF_FORMAT_PCM_U8:
		case SF_FORMAT_PCM_S8:
		case SF_FORMAT_PCM_16:
				_writeMethod = Short;
			break;
		case SF_FORMAT_PCM_24:
		case SF_FORMAT_PCM_32:
			_writeMethod = Int;
			break;
		case SF_FORMAT_FLOAT:
			_writeMethod = Float;
			break;
		case SF_FORMAT_DOUBLE:
			_writeMethod = Double;
			break;
		default:
			os_log_error(gSFBAudioEncoderLog, "Unsupported subtype: 0x%x", subtype);
			if(error)
				*error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain code:SFBAudioEncoderErrorCodeInternalError userInfo:nil];
			return NO;
	}

	// Set up the virtual IO function pointers
	SF_VIRTUAL_IO virtualIO;
	virtualIO.get_filelen	= my_sf_vio_get_filelen;
	virtualIO.seek			= my_sf_vio_seek;
	virtualIO.read			= my_sf_vio_read;
	virtualIO.write			= my_sf_vio_write;
	virtualIO.tell			= my_sf_vio_tell;

	// Open the output file
	_sndfile = sf_open_virtual(&virtualIO, SFM_WRITE, &_sfinfo, (__bridge void *)self);
	if(!_sndfile) {
		os_log_error(gSFBAudioEncoderLog, "sf_open_virtual failed: %{public}s", sf_error_number(sf_error(NULL)));

		if(error)
			*error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain code:SFBAudioEncoderErrorCodeInvalidFormat userInfo:@{
				NSLocalizedDescriptionKey: NSLocalizedString(@"The output format is not supported by Libsndfile.", @""),
				NSLocalizedFailureReasonErrorKey: NSLocalizedString(@"Unsupported format", @""),
				NSLocalizedRecoverySuggestionErrorKey: NSLocalizedString(@"The format is not supported.", @"")
			}];

		return NO;
	}

	return YES;
}

- (BOOL)closeReturningError:(NSError **)error
{
	if(_sndfile) {
		int result = sf_close(_sndfile);
		if(result)
			os_log_error(gSFBAudioEncoderLog, "sf_close failed: %{public}s", sf_error_number(result));
		_sndfile = NULL;
	}

	_writeMethod = Unknown;

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

- (BOOL)encodeFromBuffer:(AVAudioPCMBuffer *)buffer frameLength:(AVAudioFrameCount)frameLength error:(NSError **)error
{
	NSParameterAssert(buffer != nil);

	if(![buffer.format isEqual:_processingFormat]) {
		os_log_debug(gSFBAudioEncoderLog, "-encodeFromBuffer:frameLength:error: called with invalid parameters");
		if(error)
			*error = [NSError errorWithDomain:NSOSStatusErrorDomain code:paramErr userInfo:nil];
		return NO;
	}

	if(frameLength > buffer.frameLength)
		frameLength = buffer.frameLength;

	sf_count_t framesWritten = 0;
	switch(_writeMethod) {
		case Short:
			framesWritten = sf_writef_short(_sndfile, (const short *)buffer.audioBufferList->mBuffers[0].mData, frameLength);
			break;
		case Int:
			framesWritten = sf_writef_int(_sndfile, (const int *)buffer.audioBufferList->mBuffers[0].mData, frameLength);
			break;
		case Float:
			framesWritten = sf_writef_float(_sndfile, (const float *)buffer.audioBufferList->mBuffers[0].mData, frameLength);
			break;
		case Double:
			framesWritten = sf_writef_double(_sndfile, (const double *)buffer.audioBufferList->mBuffers[0].mData, frameLength);
			break;
		default:
			os_log_error(gSFBAudioEncoderLog, "Unknown libsndfile write method: %d", _writeMethod);
			if(error)
				*error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain code:SFBAudioEncoderErrorCodeInternalError userInfo:nil];
			return NO;
	}

	int result = sf_error(_sndfile);
	if(result) {
		os_log_error(gSFBAudioEncoderLog, "sf_writef_XXX failed: %{public}s", sf_error_number(result));
		if(error)
			*error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain code:SFBAudioEncoderErrorCodeInternalError userInfo:nil];
		return NO;
	}

	return YES;
}

- (BOOL)finishEncodingReturningError:(NSError **)error
{
	return YES;
}

@end
