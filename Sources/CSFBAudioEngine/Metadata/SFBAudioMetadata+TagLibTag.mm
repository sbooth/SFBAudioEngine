//
// SPDX-FileCopyrightText: 2010 Stephen F. Booth <contact@sbooth.dev>
// SPDX-License-Identifier: MIT
//
// Part of https://github.com/sbooth/SFBAudioEngine
//

#import "SFBAudioMetadata+TagLibTag.h"
#import "TagLibStringUtilities.h"

@implementation SFBAudioMetadata (TagLibTag)

- (void)addMetadataFromTagLibTag:(const TagLib::Tag *)tag {
    NSParameterAssert(tag != nil);

    self.title = [NSString stringWithUTF8String:tag->title().toCString(true)];
    self.artist = [NSString stringWithUTF8String:tag->artist().toCString(true)];
    self.albumTitle = [NSString stringWithUTF8String:tag->album().toCString(true)];
    self.genre = [NSString stringWithUTF8String:tag->genre().toCString(true)];
    self.comment = [NSString stringWithUTF8String:tag->comment().toCString(true)];

    if (auto year = tag->year(); year != 0) {
        self.releaseDate = @(year).stringValue;
    }

    if (auto track = tag->track(); track != 0) {
        self.trackNumber = @(track);
    }
}

@end

void sfb::setTagFromMetadata(SFBAudioMetadata *metadata, TagLib::Tag *tag) {
    assert(metadata != nil);
    assert(tag != nullptr);

    tag->setTitle(TagLib::StringFromNSString(metadata.title));
    tag->setArtist(TagLib::StringFromNSString(metadata.artist));
    tag->setAlbum(TagLib::StringFromNSString(metadata.albumTitle));
    tag->setGenre(TagLib::StringFromNSString(metadata.genre));
    tag->setComment(TagLib::StringFromNSString(metadata.comment));

    if (NSString *releaseDate = metadata.releaseDate; releaseDate != nil) {
        tag->setYear(static_cast<unsigned int>(releaseDate.intValue));
    } else {
        tag->setYear(0);
    }

    if (NSNumber *trackNumber = metadata.trackNumber; trackNumber != nil) {
        tag->setTrack(trackNumber.unsignedIntValue);
    } else {
        tag->setTrack(0);
    }
}
