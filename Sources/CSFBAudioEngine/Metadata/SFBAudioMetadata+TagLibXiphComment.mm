//
// Copyright (c) 2010-2026 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import "SFBAudioMetadata+TagLibXiphComment.h"
#import "TagLibStringUtilities.h"

#import <memory>

#import <ImageIO/ImageIO.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

namespace {

/// A `std::unique_ptr` deleter for `CFTypeRef` objects
struct cf_type_ref_deleter {
    void operator()(CFTypeRef CF_RELEASES_ARGUMENT cf) {
        CFRelease(cf);
    }
};

using cg_image_source_unique_ptr = std::unique_ptr<CGImageSource, cf_type_ref_deleter>;

} /* namespace */

@implementation SFBAudioMetadata (TagLibXiphComment)

- (void)addMetadataFromTagLibXiphComment:(const TagLib::Ogg::XiphComment *)tag {
    NSParameterAssert(tag != nil);

    NSMutableDictionary *additionalMetadata = [NSMutableDictionary dictionary];

    for (auto it : tag->fieldListMap()) {
        // According to the Xiph comment specification keys should only contain a limited subset of ASCII, but UTF-8 is
        // a safer choice
        NSString *key = [NSString stringWithUTF8String:it.first.toCString(true)];

        // Vorbis allows multiple comments with the same key, but this isn't supported by AudioMetadata
        NSString *value = [NSString stringWithUTF8String:it.second.front().toCString(true)];

        if ([key caseInsensitiveCompare:@"ALBUM"] == NSOrderedSame)
            self.albumTitle = value;
        else if ([key caseInsensitiveCompare:@"ARTIST"] == NSOrderedSame)
            self.artist = value;
        else if ([key caseInsensitiveCompare:@"ALBUMARTIST"] == NSOrderedSame)
            self.albumArtist = value;
        else if ([key caseInsensitiveCompare:@"COMPOSER"] == NSOrderedSame)
            self.composer = value;
        else if ([key caseInsensitiveCompare:@"GENRE"] == NSOrderedSame)
            self.genre = value;
        else if ([key caseInsensitiveCompare:@"DATE"] == NSOrderedSame)
            self.releaseDate = value;
        else if ([key caseInsensitiveCompare:@"DESCRIPTION"] == NSOrderedSame)
            self.comment = value;
        else if ([key caseInsensitiveCompare:@"TITLE"] == NSOrderedSame)
            self.title = value;
        else if ([key caseInsensitiveCompare:@"TRACKNUMBER"] == NSOrderedSame)
            self.trackNumber = @(value.integerValue);
        else if ([key caseInsensitiveCompare:@"TRACKTOTAL"] == NSOrderedSame)
            self.trackTotal = @(value.integerValue);
        else if ([key caseInsensitiveCompare:@"COMPILATION"] == NSOrderedSame)
            self.compilation = @(value.boolValue);
        else if ([key caseInsensitiveCompare:@"DISCNUMBER"] == NSOrderedSame)
            self.discNumber = @(value.integerValue);
        else if ([key caseInsensitiveCompare:@"DISCTOTAL"] == NSOrderedSame)
            self.discTotal = @(value.integerValue);
        else if ([key caseInsensitiveCompare:@"LYRICS"] == NSOrderedSame)
            self.lyrics = value;
        else if ([key caseInsensitiveCompare:@"BPM"] == NSOrderedSame)
            self.bpm = @(value.integerValue);
        else if ([key caseInsensitiveCompare:@"RATING"] == NSOrderedSame)
            self.rating = @(value.integerValue);
        else if ([key caseInsensitiveCompare:@"ISRC"] == NSOrderedSame)
            self.isrc = value;
        else if ([key caseInsensitiveCompare:@"MCN"] == NSOrderedSame)
            self.mcn = value;
        else if ([key caseInsensitiveCompare:@"MUSICBRAINZ_ALBUMID"] == NSOrderedSame)
            self.musicBrainzReleaseID = value;
        else if ([key caseInsensitiveCompare:@"MUSICBRAINZ_TRACKID"] == NSOrderedSame)
            self.musicBrainzRecordingID = value;
        else if ([key caseInsensitiveCompare:@"TITLESORT"] == NSOrderedSame)
            self.titleSortOrder = value;
        else if ([key caseInsensitiveCompare:@"ALBUMTITLESORT"] == NSOrderedSame)
            self.albumTitleSortOrder = value;
        else if ([key caseInsensitiveCompare:@"ARTISTSORT"] == NSOrderedSame)
            self.artistSortOrder = value;
        else if ([key caseInsensitiveCompare:@"ALBUMARTISTSORT"] == NSOrderedSame)
            self.albumArtistSortOrder = value;
        else if ([key caseInsensitiveCompare:@"COMPOSERSORT"] == NSOrderedSame)
            self.composerSortOrder = value;
        else if ([key caseInsensitiveCompare:@"GROUPING"] == NSOrderedSame)
            self.grouping = value;
        else if ([key caseInsensitiveCompare:@"REPLAYGAIN_REFERENCE_LOUDNESS"] == NSOrderedSame)
            self.replayGainReferenceLoudness = @(value.doubleValue);
        else if ([key caseInsensitiveCompare:@"REPLAYGAIN_TRACK_GAIN"] == NSOrderedSame)
            self.replayGainTrackGain = @(value.doubleValue);
        else if ([key caseInsensitiveCompare:@"REPLAYGAIN_TRACK_PEAK"] == NSOrderedSame)
            self.replayGainTrackPeak = @(value.doubleValue);
        else if ([key caseInsensitiveCompare:@"REPLAYGAIN_ALBUM_GAIN"] == NSOrderedSame)
            self.replayGainAlbumGain = @(value.doubleValue);
        else if ([key caseInsensitiveCompare:@"REPLAYGAIN_ALBUM_PEAK"] == NSOrderedSame)
            self.replayGainAlbumPeak = @(value.doubleValue);
        // TagLib parses "METADATA_BLOCK_PICTURE" and "COVERART" Xiph comments as pictures, so ignore them here
        else if ([key caseInsensitiveCompare:@"METADATA_BLOCK_PICTURE"] == NSOrderedSame ||
                 [key caseInsensitiveCompare:@"COVERART"] == NSOrderedSame)
            ;
        // Put all unknown tags into the additional metadata
        else
            [additionalMetadata setObject:value forKey:key];
    }

    if (additionalMetadata.count) {
        self.additionalMetadata = additionalMetadata;
    }

    // Add the pictures parsed by TagLib from the "METADATA_BLOCK_PICTURE" and "COVERART" Xiph comments
    for (auto iter : const_cast<TagLib::Ogg::XiphComment *>(tag)->pictureList()) {
        NSData *imageData = [NSData dataWithBytes:iter->data().data() length:iter->data().size()];

        NSString *description = nil;
        if (!iter->description().isEmpty())
            description = [NSString stringWithUTF8String:iter->description().toCString(true)];

        [self attachPicture:[[SFBAttachedPicture alloc]
                                  initWithImageData:imageData
                                               type:static_cast<SFBAttachedPictureType>(iter->type())
                                        description:description]];
    }
}

@end

namespace {

void SetXiphComment(TagLib::Ogg::XiphComment *tag, const char *key, NSString *value) {
    assert(nullptr != tag);
    assert(nullptr != key);

    // Remove the existing comment with this name
    tag->removeFields(key);

    if (value) {
        tag->addField(key, TagLib::StringFromNSString(value));
    }
}

void SetXiphCommentNumber(TagLib::Ogg::XiphComment *tag, const char *key, NSNumber *value) {
    assert(nullptr != tag);
    assert(nullptr != key);

    SetXiphComment(tag, key, value.stringValue);
}

void SetXiphCommentBoolean(TagLib::Ogg::XiphComment *tag, const char *key, NSNumber *value) {
    assert(nullptr != tag);
    assert(nullptr != key);

    if (value == nil) {
        SetXiphComment(tag, key, nil);
    } else {
        SetXiphComment(tag, key, value.boolValue ? @"1" : @"0");
    }
}

void SetXiphCommentDoubleWithFormat(TagLib::Ogg::XiphComment *tag, const char *key, NSNumber *value,
                                    NSString *format = nil) {
    assert(nullptr != tag);
    assert(nullptr != key);

    SetXiphComment(tag, key, value != nil ? [NSString stringWithFormat:(format ?: @"%f"), value.doubleValue] : nil);
}

} /* namespace */

void sfb::setXiphCommentFromMetadata(SFBAudioMetadata *metadata, TagLib::Ogg::XiphComment *tag, bool setAlbumArt) {
    NSCParameterAssert(metadata != nil);
    assert(nullptr != tag);

    // Standard tags
    SetXiphComment(tag, "ALBUM", metadata.albumTitle);
    SetXiphComment(tag, "ARTIST", metadata.artist);
    SetXiphComment(tag, "ALBUMARTIST", metadata.albumArtist);
    SetXiphComment(tag, "COMPOSER", metadata.composer);
    SetXiphComment(tag, "GENRE", metadata.genre);
    SetXiphComment(tag, "DATE", metadata.releaseDate);
    SetXiphComment(tag, "DESCRIPTION", metadata.comment);
    SetXiphComment(tag, "TITLE", metadata.title);
    SetXiphCommentNumber(tag, "TRACKNUMBER", metadata.trackNumber);
    SetXiphCommentNumber(tag, "TRACKTOTAL", metadata.trackTotal);
    SetXiphCommentBoolean(tag, "COMPILATION", metadata.compilation);
    SetXiphCommentNumber(tag, "DISCNUMBER", metadata.discNumber);
    SetXiphCommentNumber(tag, "DISCTOTAL", metadata.discTotal);
    SetXiphComment(tag, "LYRICS", metadata.lyrics);
    SetXiphCommentNumber(tag, "BPM", metadata.bpm);
    SetXiphCommentNumber(tag, "RATING", metadata.rating);
    SetXiphComment(tag, "ISRC", metadata.isrc);
    SetXiphComment(tag, "MCN", metadata.mcn);
    SetXiphComment(tag, "MUSICBRAINZ_ALBUMID", metadata.musicBrainzReleaseID);
    SetXiphComment(tag, "MUSICBRAINZ_TRACKID", metadata.musicBrainzRecordingID);
    SetXiphComment(tag, "TITLESORT", metadata.titleSortOrder);
    SetXiphComment(tag, "ALBUMTITLESORT", metadata.albumTitleSortOrder);
    SetXiphComment(tag, "ARTISTSORT", metadata.artistSortOrder);
    SetXiphComment(tag, "ALBUMARTISTSORT", metadata.albumArtistSortOrder);
    SetXiphComment(tag, "COMPOSERSORT", metadata.composerSortOrder);
    SetXiphComment(tag, "GROUPING", metadata.grouping);

    // Additional metadata
    NSDictionary *additionalMetadata = metadata.additionalMetadata;
    if (additionalMetadata) {
        for (NSString *key in additionalMetadata)
            SetXiphComment(tag, key.UTF8String, additionalMetadata[key]);
    }

    // ReplayGain info
    SetXiphCommentDoubleWithFormat(tag, "REPLAYGAIN_REFERENCE_LOUDNESS", metadata.replayGainReferenceLoudness,
                                   @"%2.1f dB");
    SetXiphCommentDoubleWithFormat(tag, "REPLAYGAIN_TRACK_GAIN", metadata.replayGainTrackGain, @"%+2.2f dB");
    SetXiphCommentDoubleWithFormat(tag, "REPLAYGAIN_TRACK_PEAK", metadata.replayGainTrackPeak, @"%1.8f");
    SetXiphCommentDoubleWithFormat(tag, "REPLAYGAIN_ALBUM_GAIN", metadata.replayGainAlbumGain, @"%+2.2f dB");
    SetXiphCommentDoubleWithFormat(tag, "REPLAYGAIN_ALBUM_PEAK", metadata.replayGainAlbumPeak, @"%1.8f");

    // Album art
    tag->removeAllPictures();

    if (setAlbumArt) {
        for (SFBAttachedPicture *attachedPicture in metadata.attachedPictures) {
            auto picture = ConvertAttachedPictureToFLACPicture(attachedPicture);
            if (picture)
                tag->addPicture(picture.release());
        }
    }
}

std::unique_ptr<TagLib::FLAC::Picture> sfb::ConvertAttachedPictureToFLACPicture(SFBAttachedPicture *attachedPicture) {
    NSCParameterAssert(attachedPicture != nil);

    cg_image_source_unique_ptr imageSource{
          CGImageSourceCreateWithData((__bridge CFDataRef)attachedPicture.imageData, nullptr)};
    if (!imageSource) {
        return nullptr;
    }

    auto picture = std::make_unique<TagLib::FLAC::Picture>();
    picture->setData(TagLib::ByteVector(static_cast<const char *>(attachedPicture.imageData.bytes),
                                        static_cast<unsigned int>(attachedPicture.imageData.length)));
    picture->setType(static_cast<TagLib::FLAC::Picture::Type>(attachedPicture.pictureType));
    if (attachedPicture.pictureDescription) {
        picture->setDescription(TagLib::StringFromNSString(attachedPicture.pictureDescription));
    }

    // Convert the image's UTI into a MIME type
    if (CFStringRef typeIdentifier = CGImageSourceGetType(imageSource.get()); typeIdentifier) {
        UTType *type = [UTType typeWithIdentifier:(__bridge NSString *)typeIdentifier];
        if (NSString *mimeType = [type preferredMIMEType]; mimeType) {
            picture->setMimeType(TagLib::StringFromNSString(mimeType));
        }
    }

    // Flesh out the height, width, and depth
    NSDictionary *imagePropertiesDictionary =
          (__bridge_transfer NSDictionary *)CGImageSourceCopyPropertiesAtIndex(imageSource.get(), 0, nullptr);
    if (imagePropertiesDictionary) {
        NSNumber *imageWidth = imagePropertiesDictionary[(__bridge NSString *)kCGImagePropertyPixelWidth];
        NSNumber *imageHeight = imagePropertiesDictionary[(__bridge NSString *)kCGImagePropertyPixelHeight];
        NSNumber *imageDepth = imagePropertiesDictionary[(__bridge NSString *)kCGImagePropertyDepth];

        picture->setHeight(imageHeight.intValue);
        picture->setWidth(imageWidth.intValue);
        picture->setColorDepth(imageDepth.intValue);
    }

    return picture;
}
