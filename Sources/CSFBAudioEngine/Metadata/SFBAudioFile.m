//
// Copyright (c) 2020-2026 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import "SFBAudioFile.h"

#import "SFBAudioFile+Internal.h"
#import "SFBErrorWithLocalizedDescription.h"
#import "SFBLocalizedNameForURL.h"

#import <os/log.h>

// NSError domain for AudioFile and subclasses
NSErrorDomain const SFBAudioFileErrorDomain = @"org.sbooth.AudioEngine.AudioFile";

os_log_t gSFBAudioFileLog = NULL;

static void SFBCreateAudioFileLog(void) __attribute__((constructor));
static void SFBCreateAudioFileLog(void) {
    gSFBAudioFileLog = os_log_create("org.sbooth.AudioEngine", "AudioFile");
}

@interface SFBAudioFileSubclassInfo : NSObject
@property(nonatomic) Class klass;
@property(nonatomic) int priority;
@end

@implementation SFBAudioFile

static NSMutableArray *_registeredSubclasses = nil;

+ (void)load {
    [NSError
          setUserInfoValueProviderForDomain:SFBAudioFileErrorDomain
                                   provider:^id(NSError *err, NSErrorUserInfoKey userInfoKey) {
                                       switch (err.code) {
                                       case SFBAudioFileErrorCodeInternalError:
                                           if ([userInfoKey isEqualToString:NSLocalizedDescriptionKey]) {
                                               return NSLocalizedString(@"An internal error occurred.", @"");
                                           }
                                           break;

                                       case SFBAudioFileErrorCodeUnknownFormatName:
                                           if ([userInfoKey isEqualToString:NSLocalizedDescriptionKey]) {
                                               return NSLocalizedString(@"The requested format is unavailable.", @"");
                                           }
                                           break;

                                       case SFBAudioFileErrorCodeInputOutput:
                                           if ([userInfoKey isEqualToString:NSLocalizedDescriptionKey]) {
                                               return NSLocalizedString(@"An input/output error occurred.", @"");
                                           }
                                           break;

                                       case SFBAudioFileErrorCodeInvalidFormat:
                                           if ([userInfoKey isEqualToString:NSLocalizedDescriptionKey]) {
                                               return NSLocalizedString(
                                                     @"The file's format is invalid, unknown, or unsupported.", @"");
                                           }
                                           break;
                                       }

                                       return nil;
                                   }];
}

+ (NSSet *)supportedPathExtensions {
    NSMutableSet *result = [NSMutableSet set];
    for (SFBAudioFileSubclassInfo *subclassInfo in _registeredSubclasses) {
        NSSet *supportedPathExtensions = [subclassInfo.klass supportedPathExtensions];
        [result unionSet:supportedPathExtensions];
    }

    return result;
}

+ (NSSet *)supportedMIMETypes {
    NSMutableSet *result = [NSMutableSet set];
    for (SFBAudioFileSubclassInfo *subclassInfo in _registeredSubclasses) {
        NSSet *supportedMIMETypes = [subclassInfo.klass supportedMIMETypes];
        [result unionSet:supportedMIMETypes];
    }

    return result;
}

+ (SFBAudioFileFormatName)formatName {
    [self doesNotRecognizeSelector:_cmd];
    __builtin_unreachable();
}

+ (BOOL)testFileHandle:(NSFileHandle *)fileHandle
      formatIsSupported:(SFBTernaryTruthValue *)formatIsSupported
                  error:(NSError **)error {
    [self doesNotRecognizeSelector:_cmd];
    __builtin_unreachable();
}

+ (BOOL)handlesPathsWithExtension:(NSString *)extension {
    NSString *lowercaseExtension = extension.lowercaseString;
    for (SFBAudioFileSubclassInfo *subclassInfo in _registeredSubclasses) {
        NSSet *supportedPathExtensions = [subclassInfo.klass supportedPathExtensions];
        if ([supportedPathExtensions containsObject:lowercaseExtension]) {
            return YES;
        }
    }

    return NO;
}

+ (BOOL)handlesMIMEType:(NSString *)mimeType {
    NSString *lowercaseMIMEType = mimeType.lowercaseString;
    for (SFBAudioFileSubclassInfo *subclassInfo in _registeredSubclasses) {
        NSSet *supportedMIMETypes = [subclassInfo.klass supportedMIMETypes];
        if ([supportedMIMETypes containsObject:lowercaseMIMEType]) {
            return YES;
        }
    }

    return NO;
}

+ (BOOL)copyMetadataFromURL:(NSURL *)sourceURL toURL:(NSURL *)destinationURL error:(NSError **)error {
    NSParameterAssert(sourceURL != nil);
    NSParameterAssert(destinationURL != nil);

    SFBAudioFile *sourceAudioFile = [SFBAudioFile audioFileWithURL:sourceURL error:error];
    if (!sourceAudioFile) {
        return NO;
    }

    SFBAudioFile *destinationAudioFile = [SFBAudioFile audioFileWithURL:destinationURL error:error];
    if (!destinationAudioFile) {
        return NO;
    }

    [destinationAudioFile.metadata copyMetadataFrom:sourceAudioFile.metadata];
    [destinationAudioFile.metadata copyAttachedPicturesFrom:sourceAudioFile.metadata];

    return [destinationAudioFile writeMetadataReturningError:error];
}

+ (instancetype)audioFileWithURL:(NSURL *)url error:(NSError **)error {
    NSParameterAssert(url != nil);
    SFBAudioFile *audioFile = [[SFBAudioFile alloc] initWithURL:url];
    if (![audioFile readPropertiesAndMetadataReturningError:error]) {
        return nil;
    }
    return audioFile;
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

    NSString *lowercaseExtension = url.pathExtension.lowercaseString;
    NSString *lowercaseMIMEType = mimeTypeHint.lowercaseString;

    NSFileHandle *fileHandle = nil;
    if (detectContentType) {
        fileHandle = [NSFileHandle fileHandleForReadingFromURL:url error:error];
        if (!fileHandle) {
            return nil;
        }
    }

    int score = 10;
    Class subclass = nil;

    for (SFBAudioFileSubclassInfo *subclassInfo in _registeredSubclasses) {
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
            if ([klass testFileHandle:fileHandle formatIsSupported:&formatSupported error:error]) {
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
                    os_log_fault(gSFBAudioFileLog, "Unknown SFBTernaryTruthValue %li", (long)formatSupported);
                    break;
                }
            } else {
                os_log_error(gSFBAudioFileLog, "Error testing %{public}@ format support for %{public}@", klass,
                             fileHandle);
            }
        }

        if (currentScore > score) {
            score = currentScore;
            subclass = klass;
        }
    }

    if (!subclass) {
        os_log_debug(gSFBAudioFileLog, "Unable to determine content type for \"%{public}@\"",
                     [[NSFileManager defaultManager] displayNameAtPath:url.path]);
        if (error) {
            *error = SFBErrorWithLocalizedDescription(
                  SFBAudioFileErrorDomain, SFBAudioFileErrorCodeInvalidFormat,
                  NSLocalizedString(@"The type of the file “%@” could not be determined.", @""), @{
                      NSLocalizedRecoverySuggestionErrorKey : NSLocalizedString(
                            @"The file's extension may be missing or may not match the file's type.", @""),
                      NSURLErrorKey : self.url
                  },
                  SFBLocalizedNameForURL(self.url));
        }
        return nil;
    }

    if ((self = [[subclass alloc] init])) {
        _url = url;
        _properties = [[SFBAudioProperties alloc] init];
        _metadata = [[SFBAudioMetadata alloc] init];
#if DEBUG
        os_log_debug(gSFBAudioFileLog, "Created %{public}@ based on score of %i", self, score);
#endif /* DEBUG */
    }

    return self;
}

- (instancetype)initWithURL:(NSURL *)url formatName:(SFBAudioFileFormatName)formatName {
    return [self initWithURL:url formatName:formatName error:nil];
}

- (instancetype)initWithURL:(NSURL *)url formatName:(SFBAudioFileFormatName)formatName error:(NSError **)error {
    NSParameterAssert(url != nil);

    Class subclass = nil;
    for (SFBAudioFileSubclassInfo *subclassInfo in _registeredSubclasses) {
        SFBAudioFileFormatName subclassFormatName = [subclassInfo.klass formatName];
        if (subclassFormatName == formatName) {
            subclass = subclassInfo.klass;
            break;
        }
    }

    if (!subclass) {
        os_log_debug(gSFBAudioFileLog, "SFBAudioFile unknown format: \"%{public}@\"", formatName);
        if (error) {
            *error = [NSError errorWithDomain:SFBAudioFileErrorDomain
                                         code:SFBAudioFileErrorCodeUnknownFormatName
                                     userInfo:@{NSURLErrorKey : url}];
        }
        return nil;
    }

    if ((self = [[subclass alloc] init])) {
        _url = url;
        _properties = [[SFBAudioProperties alloc] init];
        _metadata = [[SFBAudioMetadata alloc] init];
    }

    return self;
}

- (BOOL)readPropertiesAndMetadataReturningError:(NSError **)error {
    [self doesNotRecognizeSelector:_cmd];
    __builtin_unreachable();
}

- (BOOL)writeMetadataReturningError:(NSError **)error {
    [self doesNotRecognizeSelector:_cmd];
    __builtin_unreachable();
}

- (NSString *)description {
    return [NSString stringWithFormat:@"<%@ %p: \"%@\">", [self class], (__bridge void *)self,
                                      [[NSFileManager defaultManager] displayNameAtPath:_url.path]];
}

@end

@implementation SFBAudioFileSubclassInfo
@end

@implementation SFBAudioFile (SFBAudioFileSubclassRegistration)

+ (void)registerSubclass:(Class)subclass {
    [self registerSubclass:subclass priority:0];
}

+ (void)registerSubclass:(Class)subclass priority:(int)priority {
    //    NSAssert([subclass isKindOfClass:[self class]],
    //             @"Unable to register class '%@' because it is not a subclass of SFBAudioFile",
    //             NSStringFromClass(subclass));

    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{ _registeredSubclasses = [NSMutableArray array]; });

    SFBAudioFileSubclassInfo *subclassInfo = [[SFBAudioFileSubclassInfo alloc] init];
    subclassInfo.klass = subclass;
    subclassInfo.priority = priority;

    [_registeredSubclasses addObject:subclassInfo];

    // N.B. `sortUsingComparator:` sorts in ascending order
    // To sort the array in descending order the comparator is reversed
    [_registeredSubclasses sortUsingComparator:^NSComparisonResult(id _Nonnull obj1, id _Nonnull obj2) {
        int a = ((SFBAudioFileSubclassInfo *)obj1).priority;
        int b = ((SFBAudioFileSubclassInfo *)obj2).priority;
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
