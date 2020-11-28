/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

@import os.log;

#import "SFBAudioEncoder.h"
#import "SFBAudioEncoder+Internal.h"

#import "NSError+SFBURLPresentation.h"

// NSError domain for AudioEncoder and subclasses
NSErrorDomain const SFBAudioEncoderErrorDomain = @"org.sbooth.AudioEngine.AudioEncoder";

os_log_t gSFBAudioEncoderLog = NULL;

static void SFBCreateAudioEncoderLog(void) __attribute__ ((constructor));
static void SFBCreateAudioEncoderLog()
{
	static dispatch_once_t onceToken;
	dispatch_once(&onceToken, ^{
		gSFBAudioEncoderLog = os_log_create("org.sbooth.AudioEngine", "AudioEncoder");
	});
}

@interface SFBAudioEncoderSubclassInfo : NSObject
@property (nonatomic) Class klass;
@property (nonatomic) int priority;
@end

@implementation SFBAudioEncoder

@synthesize outputSource = _outputSource;
@synthesize sourceFormat = _sourceFormat;
@synthesize processingFormat = _processingFormat;
@synthesize outputFormat = _outputFormat;
@synthesize settings = _settings;
@synthesize estimatedFramesToEncode = _estimatedFramesToEncode;

@dynamic encodingIsLossless;
@dynamic framePosition;

static NSMutableArray *_registeredSubclasses = nil;

+ (NSSet *)supportedPathExtensions
{
	NSMutableSet *result = [NSMutableSet set];
	for(SFBAudioEncoderSubclassInfo *subclassInfo in _registeredSubclasses) {
		NSSet *supportedPathExtensions = [subclassInfo.klass supportedPathExtensions];
		[result unionSet:supportedPathExtensions];
	}
	return result;
}

+ (NSSet *)supportedMIMETypes
{
	NSMutableSet *result = [NSMutableSet set];
	for(SFBAudioEncoderSubclassInfo *subclassInfo in _registeredSubclasses) {
		NSSet *supportedMIMETypes = [subclassInfo.klass supportedMIMETypes];
		[result unionSet:supportedMIMETypes];
	}
	return result;
}

+ (SFBAudioEncoderName)encoderName
{
	[self doesNotRecognizeSelector:_cmd];
	__builtin_unreachable();
}

+ (BOOL)handlesPathsWithExtension:(NSString *)extension
{
	NSString *lowercaseExtension = extension.lowercaseString;
	for(SFBAudioEncoderSubclassInfo *subclassInfo in _registeredSubclasses) {
		NSSet *supportedPathExtensions = [subclassInfo.klass supportedPathExtensions];
		if([supportedPathExtensions containsObject:lowercaseExtension])
			return YES;
	}
	return NO;
}

+ (BOOL)handlesMIMEType:(NSString *)mimeType
{
	NSString *lowercaseMIMEType = mimeType.lowercaseString;
	for(SFBAudioEncoderSubclassInfo *subclassInfo in _registeredSubclasses) {
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

	// The MIME type takes precedence over the file extension
	if(mimeType) {
		Class subclass = [SFBAudioEncoder subclassForMIMEType:mimeType.lowercaseString];
		if(subclass && (self = [[subclass alloc] init])) {
			_outputSource = outputSource;
			return self;
		}
		os_log_debug(gSFBAudioEncoderLog, "SFBAudioEncoder unsupported MIME type: %{public}@", mimeType);
	}

	// If no MIME type was specified, use the extension-based resolvers

	// TODO: Some extensions (.oga for example) support multiple audio codecs (Vorbis, FLAC, Speex)
	// and if openEncoder is false the wrong encoder type may be returned, since the file isn't analyzed
	// until Open() is called

	NSString *pathExtension = outputSource.url.pathExtension;
	if(!pathExtension) {
		if(error)
			*error = [NSError SFB_errorWithDomain:SFBAudioEncoderErrorDomain
											 code:SFBAudioEncoderErrorCodeInvalidFormat
					descriptionFormatStringForURL:NSLocalizedString(@"The type of the file “%@” could not be determined.", @"")
											  url:outputSource.url
									failureReason:NSLocalizedString(@"Unknown file type", @"")
							   recoverySuggestion:NSLocalizedString(@"The file's extension may be missing or may not match the file's type.", @"")];
		return nil;
	}

	Class subclass = [SFBAudioEncoder subclassForPathExtension:pathExtension.lowercaseString];
	if(!subclass) {
		os_log_debug(gSFBAudioEncoderLog, "SFBAudioEncoder unsupported path extension: %{public}@", pathExtension);

		if(error)
			*error = [NSError SFB_errorWithDomain:SFBAudioEncoderErrorDomain
											 code:SFBAudioEncoderErrorCodeInvalidFormat
					descriptionFormatStringForURL:NSLocalizedString(@"The type of the file “%@” is not supported.", @"")
											  url:outputSource.url
									failureReason:NSLocalizedString(@"Unsupported file type", @"")
							   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];

		return nil;
	}

	if((self = [[subclass alloc] init]))
		_outputSource = outputSource;

	return self;
}

- (instancetype)initWithURL:(NSURL *)url encoderName:(SFBAudioEncoderName)encoderName
{
	return [self initWithURL:url encoderName:encoderName error:nil];
}

- (instancetype)initWithURL:(NSURL *)url encoderName:(SFBAudioEncoderName)encoderName error:(NSError **)error
{
	NSParameterAssert(url != nil);

	SFBOutputSource *outputSource = [SFBOutputSource outputSourceForURL:url error:error];
	if(!outputSource)
		return nil;
	return [self initWithOutputSource:outputSource encoderName:encoderName error:error];
}

- (instancetype)initWithOutputSource:(SFBOutputSource *)outputSource encoderName:(SFBAudioEncoderName)encoderName
{
	return [self initWithOutputSource:outputSource encoderName:encoderName error:nil];
}

- (instancetype)initWithOutputSource:(SFBOutputSource *)outputSource encoderName:(SFBAudioEncoderName)encoderName error:(NSError **)error
{
	NSParameterAssert(outputSource != nil);

	Class subclass = [SFBAudioEncoder subclassForEncoderName:encoderName];
	if(!subclass) {
		os_log_debug(gSFBAudioEncoderLog, "SFBAudioEncoder unsupported encoder: %{public}@", encoderName);

		if(error)
			*error = [NSError SFB_errorWithDomain:SFBAudioEncoderErrorDomain
											 code:SFBAudioEncoderErrorCodeInvalidFormat
					descriptionFormatStringForURL:NSLocalizedString(@"The type of the file “%@” is not supported.", @"")
											  url:outputSource.url
									failureReason:NSLocalizedString(@"Unsupported file type", @"")
							   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];

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
		os_log_error(gSFBAudioEncoderLog, "-setSourceFormat:error: called with non-PCM format: %{public}@", sourceFormat);
		if(error)
			*error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain code:SFBAudioEncoderErrorCodeInvalidFormat userInfo:nil];
		return NO;
	}

	AVAudioFormat *processingFormat = [self processingFormatForSourceFormat:sourceFormat];
	if(processingFormat == nil) {
		os_log_error(gSFBAudioEncoderLog, "-setSourceFormat:error: called with invalid format: %{public}@", sourceFormat);
		if(error)
			*error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain code:SFBAudioEncoderErrorCodeInvalidFormat userInfo:nil];
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
	if(![buffer isKindOfClass:[AVAudioPCMBuffer class]])
		return NO;
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

@implementation SFBAudioEncoderSubclassInfo
@end

@implementation SFBAudioEncoder (SFBAudioEncoderSubclassRegistration)

+ (void)registerSubclass:(Class)subclass
{
	[self registerSubclass:subclass priority:0];
}

+ (void)registerSubclass:(Class)subclass priority:(int)priority
{
	//	NSAssert([subclass isKindOfClass:[self class]], @"Unable to register class '%@' because it is not a subclass of SFBAudioEncoder", NSStringFromClass(subclass));

	static dispatch_once_t onceToken;
	dispatch_once(&onceToken, ^{
		_registeredSubclasses = [NSMutableArray array];
	});

	SFBAudioEncoderSubclassInfo *subclassInfo = [[SFBAudioEncoderSubclassInfo alloc] init];
	subclassInfo.klass = subclass;
	subclassInfo.priority = priority;

	[_registeredSubclasses addObject:subclassInfo];
	[_registeredSubclasses sortUsingComparator:^NSComparisonResult(id _Nonnull obj1, id _Nonnull obj2) {
		return ((SFBAudioEncoderSubclassInfo *)obj1).priority < ((SFBAudioEncoderSubclassInfo *)obj2).priority;
	}];
}

@end

@implementation SFBAudioEncoder (SFBAudioEncoderSubclassLookup)

+ (Class)subclassForURL:(NSURL *)url
{
	// TODO: Handle MIME types?
	if(url.isFileURL)
		return [self subclassForPathExtension:url.pathExtension.lowercaseString];

	return nil;
}

+ (Class)subclassForPathExtension:(NSString *)extension
{
	for(SFBAudioEncoderSubclassInfo *subclassInfo in _registeredSubclasses) {
		NSSet *supportedPathExtensions = [subclassInfo.klass supportedPathExtensions];
		if([supportedPathExtensions containsObject:extension])
			return subclassInfo.klass;
	}

	return nil;
}

+ (Class)subclassForMIMEType:(NSString *)mimeType
{
	for(SFBAudioEncoderSubclassInfo *subclassInfo in _registeredSubclasses) {
		NSSet *supportedMIMETypes = [subclassInfo.klass supportedMIMETypes];
		if([supportedMIMETypes containsObject:mimeType])
			return subclassInfo.klass;
	}

	return nil;
}

+ (Class)subclassForEncoderName:(SFBAudioEncoderName)encoderName
{
	for(SFBAudioEncoderSubclassInfo *subclassInfo in _registeredSubclasses) {
		SFBAudioEncoderName subclassEncoderName = [subclassInfo.klass encoderName];
		if([subclassEncoderName isEqualToString:encoderName])
			return subclassInfo.klass;
	}

	return nil;
}

@end
