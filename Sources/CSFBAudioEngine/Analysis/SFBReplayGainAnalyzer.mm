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

namespace {

/// A `std::unique_ptr` deleter for `ebur128_state`
struct ebur128_state_deleter {
    void operator()(ebur128_state *state) { ebur128_destroy(&state); }
};

using ebur128_ptr = std::unique_ptr<ebur128_state, ebur128_state_deleter>;

constexpr std::size_t bufferSizeFrames = 2048;
constexpr float referenceLoudness = -18.f;

struct ReplayGainContext {
    NSArray *urls_;
    std::vector<ebur128_ptr> analyzers_;
    std::vector<NSError *> errors_;
};

void analyzeURL(void *context, size_t iteration) noexcept {
    auto ctx = static_cast<ReplayGainContext *>(context);

    NSURL *url = [ctx->urls_ objectAtIndex:iteration];

    NSError *error = nil;
    SFBAudioDecoder *decoder = [[SFBAudioDecoder alloc] initWithURL:url error:&error];
    if (decoder == nil || ![decoder openReturningError:&error]) {
        ctx->errors_[iteration] = error;
        return;
    }

    AVAudioFormat *inputFormat = decoder.processingFormat;
    AVAudioFormat *outputFormat = nil;
    if (AVAudioChannelLayout *channelLayout = inputFormat.channelLayout; channelLayout != nil) {
        outputFormat = [[AVAudioFormat alloc] initWithCommonFormat:AVAudioPCMFormatFloat32
                                                        sampleRate:inputFormat.sampleRate
                                                       interleaved:YES
                                                     channelLayout:channelLayout];
    } else {
        outputFormat = [[AVAudioFormat alloc] initWithCommonFormat:AVAudioPCMFormatFloat32
                                                        sampleRate:inputFormat.sampleRate
                                                          channels:inputFormat.channelCount
                                                       interleaved:YES];
    }

    AVAudioConverter *converter = [[AVAudioConverter alloc] initFromFormat:decoder.processingFormat
                                                                  toFormat:outputFormat];
    if (converter == nil) {
        return;
    }

    AVAudioPCMBuffer *decodeBuffer = [[AVAudioPCMBuffer alloc] initWithPCMFormat:converter.inputFormat
                                                                   frameCapacity:bufferSizeFrames];
    AVAudioPCMBuffer *outputBuffer = [[AVAudioPCMBuffer alloc] initWithPCMFormat:converter.outputFormat
                                                                   frameCapacity:bufferSizeFrames];

    try {

        ctx->analyzers_[iteration] = ebur128_ptr(ebur128_init(outputFormat.channelCount, outputFormat.sampleRate,
                                                              EBUR128_MODE_SAMPLE_PEAK | EBUR128_MODE_I));
        auto &analyzer = ctx->analyzers_[iteration];

        for (;;) {
            auto result = [decoder decodeIntoBuffer:decodeBuffer frameLength:decodeBuffer.frameCapacity error:&error];
            if (!result) {
                os_log_error(OS_LOG_DEFAULT, "Error decoding audio: %{public}@", error);
                ctx->analyzers_[iteration].reset();
                ctx->errors_[iteration] = error;
                return;
            }

            if (decodeBuffer.frameLength == 0) {
                return;
            }

            result = [converter convertToBuffer:outputBuffer fromBuffer:decodeBuffer error:&error];
            if (!result) {
                os_log_error(OS_LOG_DEFAULT, "Error converting audio: %{public}@", error);
                ctx->analyzers_[iteration].reset();
                ctx->errors_[iteration] = error;
                return;
            }

            auto status = ebur128_add_frames_float(analyzer.get(), outputBuffer.floatChannelData[0],
                                                   outputBuffer.frameLength);
            if (status != EBUR128_SUCCESS) {
                os_log_error(OS_LOG_DEFAULT, "ebur128_add_frames_float failed: %d", status);
                ctx->analyzers_[iteration].reset();
                ctx->errors_[iteration] = [NSError errorWithDomain:NSPOSIXErrorDomain code:EFTYPE userInfo:nil];
            }
        }
    } catch (const std::exception &e) {
        os_log_error(OS_LOG_DEFAULT, "Error analyzing audio: %{public}s", e.what());
        ctx->analyzers_[iteration].reset();
        ctx->errors_[iteration] = [NSError errorWithDomain:NSPOSIXErrorDomain code:EFTYPE userInfo:nil];
    }
}

} /* namespace */

@interface SFBReplayGain ()
- (instancetype)initWithGain:(float)gain peak:(float)peak;
@end

@implementation SFBReplayGain
- (instancetype)initWithGain:(float)gain peak:(float)peak {
    if ((self = [super init])) {
        _gain = gain;
        _peak = peak;
    }
    return self;
}
@end

@interface SFBAlbumReplayGain ()
- (instancetype)initWithReplayGain:(SFBReplayGain *)replayGain
                   trackReplayGain:(NSDictionary<NSURL *, SFBReplayGain *> *)trackReplayGain;
@end

@implementation SFBAlbumReplayGain
- (instancetype)initWithReplayGain:(SFBReplayGain *)replayGain
                   trackReplayGain:(NSDictionary<NSURL *, SFBReplayGain *> *)trackReplayGain {
    NSParameterAssert(replayGain != nil);
    NSParameterAssert(trackReplayGain != nil);
    if ((self = [super init])) {
        _replayGain = replayGain;
        _trackReplayGain = trackReplayGain;
    }
    return self;
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

+ (SFBReplayGain *)analyzeTrack:(NSURL *)url error:(NSError **)error {
    SFBReplayGainAnalyzer *analyzer = [[SFBReplayGainAnalyzer alloc] init];
    return [analyzer analyzeTrack:url error:error];
}

+ (SFBAlbumReplayGain *)analyzeAlbum:(NSArray<NSURL *> *)urls error:(NSError **)error {
    SFBReplayGainAnalyzer *analyzer = [[SFBReplayGainAnalyzer alloc] init];
    return [analyzer analyzeAlbum:urls error:error];
}

- (SFBReplayGain *)analyzeTrack:(NSURL *)url error:(NSError **)error {
    NSParameterAssert(url != nil);

    ReplayGainContext ctx{};
    ctx.urls_ = @[ url ];
    try {
        ctx.analyzers_.resize(1);
        ctx.errors_.resize(1);
    } catch (const std::exception &e) {
        if (error != nil) {
            *error = [NSError errorWithDomain:NSPOSIXErrorDomain code:ENOMEM userInfo:nil];
        }
        return nil;
    }

    analyzeURL(&ctx, 0);

    auto &analyzer = ctx.analyzers_[0];
    if (analyzer == nullptr) {
        if (error != nil) {
            if (NSError *err = ctx.errors_[0]; err != nil) {
                *error = err;
            } else {
                *error = SFBErrorWithLocalizedDescription(
                        SFBReplayGainAnalyzerErrorDomain, SFBReplayGainAnalyzerErrorCodeFileFormatNotSupported,
                        NSLocalizedString(@"The format of the file “%@” is not supported.", @""), @{
                            NSLocalizedRecoverySuggestionErrorKey : NSLocalizedString(
                                    @"The file's format is not supported for replay gain analysis.", @"")
                        },
                        SFBLocalizedNameForURL(url));
            }
        }
        return nil;
    }

    double loudness;
    auto result = ebur128_loudness_global(analyzer.get(), &loudness);
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
    for (unsigned int channel = 0; channel < analyzer->channels; ++channel) {
        double peak;
        result = ebur128_sample_peak(analyzer.get(), channel, &peak);
        if (result != EBUR128_SUCCESS) {
            os_log_error(OS_LOG_DEFAULT, "ebur128_sample_peak failed: %d", result);
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

        digitalPeak = std::max(digitalPeak, peak);
    }

    const auto gain = -18.0 - loudness;
    return [[SFBReplayGain alloc] initWithGain:gain peak:digitalPeak];
}

- (SFBAlbumReplayGain *)analyzeAlbum:(NSArray<NSURL *> *)urls error:(NSError **)error {
    NSParameterAssert(urls != nil);

    const auto count = urls.count;

    ReplayGainContext ctx{};
    ctx.urls_ = urls;

    std::vector<ebur128_state *> analyzers{};

    try {
        ctx.analyzers_.resize(count);
        ctx.errors_.resize(count);
        analyzers.resize(count);
    } catch (const std::exception &e) {
        if (error != nil) {
            *error = [NSError errorWithDomain:NSPOSIXErrorDomain code:ENOMEM userInfo:nil];
        }
        return nil;
    }

    dispatch_apply_f(count, DISPATCH_APPLY_AUTO, &ctx, analyzeURL);

    NSMutableDictionary *trackReplayGain = [NSMutableDictionary dictionary];
    double albumPeak = 0.0;

    for (NSUInteger i = 0; i < count; ++i) {
        NSURL *url = [urls objectAtIndex:i];
        auto &analyzer = ctx.analyzers_[i];
        if (analyzer == nullptr) {
            if (error != nil) {
                if (NSError *err = ctx.errors_[i]; err != nil) {
                    *error = err;
                } else {
                    *error = SFBErrorWithLocalizedDescription(
                            SFBReplayGainAnalyzerErrorDomain, SFBReplayGainAnalyzerErrorCodeFileFormatNotSupported,
                            NSLocalizedString(@"The format of the file “%@” is not supported.", @""), @{
                                NSLocalizedRecoverySuggestionErrorKey : NSLocalizedString(
                                        @"The file's format is not supported for replay gain analysis.", @"")
                            },
                            SFBLocalizedNameForURL(url));
                }
            }
            return nil;
        }

        analyzers[i] = analyzer.get();

        double loudness;
        auto result = ebur128_loudness_global(analyzer.get(), &loudness);
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
        for (unsigned int channel = 0; channel < analyzer->channels; ++channel) {
            double peak;
            result = ebur128_sample_peak(analyzer.get(), channel, &peak);
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
            albumPeak = std::max(albumPeak, digitalPeak);
        }

        const auto gain = referenceLoudness - loudness;
        [trackReplayGain setObject:[[SFBReplayGain alloc] initWithGain:gain peak:digitalPeak] forKey:url];
    }

    double loudness;
    auto result = ebur128_loudness_global_multiple(analyzers.data(), analyzers.size(), &loudness);
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

    const auto gain = referenceLoudness - loudness;

    SFBReplayGain *replayGain = [[SFBReplayGain alloc] initWithGain:gain peak:albumPeak];
    return [[SFBAlbumReplayGain alloc] initWithReplayGain:replayGain trackReplayGain:trackReplayGain];
}

@end
