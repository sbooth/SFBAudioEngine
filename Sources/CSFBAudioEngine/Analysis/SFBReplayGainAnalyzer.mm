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
#import "ebur128/ebur128.h"

#import <os/log.h>

#import <memory>
#import <vector>

// NSError domain for SFBReplayGainAnalyzer
NSErrorDomain const SFBReplayGainAnalyzerErrorDomain = @"org.sbooth.AudioEngine.ReplayGainAnalyzer";

// Key names for the metadata dictionary
NSString *const SFBReplayGainAnalyzerGainKey = @"Gain";
NSString *const SFBReplayGainAnalyzerPeakKey = @"Peak";

namespace {

/// A `std::unique_ptr` deleter for `ebur128_state`
struct ebur128_state_deleter {
    void operator()(ebur128_state *state) { ebur128_destroy(&state); }
};

using ebur128_ptr = std::unique_ptr<ebur128_state, ebur128_state_deleter>;

} /* namespace */

#define BUFFER_SIZE_FRAMES 2048

@interface SFBReplayGainAnalyzer () {
  @private
    std::vector<ebur128_ptr> _states;
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
                                          }

                                          return nil;
                                      }];
}

+ (NSDictionary<SFBReplayGainAnalyzerKey, NSNumber *> *)analyzeTrack:(NSURL *)url error:(NSError **)error {
    SFBReplayGainAnalyzer *analyzer = [[SFBReplayGainAnalyzer alloc] init];
    return [analyzer analyzeTrack:url error:error];
}

+ (NSDictionary *)analyzeAlbum:(NSArray<NSURL *> *)urls error:(NSError **)error {
    NSMutableDictionary *result = [NSMutableDictionary dictionary];

    SFBReplayGainAnalyzer *analyzer = [[SFBReplayGainAnalyzer alloc] init];

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
                                                                  interleaved:YES];

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
        if (!_albumFormat) {
            _albumFormat = outputFormat;
        }

        auto &state = _states.emplace_back(ebur128_init(outputFormat.channelCount, outputFormat.sampleRate,
                                                        EBUR128_MODE_SAMPLE_PEAK | EBUR128_MODE_I));

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

            auto result = ebur128_add_frames_float(state.get(), outputBuffer.floatChannelData[0], frameCount);
            if (result != EBUR128_SUCCESS) {
                os_log_error(OS_LOG_DEFAULT, "ebur128_add_frames_float failed: %d", result);
                if (error) {
                    *error = SFBErrorWithLocalizedDescription(
                            SFBReplayGainAnalyzerErrorDomain, SFBReplayGainAnalyzerErrorCodeFileFormatNotSupported,
                            NSLocalizedString(@"The format of the file “%@” is not supported.", @""), @{
                                NSLocalizedRecoverySuggestionErrorKey : NSLocalizedString(
                                        @"The file's format is not supported for replay gain analysis.", @"")
                            },
                            SFBLocalizedNameForURL(url));
                }
                return nil;
            }
        }

        double loudness;
        auto result = ebur128_loudness_global(state.get(), &loudness);
        if (result != EBUR128_SUCCESS) {
            os_log_error(OS_LOG_DEFAULT, "ebur128_loudness_global failed: %d", result);
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

        double digitalPeak = 0;
        for (unsigned int channel = 0; channel < state->channels; ++channel) {
            double peak;
            result = ebur128_sample_peak(state.get(), channel, &peak);
            if (result != EBUR128_SUCCESS) {
                os_log_error(OS_LOG_DEFAULT, "ebur128_sample_peak failed: %d", result);
                if (error) {
                    *error = SFBErrorWithLocalizedDescription(
                            SFBReplayGainAnalyzerErrorDomain, SFBReplayGainAnalyzerErrorCodeInsufficientSamples,
                            NSLocalizedString(@"The file “%@” does not contain sufficient audio for analysis.", @""), @{
                                NSLocalizedRecoverySuggestionErrorKey : NSLocalizedString(
                                        @"The audio duration is too short for replay gain analysis.", @"")
                            },
                            SFBLocalizedNameForURL(url));
                }
                return nil;
            }

            digitalPeak = std::max(digitalPeak, peak);
        }

        const auto gain = -18.0 - loudness;

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
    try {
        std::vector<ebur128_state *> rawStates;
        rawStates.reserve(_states.size());

        std::transform(_states.begin(), _states.end(), std::back_inserter(rawStates),
                       [](const ebur128_ptr &ptr) { return ptr.get(); });

        double loudness;
        auto result = ebur128_loudness_global_multiple(rawStates.data(), rawStates.size(), &loudness);
        if (result != EBUR128_SUCCESS) {
            os_log_error(OS_LOG_DEFAULT, "ebur128_loudness_global_multiple failed: %d", result);
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
        }

        double digitalPeak = 0;
        for (auto state : rawStates) {
            for (unsigned int channel = 0; channel < state->channels; ++channel) {
                double peak;
                result = ebur128_sample_peak(state, channel, &peak);
                if (result != EBUR128_SUCCESS) {
                    os_log_error(OS_LOG_DEFAULT, "ebur128_sample_peak failed: %d", result);
                    if (error) {
                        *error = [NSError
                                errorWithDomain:SFBReplayGainAnalyzerErrorDomain
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

                digitalPeak = std::max(digitalPeak, peak);
            }
        }

        const auto gain = -18.f - loudness;

        return @{SFBReplayGainAnalyzerGainKey : @(gain), SFBReplayGainAnalyzerPeakKey : @(digitalPeak)};
    } catch (const std::exception &e) {
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
}

@end
