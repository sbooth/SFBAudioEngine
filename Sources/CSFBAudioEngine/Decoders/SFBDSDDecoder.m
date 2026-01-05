//
// Copyright (c) 2014-2026 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

@import os.log;

#import "SFBDSDDecoder.h"
#import "SFBDSDDecoder+Internal.h"

#import "NSError+SFBURLPresentation.h"

// NSError domain for DSDDecoder and subclasses
NSErrorDomain const SFBDSDDecoderErrorDomain = @"org.sbooth.AudioEngine.DSDDecoder";

os_log_t gSFBDSDDecoderLog = NULL;

static void SFBCreateDSDDecoderLog(void) __attribute__ ((constructor));
static void SFBCreateDSDDecoderLog(void)
{
	static dispatch_once_t onceToken;
	dispatch_once(&onceToken, ^{
		gSFBDSDDecoderLog = os_log_create("org.sbooth.AudioEngine", "DSDDecoder");
	});
}

@interface SFBDSDDecoderSubclassInfo : NSObject
@property (nonatomic) Class klass;
@property (nonatomic) int priority;
@end

@implementation SFBDSDDecoder

@synthesize inputSource = _inputSource;
@synthesize sourceFormat = _sourceFormat;
@synthesize processingFormat = _processingFormat;
@synthesize properties = _properties;

@dynamic decodingIsLossless;
@dynamic packetPosition;
@dynamic packetCount;

static NSMutableArray *_registeredSubclasses = nil;

+ (void)load
{
	[NSError setUserInfoValueProviderForDomain:SFBDSDDecoderErrorDomain provider:^id(NSError *err, NSErrorUserInfoKey userInfoKey) {

		if([userInfoKey isEqualToString:NSLocalizedFailureReasonErrorKey]) {
			switch(err.code) {
				case SFBDSDDecoderErrorCodeInternalError:
					return NSLocalizedString(@"Internal decoder error", @"");
				case SFBDSDDecoderErrorCodeUnknownDecoder:
					return NSLocalizedString(@"Decoder unavailable", @"");
				case SFBDSDDecoderErrorCodeInvalidFormat:
					return NSLocalizedString(@"Invalid format", @"");
			}
		}

		if([userInfoKey isEqualToString:NSLocalizedDescriptionKey]) {
			switch(err.code) {
				case SFBDSDDecoderErrorCodeInternalError:
				{
					NSURL *url = [[err userInfo] objectForKey:NSURLErrorKey];
					if(url) {
						NSString *displayName = nil;
						[url getResourceValue:&displayName forKey:NSURLLocalizedNameKey error:nil];
						if(!displayName)
							displayName = url.lastPathComponent;
						return [NSString stringWithFormat: NSLocalizedString(@"An error occurred while decoding the file “%@”.", @""), displayName];
					}
					return NSLocalizedString(@"An error occurred during decoding.", @"");
				}
				case SFBDSDDecoderErrorCodeUnknownDecoder:
					return NSLocalizedString(@"The requested DSD decoder is unavailable.", @"");
				case SFBDSDDecoderErrorCodeInvalidFormat:
					return NSLocalizedString(@"The format is invalid, unknown, or unsupported.", @"");
			}
		}

		return nil;
	}];
}

+ (NSSet *)supportedPathExtensions
{
	NSMutableSet *result = [NSMutableSet set];
	for(SFBDSDDecoderSubclassInfo *subclassInfo in _registeredSubclasses) {
		NSSet *supportedPathExtensions = [subclassInfo.klass supportedPathExtensions];
		[result unionSet:supportedPathExtensions];
	}
	return result;
}

+ (NSSet *)supportedMIMETypes
{
	NSMutableSet *result = [NSMutableSet set];
	for(SFBDSDDecoderSubclassInfo *subclassInfo in _registeredSubclasses) {
		NSSet *supportedMIMETypes = [subclassInfo.klass supportedMIMETypes];
		[result unionSet:supportedMIMETypes];
	}
	return result;
}

+ (SFBDSDDecoderName)decoderName
{
	[self doesNotRecognizeSelector:_cmd];
	__builtin_unreachable();
}

+ (BOOL)testInputSource:(SFBInputSource *)inputSource formatIsSupported:(SFBTernaryTruthValue *)formatIsSupported error:(NSError **)error
{
	[self doesNotRecognizeSelector:_cmd];
	__builtin_unreachable();
}

+ (BOOL)handlesPathsWithExtension:(NSString *)extension
{
	NSString *lowercaseExtension = extension.lowercaseString;
	for(SFBDSDDecoderSubclassInfo *subclassInfo in _registeredSubclasses) {
		NSSet *supportedPathExtensions = [subclassInfo.klass supportedPathExtensions];
		if([supportedPathExtensions containsObject:lowercaseExtension])
			return YES;
	}
	return NO;
}

+ (BOOL)handlesMIMEType:(NSString *)mimeType
{
	NSString *lowercaseMIMEType = mimeType.lowercaseString;
	for(SFBDSDDecoderSubclassInfo *subclassInfo in _registeredSubclasses) {
		NSSet *supportedMIMETypes = [subclassInfo.klass supportedMIMETypes];
		if([supportedMIMETypes containsObject:lowercaseMIMEType])
			return YES;
	}
	return NO;
}

- (instancetype)initWithURL:(NSURL *)url
{
	return [self initWithURL:url detectContentType:YES mimeTypeHint:nil error:nil];
}

- (instancetype)initWithURL:(NSURL *)url error:(NSError **)error
{
	return [self initWithURL:url detectContentType:YES mimeTypeHint:nil error:error];
}

- (instancetype)initWithURL:(NSURL *)url detectContentType:(BOOL)detectContentType error:(NSError **)error
{
	return [self initWithURL:url detectContentType:detectContentType mimeTypeHint:nil error:error];
}

- (instancetype)initWithURL:(NSURL *)url mimeTypeHint:(NSString *)mimeTypeHint error:(NSError **)error
{
	return [self initWithURL:url detectContentType:YES mimeTypeHint:mimeTypeHint error:error];
}

- (instancetype)initWithURL:(NSURL *)url detectContentType:(BOOL)detectContentType mimeTypeHint:(NSString *)mimeTypeHint error:(NSError **)error
{
	NSParameterAssert(url != nil);

	SFBInputSource *inputSource = [SFBInputSource inputSourceForURL:url flags:0 error:error];
	if(!inputSource)
		return nil;
	return [self initWithInputSource:inputSource detectContentType:detectContentType mimeTypeHint:mimeTypeHint error:error];
}

- (instancetype)initWithInputSource:(SFBInputSource *)inputSource
{
	return [self initWithInputSource:inputSource detectContentType:YES mimeTypeHint:nil error:nil];
}

- (instancetype)initWithInputSource:(SFBInputSource *)inputSource error:(NSError **)error
{
	return [self initWithInputSource:inputSource detectContentType:YES mimeTypeHint:nil error:error];
}

- (instancetype)initWithInputSource:(SFBInputSource *)inputSource detectContentType:(BOOL)detectContentType error:(NSError **)error
{
	return [self initWithInputSource:inputSource detectContentType:detectContentType mimeTypeHint:nil error:error];
}

- (instancetype)initWithInputSource:(SFBInputSource *)inputSource mimeTypeHint:(NSString *)mimeTypeHint error:(NSError **)error
{
	return [self initWithInputSource:inputSource detectContentType:YES mimeTypeHint:mimeTypeHint error:error];
}

- (instancetype)initWithInputSource:(SFBInputSource *)inputSource detectContentType:(BOOL)detectContentType mimeTypeHint:(NSString *)mimeTypeHint error:(NSError **)error
{
	NSParameterAssert(inputSource != nil);

	NSString *lowercaseExtension = inputSource.url.pathExtension.lowercaseString;
	NSString *lowercaseMIMEType = mimeTypeHint.lowercaseString;

	if(detectContentType) {
		// If the input source can't be opened decoding is destined to fail; give up now
		if(!inputSource.isOpen && ![inputSource openReturningError:error])
			return nil;
		// Instead of failing for non-seekable inputs just skip content type detection
		if(!inputSource.supportsSeeking) {
			os_log_error(gSFBDSDDecoderLog, "Unable to detect content type for non-seekable input source %{public}@", inputSource);
			detectContentType = NO;
		}
	}

	int score = 10;
	Class subclass = nil;

	for(SFBDSDDecoderSubclassInfo *subclassInfo in _registeredSubclasses) {
		int currentScore = 0;
		Class klass = subclassInfo.klass;

		if(lowercaseMIMEType) {
			NSSet *supportedMIMETypes = [klass supportedMIMETypes];
			if([supportedMIMETypes containsObject:lowercaseMIMEType])
				currentScore += 40;
		}

		if(lowercaseExtension) {
			NSSet *supportedPathExtensions = [klass supportedPathExtensions];
			if([supportedPathExtensions containsObject:lowercaseExtension])
				currentScore += 20;
		}

		if(detectContentType) {
			SFBTernaryTruthValue formatSupported;
			if([klass testInputSource:inputSource formatIsSupported:&formatSupported error:error]) {
				switch(formatSupported) {
					case SFBTernaryTruthValueTrue:
						currentScore += 75;
						break;
					case SFBTernaryTruthValueFalse:
						break;
					case SFBTernaryTruthValueUnknown:
						currentScore += 10;
						break;
					default:
						os_log_fault(gSFBDSDDecoderLog, "Unknown SFBTernaryTruthValue %li", (long)formatSupported);
						break;
				}
			}
			else
				os_log_error(gSFBDSDDecoderLog, "Error testing %{public}@ format support for %{public}@", klass, inputSource);
		}

		if(currentScore > score) {
			score = currentScore;
			subclass = klass;
		}
	}

	if(subclass && (self = [[subclass alloc] init])) {
		_inputSource = inputSource;
		os_log_debug(gSFBDSDDecoderLog, "Created %{public}@ based on score of %i", self, score);
		return self;
	}

	if(error)
		*error = [NSError SFB_errorWithDomain:SFBDSDDecoderErrorDomain
										 code:SFBDSDDecoderErrorCodeInvalidFormat
				descriptionFormatStringForURL:NSLocalizedString(@"The type of the file “%@” could not be determined.", @"")
										  url:inputSource.url
								failureReason:NSLocalizedString(@"Unknown file type", @"")
						   recoverySuggestion:NSLocalizedString(@"The file's extension may be missing or may not match the file's type.", @"")];
	return nil;
}

- (instancetype)initWithURL:(NSURL *)url decoderName:(SFBDSDDecoderName)decoderName
{
	return [self initWithURL:url decoderName:decoderName error:nil];
}

- (instancetype)initWithURL:(NSURL *)url decoderName:(SFBDSDDecoderName)decoderName error:(NSError **)error
{
	NSParameterAssert(url != nil);

	SFBInputSource *inputSource = [SFBInputSource inputSourceForURL:url flags:0 error:error];
	if(!inputSource)
		return nil;
	return [self initWithInputSource:inputSource decoderName:decoderName error:error];
}

- (instancetype)initWithInputSource:(SFBInputSource *)inputSource decoderName:(SFBDSDDecoderName)decoderName
{
	return [self initWithInputSource:inputSource decoderName:decoderName error:nil];
}

- (instancetype)initWithInputSource:(SFBInputSource *)inputSource decoderName:(SFBDSDDecoderName)decoderName error:(NSError **)error
{
	NSParameterAssert(inputSource != nil);

	Class subclass = nil;
	for(SFBDSDDecoderSubclassInfo *subclassInfo in _registeredSubclasses) {
		SFBDSDDecoderName subclassDecoderName = [subclassInfo.klass decoderName];
		if(subclassDecoderName == decoderName) {
			subclass = subclassInfo.klass;
			break;
		}
	}

	if(!subclass) {
		os_log_error(gSFBDSDDecoderLog, "SFBDSDDecoder unknown decoder: %{public}@", decoderName);
		if(error)
			*error = [NSError errorWithDomain:SFBDSDDecoderErrorDomain
										 code:SFBDSDDecoderErrorCodeUnknownDecoder
									 userInfo:@{ NSURLErrorKey: inputSource.url }];
		return nil;
	}

	if((self = [[subclass alloc] init]))
		_inputSource = inputSource;

	return self;
}

- (void)dealloc
{
	[self closeReturningError:nil];
}

- (BOOL)openReturningError:(NSError **)error
{
	if(!_inputSource.isOpen)
		return [_inputSource openReturningError:error];
	return YES;
}

- (BOOL)closeReturningError:(NSError **)error
{
	_sourceFormat = nil;
	_processingFormat = nil;
	_properties = nil;
	if(_inputSource.isOpen)
		return [_inputSource closeReturningError:error];
	return YES;
}

- (BOOL)isOpen
{
	[self doesNotRecognizeSelector:_cmd];
	__builtin_unreachable();
}

- (BOOL)decodeIntoBuffer:(AVAudioBuffer *)buffer error:(NSError **)error
{
	NSParameterAssert(buffer != nil);
	NSParameterAssert([buffer isKindOfClass:[AVAudioCompressedBuffer class]]);
	return [self decodeIntoBuffer:(AVAudioCompressedBuffer *)buffer packetCount:((AVAudioCompressedBuffer *)buffer).packetCapacity error:error];
}

- (BOOL)decodeIntoBuffer:(AVAudioCompressedBuffer *)buffer packetCount:(AVAudioPacketCount)packetCount error:(NSError **)error
{
	[self doesNotRecognizeSelector:_cmd];
	__builtin_unreachable();
}

- (BOOL)supportsSeeking
{
	return _inputSource.supportsSeeking;
}

- (BOOL)seekToPacket:(AVAudioFramePosition)packet error:(NSError **)error
{
	[self doesNotRecognizeSelector:_cmd];
	__builtin_unreachable();
}

- (NSString *)description
{
	return [NSString stringWithFormat:@"<%@ %p: \"%@\">", [self class], self, [[NSFileManager defaultManager] displayNameAtPath:_inputSource.url.path]];
}

@end

@implementation SFBDSDDecoderSubclassInfo
@end

@implementation SFBDSDDecoder (SFBDSDDecoderSubclassRegistration)

+ (void)registerSubclass:(Class)subclass
{
	[self registerSubclass:subclass priority:0];
}

+ (void)registerSubclass:(Class)subclass priority:(int)priority
{
//	NSAssert([subclass isKindOfClass:[self class]], @"Unable to register class '%@' because it is not a subclass of SFBDSDDecoder", NSStringFromClass(subclass));

	static dispatch_once_t onceToken;
	dispatch_once(&onceToken, ^{
		_registeredSubclasses = [NSMutableArray array];
	});

	SFBDSDDecoderSubclassInfo *subclassInfo = [[SFBDSDDecoderSubclassInfo alloc] init];
	subclassInfo.klass = subclass;
	subclassInfo.priority = priority;

	[_registeredSubclasses addObject:subclassInfo];

	// N.B. `sortUsingComparator:` sorts in ascending order
	// To sort the array in descending order the comparator is reversed
	[_registeredSubclasses sortUsingComparator:^NSComparisonResult(id _Nonnull obj1, id _Nonnull obj2) {
		int a = ((SFBDSDDecoderSubclassInfo *)obj1).priority;
		int b = ((SFBDSDDecoderSubclassInfo *)obj2).priority;
		if(a > b)
			return NSOrderedAscending;
		else if(a < b)
			return NSOrderedDescending;
		else
			return NSOrderedSame;
	}];
}

@end
