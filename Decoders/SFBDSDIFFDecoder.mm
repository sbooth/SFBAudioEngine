/*
 * Copyright (c) 2014 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <os/log.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#import "SFBDSDIFFDecoder.h"

#import "AVAudioChannelLayout+SFBChannelLabels.h"
#import "NSError+SFBURLPresentation.h"
#import "SFBCStringForOSType.h"

SFBDSDDecoderName const SFBDSDDecoderNameDSDIFF = @"org.sbooth.AudioEngine.DSDDecoder.DSDIFF";

namespace {

	// Convert a four byte chunk ID to a uint32_t
	inline uint32_t BytesToID(char bytes [4])
	{
		auto one	= bytes[0];
		auto two	= bytes[1];
		auto three	= bytes[2];
		auto four	= bytes[3];

		// Verify well-formedness
		if(!isprint(one) || !isprint(two) || !isprint(three) || !isprint(four))
			return 0;

		if(isspace(one))
			return 0;

		if(isspace(two) && isspace(one))
			return 0;

		if(isspace(three) && isspace(two) && isspace(one))
			return 0;

		if(isspace(four) && isspace(three) && isspace(two) && isspace(one))
			return 0;

		return (uint32_t)((one << 24u) | (two << 16u) | (three << 8u) | four);
	}

	// Read an ID as a uint32_t, performing validation
	bool ReadID(SFBInputSource *inputSource, uint32_t& chunkID)
	{
		NSCParameterAssert(inputSource != nil);

		char chunkIDBytes [4];
		NSInteger bytesRead;
		if(![inputSource readBytes:chunkIDBytes length:4 bytesRead:&bytesRead error:nil] || bytesRead != 4) {
			os_log_error(gSFBDSDDecoderLog, "Unable to read chunk ID");
			return false;
		}

		chunkID = BytesToID(chunkIDBytes);
		if(0 == chunkID) {
			os_log_error(gSFBDSDDecoderLog, "Illegal chunk ID");
			return false;
		}

		return true;
	}

	inline AudioChannelLabel DSDIFFChannelIDToCoreAudioChannelLabel(uint32_t channelID)
	{
		switch(channelID) {
			case 'SLFT':	return kAudioChannelLabel_Left;
			case 'SRGT':	return kAudioChannelLabel_Right;
			case 'MLFT':	return kAudioChannelLabel_LeftSurroundDirect;
			case 'MRGT':	return kAudioChannelLabel_RightSurroundDirect;
			case 'LS  ':	return kAudioChannelLabel_LeftSurround;
			case 'RS  ':	return kAudioChannelLabel_RightSurround;
			case 'C   ':	return kAudioChannelLabel_Center;
			case 'LFE ':	return kAudioChannelLabel_LFE2;
		}

		return kAudioChannelLabel_Unknown;
	}

#pragma mark DSDIFF chunks

	// Base class for DSDIFF chunks
	struct DSDIFFChunk : std::enable_shared_from_this<DSDIFFChunk>
	{
		using shared_ptr = std::shared_ptr<DSDIFFChunk>;
		using chunk_map = std::map<uint32_t, shared_ptr>;

		// Shared pointer support
		shared_ptr getptr()					{ return shared_from_this(); }

		uint32_t mChunkID;
		uint64_t mDataSize;

		int64_t mDataOffset;
	};

	// 'FRM8'
	struct FormDSDChunk : public DSDIFFChunk
	{
		uint32_t mFormType;
		chunk_map mLocalChunks;
	};

	// 'FVER' in 'FRM8'
	struct FormatVersionChunk : public DSDIFFChunk
	{
		static const uint32_t kSupportedFormatVersion = 0x01050000;
		uint32_t mFormatVersion;
	};

	// 'PROP' in 'FRM8'
	struct PropertyChunk : public DSDIFFChunk
	{
		uint32_t mPropertyType;
		chunk_map mLocalChunks;
	};

	// 'FS  ' in 'PROP'
	struct SampleRateChunk : public DSDIFFChunk
	{
		uint32_t mSampleRate;
	};

	// 'CHNL' in 'PROP'
	struct ChannelsChunk : public DSDIFFChunk
	{
		uint16_t mNumberChannels;
		std::vector<uint32_t> mChannelIDs;
	};

	// 'CMPR' in 'PROP'
	struct CompressionTypeChunk : public DSDIFFChunk
	{
		uint32_t mCompressionType;
		std::string mCompressionName;
	};

	// 'ABSS' in 'PROP'
	struct AbsoluteStartTimeChunk : public DSDIFFChunk
	{
		uint16_t mHours;
		uint8_t mMinutes;
		uint8_t mSeconds;
		uint32_t mSamples;
	};

	// 'LSCO' in 'PROP'
	struct LoudspeakerConfigurationChunk : public DSDIFFChunk
	{
		uint16_t mLoudspeakerConfiguration;
	};

	// 'DSD ' in 'FRM8'
	struct DSDSoundDataChunk : public DSDIFFChunk
	{};

	// 'DST ', 'DSTI', 'COMT', 'DIIN', 'MANF' are not handled

//	// 'DST ' in 'FRM8'
//	class DSTSoundDataChunk : public DSDIFFChunk
//	{};
//
//	// 'FRTE' in 'DST '
//	class DSTFrameInformationChunk : public DSDIFFChunk
//	{};
//
//	// 'FRTE' in 'DST '
//	class DSTFrameDataChunk : public DSDIFFChunk
//	{};
//
//	// 'FRTE' in 'DST '
//	class DSTFrameCRCChunk : public DSDIFFChunk
//	{};
//
//	// 'DSTI' in 'FRM8'
//	class DSTSoundIndexChunk : public DSDIFFChunk
//	{};
//
//	// 'COMT' in 'FRM8'
//	class CommentsChunk : public DSDIFFChunk
//	{};
//
//	// 'DIIN' in 'FRM8'
//	class EditedMasterInformationChunk : public DSDIFFChunk
//	{};
//
//	// 'EMID' in 'DIIN'
//	class EditedMasterIDChunk : public DSDIFFChunk
//	{};
//
//	// 'MARK' in 'DIIN'
//	class MarkerChunk : public DSDIFFChunk
//	{};
//
//	// 'DIAR' in 'DIIN'
//	class ArtistChunk : public DSDIFFChunk
//	{};
//
//	// 'DITI' in 'DIIN'
//	class TitleChunk : public DSDIFFChunk
//	{};
//
//	// 'MANF' in 'FRM8'
//	class ManufacturerSpecificChunk : public DSDIFFChunk
//	{};

#pragma mark DSDIFF parsing

	bool ReadChunkIDAndDataSize(SFBInputSource *inputSource, uint32_t& chunkID, uint64_t& chunkDataSize)
	{
		if(!ReadID(inputSource, chunkID))
			return false;

		if(![inputSource readUInt64BigEndian:&chunkDataSize error:nil]) {
			os_log_error(gSFBDSDDecoderLog, "Unable to read chunk data size");
			return false;
		}

		return true;
	}

	std::shared_ptr<FormatVersionChunk> ParseFormatVersionChunk(SFBInputSource *inputSource, const uint32_t chunkID, const uint64_t chunkDataSize)
	{
		if(chunkID != 'FVER') {
			os_log_error(gSFBDSDDecoderLog, "Invalid chunk ID for 'FVER' chunk");
			return nullptr;
		}

		auto result = std::make_shared<FormatVersionChunk>();

		result->mChunkID = chunkID;
		result->mDataSize = chunkDataSize;
		NSInteger offset;
		if(![inputSource getOffset:&offset error:nil]) {
			os_log_error(gSFBDSDDecoderLog, "Error getting chunk data offset");
			return nullptr;
		}
		result->mDataOffset = offset;

		if(![inputSource readUInt32BigEndian:&result->mFormatVersion error:nil]) {
			os_log_error(gSFBDSDDecoderLog, "Unable to read format version in 'FVER' chunk");
			return nullptr;
		}

		if(result->mFormatVersion > FormatVersionChunk::kSupportedFormatVersion) {
			os_log_error(gSFBDSDDecoderLog, "Unsupported format version in 'FVER': %u", result->mFormatVersion);
			return nullptr;
		}

		return result;
	}

	std::shared_ptr<SampleRateChunk> ParseSampleRateChunk(SFBInputSource *inputSource, const uint32_t chunkID, const uint64_t chunkDataSize)
	{
		if(chunkID != 'FS  ') {
			os_log_error(gSFBDSDDecoderLog, "Invalid chunk ID for 'FS  ' chunk");
			return nullptr;
		}

		auto result = std::make_shared<SampleRateChunk>();

		result->mChunkID = chunkID;
		result->mDataSize = chunkDataSize;
		NSInteger offset;
		if(![inputSource getOffset:&offset error:nil]) {
			os_log_error(gSFBDSDDecoderLog, "Error getting chunk data offset");
			return nullptr;
		}
		result->mDataOffset = offset;

		if(![inputSource readUInt32BigEndian:&result->mSampleRate error:nil]) {
			os_log_error(gSFBDSDDecoderLog, "Unable to read sample rate in 'FS  ' chunk");
			return nullptr;
		}

		return result;
	}

	std::shared_ptr<ChannelsChunk> ParseChannelsChunk(SFBInputSource *inputSource, const uint32_t chunkID, const uint64_t chunkDataSize)
	{
		if(chunkID != 'CHNL') {
			os_log_error(gSFBDSDDecoderLog, "Invalid chunk ID for 'CHNL' chunk");
			return nullptr;
		}

		auto result = std::make_shared<ChannelsChunk>();

		result->mChunkID = chunkID;
		result->mDataSize = chunkDataSize;
		NSInteger offset;
		if(![inputSource getOffset:&offset error:nil]) {
			os_log_error(gSFBDSDDecoderLog, "Error getting chunk data offset");
			return nullptr;
		}
		result->mDataOffset = offset;

		if(![inputSource readUInt16BigEndian:&result->mNumberChannels error:nil]) {
			os_log_error(gSFBDSDDecoderLog, "Unable to read number channels in 'CHNL' chunk");
			return nullptr;
		}

		for(uint16_t i = 0; i < result->mNumberChannels; ++i) {
			uint32_t channelID;
			if(!ReadID(inputSource, channelID)) {
				os_log_error(gSFBDSDDecoderLog, "Unable to read channel ID in 'CHNL' chunk");
				return nullptr;
			}
			result->mChannelIDs.push_back(channelID);
		}

		return result;
	}

	std::shared_ptr<CompressionTypeChunk> ParseCompressionTypeChunk(SFBInputSource *inputSource, const uint32_t chunkID, const uint64_t chunkDataSize)
	{
		if(chunkID != 'CMPR') {
			os_log_error(gSFBDSDDecoderLog, "Invalid chunk ID for 'CMPR' chunk");
			return nullptr;
		}

		auto result = std::make_shared<CompressionTypeChunk>();

		result->mChunkID = chunkID;
		result->mDataSize = chunkDataSize;
		NSInteger offset;
		if(![inputSource getOffset:&offset error:nil]) {
			os_log_error(gSFBDSDDecoderLog, "Error getting chunk data offset");
			return nullptr;
		}
		result->mDataOffset = offset;

		if(!ReadID(inputSource, result->mCompressionType)) {
			os_log_error(gSFBDSDDecoderLog, "Unable to read compression type in 'CMPR' chunk");
			return nullptr;
		}

		uint8_t count;
		if(![inputSource readUInt8:&count error:nil]) {
			os_log_error(gSFBDSDDecoderLog, "Unable to read count in 'CMPR' chunk");
			return nullptr;
		}

		char compressionName [count];
		NSInteger bytesRead;
		if(![inputSource readBytes:compressionName length:count bytesRead:&bytesRead error:nil] || bytesRead != count) {
			os_log_error(gSFBDSDDecoderLog, "Unable to read compressionName in 'CMPR' chunk");
			return nullptr;
		}

		result->mCompressionName = std::string(compressionName, count);

		// Chunks always have an even length
		if(![inputSource getOffset:&offset error:nil]) {
			os_log_error(gSFBDSDDecoderLog, "Error getting chunk data offset");
			return nullptr;
		}

		if(offset % 2) {
			uint8_t unused;
			if(![inputSource readUInt8:&unused error:nil]) {
				os_log_error(gSFBDSDDecoderLog, "Unable to read dummy byte in 'CMPR' chunk");
				return nullptr;
			}

		}

		return result;
	}

	std::shared_ptr<AbsoluteStartTimeChunk> ParseAbsoluteStartTimeChunk(SFBInputSource *inputSource, const uint32_t chunkID, const uint64_t chunkDataSize)
	{
		if(chunkID != 'ABSS') {
			os_log_error(gSFBDSDDecoderLog, "Invalid chunk ID for 'ABSS' chunk");
			return nullptr;
		}

		auto result = std::make_shared<AbsoluteStartTimeChunk>();

		result->mChunkID = chunkID;
		result->mDataSize = chunkDataSize;
		NSInteger offset;
		if(![inputSource getOffset:&offset error:nil]) {
			os_log_error(gSFBDSDDecoderLog, "Error getting chunk data offset");
			return nullptr;
		}
		result->mDataOffset = offset;

		if(![inputSource readUInt16BigEndian:&result->mHours error:nil]) {
			os_log_error(gSFBDSDDecoderLog, "Unable to read hours in 'ABSS' chunk");
			return nullptr;
		}

		if(![inputSource readUInt8:&result->mMinutes error:nil]) {
			os_log_error(gSFBDSDDecoderLog, "Unable to read minutes in 'ABSS' chunk");
			return nullptr;
		}

		if(![inputSource readUInt8:&result->mSeconds error:nil]) {
			os_log_error(gSFBDSDDecoderLog, "Unable to read seconds in 'ABSS' chunk");
			return nullptr;
		}

		if(![inputSource readUInt32BigEndian:&result->mSamples error:nil]) {
			os_log_error(gSFBDSDDecoderLog, "Unable to read samples in 'ABSS' chunk");
			return nullptr;
		}

		return result;
	}

	std::shared_ptr<LoudspeakerConfigurationChunk> ParseLoudspeakerConfigurationChunk(SFBInputSource *inputSource, const uint32_t chunkID, const uint64_t chunkDataSize)
	{
		if(chunkID != 'LSCO') {
			os_log_error(gSFBDSDDecoderLog, "Invalid chunk ID for 'LSCO' chunk");
			return nullptr;
		}

		auto result = std::make_shared<LoudspeakerConfigurationChunk>();

		result->mChunkID = chunkID;
		result->mDataSize = chunkDataSize;
		NSInteger offset;
		if(![inputSource getOffset:&offset error:nil]) {
			os_log_error(gSFBDSDDecoderLog, "Error getting chunk data offset");
			return nullptr;
		}
		result->mDataOffset = offset;

		if(![inputSource readUInt16BigEndian:&result->mLoudspeakerConfiguration error:nil]) {
			os_log_error(gSFBDSDDecoderLog, "Unable to read loudspeaker configuration in 'LSCO' chunk");
			return nullptr;
		}

		return result;
	}

	std::shared_ptr<PropertyChunk> ParsePropertyChunk(SFBInputSource *inputSource, const uint32_t chunkID, const uint64_t chunkDataSize)
	{
		if(chunkID != 'PROP') {
			os_log_error(gSFBDSDDecoderLog, "Invalid chunk ID for 'PROP' chunk");
			return nullptr;
		}

		auto result = std::make_shared<PropertyChunk>();

		result->mChunkID = chunkID;
		result->mDataSize = chunkDataSize;
		NSInteger offset;
		if(![inputSource getOffset:&offset error:nil]) {
			os_log_error(gSFBDSDDecoderLog, "Error getting chunk data offset");
			return nullptr;
		}
		result->mDataOffset = offset;

		if(!ReadID(inputSource, result->mPropertyType)) {
			os_log_error(gSFBDSDDecoderLog, "Unable to read property type in 'PROP' chunk");
			return nullptr;
		}

		if(result->mPropertyType != 'SND ') {
			os_log_error(gSFBDSDDecoderLog, "Unexpected property type in 'PROP' chunk: %u", result->mPropertyType);
			return nullptr;
		}

		// Parse the local chunks
		auto chunkDataSizeRemaining = result->mDataSize - 4; // adjust for mPropertyType
		while(chunkDataSizeRemaining) {

			uint32_t localChunkID;
			uint64_t localChunkDataSize;

			if(ReadChunkIDAndDataSize(inputSource, localChunkID, localChunkDataSize)) {
				switch(localChunkID) {
					case 'FS  ':
					{
						auto chunk = ParseSampleRateChunk(inputSource, localChunkID, localChunkDataSize);
						if(chunk)
							result->mLocalChunks[chunk->mChunkID] = chunk;
						break;
					}

					case 'CHNL':
					{
						auto chunk = ParseChannelsChunk(inputSource, localChunkID, localChunkDataSize);
						if(chunk)
							result->mLocalChunks[chunk->mChunkID] = chunk;
						break;
					}

					case 'CMPR':
					{
						auto chunk = ParseCompressionTypeChunk(inputSource, localChunkID, localChunkDataSize);
						if(chunk)
							result->mLocalChunks[chunk->mChunkID] = chunk;
						break;
					}

					case 'ABSS':
					{
						auto chunk = ParseAbsoluteStartTimeChunk(inputSource, localChunkID, localChunkDataSize);
						if(chunk)
							result->mLocalChunks[chunk->mChunkID] = chunk;
						break;
					}

					case 'LSCO':
					{
						auto chunk = ParseLoudspeakerConfigurationChunk(inputSource, localChunkID, localChunkDataSize);
						if(chunk)
							result->mLocalChunks[chunk->mChunkID] = chunk;
						break;
					}

						// Skip unrecognized or ignored chunks
					default:
						if(![inputSource getOffset:&offset error:nil]) {
							os_log_error(gSFBDSDDecoderLog, "Error getting chunk data offset");
							return nullptr;
						}

						if(![inputSource seekToOffset:(offset + (NSInteger)localChunkDataSize) error:nil]) {
							os_log_error(gSFBDSDDecoderLog, "Error skipping chunk data");
							return nullptr;
						}

						break;
				}

				chunkDataSizeRemaining -= 12;
				chunkDataSizeRemaining -= localChunkDataSize;
			}
			else {
				os_log_error(gSFBDSDDecoderLog, "Error reading local chunk in 'PROP' chunk");
				return nullptr;
			}
		}

		return result;
	}

	std::shared_ptr<DSDSoundDataChunk> ParseDSDSoundDataChunk(SFBInputSource *inputSource, const uint32_t chunkID, const uint64_t chunkDataSize)
	{
		if(chunkID != 'DSD ') {
			os_log_error(gSFBDSDDecoderLog, "Invalid chunk ID for 'DSD ' chunk");
			return nullptr;
		}

		auto result = std::make_shared<DSDSoundDataChunk>();

		result->mChunkID = chunkID;
		result->mDataSize = chunkDataSize;
		NSInteger offset;
		if(![inputSource getOffset:&offset error:nil]) {
			os_log_error(gSFBDSDDecoderLog, "Error getting chunk data offset");
			return nullptr;
		}
		result->mDataOffset = offset;

		// Skip the data
		if(![inputSource seekToOffset:(offset + (NSInteger)chunkDataSize) error:nil]) {
			os_log_error(gSFBDSDDecoderLog, "Error skipping chunk data");
			return nullptr;
		}

		return result;
	}

	std::unique_ptr<FormDSDChunk> ParseFormDSDChunk(SFBInputSource *inputSource, const uint32_t chunkID, const uint64_t chunkDataSize)
	{
		if(chunkID != 'FRM8') {
			os_log_error(gSFBDSDDecoderLog, "Missing 'FRM8' chunk");
			return nullptr;
		}

		auto result = std::make_unique<FormDSDChunk>();

		result->mChunkID = chunkID;
		result->mDataSize = chunkDataSize;
		NSInteger offset;
		if(![inputSource getOffset:&offset error:nil]) {
			os_log_error(gSFBDSDDecoderLog, "Error getting chunk data offset");
			return nullptr;
		}
		result->mDataOffset = offset;

		if(!ReadID(inputSource, result->mFormType)) {
			os_log_error(gSFBDSDDecoderLog, "Unable to read formType in 'FRM8' chunk");
			return nullptr;
		}

		if(result->mFormType != 'DSD ') {
			os_log_error(gSFBDSDDecoderLog, "Unexpected formType in 'FRM8' chunk: '%{public}.4s'", SFBCStringForOSType(result->mFormType));
			return nullptr;
		}

		// Parse the local chunks
		auto chunkDataSizeRemaining = result->mDataSize - 4; // adjust for mFormType
		while(chunkDataSizeRemaining) {

			uint32_t localChunkID;
			uint64_t localChunkDataSize;

			if(ReadChunkIDAndDataSize(inputSource, localChunkID, localChunkDataSize)) {
				switch(localChunkID) {
					case 'FVER':
					{
						auto chunk = ParseFormatVersionChunk(inputSource, localChunkID, localChunkDataSize);
						if(chunk)
							result->mLocalChunks[chunk->mChunkID] = chunk;
						break;
					}

					case 'PROP':
					{
						auto chunk = ParsePropertyChunk(inputSource, localChunkID, localChunkDataSize);
						if(chunk)
							result->mLocalChunks[chunk->mChunkID] = chunk;
						break;
					}

					case 'DSD ':
					{
						auto chunk = ParseDSDSoundDataChunk(inputSource, localChunkID, localChunkDataSize);
						if(chunk)
							result->mLocalChunks[chunk->mChunkID] = chunk;
						break;
					}

						// Skip unrecognized or ignored chunks
					default:
						if(![inputSource getOffset:&offset error:nil]) {
							os_log_error(gSFBDSDDecoderLog, "Error getting chunk data offset");
							return nullptr;
						}

						if(![inputSource seekToOffset:(offset + (NSInteger)localChunkDataSize) error:nil]) {
							os_log_error(gSFBDSDDecoderLog, "Error skipping chunk data");
							return nullptr;
						}

						break;
				}

				chunkDataSizeRemaining -= 12;
				chunkDataSizeRemaining -= localChunkDataSize;
			}
			else {
				os_log_error(gSFBDSDDecoderLog, "Error reading local chunk in 'FRM8' chunk");
				return nullptr;
			}
		}

		return result;
	}

	std::unique_ptr<FormDSDChunk> ParseDSDIFF(SFBInputSource *inputSource)
	{
		uint32_t chunkID;
		uint64_t chunkDataSize;
		if(!ReadChunkIDAndDataSize(inputSource, chunkID, chunkDataSize))
			return nullptr;

		return ParseFormDSDChunk(inputSource, chunkID, chunkDataSize);
	}

	static NSError * CreateInvalidDSDIFFFileError(NSURL * url)
	{
		return [NSError SFB_errorWithDomain:SFBDSDDecoderErrorDomain
									   code:SFBDSDDecoderErrorCodeInputOutput
			  descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid DSDIFF file.", @"")
										url:url
							  failureReason:NSLocalizedString(@"Not a DSDIFF file", @"")
						 recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
	}

}


@interface SFBDSDIFFDecoder ()
{
@private
	BOOL _isOpen;
	AVAudioFramePosition _packetPosition;
	AVAudioFramePosition _packetCount;
	int64_t _audioOffset;
	AVAudioCompressedBuffer *_buffer;
}
@end

@implementation SFBDSDIFFDecoder

+ (void)load
{
	[SFBDSDDecoder registerSubclass:[self class]];
}

+ (NSSet *)supportedPathExtensions
{
	return [NSSet setWithObject:@"dff"];
}

+ (NSSet *)supportedMIMETypes
{
	return [NSSet setWithObject:@"audio/dsdiff"];
}

+ (SFBDSDDecoderName)decoderName
{
	return SFBDSDDecoderNameDSDIFF;
}

- (BOOL)decodingIsLossless
{
	return YES;
}

- (BOOL)openReturningError:(NSError **)error
{
	if(![super openReturningError:error])
		return NO;

	auto chunks = ParseDSDIFF(_inputSource);
	if(!chunks) {
		os_log_error(gSFBDSDDecoderLog, "Error parsing file");
		if(error)
			*error = CreateInvalidDSDIFFFileError(_inputSource.url);
		return NO;
	}

	auto propertyChunk = std::static_pointer_cast<PropertyChunk>(chunks->mLocalChunks['PROP']);
	auto sampleRateChunk = std::static_pointer_cast<SampleRateChunk>(propertyChunk->mLocalChunks['FS  ']);
	auto channelsChunk = std::static_pointer_cast<ChannelsChunk>(propertyChunk->mLocalChunks['CHNL']);

	if(!propertyChunk || !sampleRateChunk || !channelsChunk) {
		os_log_error(gSFBDSDDecoderLog, "Missing chunk in file");
		if(error)
			*error = CreateInvalidDSDIFFFileError(_inputSource.url);
		return NO;
	}

	// Channel layouts are defined in the DSDIFF file format specification
	AVAudioChannelLayout *channelLayout = nil;
	if(channelsChunk->mChannelIDs.size() == 2 && channelsChunk->mChannelIDs[0] == 'SLFT' && channelsChunk->mChannelIDs[1] == 'SRGT')
		channelLayout = [AVAudioChannelLayout layoutWithLayoutTag:kAudioChannelLayoutTag_Stereo];
	else if(channelsChunk->mChannelIDs.size() == 5 && channelsChunk->mChannelIDs[0] == 'MLFT' && channelsChunk->mChannelIDs[1] == 'MRGT' && channelsChunk->mChannelIDs[2] == 'C   ' && channelsChunk->mChannelIDs[3] == 'LS  ' && channelsChunk->mChannelIDs[4] == 'RS  ')
		channelLayout = [AVAudioChannelLayout layoutWithLayoutTag:kAudioChannelLayoutTag_MPEG_5_0_A];
	else if(channelsChunk->mChannelIDs.size() == 6 && channelsChunk->mChannelIDs[0] == 'MLFT' && channelsChunk->mChannelIDs[1] == 'MRGT' && channelsChunk->mChannelIDs[2] == 'C   ' && channelsChunk->mChannelIDs[3] == 'LFE ' && channelsChunk->mChannelIDs[4] == 'LS  ' && channelsChunk->mChannelIDs[5] == 'RS  ')
		channelLayout = [AVAudioChannelLayout layoutWithLayoutTag:kAudioChannelLayoutTag_MPEG_5_1_A];
	else if(!channelsChunk->mChannelIDs.empty()) {
		std::vector<AudioChannelLabel> labels;
		for(auto channelID : channelsChunk->mChannelIDs)
			labels.push_back(DSDIFFChannelIDToCoreAudioChannelLabel(channelID));
		channelLayout = [[AVAudioChannelLayout alloc] initWithChannelLabels:&labels[0] count:(AVAudioChannelCount)labels.size()];
	}

	AudioStreamBasicDescription processingStreamDescription{};

	// The output format is raw DSD
	processingStreamDescription.mFormatID			= SFBAudioFormatIDDirectStreamDigital;
	processingStreamDescription.mFormatFlags		= kAudioFormatFlagIsBigEndian;

	processingStreamDescription.mSampleRate			= (Float64)sampleRateChunk->mSampleRate;
	processingStreamDescription.mChannelsPerFrame	= channelsChunk->mNumberChannels;
	processingStreamDescription.mBitsPerChannel		= 1;

	processingStreamDescription.mBytesPerPacket		= SFB_BYTES_PER_DSD_PACKET_PER_CHANNEL * channelsChunk->mNumberChannels;
	processingStreamDescription.mFramesPerPacket	= SFB_PCM_FRAMES_PER_DSD_PACKET;

	_processingFormat = [[AVAudioFormat alloc] initWithStreamDescription:&processingStreamDescription channelLayout:channelLayout];

	// Set up the source format
	AudioStreamBasicDescription sourceStreamDescription{};

	sourceStreamDescription.mFormatID			= SFBAudioFormatIDDirectStreamDigital;

	sourceStreamDescription.mSampleRate			= (Float64)sampleRateChunk->mSampleRate;
	sourceStreamDescription.mChannelsPerFrame	= channelsChunk->mNumberChannels;
	sourceStreamDescription.mBitsPerChannel		= 1;

	_sourceFormat = [[AVAudioFormat alloc] initWithStreamDescription:&sourceStreamDescription];

	auto soundDataChunk = std::static_pointer_cast<DSDSoundDataChunk>(chunks->mLocalChunks['DSD ']);
	if(!soundDataChunk) {
		os_log_error(gSFBDSDDecoderLog, "Missing chunk in file");
		if(error)
			*error = CreateInvalidDSDIFFFileError(_inputSource.url);
		return NO;
	}

	_audioOffset = soundDataChunk->mDataOffset;
	_packetCount = (AVAudioFramePosition)(soundDataChunk->mDataSize - 12) / (SFB_BYTES_PER_DSD_PACKET_PER_CHANNEL * channelsChunk->mNumberChannels);

	if(![_inputSource seekToOffset:_audioOffset error:error])
		return NO;

	_isOpen = YES;

	return YES;
}

- (BOOL)closeReturningError:(NSError **)error
{
	_isOpen = NO;
	return [super closeReturningError:error];
}

- (BOOL)isOpen
{
	return _isOpen;
}

- (AVAudioFramePosition)packetPosition
{
	return _packetPosition;
}

- (AVAudioFramePosition)packetCount
{
	return _packetCount;
}

- (BOOL)decodeIntoBuffer:(AVAudioCompressedBuffer *)buffer packetCount:(AVAudioPacketCount)packetCount error:(NSError **)error
{
	NSParameterAssert(buffer != nil);

	// Reset output buffer data size
	buffer.packetCount = 0;
	buffer.byteLength = 0;

	if(![buffer.format isEqual:_processingFormat]) {
		os_log_debug(gSFBDSDDecoderLog, "-decodeAudio:frameLength:error: called with invalid parameters");
		if(error)
			*error = [NSError errorWithDomain:NSOSStatusErrorDomain code:paramErr userInfo:nil];
		return NO;
	}

	if(packetCount > buffer.packetCapacity)
		packetCount = buffer.packetCapacity;

	AVAudioPacketCount packetsRemaining = (AVAudioPacketCount)(_packetCount - _packetPosition);
	AVAudioPacketCount packetsToRead = std::min(packetCount, packetsRemaining);
	AVAudioPacketCount packetsRead = 0;

	uint32_t packetSize = SFB_BYTES_PER_DSD_PACKET_PER_CHANNEL * _processingFormat.channelCount;

	for(;;) {
		// Read interleaved input, grouped as 8 one bit samples per frame (a single channel byte) into
		// a clustered frame (one channel byte per channel)

		uint8_t *buf = (uint8_t *)buffer.data + buffer.byteLength;
		NSInteger bytesToRead = std::min(packetsToRead * packetSize, buffer.byteCapacity - buffer.byteLength);

		NSInteger bytesRead;
		if(![_inputSource readBytes:buf length:bytesToRead bytesRead:&bytesRead error:error] || bytesRead != bytesToRead) {
			os_log_debug(gSFBDSDDecoderLog, "Error reading audio: requested %ld bytes, got %ld", (long)bytesToRead, bytesRead);
			break;
		}

		// Decoding is finished
		if(bytesRead == 0)
			break;

		packetsRead += (bytesRead / packetSize);

		buffer.packetCount += (AVAudioPacketCount)(bytesRead / packetSize);
		buffer.byteLength += (uint32_t)bytesRead;

		// All requested frames were read
		if(packetsRead == packetCount)
			break;

		packetsToRead -= packetsRead;
	}

	_packetPosition += packetsRead;

	return YES;
}

- (BOOL)seekToPacket:(AVAudioFramePosition)packet error:(NSError **)error
{
	NSParameterAssert(packet >= 0);

	NSInteger packetOffset = packet * SFB_BYTES_PER_DSD_PACKET_PER_CHANNEL * _processingFormat.channelCount;
	if(![_inputSource seekToOffset:(_audioOffset + packetOffset) error:error]) {
		os_log_debug(gSFBDSDDecoderLog, "-seekToPacket:error: failed seeking to input offset: %lld", _audioOffset + packetOffset);
		return NO;
	}

	_packetPosition = packet;
	return YES;
}

@end
