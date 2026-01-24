//
// Copyright (c) 2020-2026 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import "SFBShortenDecoder.h"

#import "NSData+SFBExtensions.h"
#import "SFBErrorWithLocalizedDescription.h"
#import "SFBLocalizedNameForURL.h"

#import <AVFAudioExtensions/AVFAudioExtensions.h>

#import <libkern/OSByteOrder.h>
#import <os/log.h>

#import <algorithm>
#import <cmath>
#import <cstdlib>
#import <cstring>
#import <ranges>
#import <vector>

SFBAudioDecoderName const SFBAudioDecoderNameShorten = @"org.sbooth.AudioEngine.Decoder.Shorten";

SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyShortenVersion = @"_version";
SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyShortenFileType = @"_fileType";
SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyShortenNumberChannels = @"_channelCount";
SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyShortenBlockSize = @"_blocksize";
SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyShortenSampleRate = @"_sampleRate";
SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyShortenBitsPerSample = @"_bitsPerSample";
SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyShortenBigEndian = @"_bigEndian";

namespace {

// MARK: Constants

// Rice-Golomb code k values
constexpr auto parameterBitshift = 2;
constexpr auto parameterChannelCount = 0;
constexpr auto parameterEnergy = 3;
constexpr auto parameterExtraByte = 7;
constexpr auto parameterFileType = 4;
constexpr auto parameterFunction = 2;
constexpr auto parameterQLPC = 2;
constexpr auto parameterSkipBytes = 1;
constexpr auto parameterUInt32 = 2;
constexpr auto parameterVerbatimChunkSize = 5;
constexpr auto parameterVerbatimByte = 8;

// File commands
constexpr auto functionDiff0 = 0;
constexpr auto functionDiff1 = 1;
constexpr auto functionDiff2 = 2;
constexpr auto functionDiff3 = 3;
constexpr auto functionQuit = 4;
constexpr auto functionBlocksize = 5;
constexpr auto functionBitshift = 6;
constexpr auto functionQLPC = 7;
constexpr auto functionZero = 8;
constexpr auto functionVerbatim = 9;

// Format limitations
constexpr auto maxBlocksize = 65535;
constexpr auto verbatimChunkMaxSizeBytes = 256;

// File types
constexpr auto fileTypeSInt8 = 1;
constexpr auto fileTypeUInt8 = 2;
constexpr auto fileTypeSInt16BE = 3;
constexpr auto fileTypeUInt16BE = 4;
constexpr auto fileTypeSInt16LE = 5;
constexpr auto fileTypeUInt16LE = 6;

// Seeking support
constexpr auto seekTableRevision = 1;
constexpr auto seekHeaderSizeBytes = 12;
constexpr auto seekTrailerSizeBytes = 12;
constexpr auto seekEntrySizeBytes = 80;

constexpr int32_t roundedShiftDown(int32_t x, int k) noexcept {
    return (k == 0) ? x : (x >> (k - 1)) >> 1;
}

/// Returns a two-dimensional `rows` x `cols` array using one allocation from `malloc`
template <typename T>
T **allocateContiguous2DArray(size_t rows, size_t cols) noexcept {
    T **result = static_cast<T **>(std::malloc((rows * sizeof(T *)) + (rows * cols * sizeof(T))));
    if (!result) {
        return nullptr;
    }
    T *tmp = reinterpret_cast<T *>(result + rows);
    for (size_t i = 0; i < rows; ++i) {
        result[i] = tmp + (i * cols);
    }
    return result;
}

/// Variable-length input using Golomb-Rice coding
class VariableLengthInput {
  public:
    // An entry i has the lowest i bits set
    static constexpr uint32_t maskTable_[] = {
          0x0,       0x1,       0x3,        0x7,        0xf,        0x1f,      0x3f,     0x7f,      0xff,
          0x1ff,     0x3ff,     0x7ff,      0xfff,      0x1fff,     0x3fff,    0x7fff,   0xffff,    0x1ffff,
          0x3ffff,   0x7ffff,   0xfffff,    0x1fffff,   0x3fffff,   0x7fffff,  0xffffff, 0x1ffffff, 0x3ffffff,
          0x7ffffff, 0xfffffff, 0x1fffffff, 0x3fffffff, 0x7fffffff, 0xffffffff};

    /// Creates an empty `VariableLengthInput` object
    /// - important: `Allocate()` must be called before using
    VariableLengthInput() noexcept = default;

    ~VariableLengthInput() {
        delete[] byteBuffer_;
    }

    VariableLengthInput(const VariableLengthInput&) = delete;
    VariableLengthInput(VariableLengthInput&&) = delete;
    VariableLengthInput& operator=(const VariableLengthInput&) = delete;
    VariableLengthInput& operator=(VariableLengthInput&&) = delete;

    /// Input callback type
    using InputBlock = bool (^)(void *buf, size_t len, size_t& read);

    /// Sets the input callback
    void setInputCallback(InputBlock block) noexcept {
        inputBlock_ = block;
    }

    /// Allocates an internal buffer of the specified size
    /// - warning: Sizes other than `512` will break seeking
    bool allocate(size_t size = 512) noexcept {
        if (byteBuffer_) {
            return false;
        }

        auto *byteBuffer = new (std::nothrow) unsigned char[size];
        if (!byteBuffer) {
            return false;
        }

        byteBuffer_ = byteBuffer;
        byteBufferPosition_ = byteBuffer_;
        size_ = size;

        return true;
    }

    bool getRiceGolombCode(int32_t& i32, int k) noexcept {
#if DEBUG
        assert(k < 32);
#endif /* DEBUG */
        if (bitsAvailable_ == 0 && !refillBitBuffer()) {
            return false;
        }

        // Calculate unary quotient
        int32_t result;
        for (result = 0; !(bitBuffer_ & (1L << --bitsAvailable_)); ++result) {
            if (bitsAvailable_ == 0 && !refillBitBuffer()) {
                return false;
            }
        }

        while (k != 0) {
            if (bitsAvailable_ >= k) {
                result = (result << k) | static_cast<int32_t>((bitBuffer_ >> (bitsAvailable_ - k)) & maskTable_[k]);
                bitsAvailable_ -= k;
                k = 0;
            } else {
#if DEBUG
                assert(bitsAvailable_ < 32);
#endif /* DEBUG */
                result = (result << bitsAvailable_) | static_cast<int32_t>(bitBuffer_ & maskTable_[bitsAvailable_]);
                k -= bitsAvailable_;
                if (!refillBitBuffer()) {
                    return false;
                }
            }
        }

        i32 = result;
        return true;
    }

    bool getInt32(int32_t& i32, int k) noexcept {
        int32_t var;
        if (!getRiceGolombCode(var, k + 1)) {
            return false;
        }

        uint32_t uvar = static_cast<uint32_t>(var);
        if (uvar & 1) {
            i32 = ~(uvar >> 1);
        } else {
            i32 = (uvar >> 1);
        }
        return true;
    }

    bool getUInt32(uint32_t& ui32, int version, int k) noexcept {
        if (version > 0 && !getRiceGolombCode(k, parameterUInt32)) {
            return false;
        }

        int32_t i32;
        if (!getRiceGolombCode(i32, k)) {
            return false;
        }
        ui32 = static_cast<uint32_t>(i32);
        return true;
    }

    void reset() noexcept {
        byteBufferPosition_ = byteBuffer_;
        bytesAvailable_ = 0;
        bitsAvailable_ = 0;
    }

    bool refill() noexcept {
        size_t bytesRead = 0;
        if (!inputBlock_ || !inputBlock_(byteBuffer_, size_, bytesRead) || bytesRead < 4) {
            return false;
        }
        bytesAvailable_ += bytesRead;
        byteBufferPosition_ = byteBuffer_;
        return true;
    }

    bool setState(uint16_t byteBufferPosition, uint16_t bytesAvailable, uint32_t bitBuffer,
                  uint16_t bitsAvailable) noexcept {
        if (byteBufferPosition > size_ || bytesAvailable > size_ - byteBufferPosition || bitsAvailable > 32) {
            return false;
        }
        byteBufferPosition_ = byteBuffer_ + byteBufferPosition;
        bytesAvailable_ = bytesAvailable;
        bitBuffer_ = bitBuffer;
        bitsAvailable_ = bitsAvailable;
        return true;
    }

  private:
    /// Input callback
    InputBlock inputBlock_ = nil;
    /// Size of `byteBuffer_` in bytes
    size_t size_ = 0;
    /// Byte buffer
    unsigned char *byteBuffer_ = nullptr;
    /// Current position in `byteBuffer_`
    unsigned char *byteBufferPosition_ = nullptr;
    /// Bytes available in `byteBuffer_`
    int bytesAvailable_ = 0;
    /// Bit buffer
    uint32_t bitBuffer_ = 0;
    /// Bits available in `mBitBuffer`
    int bitsAvailable_ = 0;

    /// Reads a single `uint32_t` from the byte buffer, refilling if necessary
    bool refillBitBuffer() noexcept {
        if (bytesAvailable_ < 4 && !refill()) {
            return false;
        }

        bitBuffer_ = static_cast<uint32_t>((static_cast<int32_t>(byteBufferPosition_[0]) << 24) |
                                           (static_cast<int32_t>(byteBufferPosition_[1]) << 16) |
                                           (static_cast<int32_t>(byteBufferPosition_[2]) << 8) |
                                           static_cast<int32_t>(byteBufferPosition_[3]));

        byteBufferPosition_ += 4;
        bytesAvailable_ -= 4;
        bitsAvailable_ = 32;

        return true;
    }
};

/// Shorten seek table header
struct SeekTableHeader {
    int8_t   signature_[4];
    uint32_t version_;
    uint32_t fileSize_;
};

SeekTableHeader parseSeekTableHeader(const void *buf) {
    SeekTableHeader header;
    std::memcpy(header.signature_, buf, 4);
    header.version_ = OSReadLittleInt32(buf, 4);
    header.fileSize_ = OSReadLittleInt32(buf, 8);

    return header;
}

/// Shorten seek table trailer
struct SeekTableTrailer {
    uint32_t seekTableSize_;
    int8_t   signature_[8];
};

SeekTableTrailer parseSeekTableTrailer(const void *buf) {
    SeekTableTrailer trailer;
    trailer.seekTableSize_ = OSReadLittleInt32(buf, 0);
    std::memcpy(trailer.signature_, static_cast<const unsigned char *>(buf) + 4, 8);

    return trailer;
}

/// A Shorten seek table entry
struct SeekTableEntry {
    uint32_t frameNumber_;
    uint32_t byteOffsetInFile_;
    uint32_t lastBufferReadPosition_;
    uint16_t bytesAvailable_;
    uint16_t byteBufferPosition_;
    uint16_t bitBufferPosition_;
    uint32_t bitBuffer_;
    uint16_t bitshift_;
    int32_t  chanBuf0_[3];
    int32_t  chanBuf1_[3];
    int32_t  offset0_[4];
    int32_t  offset1_[4];
};

SeekTableEntry parseSeekTableEntry(const void *buf) {
    SeekTableEntry entry;
    entry.frameNumber_ = OSReadLittleInt32(buf, 0);
    entry.byteOffsetInFile_ = OSReadLittleInt32(buf, 4);
    entry.lastBufferReadPosition_ = OSReadLittleInt32(buf, 8);
    entry.bytesAvailable_ = OSReadLittleInt16(buf, 12);
    entry.byteBufferPosition_ = OSReadLittleInt16(buf, 14);
    entry.bitBufferPosition_ = OSReadLittleInt16(buf, 16);
    entry.bitBuffer_ = OSReadLittleInt32(buf, 18);
    entry.bitshift_ = OSReadLittleInt16(buf, 22);
    for (auto i = 0; i < 3; ++i) {
        entry.chanBuf0_[i] = static_cast<int32_t>(OSReadLittleInt32(buf, 24 + 4 * i));
    }
    for (auto i = 0; i < 3; ++i) {
        entry.chanBuf1_[i] = static_cast<int32_t>(OSReadLittleInt32(buf, 36 + 4 * i));
    }
    for (auto i = 0; i < 4; ++i) {
        entry.offset0_[i] = static_cast<int32_t>(OSReadLittleInt32(buf, 48 + 4 * i));
    }
    for (auto i = 0; i < 4; ++i) {
        entry.offset1_[i] = static_cast<int32_t>(OSReadLittleInt32(buf, 64 + 4 * i));
    }

    return entry;
}

/// Returns a generic error for an invalid Shorten file
NSError *genericShortenInvalidFormatErrorForURL(NSURL *_Nonnull url) noexcept {
    return SFBErrorWithLocalizedDescription(SFBAudioDecoderErrorDomain, SFBAudioDecoderErrorCodeInvalidFormat,
                                            NSLocalizedString(@"The file “%@” is not a valid Shorten file.", @""), @{
                                                NSLocalizedRecoverySuggestionErrorKey : NSLocalizedString(
                                                      @"The file's extension may not match the file's type.", @""),
                                                NSURLErrorKey : url
                                            },
                                            SFBLocalizedNameForURL(url));
}

} /* namespace */

@interface SFBShortenDecoder () {
  @private
    VariableLengthInput _input;
    int                 _version;
    int32_t             _lpcQuantOffset;
    int                 _fileType;
    int                 _channelCount;
    int                 _mean;
    int                 _blocksize;
    int                 _maxLPC;
    int                 _wrap;

    uint32_t _sampleRate;
    uint32_t _bitsPerSample;
    bool     _bigEndian;

    int32_t **_buffer;
    int32_t **_offset;
    int      *_qlpc;
    int       _bitshift;

    bool                        _eos;
    std::vector<SeekTableEntry> _seekTableEntries;

    AVAudioPCMBuffer    *_frameBuffer;
    AVAudioFramePosition _framePosition;
    AVAudioFramePosition _frameLength;
    uint64_t             _blocksDecoded;
}
- (bool)parseShortenHeaderReturningError:(NSError **)error;
- (bool)parseRIFFChunk:(const unsigned char *)chunkData size:(size_t)size error:(NSError **)error;
- (bool)parseFORMChunk:(const unsigned char *)chunkData size:(size_t)size error:(NSError **)error;
- (bool)decodeBlockReturningError:(NSError **)error;
- (bool)scanForSeekTableReturningError:(NSError **)error;
- (std::vector<SeekTableEntry>)parseExternalSeekTable:(NSURL *)url;
- (bool)seekTableIsValid:(std::vector<SeekTableEntry>)entries startOffset:(NSInteger)startOffset;
@end

@implementation SFBShortenDecoder

+ (void)load {
    [SFBAudioDecoder registerSubclass:[self class]];
}

+ (NSSet *)supportedPathExtensions {
    return [NSSet setWithObject:@"shn"];
}

+ (NSSet *)supportedMIMETypes {
    return [NSSet setWithObject:@"audio/x-shorten"];
}

+ (SFBAudioDecoderName)decoderName {
    return SFBAudioDecoderNameShorten;
}

+ (BOOL)testInputSource:(SFBInputSource *)inputSource
      formatIsSupported:(SFBTernaryTruthValue *)formatIsSupported
                  error:(NSError **)error {
    NSParameterAssert(inputSource != nil);
    NSParameterAssert(formatIsSupported != nullptr);

    NSData *header = [inputSource readHeaderOfLength:SFBShortenDetectionSize skipID3v2Tag:NO error:error];
    if (!header) {
        return NO;
    }

    if ([header isShortenHeader]) {
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
    if (![super openReturningError:error] || ![self parseShortenHeaderReturningError:error]) {
        return NO;
    }

    // Sanity checks
    if (_bitsPerSample != 8 && _bitsPerSample != 16) {
        os_log_error(gSFBAudioDecoderLog, "Unsupported bit depth: %u", _bitsPerSample);
        if (error) {
            if (error) {
                *error = SFBErrorWithLocalizedDescription(
                      SFBAudioDecoderErrorDomain, SFBAudioDecoderErrorCodeUnsupportedFormat,
                      NSLocalizedString(@"The file “%@” is not a supported Shorten file.", @""), @{
                          NSLocalizedRecoverySuggestionErrorKey :
                                NSLocalizedString(@"The audio bit depth is not supported.", @""),
                          NSURLErrorKey : _inputSource.url
                      },
                      SFBLocalizedNameForURL(_inputSource.url));
            }
        }
        return NO;
    }

    if ((_bitsPerSample == 8 && _fileType != fileTypeUInt8 && _fileType != fileTypeSInt8) ||
        (_bitsPerSample == 16 && _fileType != fileTypeUInt16BE && _fileType != fileTypeUInt16LE &&
         _fileType != fileTypeSInt16BE && _fileType != fileTypeSInt16LE)) {
        os_log_error(gSFBAudioDecoderLog, "Unsupported bit depth/audio type combination: %u, %u", _bitsPerSample,
                     _fileType);
        if (error) {
            if (error) {
                *error = SFBErrorWithLocalizedDescription(
                      SFBAudioDecoderErrorDomain, SFBAudioDecoderErrorCodeUnsupportedFormat,
                      NSLocalizedString(@"The file “%@” is not a supported Shorten file.", @""), @{
                          NSLocalizedRecoverySuggestionErrorKey : NSLocalizedString(
                                @"The audio bit depth and sample type combination is not supported.", @""),
                          NSURLErrorKey : _inputSource.url
                      },
                      SFBLocalizedNameForURL(_inputSource.url));
            }
        }
        return NO;
    }

    if (![self scanForSeekTableReturningError:error]) {
        return NO;
    }

    // Set up the processing format
    AudioStreamBasicDescription processingStreamDescription{};

    processingStreamDescription.mFormatID = kAudioFormatLinearPCM;
    processingStreamDescription.mFormatFlags = kAudioFormatFlagIsNonInterleaved | kAudioFormatFlagIsPacked;
    // Apparently *16BE isn't true for 'AIFF'
    //    if(_fileType == fileTypeUInt16BE || _fileType == fileTypeSInt16BE)
    if (_bigEndian) {
        processingStreamDescription.mFormatFlags |= kAudioFormatFlagIsBigEndian;
    }
    if (_fileType == fileTypeSInt8 || _fileType == fileTypeSInt16BE || _fileType == fileTypeSInt16LE) {
        processingStreamDescription.mFormatFlags |= kAudioFormatFlagIsSignedInteger;
    }

    processingStreamDescription.mSampleRate = _sampleRate;
    processingStreamDescription.mChannelsPerFrame = static_cast<UInt32>(_channelCount);
    processingStreamDescription.mBitsPerChannel = _bitsPerSample;

    processingStreamDescription.mBytesPerPacket = (_bitsPerSample + 7) / 8;
    processingStreamDescription.mFramesPerPacket = 1;
    processingStreamDescription.mBytesPerFrame =
          processingStreamDescription.mBytesPerPacket / processingStreamDescription.mFramesPerPacket;

    AVAudioChannelLayout *channelLayout = nil;
    switch (_channelCount) {
    case 1:
        channelLayout = [AVAudioChannelLayout layoutWithLayoutTag:kAudioChannelLayoutTag_Mono];
        break;
    case 2:
        channelLayout = [AVAudioChannelLayout layoutWithLayoutTag:kAudioChannelLayoutTag_Stereo];
        break;
        // FIXME: Is there a standard ordering for multichannel files? WAVEFORMATEX?
    default:
        channelLayout = [AVAudioChannelLayout layoutWithLayoutTag:(kAudioChannelLayoutTag_Unknown | _channelCount)];
        break;
    }

    _processingFormat = [[AVAudioFormat alloc] initWithStreamDescription:&processingStreamDescription
                                                           channelLayout:channelLayout];

    // Set up the source format
    AudioStreamBasicDescription sourceStreamDescription{};

    sourceStreamDescription.mFormatID = kSFBAudioFormatShorten;

    sourceStreamDescription.mSampleRate = _sampleRate;
    sourceStreamDescription.mChannelsPerFrame = static_cast<UInt32>(_channelCount);
    sourceStreamDescription.mBitsPerChannel = _bitsPerSample;

    sourceStreamDescription.mFramesPerPacket = static_cast<UInt32>(_blocksize);

    _sourceFormat = [[AVAudioFormat alloc] initWithStreamDescription:&sourceStreamDescription
                                                       channelLayout:channelLayout];

    // Populate codec properties
    _properties = @{
        SFBAudioDecodingPropertiesKeyShortenVersion : @(_version),
        SFBAudioDecodingPropertiesKeyShortenFileType : @(_fileType),
        SFBAudioDecodingPropertiesKeyShortenNumberChannels : @(_channelCount),
        SFBAudioDecodingPropertiesKeyShortenBlockSize : @(_blocksize),
        SFBAudioDecodingPropertiesKeyShortenSampleRate : @(_sampleRate),
        SFBAudioDecodingPropertiesKeyShortenBitsPerSample : @(_bitsPerSample),
        SFBAudioDecodingPropertiesKeyShortenBigEndian : _bigEndian ? @YES : @NO,
    };

    _framePosition = 0;
    _frameBuffer = [[AVAudioPCMBuffer alloc] initWithPCMFormat:_processingFormat
                                                 frameCapacity:static_cast<AVAudioFrameCount>(_blocksize)];

    // Allocate decoding buffers
    _buffer = allocateContiguous2DArray<int32_t>(static_cast<size_t>(_channelCount),
                                                 static_cast<size_t>(_blocksize + _wrap));
    if (!_buffer) {
        if (error) {
            *error = [NSError errorWithDomain:NSPOSIXErrorDomain code:ENOMEM userInfo:nil];
        }
        return NO;
    }

    _offset = allocateContiguous2DArray<int32_t>(static_cast<size_t>(_channelCount),
                                                 static_cast<size_t>(std::max(1, _mean)));
    if (!_offset) {
        std::free(_buffer);
        if (error) {
            *error = [NSError errorWithDomain:NSPOSIXErrorDomain code:ENOMEM userInfo:nil];
        }
        return NO;
    }

    if (_maxLPC > 0) {
        _qlpc = static_cast<int *>(std::malloc(sizeof(int) * _maxLPC));
        if (!_qlpc) {
            std::free(_buffer);
            std::free(_offset);
            if (error) {
                *error = [NSError errorWithDomain:NSPOSIXErrorDomain code:ENOMEM userInfo:nil];
            }
            return NO;
        }
    }

    for (auto i = 0; i < _channelCount; ++i) {
        for (auto j = 0; j < _wrap; ++j) {
            _buffer[i][j] = 0;
        }
        _buffer[i] += _wrap;
    }

    // Initialize offset
    int32_t mean = 0;
    switch (_fileType) {
    case fileTypeSInt8:
    case fileTypeSInt16BE:
    case fileTypeSInt16LE:
        mean = 0;
        break;
    case fileTypeUInt8:
        mean = 0x80;
        break;
    case fileTypeUInt16BE:
    case fileTypeUInt16LE:
        mean = 0x8000;
        break;
    default:
        os_log_error(gSFBAudioDecoderLog, "Unsupported audio type: %u", _fileType);
        return NO;
    }

    for (auto chan = 0; chan < _channelCount; ++chan) {
        for (auto i = 0; i < std::max(1, _mean); ++i) {
            _offset[chan][i] = mean;
        }
    }

    return YES;
}

- (BOOL)closeReturningError:(NSError **)error {
    if (_buffer) {
        std::free(_buffer);
        _buffer = nullptr;
    }
    if (_offset) {
        std::free(_offset);
        _offset = nullptr;
    }
    if (_qlpc) {
        std::free(_qlpc);
        _qlpc = nullptr;
    }
    _frameBuffer = nil;

    return [super closeReturningError:error];
}

- (BOOL)isOpen {
    return _buffer != nullptr;
}

- (AVAudioFramePosition)framePosition {
    return _framePosition;
}

- (AVAudioFramePosition)frameLength {
    return _frameLength;
}

- (BOOL)decodeIntoBuffer:(AVAudioPCMBuffer *)buffer frameLength:(AVAudioFrameCount)frameLength error:(NSError **)error {
    NSParameterAssert(buffer != nil);
    NSParameterAssert([buffer.format isEqual:_processingFormat]);

    // Reset output buffer data size
    buffer.frameLength = 0;

    frameLength = std::min(frameLength, buffer.frameCapacity);
    if (frameLength == 0) {
        return YES;
    }

    AVAudioFrameCount framesDecoded = 0;

    for (;;) {
        if (const auto framesToCopy = std::min(frameLength - framesDecoded, _frameBuffer.frameLength);
            framesToCopy > 0) {
            const auto framesCopied = [buffer appendFromBuffer:_frameBuffer
                                             readingFromOffset:0
                                                   frameLength:framesToCopy];
            const auto framesTrimmed = [_frameBuffer trimAtOffset:0 frameLength:framesCopied];
#if DEBUG
            assert(framesTrimmed == framesCopied);
#endif /* DEBUG */
            framesDecoded += framesCopied;
        }

        // All requested frames were read or EOS reached
        if (framesDecoded == frameLength || _eos) {
            break;
        }

        // Decode the next block
        if (![self decodeBlockReturningError:error]) {
            os_log_error(gSFBAudioDecoderLog, "Error decoding Shorten block");
            return NO;
        }
    }

    _framePosition += framesDecoded;

    return YES;
}

- (BOOL)supportsSeeking {
    return !_seekTableEntries.empty();
}

- (BOOL)seekToFrame:(AVAudioFramePosition)frame error:(NSError **)error {
    NSParameterAssert(frame >= 0);

    if (frame >= self.frameLength) {
        if (error) {
            *error = [NSError errorWithDomain:NSPOSIXErrorDomain code:EINVAL userInfo:nil];
        }
        return NO;
    }

    auto entry = std::ranges::upper_bound(_seekTableEntries, frame, {}, &SeekTableEntry::frameNumber_);
    if (entry == std::begin(_seekTableEntries)) {
        os_log_error(gSFBAudioDecoderLog, "No seek table entry for frame %lld", frame);
        if (error) {
            *error = [NSError
                  errorWithDomain:SFBAudioDecoderErrorDomain
                             code:SFBAudioDecoderErrorCodeSeekError
                         userInfo:@{
                             NSLocalizedRecoverySuggestionErrorKey : NSLocalizedString(
                                   @"There is no suitable seek table entry for the requested audio frame.", @""),
                             NSURLErrorKey : _inputSource.url
                         }];
        }
        return NO;
    }
    entry = std::prev(entry);

#if DEBUG
    os_log_debug(gSFBAudioDecoderLog, "Using seek table entry %ld for frame %d to seek to frame %lld",
                 std::ranges::distance(_seekTableEntries.cbegin(), entry), entry->frameNumber_, frame);
#endif

    if (![_inputSource seekToOffset:entry->lastBufferReadPosition_ error:error]) {
        return NO;
    }

    _eos = false;
    _input.reset();
    if (!_input.refill() || !_input.setState(entry->byteBufferPosition_, entry->bytesAvailable_, entry->bitBuffer_,
                                             entry->bitBufferPosition_)) {
        if (error) {
            *error = [NSError errorWithDomain:NSPOSIXErrorDomain code:EIO userInfo:@{NSURLErrorKey : _inputSource.url}];
        }
        return NO;
    }

    _buffer[0][-1] = entry->chanBuf0_[0];
    _buffer[0][-2] = entry->chanBuf0_[1];
    _buffer[0][-3] = entry->chanBuf0_[2];
    if (_channelCount == 2) {
        _buffer[1][-1] = entry->chanBuf1_[0];
        _buffer[1][-2] = entry->chanBuf1_[1];
        _buffer[1][-3] = entry->chanBuf1_[2];
    }

    for (auto i = 0; i < std::max(1, _mean); ++i) {
        _offset[0][i] = entry->offset0_[i];
        if (_channelCount == 2) {
            _offset[1][i] = entry->offset1_[i];
        }
    }

    _bitshift = entry->bitshift_;

    _framePosition = entry->frameNumber_;
    _frameBuffer.frameLength = 0;

    const auto        framesToSkip = static_cast<AVAudioFrameCount>(frame - entry->frameNumber_);
    AVAudioFrameCount framesSkipped = 0;

    for (;;) {
        // All requested frames were skipped or EOS reached
        if (framesSkipped == framesToSkip || _eos) {
            break;
        }

        // Decode the next block
        if (![self decodeBlockReturningError:error]) {
            os_log_error(gSFBAudioDecoderLog, "Error decoding Shorten block");
            return NO;
        }

        if (const auto framesToTrim = std::min(framesToSkip - framesSkipped, _frameBuffer.frameLength);
            framesToTrim > 0) {
            framesSkipped += [_frameBuffer trimAtOffset:0 frameLength:framesToTrim];
        }
    }

    _framePosition += framesSkipped;

    return YES;
}

- (bool)parseShortenHeaderReturningError:(NSError **)error {
    // Read magic number
    uint32_t magic;
    if (![_inputSource readUInt32BigEndian:&magic error:nil] || magic != 'ajkg') {
        if (error) {
            *error = genericShortenInvalidFormatErrorForURL(_inputSource.url);
        }
        return false;
    }

    constexpr auto minSupportedVersion = 1;
    constexpr auto maxSupportedVersion = 3;

    // Read file version
    uint8_t version;
    if (![_inputSource readUInt8:&version error:nil] || version < minSupportedVersion ||
        version > maxSupportedVersion) {
        os_log_error(gSFBAudioDecoderLog, "Unsupported version: %u", version);
        if (error) {
            *error = SFBErrorWithLocalizedDescription(
                  SFBAudioDecoderErrorDomain, SFBAudioDecoderErrorCodeUnsupportedFormat,
                  NSLocalizedString(@"The file “%@” is not a supported Shorten file.", @""), @{
                      NSLocalizedRecoverySuggestionErrorKey :
                            NSLocalizedString(@"The Shorten version is not supported.", @""),
                      NSURLErrorKey : _inputSource.url
                  },
                  SFBLocalizedNameForURL(_inputSource.url));
        }
        return false;
    }
    _version = version;

    constexpr auto v0DefaultMean = 0;
    constexpr auto v2DefaultMean = 4;

    // Default mean
    _mean = _version < 2 ? v0DefaultMean : v2DefaultMean;

    // Set up variable length input
    if (!_input.allocate()) {
        os_log_error(gSFBAudioDecoderLog, "Unable to allocate variable-length input");
        if (error) {
            *error = [NSError errorWithDomain:NSPOSIXErrorDomain code:ENOMEM userInfo:nil];
        }
        return false;
    }

    __weak SFBInputSource *inputSource = self->_inputSource;
    _input.setInputCallback(^bool(void *buf, size_t len, size_t& read) {
        NSInteger bytesRead;
        if (![inputSource readBytes:buf length:static_cast<NSInteger>(len) bytesRead:&bytesRead error:nil]) {
            return false;
        }
        read = static_cast<size_t>(bytesRead);
        return true;
    });

    // Read file type
    uint32_t fileType;
    if (!_input.getUInt32(fileType, _version, parameterFileType)) {
        if (error) {
            *error = genericShortenInvalidFormatErrorForURL(_inputSource.url);
        }
        return false;
    }
    if (fileType != fileTypeUInt8 && fileType != fileTypeSInt8 && fileType != fileTypeUInt16BE &&
        fileType != fileTypeUInt16LE && fileType != fileTypeSInt16BE && fileType != fileTypeSInt16LE) {
        os_log_error(gSFBAudioDecoderLog, "Unsupported audio type: %u", fileType);
        if (error) {
            *error = SFBErrorWithLocalizedDescription(
                  SFBAudioDecoderErrorDomain, SFBAudioDecoderErrorCodeUnsupportedFormat,
                  NSLocalizedString(@"The file “%@” is not a supported Shorten file.", @""), @{
                      NSLocalizedRecoverySuggestionErrorKey :
                            NSLocalizedString(@"The audio type is invalid or unsupported.", @""),
                      NSURLErrorKey : _inputSource.url
                  },
                  SFBLocalizedNameForURL(_inputSource.url));
        }
        return false;
    }
    _fileType = static_cast<int>(fileType);

    // Maximum supported channel count
    constexpr auto maxChannelCount = 8;

    // Read number of channels
    uint32_t channelCount = 0;
    if (!_input.getUInt32(channelCount, _version, parameterChannelCount) || channelCount == 0 ||
        channelCount > maxChannelCount) {
        os_log_error(gSFBAudioDecoderLog, "Invalid or unsupported channel count: %u", channelCount);
        if (error) {
            *error = SFBErrorWithLocalizedDescription(
                  SFBAudioDecoderErrorDomain, SFBAudioDecoderErrorCodeUnsupportedFormat,
                  NSLocalizedString(@"The file “%@” is not a supported Shorten file.", @""), @{
                      NSLocalizedRecoverySuggestionErrorKey :
                            NSLocalizedString(@"The number of channels is invalid or unsupported.", @""),
                      NSURLErrorKey : _inputSource.url
                  },
                  SFBLocalizedNameForURL(_inputSource.url));
        }
        return false;
    }
    _channelCount = static_cast<int>(channelCount);

    constexpr auto defaultBlockSize = 256;
    /// Number of extra samples in buffer
    constexpr auto defaultWrap = 3;

    // Read blocksize if version > 0
    if (_version > 0) {
        uint32_t blocksize = 0;
        if (!_input.getUInt32(blocksize, _version, static_cast<int>(std::log2(defaultBlockSize))) || blocksize == 0 ||
            blocksize > maxBlocksize || blocksize <= defaultWrap) {
            os_log_error(gSFBAudioDecoderLog, "Invalid or unsupported block size: %u", blocksize);
            if (error) {
                *error = SFBErrorWithLocalizedDescription(
                      SFBAudioDecoderErrorDomain, SFBAudioDecoderErrorCodeUnsupportedFormat,
                      NSLocalizedString(@"The file “%@” is not a supported Shorten file.", @""), @{
                          NSLocalizedRecoverySuggestionErrorKey :
                                NSLocalizedString(@"The block size is invalid or unsupported.", @""),
                          NSURLErrorKey : _inputSource.url
                      },
                      SFBLocalizedNameForURL(_inputSource.url));
            }
            return false;
        }
        _blocksize = static_cast<int>(blocksize);

        uint32_t maxLPC = 0;
        if (!_input.getUInt32(maxLPC, _version, parameterQLPC) || maxLPC > 1024) {
            os_log_error(gSFBAudioDecoderLog, "Invalid maximum linear predictor order: %u", maxLPC);
            if (error) {
                *error = SFBErrorWithLocalizedDescription(
                      SFBAudioDecoderErrorDomain, SFBAudioDecoderErrorCodeUnsupportedFormat,
                      NSLocalizedString(@"The file “%@” is not a supported Shorten file.", @""), @{
                          NSLocalizedRecoverySuggestionErrorKey : NSLocalizedString(
                                @"The maximum linear predictor order is invalid or unsupported.", @""),
                          NSURLErrorKey : _inputSource.url
                      },
                      SFBLocalizedNameForURL(_inputSource.url));
            }
            return false;
        }
        _maxLPC = static_cast<int>(maxLPC);

        uint32_t mean = 0;
        if (!_input.getUInt32(mean, _version, 0) || mean > 32768) {
            os_log_error(gSFBAudioDecoderLog, "Invalid mean: %u", mean);
            if (error) {
                *error = SFBErrorWithLocalizedDescription(
                      SFBAudioDecoderErrorDomain, SFBAudioDecoderErrorCodeUnsupportedFormat,
                      NSLocalizedString(@"The file “%@” is not a supported Shorten file.", @""), @{
                          NSLocalizedRecoverySuggestionErrorKey :
                                NSLocalizedString(@"The mean is invalid or unsupported.", @""),
                          NSURLErrorKey : _inputSource.url
                      },
                      SFBLocalizedNameForURL(_inputSource.url));
            }
            return false;
        }
        _mean = static_cast<int>(mean);

        uint32_t skipCount;
        if (!_input.getUInt32(skipCount, _version, parameterSkipBytes) /* || nskip > bits_remaining_in_input */) {
            if (error) {
                *error = genericShortenInvalidFormatErrorForURL(_inputSource.url);
            }
            return false;
        }

        for (uint32_t i = 0; i < skipCount; ++i) {
            uint32_t dummy;
            if (!_input.getUInt32(dummy, _version, parameterExtraByte)) {
                if (error) {
                    *error = genericShortenInvalidFormatErrorForURL(_inputSource.url);
                }
                return false;
            }
        }
    } else {
        constexpr auto defaultMaxLPC = 0;
        _blocksize = defaultBlockSize;
        _maxLPC = defaultMaxLPC;
    }

    _wrap = std::max(defaultWrap, _maxLPC);

    if (_version > 1) {
        constexpr auto v2LPCQuantOffset = (1 << parameterQLPC);
        _lpcQuantOffset = v2LPCQuantOffset;
    }

    // Parse the WAVE or AIFF header in the verbatim section

    int32_t function;
    if (!_input.getRiceGolombCode(function, parameterFunction) || function != functionVerbatim) {
        os_log_error(gSFBAudioDecoderLog, "Missing initial verbatim section");
        if (error) {
            *error = SFBErrorWithLocalizedDescription(
                  SFBAudioDecoderErrorDomain, SFBAudioDecoderErrorCodeInvalidFormat,
                  NSLocalizedString(@"The file “%@” is not a valid Shorten file.", @""), @{
                      NSLocalizedRecoverySuggestionErrorKey :
                            NSLocalizedString(@"The initial verbatim section is missing.", @""),
                      NSURLErrorKey : _inputSource.url
                  },
                  SFBLocalizedNameForURL(_inputSource.url));
        }
        return false;
    }

    constexpr auto canonicalHeaderSizeBytes = 44;

    int32_t headerSize;
    if (!_input.getRiceGolombCode(headerSize, parameterVerbatimChunkSize) || headerSize < canonicalHeaderSizeBytes ||
        headerSize > verbatimChunkMaxSizeBytes) {
        os_log_error(gSFBAudioDecoderLog, "Incorrect header size: %u", headerSize);
        if (error) {
            *error = genericShortenInvalidFormatErrorForURL(_inputSource.url);
        }
        return false;
    }

    std::vector<unsigned char> headerBytes(headerSize);
    for (int32_t i = 0; i < headerSize; ++i) {
        int32_t byte;
        if (!_input.getRiceGolombCode(byte, parameterVerbatimByte)) {
            if (error) {
                *error = genericShortenInvalidFormatErrorForURL(_inputSource.url);
            }
            return false;
        }

        headerBytes[i] = static_cast<unsigned char>(byte);
    }

    // headerBytes is at least canonicalHeaderSizeBytes (44) in size

    auto chunkID = OSReadBigInt32(headerBytes.data(), 0);
    //	auto chunkSize = OSReadBigInt32(headerBytes.data(), 4);

    if (chunkID == 'RIFF') {
        // WAVE
        if (![self parseRIFFChunk:(headerBytes.data() + 8) size:(headerSize - 8) error:error]) {
            return false;
        }
    } else if (chunkID == 'FORM') {
        // AIFF
        if (![self parseFORMChunk:(headerBytes.data() + 8) size:(headerSize - 8) error:error]) {
            return false;
        }
    } else {
        os_log_error(gSFBAudioDecoderLog, "Unsupported data format: %u", chunkID);
        if (error) {
            *error = SFBErrorWithLocalizedDescription(
                  SFBAudioDecoderErrorDomain, SFBAudioDecoderErrorCodeUnsupportedFormat,
                  NSLocalizedString(@"The file “%@” is not a supported Shorten file.", @""), @{
                      NSLocalizedRecoverySuggestionErrorKey :
                            NSLocalizedString(@"The audio data format is not supported.", @""),
                      NSURLErrorKey : _inputSource.url
                  },
                  SFBLocalizedNameForURL(_inputSource.url));
        }
        return false;
    }

    return true;
}

- (bool)parseRIFFChunk:(const unsigned char *)chunkData size:(size_t)size error:(NSError **)error {
    NSParameterAssert(chunkData != nullptr);
    NSParameterAssert(size >= 28);

    constexpr auto waveFormatPCMTag = 0x0001;

    uintptr_t offset = 0;

    auto chunkID = OSReadBigInt32(chunkData, offset);
    offset += 4;
    if (chunkID != 'WAVE') {
        os_log_error(gSFBAudioDecoderLog, "Missing 'WAVE' in 'RIFF' chunk");
        if (error) {
            *error = genericShortenInvalidFormatErrorForURL(_inputSource.url);
        }
        return false;
    }

    auto     sawFormatChunk = false;
    uint32_t dataChunkSize = 0;
    uint16_t blockAlign = 0;

    while (offset < size) {
        chunkID = OSReadBigInt32(chunkData, offset);
        offset += 4;

        auto chunkSize = OSReadLittleInt32(chunkData, offset);
        offset += 4;

        switch (chunkID) {
        case 'fmt ': {
            if (chunkSize < 16) {
                os_log_error(gSFBAudioDecoderLog, "'fmt ' chunk is too small (%u bytes)", chunkSize);
                if (error) {
                    *error = genericShortenInvalidFormatErrorForURL(_inputSource.url);
                }
                return false;
            }

            auto formatTag = OSReadLittleInt16(chunkData, offset);
            offset += 2;
            if (formatTag != waveFormatPCMTag) {
                os_log_error(gSFBAudioDecoderLog, "Unsupported WAVE format tag: %x", formatTag);
                if (error) {
                    *error = SFBErrorWithLocalizedDescription(
                          SFBAudioDecoderErrorDomain, SFBAudioDecoderErrorCodeUnsupportedFormat,
                          NSLocalizedString(@"The file “%@” is not a supported Shorten file.", @""), @{
                              NSLocalizedRecoverySuggestionErrorKey :
                                    NSLocalizedString(@"The WAVE format tag is not supported.", @""),
                              NSURLErrorKey : _inputSource.url
                          },
                          SFBLocalizedNameForURL(_inputSource.url));
                }
                return false;
            }

            auto channels = OSReadLittleInt16(chunkData, offset);
            offset += 2;
            if (_channelCount != channels) {
                os_log_info(gSFBAudioDecoderLog, "Channel count mismatch between Shorten (%d) and 'fmt ' chunk (%u)",
                            _channelCount, channels);
            }

            _sampleRate = OSReadLittleInt32(chunkData, offset);
            offset += 4;

            // Skip average bytes per second
            offset += 4;

            blockAlign = OSReadLittleInt16(chunkData, offset);
            offset += 2;

            _bitsPerSample = OSReadLittleInt16(chunkData, offset);
            offset += 2;

            if (chunkSize > 16) {
                os_log_info(gSFBAudioDecoderLog, "%u bytes in 'fmt ' chunk not parsed", chunkSize - 16);
            }

            sawFormatChunk = true;

            break;
        }

        case 'data':
            dataChunkSize = chunkSize;
            break;
        }
    }

    if (!sawFormatChunk) {
        os_log_error(gSFBAudioDecoderLog, "Missing 'fmt ' chunk");
        if (error) {
            *error = genericShortenInvalidFormatErrorForURL(_inputSource.url);
        }
        return false;
    }

    if (dataChunkSize && blockAlign) {
        _frameLength = dataChunkSize / blockAlign;
    }

    return true;
}

- (bool)parseFORMChunk:(const unsigned char *)chunkData size:(size_t)size error:(NSError **)error {
    NSParameterAssert(chunkData != nullptr);
    NSParameterAssert(size >= 30);

    uintptr_t offset = 0;

    auto chunkID = OSReadBigInt32(chunkData, offset);
    offset += 4;
    if (chunkID != 'AIFF' && chunkID != 'AIFC') {
        os_log_error(gSFBAudioDecoderLog, "Missing 'AIFF' or 'AIFC' in 'FORM' chunk");
        if (error) {
            *error = genericShortenInvalidFormatErrorForURL(_inputSource.url);
        }
        return false;
    }

    if (chunkID == 'AIFC') {
        _bigEndian = true;
    }

    auto sawCommonChunk = false;
    while (offset < size) {
        chunkID = OSReadBigInt32(chunkData, offset);
        offset += 4;

        auto chunkSize = OSReadBigInt32(chunkData, offset);
        offset += 4;

        // All chunks must have an even length but the pad byte is not included in ckSize
        chunkSize += (chunkSize & 1);

        switch (chunkID) {
        case 'COMM': {
            if (chunkSize < 18) {
                os_log_error(gSFBAudioDecoderLog, "'COMM' chunk is too small (%u bytes)", chunkSize);
                if (error) {
                    *error = genericShortenInvalidFormatErrorForURL(_inputSource.url);
                }
                return false;
            }

            auto channels = OSReadBigInt16(chunkData, offset);
            offset += 2;
            if (_channelCount != channels) {
                os_log_info(gSFBAudioDecoderLog, "Channel count mismatch between Shorten (%d) and 'COMM' chunk (%u)",
                            _channelCount, channels);
            }

            _frameLength = OSReadBigInt32(chunkData, offset);
            offset += 4;

            _bitsPerSample = OSReadBigInt16(chunkData, offset);
            offset += 2;

            // sample rate is IEEE 754 80-bit extended float (16-bit exponent, 1-bit integer part, 63-bit fraction)
            auto exp = static_cast<int16_t>(OSReadBigInt16(chunkData, offset)) - 16383 - 63;
            offset += 2;
            if (exp < -63 || exp > 63) {
                os_log_error(gSFBAudioDecoderLog, "exp out of range: %d", exp);
                if (error) {
                    *error = genericShortenInvalidFormatErrorForURL(_inputSource.url);
                }
                return false;
            }

            auto frac = OSReadBigInt64(chunkData, offset);
            offset += 8;
            if (exp >= 0) {
                _sampleRate = static_cast<uint32_t>(frac << exp);
            } else {
                _sampleRate = static_cast<uint32_t>((frac + (static_cast<uint64_t>(1) << (-exp - 1))) >> -exp);
            }

            if (chunkSize > 18) {
                os_log_info(gSFBAudioDecoderLog, "%u bytes in 'COMM' chunk not parsed", chunkSize - 16);
            }

            sawCommonChunk = true;

            break;
        }

            // Skip all other chunks
        default:
            offset += chunkSize;
            break;
        }
    }

    if (!sawCommonChunk) {
        os_log_error(gSFBAudioDecoderLog, "Missing 'COMM' chunk");
        if (error) {
            *error = genericShortenInvalidFormatErrorForURL(_inputSource.url);
        }
        return false;
    }

    return true;
}

- (bool)decodeBlockReturningError:(NSError **)error {
    int chan = 0;
    for (;;) {
        int32_t cmd;
        if (!_input.getRiceGolombCode(cmd, parameterFunction)) {
            if (error) {
                *error = [NSError errorWithDomain:SFBAudioDecoderErrorDomain
                                             code:SFBAudioDecoderErrorCodeDecodingError
                                         userInfo:@{NSURLErrorKey : _inputSource.url}];
            }
            return false;
        }

        if (cmd == functionQuit) {
            _eos = true;
            return true;
        }

        switch (cmd) {
        case functionZero:
        case functionDiff0:
        case functionDiff1:
        case functionDiff2:
        case functionDiff3:
        case functionQLPC: {
            int32_t  chanOffset;
            int32_t *chanBuffer = _buffer[chan];
            int      resn = 0;
            int      lpc;

            if (cmd != functionZero) {
                if (!_input.getRiceGolombCode(resn, parameterEnergy)) {
                    if (error) {
                        *error = [NSError errorWithDomain:SFBAudioDecoderErrorDomain
                                                     code:SFBAudioDecoderErrorCodeDecodingError
                                                 userInfo:@{NSURLErrorKey : _inputSource.url}];
                    }
                    return false;
                }
                // Versions > 0 changed the behavior
                if (_version == 0) {
                    resn--;
                }
            }

            if (_mean == 0) {
                chanOffset = _offset[chan][0];
            } else {
                int32_t sum = (_version < 2) ? 0 : _mean / 2;
                for (auto i = 0; i < _mean; i++) {
                    sum += _offset[chan][i];
                }
                if (_version < 2) {
                    chanOffset = sum / _mean;
                } else {
                    chanOffset = roundedShiftDown(sum / _mean, _bitshift);
                }
            }

            switch (cmd) {
            case functionZero:
                for (auto i = 0; i < _blocksize; ++i) {
                    chanBuffer[i] = 0;
                }
                break;
            case functionDiff0:
                for (auto i = 0; i < _blocksize; ++i) {
                    int32_t var;
                    if (!_input.getInt32(var, resn)) {
                        if (error) {
                            *error = [NSError errorWithDomain:SFBAudioDecoderErrorDomain
                                                         code:SFBAudioDecoderErrorCodeDecodingError
                                                     userInfo:@{NSURLErrorKey : _inputSource.url}];
                        }
                        return false;
                    }
                    chanBuffer[i] = var + chanOffset;
                }
                break;
            case functionDiff1:
                for (auto i = 0; i < _blocksize; ++i) {
                    int32_t var;
                    if (!_input.getInt32(var, resn)) {
                        if (error) {
                            *error = [NSError errorWithDomain:SFBAudioDecoderErrorDomain
                                                         code:SFBAudioDecoderErrorCodeDecodingError
                                                     userInfo:@{NSURLErrorKey : _inputSource.url}];
                        }
                        return false;
                    }
                    chanBuffer[i] = var + chanBuffer[i - 1];
                }
                break;
            case functionDiff2:
                for (auto i = 0; i < _blocksize; ++i) {
                    int32_t var;
                    if (!_input.getInt32(var, resn)) {
                        if (error) {
                            *error = [NSError errorWithDomain:SFBAudioDecoderErrorDomain
                                                         code:SFBAudioDecoderErrorCodeDecodingError
                                                     userInfo:@{NSURLErrorKey : _inputSource.url}];
                        }
                        return false;
                    }
                    chanBuffer[i] = var + (2 * chanBuffer[i - 1] - chanBuffer[i - 2]);
                }
                break;
            case functionDiff3:
                for (auto i = 0; i < _blocksize; ++i) {
                    int32_t var;
                    if (!_input.getInt32(var, resn)) {
                        if (error) {
                            *error = [NSError errorWithDomain:SFBAudioDecoderErrorDomain
                                                         code:SFBAudioDecoderErrorCodeDecodingError
                                                     userInfo:@{NSURLErrorKey : _inputSource.url}];
                        }
                        return false;
                    }
                    chanBuffer[i] = var + (3 * (chanBuffer[i - 1] - chanBuffer[i - 2])) + chanBuffer[i - 3];
                }
                break;
            case functionQLPC:
                if (!_input.getRiceGolombCode(lpc, parameterQLPC) || lpc > _maxLPC) {
                    os_log_error(gSFBAudioDecoderLog, "Invalid or unsupported linear predictor order: %d", lpc);
                    if (error) {
                        *error = [NSError errorWithDomain:SFBAudioDecoderErrorDomain
                                                     code:SFBAudioDecoderErrorCodeDecodingError
                                                 userInfo:@{NSURLErrorKey : _inputSource.url}];
                    }
                    return false;
                }

                for (auto i = 0; i < lpc; ++i) {
                    if (!_input.getInt32(_qlpc[i], parameterQLPC)) {
                        if (error) {
                            *error = [NSError errorWithDomain:SFBAudioDecoderErrorDomain
                                                         code:SFBAudioDecoderErrorCodeDecodingError
                                                     userInfo:@{NSURLErrorKey : _inputSource.url}];
                        }
                        return false;
                    }
                }
                for (auto i = 0; i < lpc; ++i) {
                    chanBuffer[i - lpc] -= chanOffset;
                }
                for (auto i = 0; i < _blocksize; ++i) {
                    int32_t sum = _lpcQuantOffset;

                    for (auto j = 0; j < lpc; ++j) {
                        sum += _qlpc[j] * chanBuffer[i - j - 1];
                    }
                    int32_t var;
                    if (!_input.getInt32(var, resn)) {
                        if (error) {
                            *error = [NSError errorWithDomain:SFBAudioDecoderErrorDomain
                                                         code:SFBAudioDecoderErrorCodeDecodingError
                                                     userInfo:@{NSURLErrorKey : _inputSource.url}];
                        }
                        return false;
                    }
                    chanBuffer[i] = var + (sum >> parameterQLPC);
                }
                if (chanOffset != 0) {
                    for (auto i = 0; i < _blocksize; ++i) {
                        chanBuffer[i] += chanOffset;
                    }
                }
                break;
            }

            if (_mean > 0) {
                int32_t sum = (_version < 2) ? 0 : _blocksize / 2;

                for (auto i = 0; i < _blocksize; ++i) {
                    sum += chanBuffer[i];
                }

                for (auto i = 1; i < _mean; ++i) {
                    _offset[chan][i - 1] = _offset[chan][i];
                }
                if (_version < 2) {
                    _offset[chan][_mean - 1] = sum / _blocksize;
                } else {
                    _offset[chan][_mean - 1] = (sum / _blocksize) << _bitshift;
                }
            }

            for (auto i = -_wrap; i < 0; i++) {
                chanBuffer[i] = chanBuffer[i + _blocksize];
            }

            if (chan == _channelCount - 1) {
                const auto *abl = _frameBuffer.audioBufferList;

                switch (_fileType) {
                case fileTypeUInt8:
                    for (auto channel = 0; channel < _channelCount; ++channel) {
                        auto *channel_buf = static_cast<uint8_t *>(abl->mBuffers[channel].mData);
                        for (auto sample = 0; sample < _blocksize; ++sample) {
                            const auto value = _buffer[channel][sample] << _bitshift;
                            channel_buf[sample] = static_cast<uint8_t>(std::clamp(value, 0, UINT8_MAX));
                        }
                    }
                    break;
                case fileTypeSInt8:
                    for (auto channel = 0; channel < _channelCount; ++channel) {
                        auto *channel_buf = static_cast<int8_t *>(abl->mBuffers[channel].mData);
                        for (auto sample = 0; sample < _blocksize; ++sample) {
                            const auto value = _buffer[channel][sample] << _bitshift;
                            channel_buf[sample] = static_cast<int8_t>(std::clamp(value, INT8_MIN, INT8_MAX));
                        }
                    }
                    break;
                case fileTypeUInt16BE:
                case fileTypeUInt16LE:
                    for (auto channel = 0; channel < _channelCount; ++channel) {
                        auto *channel_buf = static_cast<uint16_t *>(abl->mBuffers[channel].mData);
                        for (auto sample = 0; sample < _blocksize; ++sample) {
                            const auto value = _buffer[channel][sample] << _bitshift;
                            channel_buf[sample] = static_cast<uint16_t>(std::clamp(value, 0, UINT16_MAX));
                        }
                    }
                    break;
                case fileTypeSInt16BE:
                case fileTypeSInt16LE:
                    for (auto channel = 0; channel < _channelCount; ++channel) {
                        auto *channel_buf = static_cast<int16_t *>(abl->mBuffers[channel].mData);
                        for (auto sample = 0; sample < _blocksize; ++sample) {
                            const auto value = _buffer[channel][sample] << _bitshift;
                            channel_buf[sample] = static_cast<int16_t>(std::clamp(value, INT16_MIN, INT16_MAX));
                        }
                    }
                    break;
                }

                _frameBuffer.frameLength = static_cast<AVAudioFrameCount>(_blocksize);

                ++_blocksDecoded;
                return true;
            }
            chan = (chan + 1) % _channelCount;
            break;
        }

        case functionBlocksize: {
            uint32_t uint = 0;
            if (!_input.getUInt32(uint, _version, static_cast<int>(std::log2(_blocksize))) || uint == 0 ||
                uint > maxBlocksize || uint <= static_cast<uint32_t>(_wrap) ||
                uint > static_cast<uint32_t>(_blocksize)) {
                os_log_error(gSFBAudioDecoderLog, "Invalid or unsupported block size: %u", uint);
                if (error) {
                    *error = [NSError errorWithDomain:SFBAudioDecoderErrorDomain
                                                 code:SFBAudioDecoderErrorCodeDecodingError
                                             userInfo:@{NSURLErrorKey : _inputSource.url}];
                }
                return false;
            }
            _blocksize = static_cast<int>(uint);
            break;
        }
        case functionBitshift:
            if (!_input.getRiceGolombCode(_bitshift, parameterBitshift) || _bitshift > 32) {
                os_log_error(gSFBAudioDecoderLog, "Invalid or unsupported bit shift: %u", _bitshift);
                if (error) {
                    *error = [NSError errorWithDomain:SFBAudioDecoderErrorDomain
                                                 code:SFBAudioDecoderErrorCodeDecodingError
                                             userInfo:@{NSURLErrorKey : _inputSource.url}];
                }
                return false;
            }
            break;
        case functionVerbatim: {
            int32_t chunk_len;
            if (!_input.getRiceGolombCode(chunk_len, parameterVerbatimChunkSize) || chunk_len < 0 ||
                chunk_len > verbatimChunkMaxSizeBytes) {
                os_log_error(gSFBAudioDecoderLog, "Invalid verbatim chunk length: %u", chunk_len);
                if (error) {
                    *error = [NSError errorWithDomain:SFBAudioDecoderErrorDomain
                                                 code:SFBAudioDecoderErrorCodeDecodingError
                                             userInfo:@{NSURLErrorKey : _inputSource.url}];
                }
                return false;
            }
            while (chunk_len--) {
                int32_t dummy;
                if (!_input.getRiceGolombCode(dummy, parameterVerbatimByte)) {
                    if (error) {
                        *error = [NSError errorWithDomain:SFBAudioDecoderErrorDomain
                                                     code:SFBAudioDecoderErrorCodeDecodingError
                                                 userInfo:@{NSURLErrorKey : _inputSource.url}];
                    }
                    return false;
                }
            }
            break;
        }

        default:
            os_log_error(gSFBAudioDecoderLog, "Sanity check failed for function: %d", cmd);
            if (error) {
                *error = [NSError errorWithDomain:SFBAudioDecoderErrorDomain
                                             code:SFBAudioDecoderErrorCodeDecodingError
                                         userInfo:@{NSURLErrorKey : _inputSource.url}];
            }
            return false;
        }
    }

    return true;
}

// A return value of true indicates that decoding may continue, not that no errors exist with the seek table itself
- (bool)scanForSeekTableReturningError:(NSError **)error {
    // Non-seekable input source; not an error
    if (!_inputSource.supportsSeeking) {
        return true;
    }

    NSInteger startOffset;
    if (![_inputSource getOffset:&startOffset error:error]) {
        return false;
    }

    NSInteger fileLength;
    if (![_inputSource getLength:&fileLength error:error] ||
        ![_inputSource seekToOffset:(fileLength - seekTrailerSizeBytes) error:error]) {
        return false;
    }

    SeekTableTrailer trailer;
    {
        unsigned char buf[seekTrailerSizeBytes];
        NSInteger     bytesRead;
        if (![_inputSource readBytes:buf length:seekTrailerSizeBytes bytesRead:&bytesRead error:error]) {
            return false;
        }
        if (bytesRead != seekTrailerSizeBytes) {
            if (error) {
                *error = [NSError errorWithDomain:SFBAudioDecoderErrorDomain
                                             code:SFBAudioDecoderErrorCodeInvalidFormat
                                         userInfo:nil];
            }
            return false;
        }
        trailer = parseSeekTableTrailer(buf);
    }

    // No appended seek table found; not an error
    if (memcmp("SHNAMPSK", trailer.signature_, 8)) {
        // Check for separate seek table
        NSURL *externalSeekTableURL = [_inputSource.url.URLByDeletingPathExtension URLByAppendingPathExtension:@"skt"];
        if ([externalSeekTableURL checkResourceIsReachableAndReturnError:nil]) {
            auto entries = [self parseExternalSeekTable:externalSeekTableURL];
            if (!entries.empty() && [self seekTableIsValid:entries startOffset:startOffset]) {
                _seekTableEntries = entries;
            }
        }
        if (![_inputSource seekToOffset:startOffset error:error]) {
            return false;
        }
        return true;
    }

    if (![_inputSource seekToOffset:(fileLength - trailer.seekTableSize_) error:error]) {
        return false;
    }

    SeekTableHeader header;
    {
        unsigned char buf[seekHeaderSizeBytes];
        NSInteger     bytesRead;
        if (![_inputSource readBytes:buf length:seekHeaderSizeBytes bytesRead:&bytesRead error:error]) {
            return false;
        }
        if (bytesRead != seekHeaderSizeBytes) {
            if (error) {
                *error = [NSError errorWithDomain:SFBAudioDecoderErrorDomain
                                             code:SFBAudioDecoderErrorCodeInvalidFormat
                                         userInfo:nil];
            }
            return false;
        }
        header = parseSeekTableHeader(buf);
    }

    // A corrupt seek table is an error, however YES is returned to try and permit decoding to continue
    if (memcmp("SEEK", header.signature_, 4)) {
        os_log_error(gSFBAudioDecoderLog, "Unexpected seek table header signature: %{public}.4s", header.signature_);
        if (![_inputSource seekToOffset:startOffset error:error]) {
            return false;
        }
        return true;
    }

    // Validate seek table version
    if (header.version_ != seekTableRevision) {
        os_log_error(gSFBAudioDecoderLog, "Unsupported seek table header version: %d", header.version_);
        if (![_inputSource seekToOffset:startOffset error:error]) {
            return false;
        }
        return true;
    }

    std::vector<SeekTableEntry> entries;

    auto count = (trailer.seekTableSize_ - seekTrailerSizeBytes - seekHeaderSizeBytes) / seekEntrySizeBytes;
    for (uint32_t i = 0; i < count; ++i) {
        unsigned char buf[seekEntrySizeBytes];
        NSInteger     bytesRead;
        if (![_inputSource readBytes:buf length:seekEntrySizeBytes bytesRead:&bytesRead error:error]) {
            return false;
        }
        if (bytesRead != seekEntrySizeBytes) {
            if (error) {
                *error = [NSError errorWithDomain:SFBAudioDecoderErrorDomain
                                             code:SFBAudioDecoderErrorCodeInvalidFormat
                                         userInfo:nil];
            }
            return false;
        }

        auto entry = parseSeekTableEntry(buf);
        entries.push_back(entry);
    }

    // Reset file marker
    if (![_inputSource seekToOffset:startOffset error:error]) {
        return false;
    }

    if (!entries.empty() && [self seekTableIsValid:entries startOffset:startOffset]) {
        _seekTableEntries = entries;
    }

    return true;
}

- (std::vector<SeekTableEntry>)parseExternalSeekTable:(NSURL *)url {
    NSParameterAssert(url != nil);

    NSError        *error;
    SFBInputSource *inputSource = [SFBInputSource inputSourceForURL:url flags:0 error:&error];
    if (!inputSource || ![inputSource openReturningError:&error]) {
        os_log_error(gSFBAudioDecoderLog, "Error opening external seek table: %{public}@", error);
        return {};
    }

    {
        unsigned char buf[seekHeaderSizeBytes];
        NSInteger     bytesRead;
        if (![inputSource readBytes:buf length:seekHeaderSizeBytes bytesRead:&bytesRead error:&error] ||
            bytesRead != seekHeaderSizeBytes) {
            os_log_error(gSFBAudioDecoderLog, "Error reading external seek table header: %{public}@", error);
            return {};
        }

        auto header = parseSeekTableHeader(buf);
        if (memcmp("SEEK", header.signature_, 4)) {
            os_log_error(gSFBAudioDecoderLog, "Unexpected seek table header signature: %{public}.4s",
                         header.signature_);
            return {};
        }
    }

    std::vector<SeekTableEntry> entries;

    for (;;) {
        unsigned char buf[seekEntrySizeBytes];
        NSInteger     bytesRead;
        if (![inputSource readBytes:buf length:seekEntrySizeBytes bytesRead:&bytesRead error:&error] ||
            bytesRead != seekEntrySizeBytes) {
            os_log_error(gSFBAudioDecoderLog, "Error reading external seek table entry: %{public}@", error);
            return {};
        }

        auto entry = parseSeekTableEntry(buf);
        entries.push_back(entry);

        if (inputSource.atEOF) {
            break;
        }
    }

    return entries;
}

- (bool)seekTableIsValid:(std::vector<SeekTableEntry>)entries startOffset:(NSInteger)startOffset {
    if (entries.empty()) {
        return false;
    }
    if (startOffset != entries[0].byteOffsetInFile_) {
        os_log_error(
              gSFBAudioDecoderLog,
              "Seek table error: Mismatch between actual data start (%ld) and start in first seek table entry (%d)",
              (long)startOffset, entries[0].byteOffsetInFile_);
        return false;
    }
    if (_bitshift != entries[0].bitshift_) {
        os_log_error(gSFBAudioDecoderLog, "Seek table error: Invalid bitshift (%d) in first seek table entry",
                     entries[0].bitshift_);
        return false;
    }
    if (_channelCount != 1 && _channelCount != 2) {
        os_log_error(gSFBAudioDecoderLog, "Seek table error: Invalid channel count (%d); mono or stereo required",
                     _channelCount);
        return false;
    }
    if (_maxLPC > 3) {
        os_log_error(gSFBAudioDecoderLog, "Seek table error: Invalid maxnlpc (%d); [0, 3] required", _maxLPC);
        return false;
    }
    if (_mean > 4) {
        os_log_error(gSFBAudioDecoderLog, "Seek table error: Invalid mean (%d); [0, 4] required", _mean);
        return false;
    }

    return true;
}

@end
