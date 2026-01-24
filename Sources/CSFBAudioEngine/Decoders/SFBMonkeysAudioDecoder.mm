//
// Copyright (c) 2011-2026 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import <os/log.h>

#import <algorithm>
#import <memory>

#define PLATFORM_APPLE

#import <MAC/All.h>
#import <MAC/IAPEIO.h>
#import <MAC/MACLib.h>

#undef PLATFORM_APPLE

#import "NSData+SFBExtensions.h"
#import "SFBErrorWithLocalizedDescription.h"
#import "SFBLocalizedNameForURL.h"
#import "SFBMonkeysAudioDecoder.h"

SFBAudioDecoderName const SFBAudioDecoderNameMonkeysAudio = @"org.sbooth.AudioEngine.Decoder.MonkeysAudio";

SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMonkeysAudioFileVersion = @"APE_INFO_FILE_VERSION";
SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMonkeysAudioCompressionLevel =
      @"APE_INFO_COMPRESSION_LEVEL";
SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMonkeysAudioFormatFlags = @"APE_INFO_FORMAT_FLAGS";
SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMonkeysAudioSampleRate = @"APE_INFO_SAMPLE_RATE";
SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMonkeysAudioBitsPerSample =
      @"APE_INFO_BITS_PER_SAMPLE";
SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMonkeysAudioBytesPerSample =
      @"APE_INFO_BYTES_PER_SAMPLE";
SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMonkeysAudioChannels = @"APE_INFO_CHANNELS";
SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMonkeysAudioBlockAlignment = @"APE_INFO_BLOCK_ALIGN";
SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMonkeysAudioBlocksPerFrame =
      @"APE_INFO_BLOCKS_PER_FRAME";
SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMonkeysAudioFinalFrameBlocks =
      @"APE_INFO_FINAL_FRAME_BLOCKS";
SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMonkeysAudioTotalFrames = @"APE_INFO_TOTAL_FRAMES";
SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMonkeysAudioWAVHeaderBytes =
      @"APE_INFO_WAV_HEADER_BYTES";
SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMonkeysAudioWAVTerminatingBytes =
      @"APE_INFO_WAV_TERMINATING_BYTES";
SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMonkeysAudioWAVDataBytes = @"APE_INFO_WAV_DATA_BYTES";
SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMonkeysAudioWAVTotalBytes =
      @"APE_INFO_WAV_TOTAL_BYTES";
SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMonkeysAudioAPETotalBytes =
      @"APE_INFO_APE_TOTAL_BYTES";
// SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMonkeysAudioTotalBlocks = @"APE_INFO_TOTAL_BLOCKS";
// SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMonkeysAudioLengthMilliseconds =
// @"APE_INFO_LENGTH_MS"; SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMonkeysAudioAverageBitrate =
// @"APE_INFO_AVERAGE_BITRATE";
SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMonkeysAudioDecompressedBitrate =
      @"APE_INFO_DECOMPRESSED_BITRATE";
SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMonkeysAudioAPL = @"APE_INFO_APL";

SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMonkeysAudioTotalBlocks =
      @"APE_DECOMPRESS_TOTAL_BLOCKS";
SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMonkeysAudioLengthMilliseconds =
      @"APE_DECOMPRESS_LENGTH_MS";
SFBAudioDecodingPropertiesKey const SFBAudioDecodingPropertiesKeyMonkeysAudioAverageBitrate =
      @"APE_DECOMPRESS_AVERAGE_BITRATE";

namespace {

// The I/O interface for MAC
class APEIOInterface final : public APE::IAPEIO {
  public:
    explicit APEIOInterface(SFBInputSource *inputSource)
      : inputSource_(inputSource) {}

    int Open(const wchar_t *pName, bool bOpenReadOnly) override {
#pragma unused(pName)
#pragma unused(bOpenReadOnly)

        return ERROR_INVALID_INPUT_FILE;
    }

    int Close() override {
        return ERROR_SUCCESS;
    }

    int Read(void *pBuffer, unsigned int nBytesToRead, unsigned int *pBytesRead) override {
        NSInteger bytesRead;
        if ([inputSource_ readBytes:pBuffer length:nBytesToRead bytesRead:&bytesRead error:nil] == NO) {
            return ERROR_IO_READ;
        }

        *pBytesRead = static_cast<unsigned int>(bytesRead);

        return ERROR_SUCCESS;
    }

    int Write(const void *pBuffer, unsigned int nBytesToWrite, unsigned int *pBytesWritten) override {
#pragma unused(pBuffer)
#pragma unused(nBytesToWrite)
#pragma unused(pBytesWritten)

        return ERROR_IO_WRITE;
    }

    int Seek(APE::int64 nPosition, APE::SeekMethod nMethod) override {
        if (inputSource_.supportsSeeking == NO) {
            return ERROR_IO_READ;
        }

        NSInteger offset = nPosition;
        switch (nMethod) {
        case APE::SeekFileBegin:
            // offset remains unchanged
            break;
        case APE::SeekFileCurrent: {
            NSInteger inputSourceOffset;
            if ([inputSource_ getOffset:&inputSourceOffset error:nil] != NO) {
                offset += inputSourceOffset;
            }
            break;
        }
        case APE::SeekFileEnd: {
            NSInteger inputSourceLength;
            if ([inputSource_ getLength:&inputSourceLength error:nil] != NO) {
                offset += inputSourceLength;
            }
            break;
        }
        }

        if ([inputSource_ seekToOffset:offset error:nil] == NO) {
            return ERROR_IO_READ;
        }

        return ERROR_SUCCESS;
    }

    int Create(const wchar_t *pName) override {
#pragma unused(pName)
        return ERROR_IO_WRITE;
    }

    int Delete() override {
        return ERROR_IO_WRITE;
    }

    int SetEOF() override {
        return ERROR_IO_WRITE;
    }

    unsigned char *GetBuffer(int *pnBufferBytes) override {
#pragma unused(pnBufferBytes)
        return nullptr;
    }

    APE::int64 GetPosition() override {
        NSInteger offset;
        if ([inputSource_ getOffset:&offset error:nil] == NO) {
            return -1;
        }
        return offset;
    }

    APE::int64 GetSize() override {
        NSInteger length;
        if ([inputSource_ getLength:&length error:nil] == NO) {
            return -1;
        }
        return length;
    }

    int GetName(wchar_t *pBuffer) override {
#pragma unused(pBuffer)
        return ERROR_SUCCESS;
    }

  private:
    SFBInputSource *inputSource_;
};

} /* namespace */

@interface SFBMonkeysAudioDecoder () {
  @private
    std::unique_ptr<APEIOInterface> _ioInterface;
    std::unique_ptr<APE::IAPEDecompress> _decompressor;
}
@end

@implementation SFBMonkeysAudioDecoder

+ (void)load {
    [SFBAudioDecoder registerSubclass:[self class]];
}

+ (NSSet *)supportedPathExtensions {
    return [NSSet setWithObject:@"ape"];
}

+ (NSSet *)supportedMIMETypes {
    return [NSSet setWithArray:@[ @"audio/monkeys-audio", @"audio/x-monkeys-audio" ]];
}

+ (SFBAudioDecoderName)decoderName {
    return SFBAudioDecoderNameMonkeysAudio;
}

+ (BOOL)testInputSource:(SFBInputSource *)inputSource
      formatIsSupported:(SFBTernaryTruthValue *)formatIsSupported
                  error:(NSError **)error {
    NSParameterAssert(inputSource != nil);
    NSParameterAssert(formatIsSupported != nullptr);

    NSData *header = [inputSource readHeaderOfLength:SFBAPEDetectionSize skipID3v2Tag:YES error:error];
    if (header == nullptr) {
        return NO;
    }

    if ([header isAPEHeader] != NO) {
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
    if ([super openReturningError:error] == NO) {
        return NO;
    }

    auto ioInterface = std::make_unique<APEIOInterface>(_inputSource);
    auto decompressor = std::unique_ptr<APE::IAPEDecompress>(CreateIAPEDecompressEx(ioInterface.get(), nullptr));
    if (!decompressor) {
        if (error != nullptr) {
            *error = SFBErrorWithLocalizedDescription(
                  SFBAudioDecoderErrorDomain, SFBAudioDecoderErrorCodeInvalidFormat,
                  NSLocalizedString(@"The file “%@” is not a valid Monkey's Audio file.", @""), @{
                      NSLocalizedRecoverySuggestionErrorKey :
                            NSLocalizedString(@"The file's extension may not match the file's type.", @""),
                      NSURLErrorKey : _inputSource.url
                  },
                  SFBLocalizedNameForURL(_inputSource.url));
        }
        return NO;
    }

    _decompressor = std::move(decompressor);
    _ioInterface = std::move(ioInterface);

    AVAudioChannelLayout *channelLayout = nil;
    switch (_decompressor->GetInfo(APE::IAPEDecompress::APE_INFO_CHANNELS)) {
    case 1:
        channelLayout = [AVAudioChannelLayout layoutWithLayoutTag:kAudioChannelLayoutTag_Mono];
        break;
    case 2:
        channelLayout = [AVAudioChannelLayout layoutWithLayoutTag:kAudioChannelLayoutTag_Stereo];
        break;
        // FIXME: Is there a standard ordering for multichannel files? WAVEFORMATEX?
    default:
        channelLayout = [AVAudioChannelLayout
              layoutWithLayoutTag:(kAudioChannelLayoutTag_Unknown | static_cast<UInt32>(_decompressor->GetInfo(
                                                                          APE::IAPEDecompress::APE_INFO_CHANNELS)))];
        break;
    }

    // The file format
    AudioStreamBasicDescription processingStreamDescription{};

    processingStreamDescription.mFormatID = kAudioFormatLinearPCM;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-anon-enum-enum-conversion"
    processingStreamDescription.mFormatFlags =
          kAudioFormatFlagIsSignedInteger | kAudioFormatFlagsNativeEndian | kAudioFormatFlagIsPacked;
#pragma clang diagnostic pop

    processingStreamDescription.mBitsPerChannel =
          static_cast<UInt32>(_decompressor->GetInfo(APE::IAPEDecompress::APE_INFO_BITS_PER_SAMPLE));
    processingStreamDescription.mSampleRate = _decompressor->GetInfo(APE::IAPEDecompress::APE_INFO_SAMPLE_RATE);
    processingStreamDescription.mChannelsPerFrame =
          static_cast<UInt32>(_decompressor->GetInfo(APE::IAPEDecompress::APE_INFO_CHANNELS));

    processingStreamDescription.mBytesPerPacket =
          (processingStreamDescription.mBitsPerChannel / 8) * processingStreamDescription.mChannelsPerFrame;
    processingStreamDescription.mFramesPerPacket = 1;
    processingStreamDescription.mBytesPerFrame =
          processingStreamDescription.mBytesPerPacket / processingStreamDescription.mFramesPerPacket;

    processingStreamDescription.mReserved = 0;

    _processingFormat = [[AVAudioFormat alloc] initWithStreamDescription:&processingStreamDescription
                                                           channelLayout:channelLayout];

    // Set up the source format
    AudioStreamBasicDescription sourceStreamDescription{};

    sourceStreamDescription.mFormatID = kSFBAudioFormatMonkeysAudio;

    sourceStreamDescription.mBitsPerChannel =
          static_cast<UInt32>(_decompressor->GetInfo(APE::IAPEDecompress::APE_INFO_BITS_PER_SAMPLE));
    sourceStreamDescription.mSampleRate = _decompressor->GetInfo(APE::IAPEDecompress::APE_INFO_SAMPLE_RATE);
    sourceStreamDescription.mChannelsPerFrame =
          static_cast<UInt32>(_decompressor->GetInfo(APE::IAPEDecompress::APE_INFO_CHANNELS));

    _sourceFormat = [[AVAudioFormat alloc] initWithStreamDescription:&sourceStreamDescription
                                                       channelLayout:channelLayout];

    // Populate codec properties
    _properties = @{
        SFBAudioDecodingPropertiesKeyMonkeysAudioFileVersion :
              @(_decompressor->GetInfo(APE::IAPEDecompress::APE_INFO_FILE_VERSION)),
        SFBAudioDecodingPropertiesKeyMonkeysAudioCompressionLevel :
              @(_decompressor->GetInfo(APE::IAPEDecompress::APE_INFO_COMPRESSION_LEVEL)),
        SFBAudioDecodingPropertiesKeyMonkeysAudioFormatFlags :
              @(_decompressor->GetInfo(APE::IAPEDecompress::APE_INFO_FORMAT_FLAGS)),
        SFBAudioDecodingPropertiesKeyMonkeysAudioSampleRate :
              @(_decompressor->GetInfo(APE::IAPEDecompress::APE_INFO_SAMPLE_RATE)),
        SFBAudioDecodingPropertiesKeyMonkeysAudioBitsPerSample :
              @(_decompressor->GetInfo(APE::IAPEDecompress::APE_INFO_BITS_PER_SAMPLE)),
        SFBAudioDecodingPropertiesKeyMonkeysAudioBytesPerSample :
              @(_decompressor->GetInfo(APE::IAPEDecompress::APE_INFO_BYTES_PER_SAMPLE)),
        SFBAudioDecodingPropertiesKeyMonkeysAudioChannels :
              @(_decompressor->GetInfo(APE::IAPEDecompress::APE_INFO_CHANNELS)),
        SFBAudioDecodingPropertiesKeyMonkeysAudioBlockAlignment :
              @(_decompressor->GetInfo(APE::IAPEDecompress::APE_INFO_BLOCK_ALIGN)),
        SFBAudioDecodingPropertiesKeyMonkeysAudioBlocksPerFrame :
              @(_decompressor->GetInfo(APE::IAPEDecompress::APE_INFO_BLOCKS_PER_FRAME)),
        SFBAudioDecodingPropertiesKeyMonkeysAudioFinalFrameBlocks :
              @(_decompressor->GetInfo(APE::IAPEDecompress::APE_INFO_FINAL_FRAME_BLOCKS)),
        SFBAudioDecodingPropertiesKeyMonkeysAudioTotalFrames :
              @(_decompressor->GetInfo(APE::IAPEDecompress::APE_INFO_TOTAL_FRAMES)),
        SFBAudioDecodingPropertiesKeyMonkeysAudioWAVHeaderBytes :
              @(_decompressor->GetInfo(APE::IAPEDecompress::APE_INFO_WAV_HEADER_BYTES)),
        SFBAudioDecodingPropertiesKeyMonkeysAudioWAVTerminatingBytes :
              @(_decompressor->GetInfo(APE::IAPEDecompress::APE_INFO_WAV_TERMINATING_BYTES)),
        SFBAudioDecodingPropertiesKeyMonkeysAudioWAVDataBytes :
              @(_decompressor->GetInfo(APE::IAPEDecompress::APE_INFO_WAV_DATA_BYTES)),
        SFBAudioDecodingPropertiesKeyMonkeysAudioWAVTotalBytes :
              @(_decompressor->GetInfo(APE::IAPEDecompress::APE_INFO_WAV_TOTAL_BYTES)),
        SFBAudioDecodingPropertiesKeyMonkeysAudioAPETotalBytes :
              @(_decompressor->GetInfo(APE::IAPEDecompress::APE_INFO_APE_TOTAL_BYTES)),
        //        SFBAudioDecodingPropertiesKeyMonkeysAudioTotalBlocks:
        //            @(_decompressor->GetInfo(APE::IAPEDecompress::APE_INFO_TOTAL_BLOCKS)),
        //        SFBAudioDecodingPropertiesKeyMonkeysAudioLengthMilliseconds:
        //            @(_decompressor->GetInfo(APE::IAPEDecompress::APE_INFO_LENGTH_MS)),
        //        SFBAudioDecodingPropertiesKeyMonkeysAudioAverageBitrate:
        //            @(_decompressor->GetInfo(APE::IAPEDecompress::APE_INFO_AVERAGE_BITRATE)),
        // APE_INFO_FRAME_BITRATE
        SFBAudioDecodingPropertiesKeyMonkeysAudioDecompressedBitrate :
              @(_decompressor->GetInfo(APE::IAPEDecompress::APE_INFO_DECOMPRESSED_BITRATE)),
        // APE_INFO_PEAK_LEVEL
        // APE_INFO_SEEK_BIT
        // APE_INFO_SEEK_BYTE
        // APE_INFO_WAV_HEADER_DATA
        // APE_INFO_WAV_TERMINATING_DATA
        // APE_INFO_WAVEFORMATEX
        // APE_INFO_IO_SOURCE
        // APE_INFO_FRAME_BYTES
        // APE_INFO_FRAME_BLOCKS
        // APE_INFO_TAG
        SFBAudioDecodingPropertiesKeyMonkeysAudioAPL :
                    (_decompressor->GetInfo(APE::IAPEDecompress::APE_INFO_APL) != 0) ? @YES : @NO,

        // APE_DECOMPRESS_CURRENT_BLOCK
        // APE_DECOMPRESS_CURRENT_MS
        SFBAudioDecodingPropertiesKeyMonkeysAudioTotalBlocks :
              @(_decompressor->GetInfo(APE::IAPEDecompress::APE_DECOMPRESS_TOTAL_BLOCKS)),
        SFBAudioDecodingPropertiesKeyMonkeysAudioLengthMilliseconds :
              @(_decompressor->GetInfo(APE::IAPEDecompress::APE_DECOMPRESS_LENGTH_MS)),
        // APE_DECOMPRESS_CURRENT_BITRATE
        SFBAudioDecodingPropertiesKeyMonkeysAudioAverageBitrate :
              @(_decompressor->GetInfo(APE::IAPEDecompress::APE_DECOMPRESS_AVERAGE_BITRATE)),
        // APE_DECOMPRESS_CURRENT_FRAME
    };

    return YES;
}

- (BOOL)closeReturningError:(NSError **)error {
    _ioInterface.reset();
    _decompressor.reset();

    return [super closeReturningError:error];
}

- (BOOL)isOpen {
    return static_cast<BOOL>(_decompressor != nullptr);
}

- (AVAudioFramePosition)framePosition {
    return _decompressor->GetInfo(APE::IAPEDecompress::APE_DECOMPRESS_CURRENT_BLOCK);
}

- (AVAudioFramePosition)frameLength {
    return _decompressor->GetInfo(APE::IAPEDecompress::APE_DECOMPRESS_TOTAL_BLOCKS);
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

    int64_t blocksRead = 0;
    if (_decompressor->GetData(static_cast<unsigned char *>(buffer.audioBufferList->mBuffers[0].mData),
                               static_cast<int64_t>(frameLength), &blocksRead) != 0) {
        os_log_error(gSFBAudioDecoderLog, "Monkey's Audio invalid checksum");
        if (error != nullptr) {
            *error = [NSError errorWithDomain:SFBAudioDecoderErrorDomain
                                         code:SFBAudioDecoderErrorCodeDecodingError
                                     userInfo:@{NSURLErrorKey : _inputSource.url}];
        }
        return NO;
    }

    buffer.frameLength = static_cast<AVAudioFrameCount>(blocksRead);

    return YES;
}

- (BOOL)seekToFrame:(AVAudioFramePosition)frame error:(NSError **)error {
    NSParameterAssert(frame >= 0);
    if (const auto result = _decompressor->Seek(frame); result != ERROR_SUCCESS) {
        os_log_error(gSFBAudioDecoderLog, "Monkey's Audio seek error: %d", result);
        if (error != nullptr) {
            *error = [NSError errorWithDomain:SFBAudioDecoderErrorDomain
                                         code:SFBAudioDecoderErrorCodeSeekError
                                     userInfo:@{NSURLErrorKey : _inputSource.url}];
        }
        return NO;
    }
    return YES;
}

@end
