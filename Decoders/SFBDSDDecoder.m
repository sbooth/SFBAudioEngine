/*
 * Copyright (c) 2014 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

@import os.log;

#import "SFBDSDDecoder.h"
#import "SFBDSDDecoder+Internal.h"

#import "NSError+SFBURLPresentation.h"
#import "SFBAudioDecoder.h"

// Constants declared in SFBDSDDecoding.h but defined here
const NSInteger SFBPCMFramesPerDSDPacket = 8;
const NSInteger SFBBytesPerDSDPacketPerChannel = 1;

// NSError domain for DSDDecoder and subclasses
NSErrorDomain const SFBDSDDecoderErrorDomain = @"org.sbooth.AudioEngine.DSDDecoder";

os_log_t gSFBDSDDecoderLog = NULL;

static void SFBCreateDSDDecoderLog(void) __attribute__ ((constructor));
static void SFBCreateDSDDecoderLog()
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
@synthesize processingFormat = _processingFormat;
@synthesize sourceFormat = _sourceFormat;

@dynamic decodingIsLossless;
@dynamic packetPosition;
@dynamic packetCount;

static NSMutableArray *_registeredSubclasses = nil;

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
		Class subclass = [SFBDSDDecoder subclassForMIMEType:mimeType.lowercaseString];
		if(subclass && (self = [[subclass alloc] init])) {
			_inputSource = inputSource;
			return self;
		}
		os_log_debug(gSFBDSDDecoderLog, "SFBDSDDecoder unsupported MIME type: %{public}@", mimeType);
	}

	// If no MIME type was specified, use the extension-based resolvers

	// TODO: Some extensions (.oga for example) support multiple audio codecs (Vorbis, FLAC, Speex)
	// and if openDecoder is false the wrong decoder type may be returned, since the file isn't analyzed
	// until Open() is called

	NSString *pathExtension = inputSource.url.pathExtension;
	if(!pathExtension) {
		if(error)
			*error = [NSError SFB_errorWithDomain:SFBDSDDecoderErrorDomain
											 code:SFBDSDDecoderErrorCodeInputOutput
					descriptionFormatStringForURL:NSLocalizedString(@"The type of the file “%@” could not be determined.", @"")
											  url:inputSource.url
									failureReason:NSLocalizedString(@"Unknown file type", @"")
							   recoverySuggestion:NSLocalizedString(@"The file's extension may be missing or may not match the file's type.", @"")];
		return nil;
	}

	Class subclass = [SFBDSDDecoder subclassForPathExtension:pathExtension.lowercaseString];
	if(!subclass) {
		os_log_debug(gSFBDSDDecoderLog, "SFBDSDDecoder unsupported path extension: %{public}@", pathExtension);

		if(error)
			*error = [NSError SFB_errorWithDomain:SFBDSDDecoderErrorDomain
											 code:SFBDSDDecoderErrorCodeInputOutput
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
	if(![buffer isKindOfClass:[AVAudioCompressedBuffer class]])
		return NO;
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
	[_registeredSubclasses sortUsingComparator:^NSComparisonResult(id  _Nonnull obj1, id  _Nonnull obj2) {
		return ((SFBDSDDecoderSubclassInfo *)obj1).priority < ((SFBDSDDecoderSubclassInfo *)obj2).priority;
	}];
}

@end

@implementation SFBDSDDecoder (SFBDSDDecoderSubclassLookup)

+ (Class)subclassForURL:(NSURL *)url
{
	// TODO: Handle MIME types?
	if(url.isFileURL)
		return [self subclassForPathExtension:url.pathExtension.lowercaseString];

	return nil;
}

+ (Class)subclassForPathExtension:(NSString *)extension
{
	for(SFBDSDDecoderSubclassInfo *subclassInfo in _registeredSubclasses) {
		NSSet *supportedPathExtensions = [subclassInfo.klass supportedPathExtensions];
		if([supportedPathExtensions containsObject:extension])
			return subclassInfo.klass;
	}

	return nil;
}

+ (Class)subclassForMIMEType:(NSString *)mimeType
{
	for(SFBDSDDecoderSubclassInfo *subclassInfo in _registeredSubclasses) {
		NSSet *supportedMIMETypes = [subclassInfo.klass supportedMIMETypes];
		if([supportedMIMETypes containsObject:mimeType])
			return subclassInfo.klass;
	}

	return nil;
}

@end
