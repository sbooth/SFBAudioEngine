//
// Copyright (c) 2020-2026 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

@import os.log;

#import "SFBPCMEncoder.h"
#import "SFBPCMEncoder+Internal.h"

#import "SFBErrorWithLocalizedDescription.h"
#import "SFBLocalizedNameForURL.h"

// NSError domain for PCMEncoder and subclasses
NSErrorDomain const SFBPCMEncoderErrorDomain = @"org.sbooth.AudioEngine.PCMEncoder";

os_log_t gSFBPCMEncoderLog = NULL;

static void SFBCreatePCMEncoderLog(void) __attribute__ ((constructor));
static void SFBCreatePCMEncoderLog(void)
{
	gSFBPCMEncoderLog = os_log_create("org.sbooth.AudioEngine", "PCMEncoder");
}

@interface SFBPCMEncoderSubclassInfo : NSObject
@property (nonatomic) Class klass;
@property (nonatomic) int priority;
@end

@implementation SFBPCMEncoder

@synthesize outputSource = _outputSource;
@synthesize sourceFormat = _sourceFormat;
@synthesize processingFormat = _processingFormat;
@synthesize outputFormat = _outputFormat;
@synthesize settings = _settings;
@synthesize estimatedFramesToEncode = _estimatedFramesToEncode;

@dynamic encodingIsLossless;
@dynamic framePosition;

static NSMutableArray *_registeredSubclasses = nil;

+ (void)load
{
	[NSError setUserInfoValueProviderForDomain:SFBPCMEncoderErrorDomain provider:^id(NSError *err, NSErrorUserInfoKey userInfoKey) {
		switch(err.code) {
			case SFBPCMEncoderErrorCodeUnknownEncoder:
				if([userInfoKey isEqualToString:NSLocalizedDescriptionKey])
					return NSLocalizedString(@"The requested encoder is unavailable.", @"");
				break;

			case SFBPCMEncoderErrorCodeInvalidFormat:
				if([userInfoKey isEqualToString:NSLocalizedDescriptionKey])
					return NSLocalizedString(@"The format is invalid, unknown, or unsupported.", @"");
				break;

			case SFBPCMEncoderErrorCodeInternalError:
				if([userInfoKey isEqualToString:NSLocalizedDescriptionKey])
					return NSLocalizedString(@"An internal encoder error occurred.", @"");
				break;
		}

		return nil;
	}];
}

+ (NSSet *)supportedPathExtensions
{
	NSMutableSet *result = [NSMutableSet set];
	for(SFBPCMEncoderSubclassInfo *subclassInfo in _registeredSubclasses) {
		NSSet *supportedPathExtensions = [subclassInfo.klass supportedPathExtensions];
		[result unionSet:supportedPathExtensions];
	}
	return result;
}

+ (NSSet *)supportedMIMETypes
{
	NSMutableSet *result = [NSMutableSet set];
	for(SFBPCMEncoderSubclassInfo *subclassInfo in _registeredSubclasses) {
		NSSet *supportedMIMETypes = [subclassInfo.klass supportedMIMETypes];
		[result unionSet:supportedMIMETypes];
	}
	return result;
}

+ (SFBPCMEncoderName)encoderName
{
	[self doesNotRecognizeSelector:_cmd];
	__builtin_unreachable();
}

+ (BOOL)handlesPathsWithExtension:(NSString *)extension
{
	NSString *lowercaseExtension = extension.lowercaseString;
	for(SFBPCMEncoderSubclassInfo *subclassInfo in _registeredSubclasses) {
		NSSet *supportedPathExtensions = [subclassInfo.klass supportedPathExtensions];
		if([supportedPathExtensions containsObject:lowercaseExtension])
			return YES;
	}
	return NO;
}

+ (BOOL)handlesMIMEType:(NSString *)mimeType
{
	NSString *lowercaseMIMEType = mimeType.lowercaseString;
	for(SFBPCMEncoderSubclassInfo *subclassInfo in _registeredSubclasses) {
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

	SFBOutputSource *outputSource = [SFBOutputSource outputSourceForURL:url error:error];
	if(!outputSource)
		return nil;
	return [self initWithOutputSource:outputSource mimeType:mimeType error:error];
}

- (instancetype)initWithOutputSource:(SFBOutputSource *)outputSource
{
	return [self initWithOutputSource:outputSource mimeType:nil error:nil];
}

- (instancetype)initWithOutputSource:(SFBOutputSource *)outputSource error:(NSError **)error
{
	return [self initWithOutputSource:outputSource mimeType:nil error:error];
}

- (instancetype)initWithOutputSource:(SFBOutputSource *)outputSource mimeType:(NSString *)mimeType error:(NSError **)error
{
	NSParameterAssert(outputSource != nil);

	NSString *lowercaseExtension = outputSource.url.pathExtension.lowercaseString;
	NSString *lowercaseMIMEType = mimeType.lowercaseString;

	int score = 10;
	Class subclass = nil;

	for(SFBPCMEncoderSubclassInfo *subclassInfo in _registeredSubclasses) {
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

		if(currentScore > score) {
			score = currentScore;
			subclass = klass;
		}
	}

	if(!subclass) {
		os_log_debug(gSFBPCMEncoderLog, "Unable to determine content type for %{public}@", outputSource);
		if(error)
			*error = SFBErrorWithLocalizedDescription(SFBPCMEncoderErrorDomain, SFBPCMEncoderErrorCodeInvalidFormat,
													  NSLocalizedString(@"The format of the file “%@” could not be determined.", @""),
													  @{ NSLocalizedRecoverySuggestionErrorKey: NSLocalizedString(@"The file's extension may be missing or may not match the file's type.", @""),
														 NSURLErrorKey: outputSource.url },
													  SFBLocalizedNameForURL(outputSource.url));
		return nil;
	}

	if((self = [[subclass alloc] init])) {
		_outputSource = outputSource;
		os_log_debug(gSFBPCMEncoderLog, "Created %{public}@ based on score of %i", self, score);
	}

	return self;
}

- (instancetype)initWithURL:(NSURL *)url encoderName:(SFBPCMEncoderName)encoderName
{
	return [self initWithURL:url encoderName:encoderName error:nil];
}

- (instancetype)initWithURL:(NSURL *)url encoderName:(SFBPCMEncoderName)encoderName error:(NSError **)error
{
	NSParameterAssert(url != nil);

	SFBOutputSource *outputSource = [SFBOutputSource outputSourceForURL:url error:error];
	if(!outputSource)
		return nil;
	return [self initWithOutputSource:outputSource encoderName:encoderName error:error];
}

- (instancetype)initWithOutputSource:(SFBOutputSource *)outputSource encoderName:(SFBPCMEncoderName)encoderName
{
	return [self initWithOutputSource:outputSource encoderName:encoderName error:nil];
}

- (instancetype)initWithOutputSource:(SFBOutputSource *)outputSource encoderName:(SFBPCMEncoderName)encoderName error:(NSError **)error
{
	NSParameterAssert(outputSource != nil);

	Class subclass = nil;
	for(SFBPCMEncoderSubclassInfo *subclassInfo in _registeredSubclasses) {
		SFBPCMEncoderName subclassEncoderName = [subclassInfo.klass encoderName];
		if(subclassEncoderName == encoderName) {
			subclass = subclassInfo.klass;
			break;
		}
	}

	if(!subclass) {
		os_log_debug(gSFBPCMEncoderLog, "SFBPCMEncoder unknown encoder: \"%{public}@\"", encoderName);
		if(error)
			*error = [NSError errorWithDomain:SFBPCMEncoderErrorDomain code:SFBPCMEncoderErrorCodeUnknownEncoder userInfo:nil];
		return nil;
	}

	if((self = [[subclass alloc] init]))
		_outputSource = outputSource;

	return self;
}

- (void)dealloc
{
	[self closeReturningError:nil];
}

- (AVAudioFormat *)processingFormatForSourceFormat:(AVAudioFormat *)sourceFormat
{
	[self doesNotRecognizeSelector:_cmd];
	__builtin_unreachable();
}

- (BOOL)setSourceFormat:(AVAudioFormat *)sourceFormat error:(NSError **)error
{
	NSParameterAssert(sourceFormat != nil);

	if(sourceFormat.streamDescription->mFormatID != kAudioFormatLinearPCM) {
		os_log_error(gSFBPCMEncoderLog, "-setSourceFormat:error: called with non-PCM format: %{public}@", sourceFormat);
		if(error)
			*error = [NSError errorWithDomain:SFBPCMEncoderErrorDomain code:SFBPCMEncoderErrorCodeInvalidFormat userInfo:nil];
		return NO;
	}

	AVAudioFormat *processingFormat = [self processingFormatForSourceFormat:sourceFormat];
	if(processingFormat == nil) {
		os_log_error(gSFBPCMEncoderLog, "-setSourceFormat:error: called with invalid format: %{public}@", sourceFormat);
		if(error)
			*error = [NSError errorWithDomain:SFBPCMEncoderErrorDomain code:SFBPCMEncoderErrorCodeInvalidFormat userInfo:nil];
		return NO;
	}

	_sourceFormat = sourceFormat;
	_processingFormat = processingFormat;

	return YES;
}

- (BOOL)openReturningError:(NSError **)error
{
	if(!_outputSource.isOpen)
		return [_outputSource openReturningError:error];
	return YES;
}

- (BOOL)closeReturningError:(NSError **)error
{
	_sourceFormat = nil;
	_processingFormat = nil;
	_outputFormat = nil;
	_settings = nil;
	if(_outputSource.isOpen)
		return [_outputSource closeReturningError:error];
	return YES;
}

- (BOOL)isOpen
{
	[self doesNotRecognizeSelector:_cmd];
	__builtin_unreachable();
}

- (BOOL)encodeFromBuffer:(nonnull AVAudioBuffer *)buffer error:(NSError **)error
{
	NSParameterAssert(buffer != nil);
	NSParameterAssert([buffer isKindOfClass:[AVAudioPCMBuffer class]]);
	return [self encodeFromBuffer:(AVAudioPCMBuffer *)buffer frameLength:((AVAudioPCMBuffer *)buffer).frameCapacity error:error];
}

- (BOOL)encodeFromBuffer:(AVAudioPCMBuffer *)buffer frameLength:(AVAudioFrameCount)frameLength error:(NSError **)error
{
	[self doesNotRecognizeSelector:_cmd];
	__builtin_unreachable();
}

- (BOOL)finishEncodingReturningError:(NSError **)error
{
	[self doesNotRecognizeSelector:_cmd];
	__builtin_unreachable();
}

@end

@implementation SFBPCMEncoderSubclassInfo
@end

@implementation SFBPCMEncoder (SFBPCMEncoderSubclassRegistration)

+ (void)registerSubclass:(Class)subclass
{
	[self registerSubclass:subclass priority:0];
}

+ (void)registerSubclass:(Class)subclass priority:(int)priority
{
//	NSAssert([subclass isKindOfClass:[self class]], @"Unable to register class '%@' because it is not a subclass of SFBPCMEncoder", NSStringFromClass(subclass));

	static dispatch_once_t onceToken;
	dispatch_once(&onceToken, ^{
		_registeredSubclasses = [NSMutableArray array];
	});

	SFBPCMEncoderSubclassInfo *subclassInfo = [[SFBPCMEncoderSubclassInfo alloc] init];
	subclassInfo.klass = subclass;
	subclassInfo.priority = priority;

	[_registeredSubclasses addObject:subclassInfo];

	// N.B. `sortUsingComparator:` sorts in ascending order
	// To sort the array in descending order the comparator is reversed
	[_registeredSubclasses sortUsingComparator:^NSComparisonResult(id _Nonnull obj1, id _Nonnull obj2) {
		int a = ((SFBPCMEncoderSubclassInfo *)obj1).priority;
		int b = ((SFBPCMEncoderSubclassInfo *)obj2).priority;
		if(a > b)
			return NSOrderedAscending;
		else if(a < b)
			return NSOrderedDescending;
		else
			return NSOrderedSame;
	}];
}

@end
