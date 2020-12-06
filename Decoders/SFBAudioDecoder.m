/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

@import os.log;

#import "SFBAudioDecoder.h"
#import "SFBAudioDecoder+Internal.h"

#import "NSError+SFBURLPresentation.h"

const AVAudioFramePosition SFBUnknownFramePosition = SFB_UNKNOWN_FRAME_POSITION;
const AVAudioFramePosition SFBUnknownFrameLength = SFB_UNKNOWN_FRAME_LENGTH;

// NSError domain for AudioDecoder and subclasses
NSErrorDomain const SFBAudioDecoderErrorDomain = @"org.sbooth.AudioEngine.AudioDecoder";

os_log_t gSFBAudioDecoderLog = NULL;

static void SFBCreateAudioDecoderLog(void) __attribute__ ((constructor));
static void SFBCreateAudioDecoderLog()
{
	static dispatch_once_t onceToken;
	dispatch_once(&onceToken, ^{
		gSFBAudioDecoderLog = os_log_create("org.sbooth.AudioEngine", "AudioDecoder");
	});
}

@interface SFBAudioDecoderSubclassInfo : NSObject
@property (nonatomic) Class klass;
@property (nonatomic) int priority;
@end

@implementation SFBAudioDecoder

@synthesize inputSource = _inputSource;
@synthesize sourceFormat = _sourceFormat;
@synthesize processingFormat = _processingFormat;

@dynamic decodingIsLossless;
@dynamic framePosition;
@dynamic frameLength;

static NSMutableArray *_registeredSubclasses = nil;

+ (NSSet *)supportedPathExtensions
{
	NSMutableSet *result = [NSMutableSet set];
	for(SFBAudioDecoderSubclassInfo *subclassInfo in _registeredSubclasses) {
		NSSet *supportedPathExtensions = [subclassInfo.klass supportedPathExtensions];
		[result unionSet:supportedPathExtensions];
	}
	return result;
}

+ (NSSet *)supportedMIMETypes
{
	NSMutableSet *result = [NSMutableSet set];
	for(SFBAudioDecoderSubclassInfo *subclassInfo in _registeredSubclasses) {
		NSSet *supportedMIMETypes = [subclassInfo.klass supportedMIMETypes];
		[result unionSet:supportedMIMETypes];
	}
	return result;
}

+ (SFBAudioDecoderName)decoderName
{
	[self doesNotRecognizeSelector:_cmd];
	__builtin_unreachable();
}

+ (BOOL)handlesPathsWithExtension:(NSString *)extension
{
	NSString *lowercaseExtension = extension.lowercaseString;
	for(SFBAudioDecoderSubclassInfo *subclassInfo in _registeredSubclasses) {
		NSSet *supportedPathExtensions = [subclassInfo.klass supportedPathExtensions];
		if([supportedPathExtensions containsObject:lowercaseExtension])
			return YES;
	}
	return NO;
}

+ (BOOL)handlesMIMEType:(NSString *)mimeType
{
	NSString *lowercaseMIMEType = mimeType.lowercaseString;
	for(SFBAudioDecoderSubclassInfo *subclassInfo in _registeredSubclasses) {
		NSSet *supportedMIMETypes = [subclassInfo.klass supportedMIMETypes];
		if([supportedMIMETypes containsObject:lowercaseMIMEType])
			return YES;
	}
	return NO;
}

- (instancetype)initWithURL:(NSURL *)url
{
	return [self initWithURL:url mimeType:nil error:nil];
}

- (instancetype)initWithURL:(NSURL *)url error:(NSError **)error
{
	return [self initWithURL:url mimeType:nil error:error];
}

- (instancetype)initWithURL:(NSURL *)url mimeType:(NSString *)mimeType error:(NSError **)error
{
	NSParameterAssert(url != nil);

	SFBInputSource *inputSource = [SFBInputSource inputSourceForURL:url flags:0 error:error];
	if(!inputSource)
		return nil;
	return [self initWithInputSource:inputSource mimeType:mimeType error:error];
}

- (instancetype)initWithInputSource:(SFBInputSource *)inputSource
{
	return [self initWithInputSource:inputSource mimeType:nil error:nil];
}

- (instancetype)initWithInputSource:(SFBInputSource *)inputSource error:(NSError **)error
{
	return [self initWithInputSource:inputSource mimeType:nil error:error];
}

- (instancetype)initWithInputSource:(SFBInputSource *)inputSource mimeType:(NSString *)mimeType error:(NSError **)error
{
	NSParameterAssert(inputSource != nil);

	// The MIME type takes precedence over the file extension
	if(mimeType) {
		Class subclass = [SFBAudioDecoder subclassForMIMEType:mimeType.lowercaseString];
		if(subclass && (self = [[subclass alloc] init])) {
			_inputSource = inputSource;
			return self;
		}
		os_log_debug(gSFBAudioDecoderLog, "SFBAudioDecoder unsupported MIME type: %{public}@", mimeType);
	}

	// If no MIME type was specified, use the extension-based resolvers

	// TODO: Some extensions (.oga for example) support multiple audio codecs (Vorbis, FLAC, Speex)
	// and if openDecoder is false the wrong decoder type may be returned, since the file isn't analyzed
	// until Open() is called

	NSString *pathExtension = inputSource.url.pathExtension;
	if(!pathExtension) {
		if(error)
			*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
											 code:SFBAudioDecoderErrorCodeInvalidFormat
					descriptionFormatStringForURL:NSLocalizedString(@"The type of the file “%@” could not be determined.", @"")
											  url:inputSource.url
									failureReason:NSLocalizedString(@"Unknown file type", @"")
							   recoverySuggestion:NSLocalizedString(@"The file's extension may be missing or may not match the file's type.", @"")];
		return nil;
	}

	Class subclass = [SFBAudioDecoder subclassForPathExtension:pathExtension.lowercaseString];
	if(!subclass) {
		os_log_debug(gSFBAudioDecoderLog, "SFBAudioDecoder unsupported path extension: %{public}@", pathExtension);

		if(error)
			*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
											 code:SFBAudioDecoderErrorCodeInvalidFormat
					descriptionFormatStringForURL:NSLocalizedString(@"The type of the file “%@” is not supported.", @"")
											  url:inputSource.url
									failureReason:NSLocalizedString(@"Unsupported file type", @"")
							   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];

		return nil;
	}

	if((self = [[subclass alloc] init]))
		_inputSource = inputSource;

	return self;
}

- (instancetype)initWithURL:(NSURL *)url decoderName:(SFBAudioDecoderName)decoderName
{
	return [self initWithURL:url decoderName:decoderName error:nil];
}

- (instancetype)initWithURL:(NSURL *)url decoderName:(SFBAudioDecoderName)decoderName error:(NSError **)error
{
	NSParameterAssert(url != nil);

	SFBInputSource *inputSource = [SFBInputSource inputSourceForURL:url flags:0 error:error];
	if(!inputSource)
		return nil;
	return [self initWithInputSource:inputSource decoderName:decoderName error:error];
}

- (instancetype)initWithInputSource:(SFBInputSource *)inputSource decoderName:(SFBAudioDecoderName)decoderName
{
	return [self initWithInputSource:inputSource decoderName:decoderName error:nil];
}

- (instancetype)initWithInputSource:(SFBInputSource *)inputSource decoderName:(SFBAudioDecoderName)decoderName error:(NSError **)error
{
	NSParameterAssert(inputSource != nil);

	Class subclass = [SFBAudioDecoder subclassForDecoderName:decoderName];
	if(!subclass) {
		os_log_debug(gSFBAudioDecoderLog, "SFBAudioDecoder unsupported decoder: %{public}@", decoderName);
		if(error)
			*error = [NSError errorWithDomain:NSOSStatusErrorDomain code:paramErr userInfo:nil];
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
	NSParameterAssert([buffer isKindOfClass:[AVAudioPCMBuffer class]]);
	return [self decodeIntoBuffer:(AVAudioPCMBuffer *)buffer frameLength:((AVAudioPCMBuffer *)buffer).frameCapacity error:error];
}

- (BOOL)decodeIntoBuffer:(AVAudioPCMBuffer *)buffer frameLength:(AVAudioFrameCount)frameLength error:(NSError **)error
{
	[self doesNotRecognizeSelector:_cmd];
	__builtin_unreachable();
}

- (BOOL)supportsSeeking
{
	return _inputSource.supportsSeeking;
}

- (BOOL)seekToFrame:(AVAudioFramePosition)frame error:(NSError **)error
{
	[self doesNotRecognizeSelector:_cmd];
	__builtin_unreachable();
}

@end

@implementation SFBAudioDecoderSubclassInfo
@end

@implementation SFBAudioDecoder (SFBAudioDecoderSubclassRegistration)

+ (void)registerSubclass:(Class)subclass
{
	[self registerSubclass:subclass priority:0];
}

+ (void)registerSubclass:(Class)subclass priority:(int)priority
{
//	NSAssert([subclass isKindOfClass:[self class]], @"Unable to register class '%@' because it is not a subclass of SFBAudioDecoder", NSStringFromClass(subclass));

	static dispatch_once_t onceToken;
	dispatch_once(&onceToken, ^{
		_registeredSubclasses = [NSMutableArray array];
	});

	SFBAudioDecoderSubclassInfo *subclassInfo = [[SFBAudioDecoderSubclassInfo alloc] init];
	subclassInfo.klass = subclass;
	subclassInfo.priority = priority;

	[_registeredSubclasses addObject:subclassInfo];
	[_registeredSubclasses sortUsingComparator:^NSComparisonResult(id _Nonnull obj1, id _Nonnull obj2) {
		return ((SFBAudioDecoderSubclassInfo *)obj1).priority < ((SFBAudioDecoderSubclassInfo *)obj2).priority;
	}];
}

@end

@implementation SFBAudioDecoder (SFBAudioDecoderSubclassLookup)

+ (Class)subclassForURL:(NSURL *)url
{
	// TODO: Handle MIME types?
	if(url.isFileURL)
		return [self subclassForPathExtension:url.pathExtension.lowercaseString];

	return nil;
}

+ (Class)subclassForPathExtension:(NSString *)extension
{
	for(SFBAudioDecoderSubclassInfo *subclassInfo in _registeredSubclasses) {
		NSSet *supportedPathExtensions = [subclassInfo.klass supportedPathExtensions];
		if([supportedPathExtensions containsObject:extension])
			return subclassInfo.klass;
	}

	return nil;
}

+ (Class)subclassForMIMEType:(NSString *)mimeType
{
	for(SFBAudioDecoderSubclassInfo *subclassInfo in _registeredSubclasses) {
		NSSet *supportedMIMETypes = [subclassInfo.klass supportedMIMETypes];
		if([supportedMIMETypes containsObject:mimeType])
			return subclassInfo.klass;
	}

	return nil;
}

+ (Class)subclassForDecoderName:(SFBAudioDecoderName)decoderName
{
	for(SFBAudioDecoderSubclassInfo *subclassInfo in _registeredSubclasses) {
		SFBAudioDecoderName subclassDecoderName = [subclassInfo.klass decoderName];
		if([subclassDecoderName isEqualToString:decoderName])
			return subclassInfo.klass;
	}

	return nil;
}

@end
