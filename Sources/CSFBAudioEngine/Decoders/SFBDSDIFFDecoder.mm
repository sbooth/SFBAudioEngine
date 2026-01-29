//
// Copyright (c) 2014-2026 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import "SFBDSDIFFDecoder.h"

#import "NSData+SFBExtensions.h"
#import "SFBCStringForOSType.h"
#import "SFBLocalizedNameForURL.h"

#import <AVFAudioExtensions/AVFAudioExtensions.h>

#import <os/log.h>

#import <algorithm>
#import <cstdint>
#import <map>
#import <memory>
#import <string>
#import <vector>

SFBDSDDecoderName const SFBDSDDecoderNameDSDIFF = @"org.sbooth.AudioEngine.DSDDecoder.DSDIFF";

namespace {

// Convert a four byte chunk ID to a uint32_t
uint32_t bytesToID(const char bytes[4]) noexcept {
    auto one = bytes[0];
    auto two = bytes[1];
    auto three = bytes[2];
    auto four = bytes[3];

    // Verify well-formedness
    if (!std::isprint(one) || !std::isprint(two) || !std::isprint(three) || !std::isprint(four)) {
        return 0;
    }

    if (std::isspace(one)) {
        return 0;
    }

    if (std::isspace(two) && std::isspace(one)) {
        return 0;
    }

    if (std::isspace(three) && std::isspace(two) && std::isspace(one)) {
        return 0;
    }

    if (std::isspace(four) && std::isspace(three) && std::isspace(two) && std::isspace(one)) {
        return 0;
    }

    return static_cast<uint32_t>((one << 24U) | (two << 16U) | (three << 8U) | four);
}

// Read an ID as a uint32_t, performing validation
bool readID(SFBInputSource *inputSource, uint32_t& chunkID) noexcept {
    NSCParameterAssert(inputSource != nil);

    char chunkIDBytes[4];
    if (NSInteger bytesRead;
        ![inputSource readBytes:chunkIDBytes length:4 bytesRead:&bytesRead error:nil] || bytesRead != 4) {
        os_log_error(gSFBDSDDecoderLog, "Unable to read chunk ID");
        return false;
    }

    chunkID = bytesToID(chunkIDBytes);
    if (0 == chunkID) {
        os_log_error(gSFBDSDDecoderLog, "Illegal chunk ID");
        return false;
    }

    return true;
}

AudioChannelLabel channelIDToCoreAudioChannelLabel(uint32_t channelID) noexcept {
    switch (channelID) {
    case 'SLFT':
        return kAudioChannelLabel_Left;
    case 'SRGT':
        return kAudioChannelLabel_Right;
    case 'MLFT':
        return kAudioChannelLabel_LeftSurroundDirect;
    case 'MRGT':
        return kAudioChannelLabel_RightSurroundDirect;
    case 'LS  ':
        return kAudioChannelLabel_LeftSurround;
    case 'RS  ':
        return kAudioChannelLabel_RightSurround;
    case 'C   ':
        return kAudioChannelLabel_Center;
    case 'LFE ':
        return kAudioChannelLabel_LFE2;
    }

    return kAudioChannelLabel_Unknown;
}

#pragma mark DSDIFF chunks

// Base class for DSDIFF chunks
struct DSDIFFChunk : std::enable_shared_from_this<DSDIFFChunk> {
    using shared_ptr = std::shared_ptr<DSDIFFChunk>;
    using chunk_map = std::map<uint32_t, shared_ptr>;

    // Shared pointer support
    shared_ptr getptr() {
        return shared_from_this();
    }

    uint32_t chunkID_;
    uint64_t dataSize_;

    int64_t dataOffset_;
};

// 'FRM8'
struct FormDSDChunk : public DSDIFFChunk {
    uint32_t formType_;
    chunk_map localChunks_;
};

// 'FVER' in 'FRM8'
struct FormatVersionChunk : public DSDIFFChunk {
    static constexpr uint32_t kSupportedFormatVersion = 0x01050000;
    uint32_t formatVersion_;
};

// 'PROP' in 'FRM8'
struct PropertyChunk : public DSDIFFChunk {
    uint32_t propertyType_;
    chunk_map localChunks_;
};

// 'FS  ' in 'PROP'
struct SampleRateChunk : public DSDIFFChunk {
    uint32_t sampleRate_;
};

// 'CHNL' in 'PROP'
struct ChannelsChunk : public DSDIFFChunk {
    uint16_t numberChannels_;
    std::vector<uint32_t> channelIDs_;
};

// 'CMPR' in 'PROP'
struct CompressionTypeChunk : public DSDIFFChunk {
    uint32_t compressionType_;
    std::string compressionName_;
};

// 'ABSS' in 'PROP'
struct AbsoluteStartTimeChunk : public DSDIFFChunk {
    uint16_t hours_;
    uint8_t minutes_;
    uint8_t seconds_;
    uint32_t samples_;
};

// 'LSCO' in 'PROP'
struct LoudspeakerConfigurationChunk : public DSDIFFChunk {
    uint16_t loudspeakerConfiguration_;
};

// 'DSD ' in 'FRM8'
struct DSDSoundDataChunk : public DSDIFFChunk {};

// 'DST ', 'DSTI', 'COMT', 'DIIN', 'MANF' are not handled

//// 'DST ' in 'FRM8'
// class DSTSoundDataChunk : public DSDIFFChunk
//{};
//
//// 'FRTE' in 'DST '
// class DSTFrameInformationChunk : public DSDIFFChunk
//{};
//
//// 'FRTE' in 'DST '
// class DSTFrameDataChunk : public DSDIFFChunk
//{};
//
//// 'FRTE' in 'DST '
// class DSTFrameCRCChunk : public DSDIFFChunk
//{};
//
//// 'DSTI' in 'FRM8'
// class DSTSoundIndexChunk : public DSDIFFChunk
//{};
//
//// 'COMT' in 'FRM8'
// class CommentsChunk : public DSDIFFChunk
//{};
//
//// 'DIIN' in 'FRM8'
// class EditedMasterInformationChunk : public DSDIFFChunk
//{};
//
//// 'EMID' in 'DIIN'
// class EditedMasterIDChunk : public DSDIFFChunk
//{};
//
//// 'MARK' in 'DIIN'
// class MarkerChunk : public DSDIFFChunk
//{};
//
//// 'DIAR' in 'DIIN'
// class ArtistChunk : public DSDIFFChunk
//{};
//
//// 'DITI' in 'DIIN'
// class TitleChunk : public DSDIFFChunk
//{};
//
//// 'MANF' in 'FRM8'
// class ManufacturerSpecificChunk : public DSDIFFChunk
//{};

#pragma mark DSDIFF parsing

bool readChunkIDAndDataSize(SFBInputSource *inputSource, uint32_t& chunkID, uint64_t& chunkDataSize) noexcept {
    if (!readID(inputSource, chunkID)) {
        return false;
    }

    if (![inputSource readUInt64BigEndian:&chunkDataSize error:nil]) {
        os_log_error(gSFBDSDDecoderLog, "Unable to read chunk data size");
        return false;
    }

    return true;
}

std::shared_ptr<FormatVersionChunk> parseFormatVersionChunk(SFBInputSource *inputSource, const uint32_t chunkID,
                                                            const uint64_t chunkDataSize) {
    if (chunkID != 'FVER') {
        os_log_error(gSFBDSDDecoderLog, "Invalid chunk ID for 'FVER' chunk");
        return nullptr;
    }

    auto result = std::make_shared<FormatVersionChunk>();

    result->chunkID_ = chunkID;
    result->dataSize_ = chunkDataSize;
    NSInteger offset;
    if (![inputSource getOffset:&offset error:nil]) {
        os_log_error(gSFBDSDDecoderLog, "Error getting chunk data offset");
        return nullptr;
    }
    result->dataOffset_ = offset;

    if (![inputSource readUInt32BigEndian:&result->formatVersion_ error:nil]) {
        os_log_error(gSFBDSDDecoderLog, "Unable to read format version in 'FVER' chunk");
        return nullptr;
    }

    if (result->formatVersion_ > FormatVersionChunk::kSupportedFormatVersion) {
        os_log_error(gSFBDSDDecoderLog, "Unsupported format version in 'FVER': %u", result->formatVersion_);
        return nullptr;
    }

    return result;
}

std::shared_ptr<SampleRateChunk> parseSampleRateChunk(SFBInputSource *inputSource, const uint32_t chunkID,
                                                      const uint64_t chunkDataSize) {
    if (chunkID != 'FS  ') {
        os_log_error(gSFBDSDDecoderLog, "Invalid chunk ID for 'FS  ' chunk");
        return nullptr;
    }

    auto result = std::make_shared<SampleRateChunk>();

    result->chunkID_ = chunkID;
    result->dataSize_ = chunkDataSize;
    NSInteger offset;
    if (![inputSource getOffset:&offset error:nil]) {
        os_log_error(gSFBDSDDecoderLog, "Error getting chunk data offset");
        return nullptr;
    }
    result->dataOffset_ = offset;

    if (![inputSource readUInt32BigEndian:&result->sampleRate_ error:nil]) {
        os_log_error(gSFBDSDDecoderLog, "Unable to read sample rate in 'FS  ' chunk");
        return nullptr;
    }

    return result;
}

std::shared_ptr<ChannelsChunk> parseChannelsChunk(SFBInputSource *inputSource, const uint32_t chunkID,
                                                  const uint64_t chunkDataSize) {
    if (chunkID != 'CHNL') {
        os_log_error(gSFBDSDDecoderLog, "Invalid chunk ID for 'CHNL' chunk");
        return nullptr;
    }

    auto result = std::make_shared<ChannelsChunk>();

    result->chunkID_ = chunkID;
    result->dataSize_ = chunkDataSize;
    NSInteger offset;
    if (![inputSource getOffset:&offset error:nil]) {
        os_log_error(gSFBDSDDecoderLog, "Error getting chunk data offset");
        return nullptr;
    }
    result->dataOffset_ = offset;

    if (![inputSource readUInt16BigEndian:&result->numberChannels_ error:nil]) {
        os_log_error(gSFBDSDDecoderLog, "Unable to read number channels in 'CHNL' chunk");
        return nullptr;
    }

    for (uint16_t i = 0; i < result->numberChannels_; ++i) {
        uint32_t channelID;
        if (!readID(inputSource, channelID)) {
            os_log_error(gSFBDSDDecoderLog, "Unable to read channel ID in 'CHNL' chunk");
            return nullptr;
        }
        result->channelIDs_.push_back(channelID);
    }

    return result;
}

std::shared_ptr<CompressionTypeChunk> parseCompressionTypeChunk(SFBInputSource *inputSource, const uint32_t chunkID,
                                                                const uint64_t chunkDataSize) {
    if (chunkID != 'CMPR') {
        os_log_error(gSFBDSDDecoderLog, "Invalid chunk ID for 'CMPR' chunk");
        return nullptr;
    }

    auto result = std::make_shared<CompressionTypeChunk>();

    result->chunkID_ = chunkID;
    result->dataSize_ = chunkDataSize;
    NSInteger offset;
    if (![inputSource getOffset:&offset error:nil]) {
        os_log_error(gSFBDSDDecoderLog, "Error getting chunk data offset");
        return nullptr;
    }
    result->dataOffset_ = offset;

    if (!readID(inputSource, result->compressionType_)) {
        os_log_error(gSFBDSDDecoderLog, "Unable to read compression type in 'CMPR' chunk");
        return nullptr;
    }

    uint8_t count;
    if (![inputSource readUInt8:&count error:nil]) {
        os_log_error(gSFBDSDDecoderLog, "Unable to read count in 'CMPR' chunk");
        return nullptr;
    }

    std::vector<char> compressionName(count);
    NSInteger bytesRead;
    if (![inputSource readBytes:compressionName.data() length:count bytesRead:&bytesRead error:nil] ||
        bytesRead != count) {
        os_log_error(gSFBDSDDecoderLog, "Unable to read compressionName in 'CMPR' chunk");
        return nullptr;
    }

    result->compressionName_ = std::string(compressionName.begin(), compressionName.end());

    // Chunks always have an even length
    if (![inputSource getOffset:&offset error:nil]) {
        os_log_error(gSFBDSDDecoderLog, "Error getting chunk data offset");
        return nullptr;
    }

    if (offset % 2) {
        uint8_t unused;
        if (![inputSource readUInt8:&unused error:nil]) {
            os_log_error(gSFBDSDDecoderLog, "Unable to read dummy byte in 'CMPR' chunk");
            return nullptr;
        }
    }

    return result;
}

std::shared_ptr<AbsoluteStartTimeChunk> parseAbsoluteStartTimeChunk(SFBInputSource *inputSource, const uint32_t chunkID,
                                                                    const uint64_t chunkDataSize) {
    if (chunkID != 'ABSS') {
        os_log_error(gSFBDSDDecoderLog, "Invalid chunk ID for 'ABSS' chunk");
        return nullptr;
    }

    auto result = std::make_shared<AbsoluteStartTimeChunk>();

    result->chunkID_ = chunkID;
    result->dataSize_ = chunkDataSize;
    NSInteger offset;
    if (![inputSource getOffset:&offset error:nil]) {
        os_log_error(gSFBDSDDecoderLog, "Error getting chunk data offset");
        return nullptr;
    }
    result->dataOffset_ = offset;

    if (![inputSource readUInt16BigEndian:&result->hours_ error:nil]) {
        os_log_error(gSFBDSDDecoderLog, "Unable to read hours in 'ABSS' chunk");
        return nullptr;
    }

    if (![inputSource readUInt8:&result->minutes_ error:nil]) {
        os_log_error(gSFBDSDDecoderLog, "Unable to read minutes in 'ABSS' chunk");
        return nullptr;
    }

    if (![inputSource readUInt8:&result->seconds_ error:nil]) {
        os_log_error(gSFBDSDDecoderLog, "Unable to read seconds in 'ABSS' chunk");
        return nullptr;
    }

    if (![inputSource readUInt32BigEndian:&result->samples_ error:nil]) {
        os_log_error(gSFBDSDDecoderLog, "Unable to read samples in 'ABSS' chunk");
        return nullptr;
    }

    return result;
}

std::shared_ptr<LoudspeakerConfigurationChunk>
parseLoudspeakerConfigurationChunk(SFBInputSource *inputSource, const uint32_t chunkID, const uint64_t chunkDataSize) {
    if (chunkID != 'LSCO') {
        os_log_error(gSFBDSDDecoderLog, "Invalid chunk ID for 'LSCO' chunk");
        return nullptr;
    }

    auto result = std::make_shared<LoudspeakerConfigurationChunk>();

    result->chunkID_ = chunkID;
    result->dataSize_ = chunkDataSize;
    NSInteger offset;
    if (![inputSource getOffset:&offset error:nil]) {
        os_log_error(gSFBDSDDecoderLog, "Error getting chunk data offset");
        return nullptr;
    }
    result->dataOffset_ = offset;

    if (![inputSource readUInt16BigEndian:&result->loudspeakerConfiguration_ error:nil]) {
        os_log_error(gSFBDSDDecoderLog, "Unable to read loudspeaker configuration in 'LSCO' chunk");
        return nullptr;
    }

    return result;
}

std::shared_ptr<PropertyChunk> parsePropertyChunk(SFBInputSource *inputSource, const uint32_t chunkID,
                                                  const uint64_t chunkDataSize) {
    if (chunkID != 'PROP') {
        os_log_error(gSFBDSDDecoderLog, "Invalid chunk ID for 'PROP' chunk");
        return nullptr;
    }

    auto result = std::make_shared<PropertyChunk>();

    result->chunkID_ = chunkID;
    result->dataSize_ = chunkDataSize;
    NSInteger offset;
    if (![inputSource getOffset:&offset error:nil]) {
        os_log_error(gSFBDSDDecoderLog, "Error getting chunk data offset");
        return nullptr;
    }
    result->dataOffset_ = offset;

    if (!readID(inputSource, result->propertyType_)) {
        os_log_error(gSFBDSDDecoderLog, "Unable to read property type in 'PROP' chunk");
        return nullptr;
    }

    if (result->propertyType_ != 'SND ') {
        os_log_error(gSFBDSDDecoderLog, "Unexpected property type in 'PROP' chunk: %u", result->propertyType_);
        return nullptr;
    }

    // Parse the local chunks
    auto chunkDataSizeRemaining = result->dataSize_ - 4; // adjust for propertyType_
    while (chunkDataSizeRemaining >= 12) {

        uint32_t localChunkID;
        uint64_t localChunkDataSize;

        if (readChunkIDAndDataSize(inputSource, localChunkID, localChunkDataSize)) {
            if (localChunkDataSize > chunkDataSizeRemaining - 12) {
                os_log_error(gSFBDSDDecoderLog, "Invalid data size for local chunk '%{public}.4s' in 'PROP' chunk",
                             SFBCStringForOSType(localChunkID));
                return nullptr;
            }

            switch (localChunkID) {
            case 'FS  ':
                if (auto chunk = parseSampleRateChunk(inputSource, localChunkID, localChunkDataSize); chunk) {
                    result->localChunks_[chunk->chunkID_] = chunk;
                }
                break;

            case 'CHNL':
                if (auto chunk = parseChannelsChunk(inputSource, localChunkID, localChunkDataSize); chunk) {
                    result->localChunks_[chunk->chunkID_] = chunk;
                }
                break;

            case 'CMPR':
                if (auto chunk = parseCompressionTypeChunk(inputSource, localChunkID, localChunkDataSize); chunk) {
                    result->localChunks_[chunk->chunkID_] = chunk;
                }
                break;

            case 'ABSS':
                if (auto chunk = parseAbsoluteStartTimeChunk(inputSource, localChunkID, localChunkDataSize); chunk) {
                    result->localChunks_[chunk->chunkID_] = chunk;
                }
                break;

            case 'LSCO':
                if (auto chunk = parseLoudspeakerConfigurationChunk(inputSource, localChunkID, localChunkDataSize);
                    chunk) {
                    result->localChunks_[chunk->chunkID_] = chunk;
                }
                break;

                // Skip unrecognized or ignored chunks
            default:
                if (![inputSource getOffset:&offset error:nil]) {
                    os_log_error(gSFBDSDDecoderLog, "Error getting chunk data offset");
                    return nullptr;
                }

                if (![inputSource seekToOffset:(offset + static_cast<NSInteger>(localChunkDataSize)) error:nil]) {
                    os_log_error(gSFBDSDDecoderLog, "Error skipping chunk data");
                    return nullptr;
                }

                break;
            }

            chunkDataSizeRemaining -= 12;
            chunkDataSizeRemaining -= localChunkDataSize;
        } else {
            os_log_error(gSFBDSDDecoderLog, "Error reading local chunk in 'PROP' chunk");
            return nullptr;
        }
    }

    return result;
}

std::shared_ptr<DSDSoundDataChunk> parseDSDSoundDataChunk(SFBInputSource *inputSource, const uint32_t chunkID,
                                                          const uint64_t chunkDataSize) {
    if (chunkID != 'DSD ') {
        os_log_error(gSFBDSDDecoderLog, "Invalid chunk ID for 'DSD ' chunk");
        return nullptr;
    }

    auto result = std::make_shared<DSDSoundDataChunk>();

    result->chunkID_ = chunkID;
    result->dataSize_ = chunkDataSize;
    NSInteger offset;
    if (![inputSource getOffset:&offset error:nil]) {
        os_log_error(gSFBDSDDecoderLog, "Error getting chunk data offset");
        return nullptr;
    }
    result->dataOffset_ = offset;

    // Skip the data
    if (![inputSource seekToOffset:(offset + static_cast<NSInteger>(chunkDataSize)) error:nil]) {
        os_log_error(gSFBDSDDecoderLog, "Error skipping chunk data");
        return nullptr;
    }

    return result;
}

std::unique_ptr<FormDSDChunk> parseFormDSDChunk(SFBInputSource *inputSource, const uint32_t chunkID,
                                                const uint64_t chunkDataSize) {
    if (chunkID != 'FRM8') {
        os_log_error(gSFBDSDDecoderLog, "Missing 'FRM8' chunk");
        return nullptr;
    }

    auto result = std::make_unique<FormDSDChunk>();

    result->chunkID_ = chunkID;
    result->dataSize_ = chunkDataSize;
    NSInteger offset;
    if (![inputSource getOffset:&offset error:nil]) {
        os_log_error(gSFBDSDDecoderLog, "Error getting chunk data offset");
        return nullptr;
    }
    result->dataOffset_ = offset;

    if (!readID(inputSource, result->formType_)) {
        os_log_error(gSFBDSDDecoderLog, "Unable to read formType in 'FRM8' chunk");
        return nullptr;
    }

    if (result->formType_ != 'DSD ') {
        os_log_error(gSFBDSDDecoderLog, "Unexpected formType in 'FRM8' chunk: '%{public}.4s'",
                     SFBCStringForOSType(result->formType_));
        return nullptr;
    }

    // Parse the local chunks
    auto chunkDataSizeRemaining = result->dataSize_ - 4; // adjust for formType_
    while (chunkDataSizeRemaining >= 12) {

        uint32_t localChunkID;
        uint64_t localChunkDataSize;

        if (readChunkIDAndDataSize(inputSource, localChunkID, localChunkDataSize)) {
            if (localChunkDataSize > chunkDataSizeRemaining - 12) {
                os_log_error(gSFBDSDDecoderLog, "Invalid data size for local chunk '%{public}.4s' in 'FRM8' chunk",
                             SFBCStringForOSType(localChunkID));
                return nullptr;
            }

            switch (localChunkID) {
            case 'FVER':
                if (auto chunk = parseFormatVersionChunk(inputSource, localChunkID, localChunkDataSize); chunk) {
                    result->localChunks_[chunk->chunkID_] = chunk;
                }
                break;

            case 'PROP':
                if (auto chunk = parsePropertyChunk(inputSource, localChunkID, localChunkDataSize); chunk) {
                    result->localChunks_[chunk->chunkID_] = chunk;
                }
                break;

            case 'DSD ':
                if (auto chunk = parseDSDSoundDataChunk(inputSource, localChunkID, localChunkDataSize); chunk) {
                    result->localChunks_[chunk->chunkID_] = chunk;
                }
                break;

                // Skip unrecognized or ignored chunks
            default:
                if (![inputSource getOffset:&offset error:nil]) {
                    os_log_error(gSFBDSDDecoderLog, "Error getting chunk data offset");
                    return nullptr;
                }

                if (![inputSource seekToOffset:(offset + static_cast<NSInteger>(localChunkDataSize)) error:nil]) {
                    os_log_error(gSFBDSDDecoderLog, "Error skipping chunk data");
                    return nullptr;
                }

                break;
            }

            chunkDataSizeRemaining -= 12;
            chunkDataSizeRemaining -= localChunkDataSize;
        } else {
            os_log_error(gSFBDSDDecoderLog, "Error reading local chunk in 'FRM8' chunk");
            return nullptr;
        }
    }

    return result;
}

std::unique_ptr<FormDSDChunk> parseDSDIFF(SFBInputSource *inputSource) {
    uint32_t chunkID;
    uint64_t chunkDataSize;
    if (!readChunkIDAndDataSize(inputSource, chunkID, chunkDataSize)) {
        return nullptr;
    }

    return parseFormDSDChunk(inputSource, chunkID, chunkDataSize);
}

NSError *createInvalidDSDIFFFileError(NSURL *url) {
    NSMutableDictionary *userInfo = [NSMutableDictionary
          dictionaryWithObject:NSLocalizedString(@"The file's extension may not match the file's type.", @"")
                        forKey:NSLocalizedRecoverySuggestionErrorKey];

    if (url != nil) {
        userInfo[NSLocalizedDescriptionKey] =
              [NSString localizedStringWithFormat:NSLocalizedString(@"The file “%@” is not a valid DSDIFF file.", @""),
                                                  SFBLocalizedNameForURL(url)];
        userInfo[NSURLErrorKey] = url;
    } else {
        userInfo[NSLocalizedDescriptionKey] = NSLocalizedString(@"The file is not a valid DSDIFF file.", @"");
    }

    return [NSError errorWithDomain:SFBDSDDecoderErrorDomain
                               code:SFBDSDDecoderErrorCodeInvalidFormat
                           userInfo:userInfo];
}

} /* namespace */

@interface SFBDSDIFFDecoder () {
  @private
    BOOL _isOpen;
    AVAudioFramePosition _packetPosition;
    AVAudioFramePosition _packetCount;
    int64_t _audioOffset;
}
@end

@implementation SFBDSDIFFDecoder

+ (void)load {
    [SFBDSDDecoder registerSubclass:[self class]];
}

+ (NSSet *)supportedPathExtensions {
    return [NSSet setWithObject:@"dff"];
}

+ (NSSet *)supportedMIMETypes {
    return [NSSet setWithObject:@"audio/dsdiff"];
}

+ (SFBDSDDecoderName)decoderName {
    return SFBDSDDecoderNameDSDIFF;
}

+ (BOOL)testInputSource:(SFBInputSource *)inputSource
      formatIsSupported:(SFBTernaryTruthValue *)formatIsSupported
                  error:(NSError **)error {
    NSParameterAssert(inputSource != nil);
    NSParameterAssert(formatIsSupported != nullptr);

    NSData *header = [inputSource readHeaderOfLength:SFBDSDIFFDetectionSize skipID3v2Tag:NO error:error];
    if (header == nil) {
        return NO;
    }

    if ([header isDSDIFFHeader]) {
        *formatIsSupported = SFBTernaryTruthValueTrue;
    } else {
        *formatIsSupported = SFBTernaryTruthValueFalse;
    }

    return YES;
}

- (BOOL)decodingIsLossless {
    return YES;
}

- (BOOL)openReturningError:(NSError **)error {
    if (![super openReturningError:error]) {
        return NO;
    }

    auto chunks = parseDSDIFF(_inputSource);
    if (!chunks) {
        os_log_error(gSFBDSDDecoderLog, "Error parsing file");
        if (error != nullptr) {
            *error = createInvalidDSDIFFFileError(_inputSource.url);
        }
        return NO;
    }

    auto propertyChunk = std::static_pointer_cast<PropertyChunk>(chunks->localChunks_['PROP']);
    auto sampleRateChunk = std::static_pointer_cast<SampleRateChunk>(propertyChunk->localChunks_['FS  ']);
    auto channelsChunk = std::static_pointer_cast<ChannelsChunk>(propertyChunk->localChunks_['CHNL']);

    if (!propertyChunk || !sampleRateChunk || !channelsChunk) {
        os_log_error(gSFBDSDDecoderLog, "Missing chunk in file");
        if (error != nullptr) {
            *error = createInvalidDSDIFFFileError(_inputSource.url);
        }
        return NO;
    }

    // Channel layouts are defined in the DSDIFF file format specification
    AVAudioChannelLayout *channelLayout = nil;
    if (channelsChunk->channelIDs_.size() == 2 && channelsChunk->channelIDs_[0] == 'SLFT' &&
        channelsChunk->channelIDs_[1] == 'SRGT') {
        channelLayout = [AVAudioChannelLayout layoutWithLayoutTag:kAudioChannelLayoutTag_Stereo];
    } else if (channelsChunk->channelIDs_.size() == 5 && channelsChunk->channelIDs_[0] == 'MLFT' &&
               channelsChunk->channelIDs_[1] == 'MRGT' && channelsChunk->channelIDs_[2] == 'C   ' &&
               channelsChunk->channelIDs_[3] == 'LS  ' && channelsChunk->channelIDs_[4] == 'RS  ') {
        channelLayout = [AVAudioChannelLayout layoutWithLayoutTag:kAudioChannelLayoutTag_MPEG_5_0_A];
    } else if (channelsChunk->channelIDs_.size() == 6 && channelsChunk->channelIDs_[0] == 'MLFT' &&
               channelsChunk->channelIDs_[1] == 'MRGT' && channelsChunk->channelIDs_[2] == 'C   ' &&
               channelsChunk->channelIDs_[3] == 'LFE ' && channelsChunk->channelIDs_[4] == 'LS  ' &&
               channelsChunk->channelIDs_[5] == 'RS  ') {
        channelLayout = [AVAudioChannelLayout layoutWithLayoutTag:kAudioChannelLayoutTag_MPEG_5_1_A];
    } else if (!channelsChunk->channelIDs_.empty()) {
        std::vector<AudioChannelLabel> labels;
        for (auto channelID : channelsChunk->channelIDs_) {
            labels.push_back(channelIDToCoreAudioChannelLabel(channelID));
        }
        channelLayout = [AVAudioChannelLayout layoutWithChannelLabels:labels.data()
                                                                count:(AVAudioChannelCount)labels.size()];
    }

    AudioStreamBasicDescription processingStreamDescription{};

    // The output format is raw DSD
    processingStreamDescription.mFormatID = kSFBAudioFormatDSD;
    processingStreamDescription.mFormatFlags = kAudioFormatFlagIsBigEndian;

    processingStreamDescription.mSampleRate = static_cast<Float64>(sampleRateChunk->sampleRate_);
    processingStreamDescription.mChannelsPerFrame = channelsChunk->numberChannels_;
    processingStreamDescription.mBitsPerChannel = 1;

    processingStreamDescription.mBytesPerPacket = kSFBBytesPerDSDPacketPerChannel * channelsChunk->numberChannels_;
    processingStreamDescription.mFramesPerPacket = kSFBPCMFramesPerDSDPacket;

    _processingFormat = [[AVAudioFormat alloc] initWithStreamDescription:&processingStreamDescription
                                                           channelLayout:channelLayout];

    // Set up the source format
    AudioStreamBasicDescription sourceStreamDescription{};

    sourceStreamDescription.mFormatID = kSFBAudioFormatDSD;

    sourceStreamDescription.mSampleRate = static_cast<Float64>(sampleRateChunk->sampleRate_);
    sourceStreamDescription.mChannelsPerFrame = channelsChunk->numberChannels_;
    sourceStreamDescription.mBitsPerChannel = 1;

    _sourceFormat = [[AVAudioFormat alloc] initWithStreamDescription:&sourceStreamDescription
                                                       channelLayout:channelLayout];

    auto soundDataChunk = std::static_pointer_cast<DSDSoundDataChunk>(chunks->localChunks_['DSD ']);
    if (!soundDataChunk) {
        os_log_error(gSFBDSDDecoderLog, "Missing chunk in file");
        if (error != nullptr) {
            *error = createInvalidDSDIFFFileError(_inputSource.url);
        }
        return NO;
    }

    _audioOffset = soundDataChunk->dataOffset_;
    _packetPosition = 0;
    _packetCount = static_cast<AVAudioFramePosition>(soundDataChunk->dataSize_ - 12) /
                   (kSFBBytesPerDSDPacketPerChannel * channelsChunk->numberChannels_);

    if (![_inputSource seekToOffset:_audioOffset error:error]) {
        return NO;
    }

    _isOpen = YES;

    return YES;
}

- (BOOL)closeReturningError:(NSError **)error {
    _isOpen = NO;
    return [super closeReturningError:error];
}

- (BOOL)isOpen {
    return _isOpen;
}

- (AVAudioFramePosition)packetPosition {
    return _packetPosition;
}

- (AVAudioFramePosition)packetCount {
    return _packetCount;
}

- (BOOL)decodeIntoBuffer:(AVAudioCompressedBuffer *)buffer
             packetCount:(AVAudioPacketCount)packetCount
                   error:(NSError **)error {
    NSParameterAssert(buffer != nil);
    NSParameterAssert([buffer.format isEqual:_processingFormat]);

    // Reset output buffer data size
    buffer.packetCount = 0;
    buffer.byteLength = 0;

    packetCount = std::min(packetCount, buffer.packetCapacity);
    if (packetCount == 0) {
        return YES;
    }

    AVAudioPacketCount packetsRemaining = static_cast<AVAudioPacketCount>(_packetCount - _packetPosition);
    AVAudioPacketCount packetsToRead = std::min(packetCount, packetsRemaining);
    AVAudioPacketCount packetsRead = 0;

    uint32_t packetSize = kSFBBytesPerDSDPacketPerChannel * _processingFormat.channelCount;

    for (;;) {
        // Read interleaved input, grouped as 8 one bit samples per frame (a single channel byte) into
        // a clustered frame (one channel byte per channel)

        auto *buf = static_cast<unsigned char *>(buffer.data) + buffer.byteLength;
        NSInteger bytesToRead = std::min(packetsToRead * packetSize, buffer.byteCapacity - buffer.byteLength);

        NSInteger bytesRead;
        if (![_inputSource readBytes:buf length:bytesToRead bytesRead:&bytesRead error:error]) {
            os_log_error(gSFBDSDDecoderLog, "Error reading audio data");
            return NO;
        }

        if (bytesRead != bytesToRead) {
            os_log_error(gSFBDSDDecoderLog, "Missing audio data: requested %ld bytes, got %ld",
                         static_cast<long>(bytesToRead), bytesRead);
            if (error != nullptr) {
                *error = createInvalidDSDIFFFileError(_inputSource.url);
            }
            return NO;
        }

        // Decoding is finished
        if (bytesRead == 0) {
            break;
        }

        packetsRead += (bytesRead / packetSize);

        buffer.packetCount += static_cast<AVAudioPacketCount>(bytesRead / packetSize);
        buffer.byteLength += static_cast<uint32_t>(bytesRead);

        // All requested frames were read
        if (packetsRead == packetCount) {
            break;
        }

        packetsToRead -= packetsRead;
    }

    _packetPosition += packetsRead;

    return YES;
}

- (BOOL)seekToPacket:(AVAudioFramePosition)packet error:(NSError **)error {
    NSParameterAssert(packet >= 0);

    NSInteger packetOffset = packet * kSFBBytesPerDSDPacketPerChannel * _processingFormat.channelCount;
    if (![_inputSource seekToOffset:(_audioOffset + packetOffset) error:error]) {
        os_log_debug(gSFBDSDDecoderLog, "-seekToPacket:error: failed seeking to input offset: %lld",
                     _audioOffset + packetOffset);
        return NO;
    }

    _packetPosition = packet;
    return YES;
}

@end
