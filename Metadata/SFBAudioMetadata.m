/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

@import ObjectiveC;
@import OSLog;

#import "SFBAudioMetadata.h"
#import "SFBAudioMetadata+Internal.h"
#import "SFBChangeTrackingDictionary.h"

// NSError domain for AudioMetadata and subclasses
NSErrorDomain const SFBAudioMetadataErrorDomain = @"org.sbooth.AudioEngine.AudioMetadata";


// Key names for the metadata dictionary
NSString * const SFBAudioMetadataFormatNameKey					= @"Format Name";
NSString * const SFBAudioMetadataTotalFramesKey					= @"Total Frames";
NSString * const SFBAudioMetadataChannelsPerFrameKey			= @"Channels Per Frame";
NSString * const SFBAudioMetadataBitsPerChannelKey				= @"Bits Per Channel";
NSString * const SFBAudioMetadataSampleRateKey					= @"Sample Rate";
NSString * const SFBAudioMetadataDurationKey					= @"Duration";
NSString * const SFBAudioMetadataBitrateKey						= @"Bitrate";

NSString * const SFBAudioMetadataTitleKey						= @"Title";
NSString * const SFBAudioMetadataAlbumTitleKey					= @"Album Title";
NSString * const SFBAudioMetadataArtistKey						= @"Artist";
NSString * const SFBAudioMetadataAlbumArtistKey					= @"Album Artist";
NSString * const SFBAudioMetadataGenreKey						= @"Genre";
NSString * const SFBAudioMetadataComposerKey					= @"Composer";
NSString * const SFBAudioMetadataReleaseDateKey					= @"Date";
NSString * const SFBAudioMetadataCompilationKey					= @"Compilation";
NSString * const SFBAudioMetadataTrackNumberKey					= @"Track Number";
NSString * const SFBAudioMetadataTrackTotalKey					= @"Track Total";
NSString * const SFBAudioMetadataDiscNumberKey					= @"Disc Number";
NSString * const SFBAudioMetadataDiscTotalKey					= @"Disc Total";
NSString * const SFBAudioMetadataLyricsKey						= @"Lyrics";
NSString * const SFBAudioMetadataBPMKey							= @"BPM";
NSString * const SFBAudioMetadataRatingKey						= @"Rating";
NSString * const SFBAudioMetadataCommentKey						= @"Comment";
NSString * const SFBAudioMetadataISRCKey						= @"ISRC";
NSString * const SFBAudioMetadataMCNKey							= @"MCN";
NSString * const SFBAudioMetadataMusicBrainzReleaseIDKey		= @"MusicBrainz Release ID";
NSString * const SFBAudioMetadataMusicBrainzRecordingIDKey		= @"MusicBrainz Recording ID";

NSString * const SFBAudioMetadataTitleSortOrderKey				= @"Title Sort Order";
NSString * const SFBAudioMetadataAlbumTitleSortOrderKey			= @"Album Title Sort Order";
NSString * const SFBAudioMetadataArtistSortOrderKey				= @"Artist Sort Order";
NSString * const SFBAudioMetadataAlbumArtistSortOrderKey		= @"Album Artist Sort Order";
NSString * const SFBAudioMetadataComposerSortOrderKey			= @"Composer Sort Order";

NSString * const SFBAudioMetadataGroupingKey					= @"Grouping";

NSString * const SFBAudioMetadataAdditionalMetadataKey			= @"Additional Metadata";

NSString * const SFBAudioMetadataReplayGainReferenceLoudnessKey	= @"Replay Gain Reference Loudness";
NSString * const SFBAudioMetadataReplayGainTrackGainKey			= @"Replay Gain Track Gain";
NSString * const SFBAudioMetadataReplayGainTrackPeakKey			= @"Replay Gain Track Peak";
NSString * const SFBAudioMetadataReplayGainAlbumGainKey			= @"Replay Gain Album Gain";
NSString * const SFBAudioMetadataReplayGainAlbumPeakKey			= @"Replay Gain Album Peak";

NSString * const SFBAudioMetadataAttachedPicturesKey			= @"Attached Pictures";


@implementation SFBAudioMetadata

#pragma mark Supported File Formats

+ (NSArray *)supportedFileExtensions
{
	NSMutableArray *supportedFileExtensions = [NSMutableArray array];

	SEL sel = NSSelectorFromString(@"_supportedFileExtensions");
	for(SFBAudioMetadataSubclassInfo *subclassInfo in self.registeredSubclasses) {
		if(![subclassInfo.subclass respondsToSelector:sel]) {
			os_log_info(OS_LOG_DEFAULT, "%{public}@ is a malformed SFBAudioMetadata subclass: %{public}@ is required but not implemented", NSStringFromClass(subclassInfo.subclass), NSStringFromSelector(sel));
			continue;
		}
		id (*imp)(Class, SEL) = (id (*)(Class, SEL))objc_msgSend;
		NSArray *decoderFileExtensions = imp(subclassInfo.subclass, sel);
		[supportedFileExtensions addObjectsFromArray:decoderFileExtensions];
	}

	return supportedFileExtensions;
}

+ (NSArray *)supportedMIMETypes
{
	NSMutableArray *supportedMIMETypes = [NSMutableArray array];

	SEL sel = NSSelectorFromString(@"_supportedMIMETypes");
	for(SFBAudioMetadataSubclassInfo *subclassInfo in self.registeredSubclasses) {
		if(![subclassInfo.subclass respondsToSelector:sel]) {
			os_log_info(OS_LOG_DEFAULT, "%{public}@ is a malformed SFBAudioMetadata subclass: %{public}@ is required but not implemented", NSStringFromClass(subclassInfo.subclass), NSStringFromSelector(sel));
			continue;
		}
		id (*imp)(Class, SEL) = (id (*)(Class, SEL))objc_msgSend;
		NSArray *decoderMIMETypes = imp(subclassInfo.subclass, sel);
		[supportedMIMETypes addObjectsFromArray:decoderMIMETypes];
	}

	return supportedMIMETypes;
}

+ (BOOL)handlesFilesWithExtension:(NSString *)extension
{
	NSString *lowercaseExtension = [extension lowercaseString];
	SEL sel = NSSelectorFromString(@"_supportedFileExtensions");
	for(SFBAudioMetadataSubclassInfo *subclassInfo in self.registeredSubclasses) {
		if(![subclassInfo.subclass respondsToSelector:sel]) {
			os_log_info(OS_LOG_DEFAULT, "%{public}@ is a malformed SFBAudioMetadata subclass: %{public}@ is required but not implemented", NSStringFromClass(subclassInfo.subclass), NSStringFromSelector(sel));
			continue;
		}
		id (*imp)(Class, SEL) = (id (*)(Class, SEL))objc_msgSend;
		NSArray *supportedFileExtensions = imp(subclassInfo.subclass, sel);
		if([supportedFileExtensions containsObject:lowercaseExtension])
			return YES;
	}
	return NO;
}

+ (BOOL)handlesMIMEType:(NSString *)mimeType
{
	NSString *lowercaseMIMEType = [mimeType lowercaseString];
	SEL sel = NSSelectorFromString(@"_supportedMIMETypes");
	for(SFBAudioMetadataSubclassInfo *subclassInfo in self.registeredSubclasses) {
		if(![subclassInfo.subclass respondsToSelector:sel]) {
			os_log_info(OS_LOG_DEFAULT, "%{public}@ is a malformed SFBAudioMetadata subclass: %{public}@ is required but not implemented", NSStringFromClass(subclassInfo.subclass), NSStringFromSelector(sel));
			continue;
		}
		id (*imp)(Class, SEL) = (id (*)(Class, SEL))objc_msgSend;
		NSArray *supportedMIMETypes = imp(subclassInfo.subclass, sel);
		if([supportedMIMETypes containsObject:lowercaseMIMEType])
			return YES;
	}
	return NO;
}

#pragma mark Creation

+ (instancetype)metadataForURL:(NSURL *)url error:(NSError **)error
{
	NSParameterAssert(url != nil);

	// If this is a file URL, use the extension-based resolvers
	NSString *scheme = url.scheme;

	// If there is no scheme the URL is invalid
	if(!scheme) {
		if(error)
			*error = [NSError errorWithDomain:NSPOSIXErrorDomain code:EINVAL userInfo:nil];
		return nil;
	}

	if([scheme caseInsensitiveCompare:@"file"] == NSOrderedSame) {
		// Verify the file exists
		if([url checkResourceIsReachableAndReturnError:error]) {
			NSString *extension = url.pathExtension.lowercaseString;
			if(extension) {
				// Some extensions (.oga for example) support multiple audio codecs (Vorbis, FLAC, Speex)

				SEL sel = NSSelectorFromString(@"_supportedFileExtensions");
				for(SFBAudioMetadataSubclassInfo *subclassInfo in self.registeredSubclasses) {
					if(![subclassInfo.subclass respondsToSelector:sel]) {
						os_log_info(OS_LOG_DEFAULT, "%{public}@ is a malformed SFBAudioMetadata subclass: %{public}@ is required but not implemented", NSStringFromClass(subclassInfo.subclass), NSStringFromSelector(sel));
						continue;
					}
					id (*imp)(Class, SEL) = (id (*)(Class, SEL))objc_msgSend;
					NSArray *supportedFileExtensions = imp(subclassInfo.subclass, sel);
					if([supportedFileExtensions containsObject:extension]) {
						SFBAudioMetadata *instance = [[subclassInfo.subclass alloc] init];
						instance.url = url;
						if([instance readMetadata:error])
							return instance;
					}
				}
			}
		}
		else {
			os_log_debug(OS_LOG_DEFAULT, "The requested URL is not reachable: %{public}@", *error);
			return nil;
		}
	}

	return nil;
}

+ (instancetype)metadataFromDictionaryRepresentation:(NSDictionary *)dictionary
{
	SFBAudioMetadata *metadata = [[SFBAudioMetadata alloc] init];

	metadata.title = dictionary[SFBAudioMetadataTitleKey];
	metadata.albumTitle = dictionary[SFBAudioMetadataAlbumTitleKey];
	metadata.artist = dictionary[SFBAudioMetadataArtistKey];
	metadata.albumArtist = dictionary[SFBAudioMetadataAlbumArtistKey];
	metadata.genre = dictionary[SFBAudioMetadataGenreKey];
	metadata.composer = dictionary[SFBAudioMetadataComposerKey];
	metadata.releaseDate = dictionary[SFBAudioMetadataReleaseDateKey];
	metadata.compilation = dictionary[SFBAudioMetadataCompilationKey];
	metadata.trackNumber = dictionary[SFBAudioMetadataTrackNumberKey];
	metadata.trackTotal = dictionary[SFBAudioMetadataTrackTotalKey];
	metadata.discNumber = dictionary[SFBAudioMetadataDiscNumberKey];
	metadata.discTotal = dictionary[SFBAudioMetadataDiscTotalKey];
	metadata.lyrics = dictionary[SFBAudioMetadataLyricsKey];
	metadata.bpm = dictionary[SFBAudioMetadataBPMKey];
	metadata.rating = dictionary[SFBAudioMetadataRatingKey];
	metadata.comment = dictionary[SFBAudioMetadataCommentKey];
	metadata.isrc = dictionary[SFBAudioMetadataISRCKey];
	metadata.mcn = dictionary[SFBAudioMetadataMCNKey];
	metadata.musicBrainzReleaseID = dictionary[SFBAudioMetadataMusicBrainzReleaseIDKey];
	metadata.musicBrainzRecordingID = dictionary[SFBAudioMetadataMusicBrainzRecordingIDKey];

	metadata.titleSortOrder = dictionary[SFBAudioMetadataTitleSortOrderKey];
	metadata.albumTitleSortOrder = dictionary[SFBAudioMetadataAlbumTitleSortOrderKey];
	metadata.artistSortOrder = dictionary[SFBAudioMetadataArtistSortOrderKey];
	metadata.albumArtistSortOrder = dictionary[SFBAudioMetadataAlbumArtistSortOrderKey];
	metadata.composerSortOrder = dictionary[SFBAudioMetadataComposerSortOrderKey];

	metadata.grouping = dictionary[SFBAudioMetadataGroupingKey];

	metadata.additionalMetadata = dictionary[SFBAudioMetadataAdditionalMetadataKey];

	metadata.replayGainReferenceLoudness = dictionary[SFBAudioMetadataReplayGainReferenceLoudnessKey];
	metadata.replayGainTrackGain = dictionary[SFBAudioMetadataReplayGainTrackGainKey];
	metadata.replayGainTrackPeak = dictionary[SFBAudioMetadataReplayGainTrackPeakKey];
	metadata.replayGainAlbumGain = dictionary[SFBAudioMetadataReplayGainAlbumGainKey];
	metadata.replayGainAlbumPeak = dictionary[SFBAudioMetadataReplayGainAlbumPeakKey];

	NSArray *pictures = dictionary[SFBAudioMetadataAttachedPicturesKey];
	for(NSDictionary *picture in pictures)
		[metadata attachPicture:[SFBAttachedPicture attachedPictureFromDictionaryRepresentation:picture]];

	[metadata mergeChanges];

	return metadata;
}

- (instancetype)init
{
	if((self = [super init])) {
		_metadata = [[SFBChangeTrackingDictionary alloc] init];
		_pictures = [[SFBChangeTrackingSet alloc] init];
	}
	return self;
}

#pragma mark Reading and Writing

- (BOOL)readMetadata:(NSError **)error
{
	[_metadata reset];
	[_pictures reset];
	BOOL result = [self _readMetadata:error];
	if(result)
	   [self mergeChanges];
	return result;
}

- (BOOL)writeMetadata:(NSError **)error
{
	BOOL result = [self _writeMetadata:error];
	if(result)
	   [self mergeChanges];
	return result;
}

#pragma mark External Representations

- (NSDictionary *)dictionaryRepresentation
{
	NSMutableDictionary *dictionary = [[_metadata mergedValues] mutableCopy];
	NSMutableArray *pictures = [NSMutableArray arrayWithCapacity:_pictures.count];
	for(SFBAttachedPicture *picture in _pictures.mergedObjects)
		[pictures addObject:picture.dictionaryRepresentation];
	dictionary[SFBAudioMetadataAttachedPicturesKey] = pictures;
	return dictionary;
}

#pragma mark Change Management

- (BOOL)hasChanges
{
	return _metadata.hasChanges || _pictures.hasChanges;
}

- (void)mergeChanges
{
	[_metadata mergeChanges];
	[_pictures mergeChanges];
}

- (void)revertChanges
{
	[_metadata revertChanges];
	[_pictures revertChanges];
}

#pragma mark Metadata Manipulation

- (void) copyMetadataOfKind:(SFBAudioMetadataKind)kind from:(SFBAudioMetadata *)metadata
{
	if(kind & SFBAudioMetadataKindBasic) {
		self.title = metadata.title;
		self.albumTitle = metadata.albumTitle;
		self.artist = metadata.artist;
		self.albumArtist = metadata.albumArtist;
		self.genre = metadata.genre;
		self.composer = metadata.composer;
		self.releaseDate = metadata.releaseDate;
		self.compilation = metadata.compilation;
		self.trackNumber = metadata.trackNumber;
		self.trackTotal = metadata.trackTotal;
		self.discNumber = metadata.discNumber;
		self.discTotal = metadata.discTotal;
		self.lyrics = metadata.lyrics;
		self.bpm = metadata.bpm;
		self.rating = metadata.rating;
		self.comment = metadata.comment;
		self.isrc = metadata.isrc;
		self.mcn = metadata.mcn;
		self.musicBrainzReleaseID = metadata.musicBrainzReleaseID;
		self.musicBrainzRecordingID = metadata.musicBrainzRecordingID;
	}

	if(kind & SFBAudioMetadataKindSorting) {
		self.titleSortOrder = metadata.titleSortOrder;
		self.albumTitleSortOrder = metadata.albumTitleSortOrder;
		self.artistSortOrder = metadata.artistSortOrder;
		self.albumArtistSortOrder = metadata.albumArtistSortOrder;
		self.composerSortOrder = metadata.composerSortOrder;
	}

	if(kind & SFBAudioMetadataKindGrouping)
		self.grouping = metadata.grouping;

	if(kind & SFBAudioMetadataKindAdditional)
		self.additionalMetadata = metadata.additionalMetadata;

	if(kind & SFBAudioMetadataKindReplayGain) {
		self.replayGainReferenceLoudness = metadata.replayGainReferenceLoudness;
		self.replayGainTrackGain = metadata.replayGainTrackGain;
		self.replayGainTrackPeak = metadata.replayGainTrackPeak;
		self.replayGainAlbumGain = metadata.replayGainAlbumGain;
		self.replayGainAlbumPeak = metadata.replayGainAlbumPeak;
	}
}

- (void) copyMetadataFrom:(SFBAudioMetadata *)metadata
{
	[self copyMetadataOfKind:(SFBAudioMetadataKindBasic | SFBAudioMetadataKindSorting | SFBAudioMetadataKindGrouping | SFBAudioMetadataKindAdditional | SFBAudioMetadataKindReplayGain) from:metadata];
}

- (void) removeMetadataOfKind:(SFBAudioMetadataKind)kind
{
	if(kind & SFBAudioMetadataKindBasic) {
		self.title = nil;
		self.albumTitle = nil;
		self.artist = nil;
		self.albumArtist = nil;
		self.genre = nil;
		self.composer = nil;
		self.releaseDate = nil;
		self.compilation = nil;
		self.trackNumber = nil;
		self.trackTotal = nil;
		self.discNumber = nil;
		self.discTotal = nil;
		self.lyrics = nil;
		self.bpm = nil;
		self.rating = nil;
		self.comment = nil;
		self.isrc = nil;
		self.mcn = nil;
		self.musicBrainzReleaseID = nil;
		self.musicBrainzRecordingID = nil;
	}

	if(kind & SFBAudioMetadataKindSorting) {
		self.titleSortOrder = nil;
		self.albumTitleSortOrder = nil;
		self.artistSortOrder = nil;
		self.albumArtistSortOrder = nil;
		self.composerSortOrder = nil;
	}

	if(kind & SFBAudioMetadataKindGrouping)
		self.grouping = nil;

	if(kind & SFBAudioMetadataKindAdditional)
		self.additionalMetadata = nil;

	if(kind & SFBAudioMetadataKindReplayGain) {
		self.replayGainReferenceLoudness = nil;
		self.replayGainTrackGain = nil;
		self.replayGainTrackPeak = nil;
		self.replayGainAlbumGain = nil;
		self.replayGainAlbumPeak = nil;
	}
}

- (void) removeAllMetadata
{
	[self removeMetadataOfKind:(SFBAudioMetadataKindBasic | SFBAudioMetadataKindSorting | SFBAudioMetadataKindGrouping | SFBAudioMetadataKindAdditional | SFBAudioMetadataKindReplayGain)];
}

#pragma mark Audio Properties

- (NSString *)formatName
{
	return [_metadata objectForKey:SFBAudioMetadataFormatNameKey];
}

- (NSNumber *)totalFrames
{
	return [_metadata objectForKey:SFBAudioMetadataTotalFramesKey];
}

- (NSNumber *)channelsPerFrame
{
	return [_metadata objectForKey:SFBAudioMetadataChannelsPerFrameKey];
}

- (NSNumber *)bitsPerChannel
{
	return [_metadata objectForKey:SFBAudioMetadataBitsPerChannelKey];
}

- (NSNumber *)sampleRate
{
	return [_metadata objectForKey:SFBAudioMetadataSampleRateKey];
}

- (NSNumber *)duration
{
	return [_metadata objectForKey:SFBAudioMetadataDurationKey];
}

- (NSNumber *)bitrate
{
	return [_metadata objectForKey:SFBAudioMetadataBitrateKey];
}

#pragma mark Basic Metadata

- (NSString *)title
{
	return [_metadata objectForKey:SFBAudioMetadataTitleKey];
}

- (void)setTitle:(NSString *)title
{
	[_metadata setObject:[title copy] forKey:SFBAudioMetadataTitleKey];
}

- (NSString *)albumTitle
{
	return [_metadata objectForKey:SFBAudioMetadataAlbumTitleKey];
}

- (void)setAlbumTitle:(NSString *)albumTitle
{
	[_metadata setObject:[albumTitle copy] forKey:SFBAudioMetadataAlbumTitleKey];
}

- (NSString *)artist
{
	return [_metadata objectForKey:SFBAudioMetadataArtistKey];
}

- (void)setArtist:(NSString *)artist
{
	[_metadata setObject:[artist copy] forKey:SFBAudioMetadataArtistKey];
}

- (NSString *)albumArtist
{
	return [_metadata objectForKey:SFBAudioMetadataAlbumArtistKey];
}

- (void)setAlbumArtist:(NSString *)albumArtist
{
	[_metadata setObject:[albumArtist copy] forKey:SFBAudioMetadataAlbumArtistKey];
}

- (NSString *)genre
{
	return [_metadata objectForKey:SFBAudioMetadataGenreKey];
}

- (void)setGenre:(NSString *)genre
{
	[_metadata setObject:[genre copy] forKey:SFBAudioMetadataGenreKey];
}

- (NSString *)composer
{
	return [_metadata objectForKey:SFBAudioMetadataComposerKey];
}

- (void)setComposer:(NSString *)composer
{
	[_metadata setObject:[composer copy] forKey:SFBAudioMetadataComposerKey];
}

- (NSString *)releaseDate
{
	return [_metadata objectForKey:SFBAudioMetadataReleaseDateKey];
}

- (void)setReleaseDate:(NSString *)releaseDate
{
	[_metadata setObject:[releaseDate copy] forKey:SFBAudioMetadataReleaseDateKey];
}

- (NSNumber *)compilation
{
	return [_metadata objectForKey:SFBAudioMetadataCompilationKey];
}

- (void)setCompilation:(NSNumber *)compilation
{
	[_metadata setObject:compilation forKey:SFBAudioMetadataCompilationKey];
}

- (NSNumber *)trackNumber
{
	return [_metadata objectForKey:SFBAudioMetadataTrackNumberKey];
}

- (void)setTrackNumber:(NSNumber *)trackNumber
{
	[_metadata setObject:trackNumber forKey:SFBAudioMetadataTrackNumberKey];
}

- (NSNumber *)trackTotal
{
	return [_metadata objectForKey:SFBAudioMetadataTrackTotalKey];
}

- (void)setTrackTotal:(NSNumber *)trackTotal
{
	[_metadata setObject:trackTotal forKey:SFBAudioMetadataTrackTotalKey];
}

- (NSNumber *)discNumber
{
	return [_metadata objectForKey:SFBAudioMetadataDiscNumberKey];
}

- (void)setDiscNumber:(NSNumber *)discNumber
{
	[_metadata setObject:discNumber forKey:SFBAudioMetadataDiscNumberKey];
}

- (NSNumber *)discTotal
{
	return [_metadata objectForKey:SFBAudioMetadataDiscTotalKey];
}

- (void)setDiscTotal:(NSNumber *)discTotal
{
	[_metadata setObject:discTotal forKey:SFBAudioMetadataDiscTotalKey];
}

- (NSString *)lyrics
{
	return [_metadata objectForKey:SFBAudioMetadataLyricsKey];
}

- (void)setLyrics:(NSString *)lyrics
{
	[_metadata setObject:[lyrics copy] forKey:SFBAudioMetadataLyricsKey];
}

- (NSNumber *)bpm
{
	return [_metadata objectForKey:SFBAudioMetadataBPMKey];
}

- (void)setBpm:(NSNumber *)bpm
{
	[_metadata setObject:bpm forKey:SFBAudioMetadataBPMKey];
}

- (NSNumber *)rating
{
	return [_metadata objectForKey:SFBAudioMetadataRatingKey];
}

- (void)setRating:(NSNumber *)rating
{
	[_metadata setObject:rating forKey:SFBAudioMetadataRatingKey];
}

- (NSString *)comment
{
	return [_metadata objectForKey:SFBAudioMetadataCommentKey];
}

- (void)setComment:(NSString *)comment
{
	[_metadata setObject:[comment copy] forKey:SFBAudioMetadataCommentKey];
}

- (NSString *)isrc
{
	return [_metadata objectForKey:SFBAudioMetadataISRCKey];
}

- (void)setIsrc:(NSString *)isrc
{
	[_metadata setObject:[isrc copy] forKey:SFBAudioMetadataISRCKey];
}

- (NSString *)mcn
{
	return [_metadata objectForKey:SFBAudioMetadataMCNKey];
}

- (void)setMcn:(NSString *)mcn
{
	[_metadata setObject:[mcn copy] forKey:SFBAudioMetadataMCNKey];
}

- (NSString *)musicBrainzReleaseID
{
	return [_metadata objectForKey:SFBAudioMetadataMusicBrainzReleaseIDKey];
}

- (void)setMusicBrainzReleaseID:(NSString *)musicBrainzReleaseID
{
	[_metadata setObject:[musicBrainzReleaseID copy] forKey:SFBAudioMetadataMusicBrainzReleaseIDKey];
}

- (NSString *)musicBrainzRecordingID
{
	return [_metadata objectForKey:SFBAudioMetadataMusicBrainzRecordingIDKey];
}

- (void)setMusicBrainzRecordingID:(NSString *)musicBrainzRecordingID
{
	[_metadata setObject:[musicBrainzRecordingID copy] forKey:SFBAudioMetadataMusicBrainzRecordingIDKey];
}

#pragma mark Sorting Metadata

- (NSString *)titleSortOrder
{
	return [_metadata objectForKey:SFBAudioMetadataTitleSortOrderKey];
}
- (void)setTitleSortOrder:(NSString *)titleSortOrder
{
	[_metadata setObject:[titleSortOrder copy] forKey:SFBAudioMetadataTitleSortOrderKey];
}

- (NSString *)albumTitleSortOrder
{
	return [_metadata objectForKey:SFBAudioMetadataAlbumTitleSortOrderKey];
}

- (void)setAlbumTitleSortOrder:(NSString *)albumTitleSortOrder
{
	[_metadata setObject:[albumTitleSortOrder copy] forKey:SFBAudioMetadataAlbumTitleSortOrderKey];
}

- (NSString *)artistSortOrder
{
	return [_metadata objectForKey:SFBAudioMetadataArtistSortOrderKey];
}

- (void)setArtistSortOrder:(NSString *)artistSortOrder
{
	[_metadata setObject:[artistSortOrder copy] forKey:SFBAudioMetadataArtistSortOrderKey];
}

- (NSString *)albumArtistSortOrder
{
	return [_metadata objectForKey:SFBAudioMetadataAlbumArtistSortOrderKey];
}

- (void)setAlbumArtistSortOrder:(NSString *)albumArtistSortOrder
{
	[_metadata setObject:[albumArtistSortOrder copy] forKey:SFBAudioMetadataAlbumArtistSortOrderKey];
}

- (NSString *)composerSortOrder
{
	return [_metadata objectForKey:SFBAudioMetadataComposerSortOrderKey];
}

- (void)setComposerSortOrder:(NSString *)composerSortOrder
{
	[_metadata setObject:[composerSortOrder copy] forKey:SFBAudioMetadataComposerSortOrderKey];
}

#pragma mark Grouping Metadata

- (NSString *)grouping
{
	return [_metadata objectForKey:SFBAudioMetadataGroupingKey];
}

- (void)setGrouping:(NSString *)grouping
{
	[_metadata setObject:[grouping copy] forKey:SFBAudioMetadataGroupingKey];
}

#pragma mark Additional Metadata

- (NSDictionary *)additionalMetadata
{
	return [_metadata objectForKey:SFBAudioMetadataAdditionalMetadataKey];
}

- (void)setAdditionalMetadata:(NSDictionary *)additionalMetadata
{
	[_metadata setObject:[additionalMetadata copy] forKey:SFBAudioMetadataAdditionalMetadataKey];
}

#pragma mark Replay Gain Metadata

- (NSNumber *)replayGainReferenceLoudness
{
	return [_metadata objectForKey:SFBAudioMetadataReplayGainReferenceLoudnessKey];
}

- (void)setReplayGainReferenceLoudness:(NSNumber *)replayGainReferenceLoudness
{
	[_metadata setObject:replayGainReferenceLoudness forKey:SFBAudioMetadataReplayGainReferenceLoudnessKey];
}

- (NSNumber *)replayGainTrackGain
{
	return [_metadata objectForKey:SFBAudioMetadataReplayGainTrackGainKey];
}

- (void)setReplayGainTrackGain:(NSNumber *)replayGainTrackGain
{
	[_metadata setObject:replayGainTrackGain forKey:SFBAudioMetadataReplayGainTrackGainKey];
}

- (NSNumber *)replayGainTrackPeak
{
	return [_metadata objectForKey:SFBAudioMetadataReplayGainTrackPeakKey];
}

- (void)setReplayGainTrackPeak:(NSNumber *)replayGainTrackPeak
{
	[_metadata setObject:replayGainTrackPeak forKey:SFBAudioMetadataReplayGainTrackPeakKey];
}

- (NSNumber *)replayGainAlbumGain
{
	return [_metadata objectForKey:SFBAudioMetadataReplayGainAlbumGainKey];
}

- (void)setReplayGainAlbumGain:(NSNumber *)replayGainAlbumGain
{
	[_metadata setObject:replayGainAlbumGain forKey:SFBAudioMetadataReplayGainAlbumGainKey];
}

- (NSNumber *)replayGainAlbumPeak
{
	return [_metadata objectForKey:SFBAudioMetadataReplayGainAlbumPeakKey];
}

- (void)setReplayGainAlbumPeak:(NSNumber *)replayGainAlbumPeak
{
	[_metadata setObject:replayGainAlbumPeak forKey:SFBAudioMetadataReplayGainAlbumPeakKey];
}

#pragma mark Album Artwork

- (void)copyAttachedPicturesFrom:(SFBAudioMetadata *)metadata
{
	for(SFBAttachedPicture *picture in metadata.attachedPictures)
		[_pictures addObject:picture];
}

- (NSArray *)attachedPictures
{
	return [[_pictures mergedObjects] allObjects];
}

- (NSArray *)attachedPicturesOfType:(SFBAttachedPictureType)type
{
	NSMutableArray *pictures = [NSMutableArray array];
	for(SFBAttachedPicture *picture in [_pictures mergedObjects]) {
		if(picture.pictureType == type)
			[pictures addObject:picture];
	}
	return pictures;
}

- (void)attachPicture:(SFBAttachedPicture *)picture
{
	[_pictures addObject:picture];
}

- (void)removePicture:(SFBAttachedPicture *)picture
{
	[_pictures removeObject:picture];
}

- (void)removeAttachedPicturesOfType:(SFBAttachedPictureType)type
{
	for(SFBAttachedPicture *picture in [_pictures mergedObjects]) {
		if(picture.pictureType == type)
			[_pictures removeObject:picture];
	}
}

- (void)removeAllAttachedPictures
{
	[_pictures removeAllObjects];
}

@end
