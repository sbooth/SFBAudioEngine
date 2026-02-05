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
#import <vector>

// NSError domain for SFBReplayGainAnalyzer
NSErrorDomain const SFBReplayGainAnalyzerErrorDomain = @"org.sbooth.AudioEngine.ReplayGainAnalyzer";

// Key names for the metadata dictionary
NSString *const SFBReplayGainAnalyzerKeyGain = @"Gain";
NSString *const SFBReplayGainAnalyzerKeyPeak = @"Peak";

namespace {

constexpr std::size_t bufferSizeFrames = 2048;
constexpr float referenceLoudness = -18.f;

struct ReplayGainContext {
    NSArray *urls_;
    std::vector<std::unique_ptr<loudness::EbuR128Analyzer>> analyzers_;
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
                                                       interleaved:NO
                                                     channelLayout:channelLayout];
    } else {
        outputFormat = [[AVAudioFormat alloc] initWithCommonFormat:AVAudioPCMFormatFloat32
                                                        sampleRate:inputFormat.sampleRate
                                                          channels:inputFormat.channelCount
                                                       interleaved:NO];
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
        auto channelWeights = loudness::DefaultChannelWeights();

        ctx->analyzers_[iteration] = std::make_unique<loudness::EbuR128Analyzer>(
                outputFormat.channelCount, channelWeights, outputFormat.sampleRate, false);
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

            analyzer->Process(outputBuffer.floatChannelData[0], outputBuffer.frameLength,
                              loudness::EbuR128Analyzer::SampleFormat::FLOAT,
                              loudness::EbuR128Analyzer::SampleLayout::PLANAR_CONTIGUOUS);
        }
    } catch (const std::exception &e) {
        os_log_error(OS_LOG_DEFAULT, "Error analyzing audio: %{public}s", e.what());
        ctx->analyzers_[iteration].reset();
        ctx->errors_[iteration] = [NSError errorWithDomain:NSPOSIXErrorDomain code:EFTYPE userInfo:nil];
    }
}

} /* namespace */

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
    SFBReplayGainAnalyzer *analyzer = [[SFBReplayGainAnalyzer alloc] init];
    return [analyzer analyzeAlbum:urls error:error];
}

- (NSDictionary *)analyzeTrack:(NSURL *)url error:(NSError **)error {
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

    auto loudness = analyzer->GetRelativeGatedIntegratedLoudness();
    auto digitalPeak = analyzer->digital_peak();

    if (!loudness.has_value()) {
        if (error != nil) {
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

    const auto gain = referenceLoudness - loudness.value();
    return @{SFBReplayGainAnalyzerKeyGain : @(gain), SFBReplayGainAnalyzerKeyPeak : @(digitalPeak)};
}

- (NSDictionary *)analyzeAlbum:(NSArray<NSURL *> *)urls error:(NSError **)error {
    NSParameterAssert(urls != nil);

    const auto count = urls.count;

    ReplayGainContext ctx{};
    ctx.urls_ = urls;

    std::vector<loudness::EbuR128Analyzer *> analyzers{};

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

    NSMutableDictionary *result = [NSMutableDictionary dictionary];
    float albumPeak = 0.f;

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

        auto loudness = analyzer->GetRelativeGatedIntegratedLoudness();
        auto digitalPeak = analyzer->digital_peak();

        if (!loudness.has_value()) {
            if (error != nil) {
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

        albumPeak = std::max(albumPeak, digitalPeak);

        const auto gain = referenceLoudness - loudness.value();
        [result setObject:@{SFBReplayGainAnalyzerKeyGain : @(gain), SFBReplayGainAnalyzerKeyPeak : @(digitalPeak)}
                   forKey:url];
    }

    auto loudness = loudness::EbuR128Analyzer::GetRelativeGatedIntegratedLoudness(analyzers);
    if (!loudness.has_value()) {
        if (error != nil) {
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

    const auto gain = referenceLoudness - loudness.value();

    [result setObject:@(gain) forKey:SFBReplayGainAnalyzerKeyGain];
    [result setObject:@(albumPeak) forKey:SFBReplayGainAnalyzerKeyPeak];

    return result;
}

@end
