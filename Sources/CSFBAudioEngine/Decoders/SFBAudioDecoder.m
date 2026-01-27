//
// Copyright (c) 2006-2026 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import "SFBAudioDecoder.h"

#import "SFBAudioDecoder+Internal.h"
#import "SFBLocalizedNameForURL.h"

#import <os/log.h>

// NSError domain for AudioDecoder and subclasses
NSErrorDomain const SFBAudioDecoderErrorDomain = @"org.sbooth.AudioEngine.AudioDecoder";

os_log_t gSFBAudioDecoderLog = NULL;

static void SFBCreateAudioDecoderLog(void) __attribute__((constructor));
static void SFBCreateAudioDecoderLog(void) {
    gSFBAudioDecoderLog = os_log_create("org.sbooth.AudioEngine", "AudioDecoder");
}

@interface SFBAudioDecoderSubclassInfo : NSObject
@property(nonatomic) Class klass;
@property(nonatomic) int priority;
@end

@implementation SFBAudioDecoder

@synthesize inputSource = _inputSource;
@synthesize sourceFormat = _sourceFormat;
@synthesize processingFormat = _processingFormat;
@synthesize properties = _properties;

@dynamic decodingIsLossless;
@dynamic framePosition;
@dynamic frameLength;

static NSMutableArray *_registeredSubclasses = nil;

+ (void)load {
    [NSError
          setUserInfoValueProviderForDomain:SFBAudioDecoderErrorDomain
                                   provider:^id(NSError *err, NSErrorUserInfoKey userInfoKey) {
                                       switch (err.code) {
                                       case SFBAudioDecoderErrorCodeUnknownDecoder:
                                           if ([userInfoKey isEqualToString:NSLocalizedDescriptionKey]) {
                                               return NSLocalizedString(@"The requested PCM decoder is unavailable.",
                                                                        @"");
                                           }
                                           break;

                                       case SFBAudioDecoderErrorCodeInvalidFormat: {
                                           if ([userInfoKey isEqualToString:NSLocalizedDescriptionKey]) {
                                               return NSLocalizedString(@"The format is invalid or unknown.", @"");
                                           }
                                           break;
                                       }

                                       case SFBAudioDecoderErrorCodeUnsupportedFormat: {
                                           if ([userInfoKey isEqualToString:NSLocalizedDescriptionKey]) {
                                               return NSLocalizedString(@"The PCM audio format is unsupported.", @"");
                                           }
                                           break;
                                       }

                                       case SFBAudioDecoderErrorCodeInternalError: {
                                           if ([userInfoKey isEqualToString:NSLocalizedDescriptionKey]) {
                                               return NSLocalizedString(@"An internal decoder error occurred.", @"");
                                           }
                                           break;
                                       }

                                       case SFBAudioDecoderErrorCodeDecodingError: {
                                           if ([userInfoKey isEqualToString:NSLocalizedDescriptionKey]) {
                                               return NSLocalizedString(@"An error occurred during PCM audio decoding.",
                                                                        @"");
                                           }
                                           break;
                                       }

                                       case SFBAudioDecoderErrorCodeSeekError:
                                           if ([userInfoKey isEqualToString:NSLocalizedDescriptionKey]) {
                                               return NSLocalizedString(
                                                     @"An error occurred seeking to the requested PCM frame position.",
                                                     @"");
                                           }
                                           break;
                                       }

                                       return nil;
                                   }];
}

+ (NSSet *)supportedPathExtensions {
    NSMutableSet *result = [NSMutableSet set];
    for (SFBAudioDecoderSubclassInfo *subclassInfo in _registeredSubclasses) {
        NSSet *supportedPathExtensions = [subclassInfo.klass supportedPathExtensions];
        [result unionSet:supportedPathExtensions];
    }
    return result;
}

+ (NSSet *)supportedMIMETypes {
    NSMutableSet *result = [NSMutableSet set];
    for (SFBAudioDecoderSubclassInfo *subclassInfo in _registeredSubclasses) {
        NSSet *supportedMIMETypes = [subclassInfo.klass supportedMIMETypes];
        [result unionSet:supportedMIMETypes];
    }
    return result;
}

+ (SFBAudioDecoderName)decoderName {
    [self doesNotRecognizeSelector:_cmd];
    __builtin_unreachable();
}

+ (BOOL)testInputSource:(SFBInputSource *)inputSource
      formatIsSupported:(SFBTernaryTruthValue *)formatIsSupported
                  error:(NSError **)error {
    [self doesNotRecognizeSelector:_cmd];
    __builtin_unreachable();
}

+ (BOOL)handlesPathsWithExtension:(NSString *)extension {
    NSString *lowercaseExtension = extension.lowercaseString;
    for (SFBAudioDecoderSubclassInfo *subclassInfo in _registeredSubclasses) {
        NSSet *supportedPathExtensions = [subclassInfo.klass supportedPathExtensions];
        if ([supportedPathExtensions containsObject:lowercaseExtension]) {
            return YES;
        }
    }
    return NO;
}

+ (BOOL)handlesMIMEType:(NSString *)mimeType {
    NSString *lowercaseMIMEType = mimeType.lowercaseString;
    for (SFBAudioDecoderSubclassInfo *subclassInfo in _registeredSubclasses) {
        NSSet *supportedMIMETypes = [subclassInfo.klass supportedMIMETypes];
        if ([supportedMIMETypes containsObject:lowercaseMIMEType]) {
            return YES;
        }
    }
    return NO;
}

- (instancetype)initWithURL:(NSURL *)url {
    return [self initWithURL:url detectContentType:YES mimeTypeHint:nil error:nil];
}

- (instancetype)initWithURL:(NSURL *)url error:(NSError **)error {
    return [self initWithURL:url detectContentType:YES mimeTypeHint:nil error:error];
}

- (instancetype)initWithURL:(NSURL *)url detectContentType:(BOOL)detectContentType error:(NSError **)error {
    return [self initWithURL:url detectContentType:detectContentType mimeTypeHint:nil error:error];
}

- (instancetype)initWithURL:(NSURL *)url mimeTypeHint:(NSString *)mimeTypeHint error:(NSError **)error {
    return [self initWithURL:url detectContentType:YES mimeTypeHint:mimeTypeHint error:error];
}

- (instancetype)initWithURL:(NSURL *)url
          detectContentType:(BOOL)detectContentType
               mimeTypeHint:(NSString *)mimeTypeHint
                      error:(NSError **)error {
    NSParameterAssert(url != nil);

    SFBInputSource *inputSource = [SFBInputSource inputSourceForURL:url flags:0 error:error];
    if (!inputSource) {
        return nil;
    }
    return [self initWithInputSource:inputSource
                   detectContentType:detectContentType
                        mimeTypeHint:mimeTypeHint
                               error:error];
}

- (instancetype)initWithInputSource:(SFBInputSource *)inputSource {
    return [self initWithInputSource:inputSource detectContentType:YES mimeTypeHint:nil error:nil];
}

- (instancetype)initWithInputSource:(SFBInputSource *)inputSource error:(NSError **)error {
    return [self initWithInputSource:inputSource detectContentType:YES mimeTypeHint:nil error:error];
}

- (instancetype)initWithInputSource:(SFBInputSource *)inputSource
                  detectContentType:(BOOL)detectContentType
                              error:(NSError **)error {
    return [self initWithInputSource:inputSource detectContentType:detectContentType mimeTypeHint:nil error:error];
}

- (instancetype)initWithInputSource:(SFBInputSource *)inputSource
                       mimeTypeHint:(NSString *)mimeTypeHint
                              error:(NSError **)error {
    return [self initWithInputSource:inputSource detectContentType:YES mimeTypeHint:mimeTypeHint error:error];
}

- (instancetype)initWithInputSource:(SFBInputSource *)inputSource
                  detectContentType:(BOOL)detectContentType
                       mimeTypeHint:(NSString *)mimeTypeHint
                              error:(NSError **)error {
    NSParameterAssert(inputSource != nil);

    NSString *lowercaseExtension = inputSource.url.pathExtension.lowercaseString;
    NSString *lowercaseMIMEType = mimeTypeHint.lowercaseString;

    if (detectContentType) {
        // If the input source can't be opened decoding is destined to fail; give up now
        if (!inputSource.isOpen && ![inputSource openReturningError:error]) {
            return nil;
        }
        // Instead of failing for non-seekable inputs just skip content type detection
        if (!inputSource.supportsSeeking) {
            os_log_error(gSFBAudioDecoderLog, "Unable to detect content type for non-seekable input source %{public}@",
                         inputSource);
            detectContentType = NO;
        }
    }

    int score = 10;
    Class subclass = nil;

    for (SFBAudioDecoderSubclassInfo *subclassInfo in _registeredSubclasses) {
        int currentScore = 0;
        Class klass = subclassInfo.klass;

        if (lowercaseMIMEType) {
            NSSet *supportedMIMETypes = [klass supportedMIMETypes];
            if ([supportedMIMETypes containsObject:lowercaseMIMEType]) {
                currentScore += 40;
            }
        }

        if (lowercaseExtension) {
            NSSet *supportedPathExtensions = [klass supportedPathExtensions];
            if ([supportedPathExtensions containsObject:lowercaseExtension]) {
                currentScore += 20;
            }
        }

        if (detectContentType) {
            SFBTernaryTruthValue formatSupported;
            if ([klass testInputSource:inputSource formatIsSupported:&formatSupported error:error]) {
                switch (formatSupported) {
                case SFBTernaryTruthValueTrue:
                    currentScore += 75;
                    break;
                case SFBTernaryTruthValueFalse:
                    break;
                case SFBTernaryTruthValueUnknown:
                    currentScore += 10;
                    break;
                default:
                    os_log_fault(gSFBAudioDecoderLog, "Unknown SFBTernaryTruthValue %li", (long)formatSupported);
                    break;
                }
            } else {
                os_log_error(gSFBAudioDecoderLog, "Error testing %{public}@ format support for %{public}@", klass,
                             inputSource);
            }
        }

        if (currentScore > score) {
            score = currentScore;
            subclass = klass;
        }
    }

    if (!subclass) {
        os_log_debug(gSFBAudioDecoderLog, "Unable to determine content type for %{public}@", inputSource);
        if (error) {
            NSMutableDictionary *userInfo = [NSMutableDictionary
                  dictionaryWithObject:NSLocalizedString(
                                             @"The file's extension may be missing or may not match the file's type.",
                                             @"")
                                forKey:NSLocalizedRecoverySuggestionErrorKey];

            if (inputSource.url) {
                userInfo[NSLocalizedDescriptionKey] = [NSString
                      localizedStringWithFormat:NSLocalizedString(@"The type of the file “%@” could not be determined.",
                                                                  @""),
                                                SFBLocalizedNameForURL(inputSource.url)];
                userInfo[NSURLErrorKey] = inputSource.url;
            } else {
                userInfo[NSLocalizedDescriptionKey] =
                      NSLocalizedString(@"The type of the file could not be determined.", @"");
            }

            *error = [NSError errorWithDomain:SFBAudioDecoderErrorDomain
                                         code:SFBAudioDecoderErrorCodeInvalidFormat
                                     userInfo:userInfo];
        }
        return nil;
    }

    if ((self = [[subclass alloc] init])) {
        _inputSource = inputSource;
#if DEBUG
        os_log_debug(gSFBAudioDecoderLog, "Created %{public}@ based on score of %i", self, score);
#endif /* DEBUG */
    }

    return self;
}

- (instancetype)initWithURL:(NSURL *)url decoderName:(SFBAudioDecoderName)decoderName {
    return [self initWithURL:url decoderName:decoderName error:nil];
}

- (instancetype)initWithURL:(NSURL *)url decoderName:(SFBAudioDecoderName)decoderName error:(NSError **)error {
    NSParameterAssert(url != nil);

    SFBInputSource *inputSource = [SFBInputSource inputSourceForURL:url flags:0 error:error];
    if (!inputSource) {
        return nil;
    }
    return [self initWithInputSource:inputSource decoderName:decoderName error:error];
}

- (instancetype)initWithInputSource:(SFBInputSource *)inputSource decoderName:(SFBAudioDecoderName)decoderName {
    return [self initWithInputSource:inputSource decoderName:decoderName error:nil];
}

- (instancetype)initWithInputSource:(SFBInputSource *)inputSource
                        decoderName:(SFBAudioDecoderName)decoderName
                              error:(NSError **)error {
    NSParameterAssert(inputSource != nil);

    Class subclass = nil;
    for (SFBAudioDecoderSubclassInfo *subclassInfo in _registeredSubclasses) {
        SFBAudioDecoderName subclassDecoderName = [subclassInfo.klass decoderName];
        if (subclassDecoderName == decoderName) {
            subclass = subclassInfo.klass;
            break;
        }
    }

    if (!subclass) {
        os_log_error(gSFBAudioDecoderLog, "SFBAudioDecoder unknown decoder: \"%{public}@\"", decoderName);
        if (error) {
            *error = [NSError errorWithDomain:SFBAudioDecoderErrorDomain
                                         code:SFBAudioDecoderErrorCodeUnknownDecoder
                                     userInfo:nil];
        }
        return nil;
    }

    if ((self = [[subclass alloc] init])) {
        _inputSource = inputSource;
    }

    return self;
}

- (void)dealloc {
    [self closeReturningError:nil];
}

- (BOOL)openReturningError:(NSError **)error {
    if (!_inputSource.isOpen) {
        return [_inputSource openReturningError:error];
    }
    return YES;
}

- (BOOL)closeReturningError:(NSError **)error {
    _sourceFormat = nil;
    _processingFormat = nil;
    _properties = nil;
    if (_inputSource.isOpen) {
        return [_inputSource closeReturningError:error];
    }
    return YES;
}

- (BOOL)isOpen {
    [self doesNotRecognizeSelector:_cmd];
    __builtin_unreachable();
}

- (BOOL)decodeIntoBuffer:(AVAudioBuffer *)buffer error:(NSError **)error {
    NSParameterAssert(buffer != nil);
    NSParameterAssert([buffer isKindOfClass:[AVAudioPCMBuffer class]]);
    return [self decodeIntoBuffer:(AVAudioPCMBuffer *)buffer
                      frameLength:((AVAudioPCMBuffer *)buffer).frameCapacity
                            error:error];
}

- (BOOL)decodeIntoBuffer:(AVAudioPCMBuffer *)buffer frameLength:(AVAudioFrameCount)frameLength error:(NSError **)error {
    [self doesNotRecognizeSelector:_cmd];
    __builtin_unreachable();
}

- (BOOL)supportsSeeking {
    return _inputSource.supportsSeeking;
}

- (BOOL)seekToFrame:(AVAudioFramePosition)frame error:(NSError **)error {
    [self doesNotRecognizeSelector:_cmd];
    __builtin_unreachable();
}

- (NSString *)description {
    return [NSString stringWithFormat:@"<%@ %p: \"%@\">", [self class], (__bridge void *)self,
                                      [[NSFileManager defaultManager] displayNameAtPath:_inputSource.url.path]];
}

@end

@implementation SFBAudioDecoderSubclassInfo
@end

@implementation SFBAudioDecoder (SFBAudioDecoderSubclassRegistration)

+ (void)registerSubclass:(Class)subclass {
    [self registerSubclass:subclass priority:0];
}

+ (void)registerSubclass:(Class)subclass priority:(int)priority {
    //    NSAssert([subclass isKindOfClass:[self class]],
    //             @"Unable to register class '%@' because it is not a subclass of SFBAudioDecoder",
    //             NSStringFromClass(subclass));

    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{ _registeredSubclasses = [NSMutableArray array]; });

    SFBAudioDecoderSubclassInfo *subclassInfo = [[SFBAudioDecoderSubclassInfo alloc] init];
    subclassInfo.klass = subclass;
    subclassInfo.priority = priority;

    [_registeredSubclasses addObject:subclassInfo];

    // N.B. `sortUsingComparator:` sorts in ascending order
    // To sort the array in descending order the comparator is reversed
    [_registeredSubclasses sortUsingComparator:^NSComparisonResult(id _Nonnull obj1, id _Nonnull obj2) {
        int a = ((SFBAudioDecoderSubclassInfo *)obj1).priority;
        int b = ((SFBAudioDecoderSubclassInfo *)obj2).priority;
        if (a > b) {
            return NSOrderedAscending;
        }
        if (a < b) {
            return NSOrderedDescending;
        }
        return NSOrderedSame;
    }];
}

@end
