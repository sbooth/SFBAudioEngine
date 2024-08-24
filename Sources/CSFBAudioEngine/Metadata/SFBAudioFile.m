//
// Copyright (c) 2020-2023 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

@import os.log;

#import "SFBAudioFile.h"
#import "SFBAudioFile+Internal.h"

#import "NSError+SFBURLPresentation.h"

// NSError domain for AudioFile and subclasses
NSErrorDomain const SFBAudioFileErrorDomain = @"org.sbooth.AudioEngine.AudioFile";

os_log_t gSFBAudioFileLog = NULL;

static void SFBCreateAudioFileLog(void) __attribute__ ((constructor));
static void SFBCreateAudioFileLog(void)
{
	static dispatch_once_t onceToken;
	dispatch_once(&onceToken, ^{
		gSFBAudioFileLog = os_log_create("org.sbooth.AudioEngine", "AudioFile");
	});
}

@interface SFBAudioFileSubclassInfo : NSObject
@property (nonatomic) Class klass;
@property (nonatomic) int priority;
@end

@implementation SFBAudioFile

static NSMutableArray *_registeredSubclasses = nil;

+ (void)load
{
	[NSError setUserInfoValueProviderForDomain:SFBAudioFileErrorDomain provider:^id(NSError *err, NSErrorUserInfoKey userInfoKey) {
		if(userInfoKey == NSLocalizedDescriptionKey) {
			switch(err.code) {
				case SFBAudioFileErrorCodeInternalError:
					return NSLocalizedString(@"An internal error occurred.", @"");
				case SFBAudioFileErrorCodeUnknownFormatName:
					return NSLocalizedString(@"The requested format is unavailable.", @"");
				case SFBAudioFileErrorCodeInputOutput:
					return NSLocalizedString(@"An input/output error occurred.", @"");
				case SFBAudioFileErrorCodeInvalidFormat:
					return NSLocalizedString(@"The file's format is invalid, unknown, or unsupported.", @"");
			}
		}
		return nil;
	}];
}

+ (NSSet *)supportedPathExtensions
{
	NSMutableSet *result = [NSMutableSet set];
	for(SFBAudioFileSubclassInfo *subclassInfo in _registeredSubclasses) {
		NSSet *supportedPathExtensions = [subclassInfo.klass supportedPathExtensions];
		[result unionSet:supportedPathExtensions];
	}

	return result;
}

+ (NSSet *)supportedMIMETypes
{
	NSMutableSet *result = [NSMutableSet set];
	for(SFBAudioFileSubclassInfo *subclassInfo in _registeredSubclasses) {
		NSSet *supportedMIMETypes = [subclassInfo.klass supportedMIMETypes];
		[result unionSet:supportedMIMETypes];
	}

	return result;
}

+ (SFBAudioFileFormatName)formatName
{
	[self doesNotRecognizeSelector:_cmd];
	__builtin_unreachable();
}

+ (BOOL)handlesPathsWithExtension:(NSString *)extension
{
	NSString *lowercaseExtension = extension.lowercaseString;
	for(SFBAudioFileSubclassInfo *subclassInfo in _registeredSubclasses) {
		NSSet *supportedPathExtensions = [subclassInfo.klass supportedPathExtensions];
		if([supportedPathExtensions containsObject:lowercaseExtension])
			return YES;
	}

	return NO;
}

+ (BOOL)handlesMIMEType:(NSString *)mimeType
{
	NSString *lowercaseMIMEType = mimeType.lowercaseString;
	for(SFBAudioFileSubclassInfo *subclassInfo in _registeredSubclasses) {
		NSSet *supportedMIMETypes = [subclassInfo.klass supportedMIMETypes];
		if([supportedMIMETypes containsObject:lowercaseMIMEType])
			return YES;
	}

	return NO;
}

+ (BOOL)copyMetadataFromURL:(NSURL *)sourceURL toURL:(NSURL *)destinationURL error:(NSError **)error
{
	NSParameterAssert(sourceURL != nil);
	NSParameterAssert(destinationURL != nil);

	SFBAudioFile *sourceAudioFile = [SFBAudioFile audioFileWithURL:sourceURL error:error];
	if(!sourceAudioFile)
		return NO;

	SFBAudioFile *destinationAudioFile = [SFBAudioFile audioFileWithURL:destinationURL error:error];
	if(!destinationAudioFile)
		return NO;

	[destinationAudioFile.metadata copyMetadataFrom:sourceAudioFile.metadata];
	[destinationAudioFile.metadata copyAttachedPicturesFrom:sourceAudioFile.metadata];

	return [destinationAudioFile writeMetadataReturningError:error];
}

+ (instancetype)audioFileWithURL:(NSURL *)url error:(NSError **)error
{
	NSParameterAssert(url != nil);
	SFBAudioFile *audioFile = [[SFBAudioFile alloc] initWithURL:url];
	if(![audioFile readPropertiesAndMetadataReturningError:error])
		return nil;
	return audioFile;
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

	// The MIME type takes precedence over the file extension
	if(mimeType) {
		Class subclass = [SFBAudioFile subclassForMIMEType:mimeType.lowercaseString];
		if(subclass && (self = [[subclass alloc] init])) {
			_url = url;
			return self;
		}
		os_log_debug(gSFBAudioFileLog, "SFBAudioFile unsupported MIME type: %{public}@", mimeType);
	}

	// If no MIME type was specified, use the extension-based resolvers

	// TODO: Some extensions (.oga for example) support multiple audio codecs (Vorbis, FLAC, Speex)

	NSString *pathExtension = url.pathExtension;
	if(!pathExtension) {
		if(error)
			*error = [NSError SFB_errorWithDomain:SFBAudioFileErrorDomain
											 code:SFBAudioFileErrorCodeInvalidFormat
					descriptionFormatStringForURL:NSLocalizedString(@"The type of the file “%@” could not be determined.", @"")
											  url:url
									failureReason:NSLocalizedString(@"Unknown file type", @"")
							   recoverySuggestion:NSLocalizedString(@"The file's extension may be missing or may not match the file's type.", @"")];
		return nil;
	}

	Class subclass = [SFBAudioFile subclassForPathExtension:pathExtension.lowercaseString];
	if(!subclass) {
		os_log_debug(gSFBAudioFileLog, "SFBAudioFile unsupported path extension: %{public}@", pathExtension);

		if(error)
			*error = [NSError SFB_errorWithDomain:SFBAudioFileErrorDomain
											 code:SFBAudioFileErrorCodeInvalidFormat
					descriptionFormatStringForURL:NSLocalizedString(@"The type of the file “%@” is not supported.", @"")
											  url:url
									failureReason:NSLocalizedString(@"Unsupported file type", @"")
							   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];

		return nil;
	}

	if((self = [[subclass alloc] init]))
		_url = url;

	return self;
}

- (instancetype)initWithURL:(NSURL *)url formatName:(SFBAudioFileFormatName)formatName
{
	return [self initWithURL:url formatName:formatName error:nil];
}

- (instancetype)initWithURL:(NSURL *)url formatName:(SFBAudioFileFormatName)formatName error:(NSError **)error
{
	NSParameterAssert(url != nil);

	Class subclass = [SFBAudioFile subclassForFormatName:formatName];
	if(!subclass) {
		os_log_debug(gSFBAudioFileLog, "SFBAudioFile unsupported format: %{public}@", formatName);
		if(error)
			*error = [NSError errorWithDomain:SFBAudioFileErrorDomain code:SFBAudioFileErrorCodeUnknownFormatName userInfo:nil];
		return nil;
	}

	if((self = [[subclass alloc] init]))
		_url = url;

	return self;
}

- (BOOL)readPropertiesAndMetadataReturningError:(NSError **)error
{
	[self doesNotRecognizeSelector:_cmd];
	__builtin_unreachable();
}

- (BOOL)writeMetadataReturningError:(NSError **)error
{
	[self doesNotRecognizeSelector:_cmd];
	__builtin_unreachable();
}

@end

@implementation SFBAudioFileSubclassInfo
@end

@implementation SFBAudioFile (SFBAudioFileSubclassRegistration)

+ (void)registerSubclass:(Class)subclass
{
	[self registerSubclass:subclass priority:0];
}

+ (void)registerSubclass:(Class)subclass priority:(int)priority
{
//	NSAssert([subclass isKindOfClass:[self class]], @"Unable to register class '%@' because it is not a subclass of SFBAudioFile", NSStringFromClass(subclass));

	static dispatch_once_t onceToken;
	dispatch_once(&onceToken, ^{
		_registeredSubclasses = [NSMutableArray array];
	});

	SFBAudioFileSubclassInfo *subclassInfo = [[SFBAudioFileSubclassInfo alloc] init];
	subclassInfo.klass = subclass;
	subclassInfo.priority = priority;

	[_registeredSubclasses addObject:subclassInfo];
	[_registeredSubclasses sortUsingComparator:^NSComparisonResult(id _Nonnull obj1, id _Nonnull obj2) {
		return ((SFBAudioFileSubclassInfo *)obj1).priority < ((SFBAudioFileSubclassInfo *)obj2).priority;
	}];
}

@end

@implementation SFBAudioFile (SFBAudioFileSubclassLookup)

+ (Class)subclassForURL:(NSURL *)url
{
	// TODO: Handle MIME types?
	if(url.isFileURL)
		return [self subclassForPathExtension:url.pathExtension.lowercaseString];

	return nil;
}

+ (Class)subclassForPathExtension:(NSString *)extension
{
	for(SFBAudioFileSubclassInfo *subclassInfo in _registeredSubclasses) {
		NSSet *supportedPathExtensions = [subclassInfo.klass supportedPathExtensions];
		if([supportedPathExtensions containsObject:extension])
			return subclassInfo.klass;
	}

	return nil;
}

+ (Class)subclassForMIMEType:(NSString *)mimeType
{
	for(SFBAudioFileSubclassInfo *subclassInfo in _registeredSubclasses) {
		NSSet *supportedMIMETypes = [subclassInfo.klass supportedMIMETypes];
		if([supportedMIMETypes containsObject:mimeType])
			return subclassInfo.klass;
	}

	return nil;
}

+ (Class)subclassForFormatName:(SFBAudioFileFormatName)formatName
{
	for(SFBAudioFileSubclassInfo *subclassInfo in _registeredSubclasses) {
		SFBAudioFileFormatName subclassFormatName = [subclassInfo.klass formatName];
		if(subclassFormatName == formatName)
			return subclassInfo.klass;
	}

	return nil;
}

@end
