/*
 * Copyright (c) 2014 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#include <cctype>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <os/log.h>

#include <CoreFoundation/CoreFoundation.h>

#include "CFErrorUtilities.h"
#include "DSDIFFDecoder.h"
#include "SFBCStringForOSType.h"

#define BUFFER_CHANNEL_SIZE_BYTES 512u

namespace {

	void RegisterDSDIFFDecoder() __attribute__ ((constructor));
	void RegisterDSDIFFDecoder()
	{
		SFB::Audio::Decoder::RegisterSubclass<SFB::Audio::DSDIFFDecoder>();
	}

	// Missing from C++11 (from http://herbsutter.com/gotw/_102/)
	template<typename T, typename... Args>
	std::unique_ptr<T> make_unique(Args&&... args)
	{
		return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
	}

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
	bool ReadID(SFB::InputSource& inputSource, uint32_t& chunkID)
	{
		char chunkIDBytes [4];
		auto bytesRead = inputSource.Read(chunkIDBytes, 4);
		if(4 != bytesRead) {
			os_log_error(OS_LOG_DEFAULT, "Unable to read chunk ID");
			return false;
		}

		chunkID = BytesToID(chunkIDBytes);
		if(0 == chunkID) {
			os_log_error(OS_LOG_DEFAULT, "Illegal chunk ID");
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

	bool ReadChunkIDAndDataSize(SFB::InputSource& inputSource, uint32_t& chunkID, uint64_t& chunkDataSize)
	{
		if(!ReadID(inputSource, chunkID))
			return false;

		if(!inputSource.ReadBE<uint64_t>(chunkDataSize)) {
			os_log_error(OS_LOG_DEFAULT, "Unable to read chunk data size");
			return false;
		}

		return true;
	}

	std::shared_ptr<FormatVersionChunk> ParseFormatVersionChunk(SFB::InputSource& inputSource, const uint32_t chunkID, const uint64_t chunkDataSize)
	{
		if('FVER' != chunkID) {
			os_log_error(OS_LOG_DEFAULT, "Invalid chunk ID for 'FVER' chunk");
			return nullptr;
		}

		auto result = std::make_shared<FormatVersionChunk>();

		result->mChunkID = chunkID;
		result->mDataSize = chunkDataSize;
		result->mDataOffset = inputSource.GetOffset();

		if(!inputSource.ReadBE<uint32_t>(result->mFormatVersion)) {
			os_log_error(OS_LOG_DEFAULT, "Unable to read format version in 'FVER' chunk");
			return nullptr;
		}

		if(0x01050000 < result->mFormatVersion) {
			os_log_error(OS_LOG_DEFAULT, "Unsupported format version in 'FVER': %u", result->mFormatVersion);
			return nullptr;
		}

		return result;
	}

	std::shared_ptr<SampleRateChunk> ParseSampleRateChunk(SFB::InputSource& inputSource, const uint32_t chunkID, const uint64_t chunkDataSize)
	{
		if('FS  ' != chunkID) {
			os_log_error(OS_LOG_DEFAULT, "Invalid chunk ID for 'FS  ' chunk");
			return nullptr;
		}

		auto result = std::make_shared<SampleRateChunk>();

		result->mChunkID = chunkID;
		result->mDataSize = chunkDataSize;
		result->mDataOffset = inputSource.GetOffset();

		if(!inputSource.ReadBE<uint32_t>(result->mSampleRate)) {
			os_log_error(OS_LOG_DEFAULT, "Unable to read sample rate in 'FS  ' chunk");
			return nullptr;
		}

		return result;
	}

	std::shared_ptr<ChannelsChunk> ParseChannelsChunk(SFB::InputSource& inputSource, const uint32_t chunkID, const uint64_t chunkDataSize)
	{
		if('CHNL' != chunkID) {
			os_log_error(OS_LOG_DEFAULT, "Invalid chunk ID for 'CHNL' chunk");
			return nullptr;
		}

		auto result = std::make_shared<ChannelsChunk>();

		result->mChunkID = chunkID;
		result->mDataSize = chunkDataSize;
		result->mDataOffset = inputSource.GetOffset();

		if(!inputSource.ReadBE<uint16_t>(result->mNumberChannels)) {
			os_log_error(OS_LOG_DEFAULT, "Unable to read number channels in 'CHNL' chunk");
			return nullptr;
		}

		for(uint16_t i = 0; i < result->mNumberChannels; ++i) {
			uint32_t channelID;
			if(!ReadID(inputSource, channelID)) {
				os_log_error(OS_LOG_DEFAULT, "Unable to read channel ID in 'CHNL' chunk");
				return nullptr;
			}
			result->mChannelIDs.push_back(channelID);
		}

		return result;
	}

	std::shared_ptr<CompressionTypeChunk> ParseCompressionTypeChunk(SFB::InputSource& inputSource, const uint32_t chunkID, const uint64_t chunkDataSize)
	{
		if('CMPR' != chunkID) {
			os_log_error(OS_LOG_DEFAULT, "Invalid chunk ID for 'CMPR' chunk");
			return nullptr;
		}

		auto result = std::make_shared<CompressionTypeChunk>();

		result->mChunkID = chunkID;
		result->mDataSize = chunkDataSize;
		result->mDataOffset = inputSource.GetOffset();

		if(!ReadID(inputSource, result->mCompressionType)) {
			os_log_error(OS_LOG_DEFAULT, "Unable to read compression type in 'CMPR' chunk");
			return nullptr;
		}

		uint8_t count;
		if(!inputSource.ReadBE<uint8_t>(count)) {
			os_log_error(OS_LOG_DEFAULT, "Unable to read count in 'CMPR' chunk");
			return nullptr;
		}

		char compressionName [count];
		if(!inputSource.Read(compressionName, count)) {
			os_log_error(OS_LOG_DEFAULT, "Unable to read compressionName in 'CMPR' chunk");
			return nullptr;
		}

		result->mCompressionName = std::string(compressionName, count);

		// Chunks always have an even length
		if(1 == inputSource.GetOffset() % 2) {
			uint8_t unused;
			if(!inputSource.Read(&unused, 1)) {
				os_log_error(OS_LOG_DEFAULT, "Unable to read dummy byte in 'CMPR' chunk");
				return nullptr;
			}

		}

		return result;
	}

	std::shared_ptr<AbsoluteStartTimeChunk> ParseAbsoluteStartTimeChunk(SFB::InputSource& inputSource, const uint32_t chunkID, const uint64_t chunkDataSize)
	{
		if('ABSS' != chunkID) {
			os_log_error(OS_LOG_DEFAULT, "Invalid chunk ID for 'ABSS' chunk");
			return nullptr;
		}

		auto result = std::make_shared<AbsoluteStartTimeChunk>();

		result->mChunkID = chunkID;
		result->mDataSize = chunkDataSize;
		result->mDataOffset = inputSource.GetOffset();

		if(!inputSource.ReadBE<uint16_t>(result->mHours)) {
			os_log_error(OS_LOG_DEFAULT, "Unable to read hours in 'ABSS' chunk");
			return nullptr;
		}

		if(!inputSource.ReadBE<uint8_t>(result->mMinutes)) {
			os_log_error(OS_LOG_DEFAULT, "Unable to read minutes in 'ABSS' chunk");
			return nullptr;
		}

		if(!inputSource.ReadBE<uint8_t>(result->mSeconds)) {
			os_log_error(OS_LOG_DEFAULT, "Unable to read seconds in 'ABSS' chunk");
			return nullptr;
		}

		if(!inputSource.ReadBE<uint32_t>(result->mSamples)) {
			os_log_error(OS_LOG_DEFAULT, "Unable to read samples in 'ABSS' chunk");
			return nullptr;
		}

		return result;
	}

	std::shared_ptr<LoudspeakerConfigurationChunk> ParseLoudspeakerConfigurationChunk(SFB::InputSource& inputSource, const uint32_t chunkID, const uint64_t chunkDataSize)
	{
		if('LSCO' != chunkID) {
			os_log_error(OS_LOG_DEFAULT, "Invalid chunk ID for 'LSCO' chunk");
			return nullptr;
		}

		auto result = std::make_shared<LoudspeakerConfigurationChunk>();

		result->mChunkID = chunkID;
		result->mDataSize = chunkDataSize;
		result->mDataOffset = inputSource.GetOffset();

		if(!inputSource.ReadBE<uint16_t>(result->mLoudspeakerConfiguration)) {
			os_log_error(OS_LOG_DEFAULT, "Unable to read loudspeaker configuration in 'LSCO' chunk");
			return nullptr;
		}

		return result;
	}

	std::shared_ptr<PropertyChunk> ParsePropertyChunk(SFB::InputSource& inputSource, const uint32_t chunkID, const uint64_t chunkDataSize)
	{
		if('PROP' != chunkID) {
			os_log_error(OS_LOG_DEFAULT, "Invalid chunk ID for 'PROP' chunk");
			return nullptr;
		}

		auto result = std::make_shared<PropertyChunk>();

		result->mChunkID = chunkID;
		result->mDataSize = chunkDataSize;
		result->mDataOffset = inputSource.GetOffset();

		if(!ReadID(inputSource, result->mPropertyType)) {
			os_log_error(OS_LOG_DEFAULT, "Unable to read property type in 'PROP' chunk");
			return nullptr;
		}

		if('SND ' != result->mPropertyType) {
			os_log_error(OS_LOG_DEFAULT, "Unexpected property type in 'PROP' chunk: %u", result->mPropertyType);
			return nullptr;
		}

		// Parse the local chunks
		auto chunkDataSizeRemaining = result->mDataSize - 4; // adjust for mPropertyType
		while(0 < chunkDataSizeRemaining) {

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
						inputSource.SeekToOffset(inputSource.GetOffset() + (SInt64)localChunkDataSize);
						break;
				}

				chunkDataSizeRemaining -= 12;
				chunkDataSizeRemaining -= localChunkDataSize;
			}
			else {
				os_log_error(OS_LOG_DEFAULT, "Error reading local chunk in 'PROP' chunk");
				return nullptr;
			}
		}

		return result;
	}

	std::shared_ptr<DSDSoundDataChunk> ParseDSDSoundDataChunk(SFB::InputSource& inputSource, const uint32_t chunkID, const uint64_t chunkDataSize)
	{
		if('DSD ' != chunkID) {
			os_log_error(OS_LOG_DEFAULT, "Invalid chunk ID for 'DSD ' chunk");
			return nullptr;
		}

		auto result = std::make_shared<DSDSoundDataChunk>();

		result->mChunkID = chunkID;
		result->mDataSize = chunkDataSize;
		result->mDataOffset = inputSource.GetOffset();

		// Skip the data
		inputSource.SeekToOffset(inputSource.GetOffset() + (SInt64)chunkDataSize);

		return result;
	}

	std::unique_ptr<FormDSDChunk> ParseFormDSDChunk(SFB::InputSource& inputSource, const uint32_t chunkID, const uint64_t chunkDataSize)
	{
		if('FRM8' != chunkID) {
			os_log_error(OS_LOG_DEFAULT, "Missing 'FRM8' chunk");
			return nullptr;
		}

		auto result = make_unique<FormDSDChunk>();

		result->mChunkID = chunkID;
		result->mDataSize = chunkDataSize;
		result->mDataOffset = inputSource.GetOffset();

		if(!ReadID(inputSource, result->mFormType)) {
			os_log_error(OS_LOG_DEFAULT, "Unable to read formType in 'FRM8' chunk");
			return nullptr;
		}

		if('DSD ' != result->mFormType) {
			os_log_error(OS_LOG_DEFAULT, "Unexpected formType in 'FRM8' chunk: '%{public}.4s'", SFBCStringForOSType(result->mFormType));
			return nullptr;
		}

		// Parse the local chunks
		auto chunkDataSizeRemaining = result->mDataSize - 4; // adjust for mFormType
		while(0 < chunkDataSizeRemaining) {

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
						inputSource.SeekToOffset(inputSource.GetOffset() + (SInt64)localChunkDataSize);
						break;
				}

				chunkDataSizeRemaining -= 12;
				chunkDataSizeRemaining -= localChunkDataSize;
			}
			else {
				os_log_error(OS_LOG_DEFAULT, "Error reading local chunk in 'FRM8' chunk");
				return nullptr;
			}
		}

		return result;
	}

	std::unique_ptr<FormDSDChunk> ParseDSDIFF(SFB::InputSource& inputSource)
	{
		uint32_t chunkID;
		uint64_t chunkDataSize;
		if(!ReadChunkIDAndDataSize(inputSource, chunkID, chunkDataSize))
			return nullptr;

		return ParseFormDSDChunk(inputSource, chunkID, chunkDataSize);
	}

	CFErrorRef CreateInvalidDSDIFFFileError(CFURLRef url) CF_RETURNS_RETAINED
	{
		SFB::CFString description(CFCopyLocalizedString(CFSTR("The file “%@” is not a valid DSDIFF file."), ""));
		SFB::CFString failureReason(CFCopyLocalizedString(CFSTR("Not a DSDIFF file"), ""));
		SFB::CFString recoverySuggestion(CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), ""));

		return CreateErrorForURL(SFB::Audio::Decoder::ErrorDomain, SFB::Audio::Decoder::InputOutputError, description, url, failureReason, recoverySuggestion);
	}
}

#pragma mark Static Methods

CFArrayRef SFB::Audio::DSDIFFDecoder::CreateSupportedFileExtensions()
{
	CFStringRef supportedExtensions [] = { CFSTR("dff") };
	return CFArrayCreate(kCFAllocatorDefault, (const void **)supportedExtensions, 1, &kCFTypeArrayCallBacks);
}

CFArrayRef SFB::Audio::DSDIFFDecoder::CreateSupportedMIMETypes()
{
	CFStringRef supportedMIMETypes [] = { CFSTR("audio/dsdiff") };
	return CFArrayCreate(kCFAllocatorDefault, (const void **)supportedMIMETypes, 1, &kCFTypeArrayCallBacks);
}

bool SFB::Audio::DSDIFFDecoder::HandlesFilesWithExtension(CFStringRef extension)
{
	if(nullptr == extension)
		return false;

	if(kCFCompareEqualTo == CFStringCompare(extension, CFSTR("dff"), kCFCompareCaseInsensitive))
		return true;

	return false;
}

bool SFB::Audio::DSDIFFDecoder::HandlesMIMEType(CFStringRef mimeType)
{
	if(nullptr == mimeType)
		return false;

	if(kCFCompareEqualTo == CFStringCompare(mimeType, CFSTR("audio/dsdiff"), kCFCompareCaseInsensitive))
		return true;

	return false;
}

SFB::Audio::Decoder::unique_ptr SFB::Audio::DSDIFFDecoder::CreateDecoder(InputSource::unique_ptr inputSource)
{
	return unique_ptr(new DSDIFFDecoder(std::move(inputSource)));
}

#pragma mark Creation and Destruction

SFB::Audio::DSDIFFDecoder::DSDIFFDecoder(InputSource::unique_ptr inputSource)
	: Decoder(std::move(inputSource)), mTotalFrames(-1), mCurrentFrame(0), mAudioOffset(0)
{}

SFB::Audio::DSDIFFDecoder::~DSDIFFDecoder()
{
	if(IsOpen())
		Close();
}

#pragma mark Functionality

bool SFB::Audio::DSDIFFDecoder::_Open(CFErrorRef *error)
{
#pragma unused(error)

	auto chunks = ParseDSDIFF(GetInputSource());
	if(!chunks) {
		os_log_error(OS_LOG_DEFAULT, "Error parsing file");
		if(error)
			*error = CreateInvalidDSDIFFFileError(mInputSource->GetURL());

		return false;
	}

	auto propertyChunk = std::static_pointer_cast<PropertyChunk>(chunks->mLocalChunks['PROP']);
	auto sampleRateChunk = std::static_pointer_cast<SampleRateChunk>(propertyChunk->mLocalChunks['FS  ']);
	auto channelsChunk = std::static_pointer_cast<ChannelsChunk>(propertyChunk->mLocalChunks['CHNL']);

	if(!propertyChunk || !sampleRateChunk || !channelsChunk) {
		os_log_error(OS_LOG_DEFAULT, "Missing chunk in file");
		if(error)
			*error = CreateInvalidDSDIFFFileError(mInputSource->GetURL());

		return false;
	}

	// Set up the source format
	mSourceFormat.mFormatID				= kAudioFormatDirectStreamDigital;
	mSourceFormat.mSampleRate			= (Float64)sampleRateChunk->mSampleRate;
	mSourceFormat.mChannelsPerFrame		= channelsChunk->mNumberChannels;

	// The output format is raw DSD
	mFormat.mFormatID			= kAudioFormatDirectStreamDigital;
	mFormat.mFormatFlags		= kAudioFormatFlagIsNonInterleaved | kAudioFormatFlagIsBigEndian;

	mFormat.mSampleRate			= (Float64)sampleRateChunk->mSampleRate;
	mFormat.mChannelsPerFrame	= channelsChunk->mNumberChannels;
	mFormat.mBitsPerChannel		= 1;

	mFormat.mBytesPerPacket		= 1;
	mFormat.mFramesPerPacket	= 8;
	mFormat.mBytesPerFrame		= 0;

	mFormat.mReserved			= 0;


	// Channel layouts are defined in the DSDIFF file format specification
	if(2 == channelsChunk->mChannelIDs.size() && 'SLFT' == channelsChunk->mChannelIDs[0] && 'SRGT' == channelsChunk->mChannelIDs[1])
		mChannelLayout = ChannelLayout::ChannelLayoutWithTag(kAudioChannelLayoutTag_Stereo);
	else if(5 == channelsChunk->mChannelIDs.size() && 'MLFT' == channelsChunk->mChannelIDs[0] && 'MRGT' == channelsChunk->mChannelIDs[1] && 'C   ' == channelsChunk->mChannelIDs[2] && 'LS  ' == channelsChunk->mChannelIDs[3] && 'RS  ' == channelsChunk->mChannelIDs[4])
		mChannelLayout = ChannelLayout::ChannelLayoutWithTag(kAudioChannelLayoutTag_MPEG_5_0_A);
	else if(6 == channelsChunk->mChannelIDs.size() && 'MLFT' == channelsChunk->mChannelIDs[0] && 'MRGT' == channelsChunk->mChannelIDs[1] && 'C   ' == channelsChunk->mChannelIDs[2] && 'LFE ' == channelsChunk->mChannelIDs[3] && 'LS  ' == channelsChunk->mChannelIDs[4] && 'RS  ' == channelsChunk->mChannelIDs[5])
		mChannelLayout = ChannelLayout::ChannelLayoutWithTag(kAudioChannelLayoutTag_MPEG_5_1_A);
	else {
		std::vector<AudioChannelLabel> labels;
		for(auto channelID : channelsChunk->mChannelIDs)
			labels.push_back(DSDIFFChannelIDToCoreAudioChannelLabel(channelID));

		mChannelLayout = ChannelLayout::ChannelLayoutWithChannelLabels(labels);
	}


	auto soundDataChunk = std::static_pointer_cast<DSDSoundDataChunk>(chunks->mLocalChunks['DSD ']);
	if(!soundDataChunk) {
		os_log_error(OS_LOG_DEFAULT, "Missing chunk in file");
		if(error)
			*error = CreateInvalidDSDIFFFileError(mInputSource->GetURL());

		return false;
	}

	mAudioOffset = soundDataChunk->mDataOffset;
	mTotalFrames = (SInt64)mFormat.ByteCountToFrameCount(soundDataChunk->mDataSize - 12) / mFormat.mChannelsPerFrame;

	GetInputSource().SeekToOffset(mAudioOffset);

	return true;
}

bool SFB::Audio::DSDIFFDecoder::_Close(CFErrorRef */*error*/)
{
	return true;
}

SFB::CFString SFB::Audio::DSDIFFDecoder::_GetSourceFormatDescription() const
{
	return CFString(nullptr,
					CFSTR("DSD Interchange File Format, %u channels, %u Hz"),
					(unsigned int)mSourceFormat.mChannelsPerFrame,
					(unsigned int)mSourceFormat.mSampleRate);
}

UInt32 SFB::Audio::DSDIFFDecoder::_ReadAudio(AudioBufferList *bufferList, UInt32 frameCount)
{
	// Only multiples of 8 frames can be read (8 frames equals one byte)
	if(bufferList->mNumberBuffers != mFormat.mChannelsPerFrame || 0 != frameCount % 8) {
		os_log_debug(OS_LOG_DEFAULT, "_ReadAudio() called with invalid parameters");
		return 0;
	}

	UInt32 framesRemaining = (UInt32)(mTotalFrames - mCurrentFrame);
	UInt32 framesToRead = std::min(frameCount, framesRemaining);
	UInt32 framesRead = 0;

	// Reset output buffer data size
	for(UInt32 i = 0; i < bufferList->mNumberBuffers; ++i)
		bufferList->mBuffers[i].mDataByteSize = 0;

	for(;;) {
		// Read interleaved input, grouped as 8 one bit samples per frame (a single channel byte) into
		// a clustered frame (one channel byte per channel)
		// From a bit perspective for stereo: LLLLLLLLRRRRRRRRLLLLLLLLRRRRRRRR
		uint8_t buffer [BUFFER_CHANNEL_SIZE_BYTES * mFormat.mChannelsPerFrame];
		UInt32 bytesToRead = std::min(BUFFER_CHANNEL_SIZE_BYTES * mFormat.mChannelsPerFrame, (framesToRead / 8) * mFormat.mChannelsPerFrame);
		auto bytesRead = GetInputSource().Read(buffer, bytesToRead);

		if(bytesRead != bytesToRead) {
			os_log_debug(OS_LOG_DEFAULT, "Error reading audio: requested %u bytes, got %lld", bytesToRead, bytesRead);
			break;
		}

		// Decoding is finished
		if(0 == bytesRead)
			break;

		// Deinterleave the clustered frames and copy to output
		for(UInt32 i = 0; i < bufferList->mNumberBuffers; ++i) {
			uint8_t *dst = (uint8_t *)bufferList->mBuffers[i].mData + bufferList->mBuffers[i].mDataByteSize;
			for(SInt64 byteIndex = i; byteIndex < bytesRead; byteIndex += mFormat.mChannelsPerFrame)
				*dst++ = buffer[byteIndex];

			bufferList->mBuffers[i].mNumberChannels	= 1;
			bufferList->mBuffers[i].mDataByteSize	+= bytesRead / mFormat.mChannelsPerFrame;
		}

		framesRead += (bytesRead / mFormat.mChannelsPerFrame) * 8;

		// All requested frames were read
		if(framesRead == frameCount)
			break;

		framesToRead -= framesRead;
	}

	mCurrentFrame += framesRead;

	return framesRead;
}

SInt64 SFB::Audio::DSDIFFDecoder::_SeekToFrame(SInt64 frame)
{
	// Round down to nearest multiple of 8 frames
	frame = (frame / 8) * 8;

	SInt64 frameOffset = (SInt64)mFormat.FrameCountToByteCount((size_t)frame);
	if(!GetInputSource().SeekToOffset(mAudioOffset + frameOffset)) {
		os_log_debug(OS_LOG_DEFAULT, "_SeekToFrame() failed for offset: %lld", mAudioOffset + frameOffset);
		return -1;
	}

	mCurrentFrame = frame;
	return _GetCurrentFrame();
}
