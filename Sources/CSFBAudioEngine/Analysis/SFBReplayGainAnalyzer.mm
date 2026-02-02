//
// SPDX-FileCopyrightText: 2011 Stephen F. Booth <contact@sbooth.dev>
// SPDX-License-Identifier: MIT
//
// Part of https://github.com/sbooth/SFBAudioEngine
//

#import "SFBReplayGainAnalyzer.h"

#import "SFBAudioDecoder.h"
#import "SFBErrorWithLocalizedDescription.h"
#import "SFBLocalizedNameForURL.h"
#import "loudness_ebur128/ebur128_analyzer.h"

#import <os/log.h>

#import <memory>

// NSError domain for SFBReplayGainAnalyzer
NSErrorDomain const SFBReplayGainAnalyzerErrorDomain = @"org.sbooth.AudioEngine.ReplayGainAnalyzer";

// Key names for the metadata dictionary
NSString *const SFBReplayGainAnalyzerGainKey = @"Gain";
NSString *const SFBReplayGainAnalyzerPeakKey = @"Peak";

#define BUFFER_SIZE_FRAMES 2048

@interface SFBReplayGainAnalyzer () {
  @private
    std::unique_ptr<loudness::EbuR128Analyzer> _albumAnalyzer;
    AVAudioFormat *_albumFormat;
}
@end

@implementation SFBReplayGainAnalyzer

+ (void)load {
    [NSError setUserInfoValueProviderForDomain:SFBReplayGainAnalyzerErrorDomain
                                      provider:^id(NSError *err, NSErrorUserInfoKey userInfoKey) {
                                          switch (err.code) {
                                          case SFBReplayGainAnalyzerErrorCodeFileFormatNotSupported:
                                              if ([userInfoKey isEqualToString:NSLocalizedDescriptionKey]) {
                                                  return NSLocalizedString(@"The file's format is not supported.", @"");
                                              }
                                              break;

                                          case SFBReplayGainAnalyzerErrorCodeInsufficientSamples:
                                              if ([userInfoKey isEqualToString:NSLocalizedDescriptionKey]) {
                                                  return NSLocalizedString(@"The file does not contain sufficient "
                                                                           @"audio samples for analysis.",
                                                                           @"");
                                              }
                                              break;

                                          case SFBReplayGainAnalyzerErrorCodeAlbumReplayGainDisabled:
                                              if ([userInfoKey isEqualToString:NSLocalizedDescriptionKey]) {
                                                  return NSLocalizedString(
                                                          @"Album replay gain analysis was not enabled.", @"");
                                              }
                                              break;
                                          }

                                          return nil;
                                      }];
}

+ (NSDictionary<SFBReplayGainAnalyzerKey, NSNumber *> *)analyzeTrack:(NSURL *)url error:(NSError **)error {
    SFBReplayGainAnalyzer *analyzer = [[SFBReplayGainAnalyzer alloc] init];
    analyzer.calculateAlbumReplayGain = NO;
    return [analyzer analyzeTrack:url error:error];
}

+ (NSDictionary *)analyzeAlbum:(NSArray<NSURL *> *)urls error:(NSError **)error {
    NSMutableDictionary *result = [NSMutableDictionary dictionary];

    SFBReplayGainAnalyzer *analyzer = [[SFBReplayGainAnalyzer alloc] init];
    analyzer.calculateAlbumReplayGain = YES;

    for (NSURL *url in urls) {
        NSDictionary *replayGain = [analyzer analyzeTrack:url error:error];
        if (!replayGain) {
            return nil;
        }
        result[url] = replayGain;
    }

    NSDictionary *albumGainAndPeakSample = [analyzer albumGainAndPeakSampleReturningError:error];
    if (!albumGainAndPeakSample) {
        return nil;
    }

    [result addEntriesFromDictionary:albumGainAndPeakSample];
    return [result copy];
}

- (NSDictionary *)analyzeTrack:(NSURL *)url error:(NSError **)error {
    NSParameterAssert(url != nil);

    SFBAudioDecoder *decoder = [[SFBAudioDecoder alloc] initWithURL:url error:error];
    if (!decoder || ![decoder openReturningError:error]) {
        return nil;
    }

    AVAudioFormat *inputFormat = decoder.processingFormat;
    AVAudioFormat *outputFormat = [[AVAudioFormat alloc] initWithCommonFormat:AVAudioPCMFormatFloat32
                                                                   sampleRate:inputFormat.sampleRate
                                                                     channels:inputFormat.channelCount
                                                                  interleaved:NO];

    if (/*_calculateAlbumReplayGain &&*/ _albumFormat && ![_albumFormat isEqual:outputFormat]) {
        if (error) {
            *error = SFBErrorWithLocalizedDescription(
                    SFBReplayGainAnalyzerErrorDomain, SFBReplayGainAnalyzerErrorCodeFileFormatNotSupported,
                    NSLocalizedString(@"The format of the file “%@” is not supported.", @""), @{
                        NSLocalizedRecoverySuggestionErrorKey :
                                NSLocalizedString(@"The file's format is not supported for replay gain analysis.", @"")
                    },
                    SFBLocalizedNameForURL(url));
        }
        return nil;
    }

    AVAudioConverter *converter = [[AVAudioConverter alloc] initFromFormat:decoder.processingFormat
                                                                  toFormat:outputFormat];
    if (!converter) {
        if (error) {
            *error = SFBErrorWithLocalizedDescription(
                    SFBReplayGainAnalyzerErrorDomain, SFBReplayGainAnalyzerErrorCodeFileFormatNotSupported,
                    NSLocalizedString(@"The format of the file “%@” is not supported.", @""), @{
                        NSLocalizedRecoverySuggestionErrorKey :
                                NSLocalizedString(@"The file's format is not supported for replay gain analysis.", @"")
                    },
                    SFBLocalizedNameForURL(url));
        }
        return nil;
    }

    AVAudioPCMBuffer *outputBuffer = [[AVAudioPCMBuffer alloc] initWithPCMFormat:converter.outputFormat
                                                                   frameCapacity:BUFFER_SIZE_FRAMES];
    AVAudioPCMBuffer *decodeBuffer = [[AVAudioPCMBuffer alloc] initWithPCMFormat:converter.inputFormat
                                                                   frameCapacity:BUFFER_SIZE_FRAMES];

    try {
        auto channelWeights = loudness::DefaultChannelWeights();

        if (_calculateAlbumReplayGain && !_albumAnalyzer) {
            _albumAnalyzer = std::make_unique<loudness::EbuR128Analyzer>(outputFormat.channelCount, channelWeights,
                                                                         outputFormat.sampleRate, false);
            _albumFormat = outputFormat;
        }

        loudness::EbuR128Analyzer trackAnalyzer(outputFormat.channelCount, channelWeights, outputFormat.sampleRate,
                                                false);

        for (;;) {
            __block NSError *err = nil;
            AVAudioConverterOutputStatus status = [converter
                       convertToBuffer:outputBuffer
                                 error:error
                    withInputFromBlock:^AVAudioBuffer *_Nullable(AVAudioPacketCount inNumberOfPackets,
                                                                 AVAudioConverterInputStatus *_Nonnull outStatus) {
                        BOOL result = [decoder decodeIntoBuffer:decodeBuffer frameLength:inNumberOfPackets error:&err];
                        if (!result) {
                            os_log_error(OS_LOG_DEFAULT, "Error decoding audio: %{public}@", err);
                        }

                        if (result && decodeBuffer.frameLength == 0) {
                            *outStatus = AVAudioConverterInputStatus_EndOfStream;
                        } else {
                            *outStatus = AVAudioConverterInputStatus_HaveData;
                        }

                        return decodeBuffer;
                    }];

            if (status == AVAudioConverterOutputStatus_Error) {
                if (error) {
                    *error = err;
                }
                return nil;
            }
            if (status == AVAudioConverterOutputStatus_EndOfStream) {
                break;
            }

            AVAudioFrameCount frameCount = outputBuffer.frameLength;

            trackAnalyzer.Process(outputBuffer.floatChannelData[0], frameCount,
                                  loudness::EbuR128Analyzer::SampleFormat::FLOAT,
                                  loudness::EbuR128Analyzer::SampleLayout::PLANAR_CONTIGUOUS);
            if (_calculateAlbumReplayGain) {
                _albumAnalyzer->Process(outputBuffer.floatChannelData[0], frameCount,
                                        loudness::EbuR128Analyzer::SampleFormat::FLOAT,
                                        loudness::EbuR128Analyzer::SampleLayout::PLANAR_CONTIGUOUS);
            }
        }

        auto loudness = trackAnalyzer.GetRelativeGatedIntegratedLoudness();
        auto digitalPeak = trackAnalyzer.digital_peak();

        if (!loudness.has_value()) {
            if (error) {
                *error = SFBErrorWithLocalizedDescription(
                        SFBReplayGainAnalyzerErrorDomain, SFBReplayGainAnalyzerErrorCodeInsufficientSamples,
                        NSLocalizedString(@"The file “%@” does not contain sufficient audio for analysis.", @""), @{
                            NSLocalizedRecoverySuggestionErrorKey :
                                    NSLocalizedString(@"The audio duration is too short for replay gain analysis.", @"")
                        },
                        SFBLocalizedNameForURL(url));
            }
            return nil;
        }

        const auto gain = -18.f - loudness.value();

        return @{SFBReplayGainAnalyzerGainKey : @(gain), SFBReplayGainAnalyzerPeakKey : @(digitalPeak)};
    } catch (const std::exception &e) {
        if (error) {
            *error = SFBErrorWithLocalizedDescription(
                    SFBReplayGainAnalyzerErrorDomain, SFBReplayGainAnalyzerErrorCodeFileFormatNotSupported,
                    NSLocalizedString(@"The format of the file “%@” is not supported.", @""), @{
                        NSLocalizedRecoverySuggestionErrorKey :
                                NSLocalizedString(@"The file's format is not supported for replay gain analysis.", @"")
                    },
                    SFBLocalizedNameForURL(url));
        }
        return nil;
    }
}

- (NSDictionary *)albumGainAndPeakSampleReturningError:(NSError **)error {
    if (!_calculateAlbumReplayGain) {
        if (error) {
            *error = [NSError errorWithDomain:SFBReplayGainAnalyzerErrorDomain
                                         code:SFBReplayGainAnalyzerErrorCodeAlbumReplayGainDisabled
                                     userInfo:nil];
        }
        return nil;
    }

    std::optional<float> loudness;
    float digitalPeak = 0;

    if (_albumAnalyzer) {
        loudness = _albumAnalyzer->GetRelativeGatedIntegratedLoudness();
        digitalPeak = _albumAnalyzer->digital_peak();
    }

    if (!loudness.has_value()) {
        if (error) {
            *error = [NSError errorWithDomain:SFBReplayGainAnalyzerErrorDomain
                                         code:SFBReplayGainAnalyzerErrorCodeInsufficientSamples
                                     userInfo:@{
                                         NSLocalizedDescriptionKey : NSLocalizedString(
                                                 @"The files do not contain sufficient audio for analysis.", @""),
                                         NSLocalizedRecoverySuggestionErrorKey : NSLocalizedString(
                                                 @"The audio duration is too short for replay gain analysis.", @"")
                                     }];
        }
        return nil;
    }

    const auto gain = -18.f - loudness.value();

    return @{SFBReplayGainAnalyzerGainKey : @(gain), SFBReplayGainAnalyzerPeakKey : @(digitalPeak)};
}

@end
