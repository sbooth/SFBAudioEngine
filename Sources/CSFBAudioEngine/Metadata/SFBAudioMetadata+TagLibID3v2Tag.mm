//
// SPDX-FileCopyrightText: 2010 Stephen F. Booth <contact@sbooth.dev>
// SPDX-License-Identifier: MIT
//
// Part of https://github.com/sbooth/SFBAudioEngine
//

#import "SFBAudioMetadata+TagLibID3v2Tag.h"
#import "SFBAudioMetadata+TagLibTag.h"
#import "TagLibStringUtilities.h"

#import <taglib/attachedpictureframe.h>
#import <taglib/id3v2frame.h>
#import <taglib/popularimeterframe.h>
#import <taglib/relativevolumeframe.h>
#import <taglib/textidentificationframe.h>
#import <taglib/unsynchronizedlyricsframe.h>

#import <ImageIO/ImageIO.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#import <memory>

namespace {

/// A `std::unique_ptr` deleter for `CFTypeRef` objects
struct cf_type_ref_deleter {
    void operator()(CFTypeRef CF_RELEASES_ARGUMENT cf) { CFRelease(cf); }
};

using cg_image_source_unique_ptr = std::unique_ptr<CGImageSource, cf_type_ref_deleter>;

} /* namespace */

@implementation SFBAudioMetadata (TagLibID3v2Tag)

- (void)addMetadataFromTagLibID3v2Tag:(const TagLib::ID3v2::Tag *)tag {
    NSParameterAssert(tag != nil);

    // Add the basic tags not specific to ID3v2
    [self addMetadataFromTagLibTag:tag];

    // Release date
    if (auto frameList = tag->frameListMap()["TDRC"]; !frameList.isEmpty()) {
        /*
         The timestamp fields are based on a subset of ISO 8601. When being as
         precise as possible the format of a time string is
         yyyy-MM-ddTHH:mm:ss (year, "-", month, "-", day, "T", hour (out of
         24), ":", minutes, ":", seconds), but the precision may be reduced by
         removing as many time indicators as wanted. Hence valid timestamps
         are
         yyyy, yyyy-MM, yyyy-MM-dd, yyyy-MM-ddTHH, yyyy-MM-ddTHH:mm and
         yyyy-MM-ddTHH:mm:ss. All time stamps are UTC. For durations, use
         the slash character as described in 8601, and for multiple non-
         contiguous dates, use multiple strings, if allowed by the frame
         definition.
         */

        self.releaseDate = [NSString stringWithUTF8String:frameList.front()->toString().toCString(true)];
    }

    // Extract composer if present
    if (auto frameList = tag->frameListMap()["TCOM"]; !frameList.isEmpty()) {
        self.composer = [NSString stringWithUTF8String:frameList.front()->toString().toCString(true)];
    }

    // Extract album artist
    if (auto frameList = tag->frameListMap()["TPE2"]; !frameList.isEmpty()) {
        self.albumArtist = [NSString stringWithUTF8String:frameList.front()->toString().toCString(true)];
    }

    // BPM
    if (auto frameList = tag->frameListMap()["TBPM"]; !frameList.isEmpty()) {
        bool ok = false;
        int BPM = frameList.front()->toString().toInt(&ok);
        if (ok) {
            self.bpm = @(BPM);
        }
    }

    // Rating
    if (auto frameList = tag->frameListMap()["POPM"]; !frameList.isEmpty()) {
        if (auto *popularimeter = dynamic_cast<TagLib::ID3v2::PopularimeterFrame *>(frameList.front());
            popularimeter != nullptr) {
            self.rating = @(popularimeter->rating());
        }
    }

    // Extract total tracks if present
    if (auto frameList = tag->frameListMap()["TRCK"]; !frameList.isEmpty()) {
        // Split the tracks at '/'
        TagLib::String s = frameList.front()->toString();

        bool ok;
        auto pos = s.find("/", 0);
        if (pos != -1) {
            auto upos = static_cast<unsigned int>(pos);
            int trackNum = s.substr(0, upos).toInt(&ok);
            if (ok) {
                self.trackNumber = @(trackNum);
            }

            int trackTotal = s.substr(upos + 1).toInt(&ok);
            if (ok) {
                self.trackTotal = @(trackTotal);
            }
        } else if (s.length() > 0) {
            int trackNum = s.toInt(&ok);
            if (ok) {
                self.trackNumber = @(trackNum);
            }
        }
    }

    // Extract disc number and total discs
    if (auto frameList = tag->frameListMap()["TPOS"]; !frameList.isEmpty()) {
        // Split the tracks at '/'
        TagLib::String s = frameList.front()->toString();

        bool ok;
        auto pos = s.find("/", 0);
        if (pos != -1) {
            auto upos = static_cast<unsigned int>(pos);
            int discNum = s.substr(0, upos).toInt(&ok);
            if (ok) {
                self.discNumber = @(discNum);
            }

            int discTotal = s.substr(upos + 1).toInt(&ok);
            if (ok) {
                self.discTotal = @(discTotal);
            }
        } else if (s.length() > 0) {
            int discNum = s.toInt(&ok);
            if (ok) {
                self.discNumber = @(discNum);
            }
        }
    }

    // Lyrics
    if (auto frameList = tag->frameListMap()["USLT"]; !frameList.isEmpty()) {
        self.lyrics = [NSString stringWithUTF8String:frameList.front()->toString().toCString(true)];
    }

    // Extract compilation if present (iTunes TCMP tag)
    if (auto frameList = tag->frameListMap()["TCMP"]; !frameList.isEmpty()) {
        // It seems that the presence of this frame indicates a compilation
        self.compilation = @(YES);
    }

    if (auto frameList = tag->frameListMap()["TSRC"]; !frameList.isEmpty()) {
        self.isrc = [NSString stringWithUTF8String:frameList.front()->toString().toCString(true)];
    }

    // MusicBrainz
    if (auto *musicBrainzReleaseIDFrame = TagLib::ID3v2::UserTextIdentificationFrame::find(
                const_cast<TagLib::ID3v2::Tag *>(tag), "MusicBrainz Album Id");
        musicBrainzReleaseIDFrame != nullptr) {
        self.musicBrainzReleaseID =
                [NSString stringWithUTF8String:musicBrainzReleaseIDFrame->fieldList().back().toCString(true)];
    }

    if (auto *musicBrainzRecordingIDFrame = TagLib::ID3v2::UserTextIdentificationFrame::find(
                const_cast<TagLib::ID3v2::Tag *>(tag), "MusicBrainz Track Id");
        musicBrainzRecordingIDFrame != nullptr) {
        self.musicBrainzRecordingID =
                [NSString stringWithUTF8String:musicBrainzRecordingIDFrame->fieldList().back().toCString(true)];
    }

    // Sorting and grouping
    if (auto frameList = tag->frameListMap()["TSOT"]; !frameList.isEmpty()) {
        self.titleSortOrder = [NSString stringWithUTF8String:frameList.front()->toString().toCString(true)];
    }

    if (auto frameList = tag->frameListMap()["TSOA"]; !frameList.isEmpty()) {
        self.albumTitleSortOrder = [NSString stringWithUTF8String:frameList.front()->toString().toCString(true)];
    }

    if (auto frameList = tag->frameListMap()["TSOP"]; !frameList.isEmpty()) {
        self.artistSortOrder = [NSString stringWithUTF8String:frameList.front()->toString().toCString(true)];
    }

    if (auto frameList = tag->frameListMap()["TSO2"]; !frameList.isEmpty()) {
        self.albumArtistSortOrder = [NSString stringWithUTF8String:frameList.front()->toString().toCString(true)];
    }

    if (auto frameList = tag->frameListMap()["TSOC"]; !frameList.isEmpty()) {
        self.composerSortOrder = [NSString stringWithUTF8String:frameList.front()->toString().toCString(true)];
    }

    if (auto frameList = tag->frameListMap()["TIT1"]; !frameList.isEmpty()) {
        self.grouping = [NSString stringWithUTF8String:frameList.front()->toString().toCString(true)];
    }

    // ReplayGain
    auto foundReplayGain = false;

    // Preference is TXXX frames, RVA2 frame, then LAME header
    auto *trackGainFrame = TagLib::ID3v2::UserTextIdentificationFrame::find(const_cast<TagLib::ID3v2::Tag *>(tag),
                                                                            "REPLAYGAIN_TRACK_GAIN");
    auto *trackPeakFrame = TagLib::ID3v2::UserTextIdentificationFrame::find(const_cast<TagLib::ID3v2::Tag *>(tag),
                                                                            "REPLAYGAIN_TRACK_PEAK");
    auto *albumGainFrame = TagLib::ID3v2::UserTextIdentificationFrame::find(const_cast<TagLib::ID3v2::Tag *>(tag),
                                                                            "REPLAYGAIN_ALBUM_GAIN");
    auto *albumPeakFrame = TagLib::ID3v2::UserTextIdentificationFrame::find(const_cast<TagLib::ID3v2::Tag *>(tag),
                                                                            "REPLAYGAIN_ALBUM_PEAK");

    if (trackGainFrame == nullptr) {
        trackGainFrame = TagLib::ID3v2::UserTextIdentificationFrame::find(const_cast<TagLib::ID3v2::Tag *>(tag),
                                                                          "replaygain_track_gain");
    }
    if (trackGainFrame != nullptr) {
        NSString *s = [NSString stringWithUTF8String:trackGainFrame->fieldList().back().toCString(true)];
        self.replayGainTrackGain = @(s.doubleValue);
        self.replayGainReferenceLoudness = @(89.0);

        foundReplayGain = true;
    }

    if (trackPeakFrame == nullptr) {
        trackPeakFrame = TagLib::ID3v2::UserTextIdentificationFrame::find(const_cast<TagLib::ID3v2::Tag *>(tag),
                                                                          "replaygain_track_peak");
    }
    if (trackPeakFrame != nullptr) {
        NSString *s = [NSString stringWithUTF8String:trackPeakFrame->fieldList().back().toCString(true)];
        self.replayGainTrackPeak = @(s.doubleValue);
    }

    if (albumGainFrame == nullptr) {
        albumGainFrame = TagLib::ID3v2::UserTextIdentificationFrame::find(const_cast<TagLib::ID3v2::Tag *>(tag),
                                                                          "replaygain_album_gain");
    }
    if (albumGainFrame != nullptr) {
        NSString *s = [NSString stringWithUTF8String:albumGainFrame->fieldList().back().toCString(true)];
        self.replayGainAlbumGain = @(s.doubleValue);
        self.replayGainReferenceLoudness = @(89.0);

        foundReplayGain = true;
    }

    if (albumPeakFrame == nullptr) {
        albumPeakFrame = TagLib::ID3v2::UserTextIdentificationFrame::find(const_cast<TagLib::ID3v2::Tag *>(tag),
                                                                          "replaygain_album_peak");
    }
    if (albumPeakFrame != nullptr) {
        NSString *s = [NSString stringWithUTF8String:albumPeakFrame->fieldList().back().toCString(true)];
        self.replayGainAlbumPeak = @(s.doubleValue);
    }

    // If nothing found check for RVA2 frame
    if (!foundReplayGain) {
        auto frameList = tag->frameListMap()["RVA2"];

        for (auto *frameIterator : tag->frameListMap()["RVA2"]) {
            TagLib::ID3v2::RelativeVolumeFrame *relativeVolume =
                    dynamic_cast<TagLib::ID3v2::RelativeVolumeFrame *>(frameIterator);
            if (relativeVolume == nullptr) {
                continue;
            }

            // Attempt to use the master volume if present
            auto channels = relativeVolume->channels();
            auto channelType = TagLib::ID3v2::RelativeVolumeFrame::MasterVolume;

            // Fall back on whatever else exists in the frame
            if (!channels.contains(TagLib::ID3v2::RelativeVolumeFrame::MasterVolume)) {
                channelType = channels.front();
            }

            if (float volumeAdjustment = relativeVolume->volumeAdjustment(channelType); volumeAdjustment != 0.f) {
                if (auto identification = relativeVolume->identification(); identification == "track") {
                    self.replayGainTrackGain = @(volumeAdjustment);
                } else if (identification == "album") {
                    self.replayGainAlbumGain = @(volumeAdjustment);
                } else {
                    // Fall back to track gain if identification is not specified
                    self.replayGainTrackGain = @(volumeAdjustment);
                }
            }
        }
    }

    // Extract album art if present
    for (auto *it : tag->frameListMap()["APIC"]) {
        TagLib::ID3v2::AttachedPictureFrame *frame = dynamic_cast<TagLib::ID3v2::AttachedPictureFrame *>(it);
        if (frame != nullptr) {
            NSData *imageData = [NSData dataWithBytes:frame->picture().data() length:frame->picture().size()];
            NSString *description = nil;
            if (!frame->description().isEmpty()) {
                description = [NSString stringWithUTF8String:frame->description().toCString(true)];
            }

            SFBAttachedPicture *picture =
                    [[SFBAttachedPicture alloc] initWithImageData:imageData
                                                             type:static_cast<SFBAttachedPictureType>(frame->type())
                                                      description:description];
            [self attachPicture:picture];
        }
    }
}

@end

void sfb::setID3v2TagFromMetadata(SFBAudioMetadata *metadata, TagLib::ID3v2::Tag *tag, bool setAlbumArt) {
    assert(metadata != nil);
    assert(tag != nullptr);

    // Use UTF-8 as the default encoding
    (TagLib::ID3v2::FrameFactory::instance())->setDefaultTextEncoding(TagLib::String::UTF8);

    // Album title
    tag->setAlbum(TagLib::StringFromNSString(metadata.albumTitle));

    // Artist
    tag->setArtist(TagLib::StringFromNSString(metadata.artist));

    // Composer
    tag->removeFrames("TCOM");
    if (NSString *composer = metadata.composer; composer != nil) {
        auto *frame = new TagLib::ID3v2::TextIdentificationFrame("TCOM", TagLib::String::Latin1);
        frame->setText(TagLib::StringFromNSString(composer));
        tag->addFrame(frame);
    }

    // Genre
    tag->setGenre(TagLib::StringFromNSString(metadata.genre));

    // Date
    tag->removeFrames("TDRC");
    if (NSString *releaseDate = metadata.releaseDate; releaseDate != nil) {
        /*
         The timestamp fields are based on a subset of ISO 8601. When being as
         precise as possible the format of a time string is
         yyyy-MM-ddTHH:mm:ss (year, "-", month, "-", day, "T", hour (out of
         24), ":", minutes, ":", seconds), but the precision may be reduced by
         removing as many time indicators as wanted. Hence valid timestamps
         are
         yyyy, yyyy-MM, yyyy-MM-dd, yyyy-MM-ddTHH, yyyy-MM-ddTHH:mm and
         yyyy-MM-ddTHH:mm:ss. All time stamps are UTC. For durations, use
         the slash character as described in 8601, and for multiple non-
         contiguous dates, use multiple strings, if allowed by the frame
         definition.
         */
        NSISO8601DateFormatter *formatter = [[NSISO8601DateFormatter alloc] init];
        NSCalendar *gregorianCalendar = [[NSCalendar alloc] initWithCalendarIdentifier:NSCalendarIdentifierGregorian];
        NSDate *date = [formatter dateFromString:releaseDate];
        if (date) {
            tag->setYear((unsigned int)[gregorianCalendar component:NSCalendarUnitYear fromDate:date]);

            auto *frame = new TagLib::ID3v2::TextIdentificationFrame("TDRC", TagLib::String::Latin1);
            frame->setText(TagLib::StringFromNSString(releaseDate));
            tag->addFrame(frame);
        }
    }

    // Comment
    tag->setComment(TagLib::StringFromNSString(metadata.comment));

    // Album artist
    tag->removeFrames("TPE2");
    if (NSString *albumArtist = metadata.albumArtist; albumArtist != nil) {
        auto *frame = new TagLib::ID3v2::TextIdentificationFrame("TPE2", TagLib::String::Latin1);
        frame->setText(TagLib::StringFromNSString(albumArtist));
        tag->addFrame(frame);
    }

    // Track title
    tag->setTitle(TagLib::StringFromNSString(metadata.title));

    // BPM
    tag->removeFrames("TBPM");
    if (NSNumber *bpm = metadata.bpm; bpm != nil) {
        auto *frame = new TagLib::ID3v2::TextIdentificationFrame("TBPM", TagLib::String::Latin1);
        frame->setText(TagLib::StringFromNSString(bpm.stringValue));
        tag->addFrame(frame);
    }

    // Rating
    tag->removeFrames("POPM");
    if (NSNumber *rating = metadata.rating; rating != nil) {
        TagLib::ID3v2::PopularimeterFrame *frame = new TagLib::ID3v2::PopularimeterFrame();
        frame->setRating(rating.intValue);
        tag->addFrame(frame);
    }

    // Track number and total tracks
    tag->removeFrames("TRCK");
    if (NSNumber *trackNumber = metadata.trackNumber, *trackTotal = metadata.trackTotal;
        trackNumber != nil && trackTotal != nil) {
        auto *frame = new TagLib::ID3v2::TextIdentificationFrame("TRCK", TagLib::String::Latin1);
        frame->setText(TagLib::StringFromNSString([NSString stringWithFormat:@"%@/%@", trackNumber, trackTotal]));
        tag->addFrame(frame);
    } else if (trackNumber != nil) {
        auto *frame = new TagLib::ID3v2::TextIdentificationFrame("TRCK", TagLib::String::Latin1);
        frame->setText(TagLib::StringFromNSString([NSString stringWithFormat:@"%@", trackNumber]));
        tag->addFrame(frame);
    } else if (trackTotal != nil) {
        auto *frame = new TagLib::ID3v2::TextIdentificationFrame("TRCK", TagLib::String::Latin1);
        frame->setText(TagLib::StringFromNSString([NSString stringWithFormat:@"/%@", trackTotal]));
        tag->addFrame(frame);
    }

    // Compilation
    // iTunes uses the TCMP frame for this, which isn't in the standard, but we'll use it for compatibility
    tag->removeFrames("TCMP");
    if (NSNumber *compilation = metadata.compilation; compilation != nil) {
        auto *frame = new TagLib::ID3v2::TextIdentificationFrame("TCMP", TagLib::String::Latin1);
        frame->setText(compilation.boolValue ? "1" : "0");
        tag->addFrame(frame);
    }

    // Disc number and total discs
    tag->removeFrames("TPOS");
    if (NSNumber *discNumber = metadata.discNumber, *discTotal = metadata.discTotal;
        discNumber != nil && discTotal != nil) {
        auto *frame = new TagLib::ID3v2::TextIdentificationFrame("TPOS", TagLib::String::Latin1);
        frame->setText(TagLib::StringFromNSString([NSString stringWithFormat:@"%@/%@", discNumber, discTotal]));
        tag->addFrame(frame);
    } else if (discNumber != nil) {
        auto *frame = new TagLib::ID3v2::TextIdentificationFrame("TPOS", TagLib::String::Latin1);
        frame->setText(TagLib::StringFromNSString([NSString stringWithFormat:@"%@", discNumber]));
        tag->addFrame(frame);
    } else if (discTotal != nil) {
        auto *frame = new TagLib::ID3v2::TextIdentificationFrame("TPOS", TagLib::String::Latin1);
        frame->setText(TagLib::StringFromNSString([NSString stringWithFormat:@"/%@", discTotal]));
        tag->addFrame(frame);
    }

    // Lyrics
    tag->removeFrames("USLT");
    if (NSString *lyrics = metadata.lyrics; lyrics != nil) {
        auto *frame = new TagLib::ID3v2::UnsynchronizedLyricsFrame(TagLib::String::UTF8);
        frame->setText(TagLib::StringFromNSString(lyrics));
        tag->addFrame(frame);
    }

    tag->removeFrames("TSRC");
    if (NSString *isrc = metadata.isrc; isrc != nil) {
        auto *frame = new TagLib::ID3v2::TextIdentificationFrame("TSRC", TagLib::String::Latin1);
        frame->setText(TagLib::StringFromNSString(isrc));
        tag->addFrame(frame);
    }

    // MusicBrainz
    if (auto *musicBrainzReleaseIDFrame = TagLib::ID3v2::UserTextIdentificationFrame::find(tag, "MusicBrainz Album Id");
        musicBrainzReleaseIDFrame != nullptr) {
        tag->removeFrame(musicBrainzReleaseIDFrame);
    }

    if (NSString *musicBrainzReleaseID = metadata.musicBrainzReleaseID; musicBrainzReleaseID != nil) {
        auto *frame = new TagLib::ID3v2::UserTextIdentificationFrame();
        frame->setDescription("MusicBrainz Album Id");
        frame->setText(TagLib::StringFromNSString(musicBrainzReleaseID));
        tag->addFrame(frame);
    }

    if (auto *musicBrainzRecordingIDFrame =
                TagLib::ID3v2::UserTextIdentificationFrame::find(tag, "MusicBrainz Track Id");
        musicBrainzRecordingIDFrame != nullptr) {
        tag->removeFrame(musicBrainzRecordingIDFrame);
    }

    if (NSString *musicBrainzRecordingID = metadata.musicBrainzRecordingID; musicBrainzRecordingID != nil) {
        auto *frame = new TagLib::ID3v2::UserTextIdentificationFrame();
        frame->setDescription("MusicBrainz Track Id");
        frame->setText(TagLib::StringFromNSString(musicBrainzRecordingID));
        tag->addFrame(frame);
    }

    // Sorting and grouping
    tag->removeFrames("TSOT");
    if (NSString *titleSortOrder = metadata.titleSortOrder; titleSortOrder != nil) {
        auto *frame = new TagLib::ID3v2::TextIdentificationFrame("TSOT", TagLib::String::UTF8);
        frame->setText(TagLib::StringFromNSString(titleSortOrder));
        tag->addFrame(frame);
    }

    tag->removeFrames("TSOA");
    if (NSString *albumTitleSortOrder = metadata.albumTitleSortOrder; albumTitleSortOrder != nil) {
        auto *frame = new TagLib::ID3v2::TextIdentificationFrame("TSOA", TagLib::String::UTF8);
        frame->setText(TagLib::StringFromNSString(albumTitleSortOrder));
        tag->addFrame(frame);
    }

    tag->removeFrames("TSOP");
    if (NSString *artistSortOrder = metadata.artistSortOrder; artistSortOrder != nil) {
        auto *frame = new TagLib::ID3v2::TextIdentificationFrame("TSOP", TagLib::String::UTF8);
        frame->setText(TagLib::StringFromNSString(artistSortOrder));
        tag->addFrame(frame);
    }

    tag->removeFrames("TSO2");
    if (NSString *albumArtistSortOrder = metadata.albumArtistSortOrder; albumArtistSortOrder != nil) {
        auto *frame = new TagLib::ID3v2::TextIdentificationFrame("TSO2", TagLib::String::UTF8);
        frame->setText(TagLib::StringFromNSString(albumArtistSortOrder));
        tag->addFrame(frame);
    }

    tag->removeFrames("TSOC");
    if (NSString *composerSortOrder = metadata.composerSortOrder; composerSortOrder != nil) {
        auto *frame = new TagLib::ID3v2::TextIdentificationFrame("TSOC", TagLib::String::UTF8);
        frame->setText(TagLib::StringFromNSString(composerSortOrder));
        tag->addFrame(frame);
    }

    tag->removeFrames("TIT1");
    if (NSString *grouping = metadata.grouping; grouping != nil) {
        auto *frame = new TagLib::ID3v2::TextIdentificationFrame("TIT1", TagLib::String::UTF8);
        frame->setText(TagLib::StringFromNSString(grouping));
        tag->addFrame(frame);
    }

    // ReplayGain

    // Write TXXX frames
    if (auto *trackGainFrame = TagLib::ID3v2::UserTextIdentificationFrame::find(tag, "replaygain_track_gain");
        trackGainFrame != nullptr) {
        tag->removeFrame(trackGainFrame);
    }

    if (auto *trackPeakFrame = TagLib::ID3v2::UserTextIdentificationFrame::find(tag, "replaygain_track_peak");
        trackPeakFrame != nullptr) {
        tag->removeFrame(trackPeakFrame);
    }

    if (auto *albumGainFrame = TagLib::ID3v2::UserTextIdentificationFrame::find(tag, "replaygain_album_gain");
        albumGainFrame != nullptr) {
        tag->removeFrame(albumGainFrame);
    }

    if (auto *albumPeakFrame = TagLib::ID3v2::UserTextIdentificationFrame::find(tag, "replaygain_album_peak");
        albumPeakFrame != nullptr) {
        tag->removeFrame(albumPeakFrame);
    }

    // Also write the RVA2 frames
    tag->removeFrames("RVA2");

    if (NSNumber *replayGainTrackGain = metadata.replayGainTrackGain; replayGainTrackGain != nil) {
        auto *frame = new TagLib::ID3v2::UserTextIdentificationFrame();
        frame->setDescription("replaygain_track_gain");
        frame->setText(
                TagLib::StringFromNSString([NSString stringWithFormat:@"%+2.2f dB", replayGainTrackGain.doubleValue]));
        tag->addFrame(frame);

        auto *relativeVolume = new TagLib::ID3v2::RelativeVolumeFrame();
        relativeVolume->setIdentification("track");
        relativeVolume->setVolumeAdjustment(replayGainTrackGain.floatValue,
                                            TagLib::ID3v2::RelativeVolumeFrame::MasterVolume);
        tag->addFrame(relativeVolume);
    }

    if (NSNumber *replayGainTrackPeak = metadata.replayGainTrackPeak; replayGainTrackPeak != nil) {
        auto *frame = new TagLib::ID3v2::UserTextIdentificationFrame();
        frame->setDescription("replaygain_track_peak");
        frame->setText(
                TagLib::StringFromNSString([NSString stringWithFormat:@"%1.8f dB", replayGainTrackPeak.doubleValue]));
        tag->addFrame(frame);
    }

    if (NSNumber *replayGainAlbumGain = metadata.replayGainAlbumGain; replayGainAlbumGain != nil) {
        auto *frame = new TagLib::ID3v2::UserTextIdentificationFrame();
        frame->setDescription("replaygain_album_gain");
        frame->setText(
                TagLib::StringFromNSString([NSString stringWithFormat:@"%+2.2f dB", replayGainAlbumGain.doubleValue]));
        tag->addFrame(frame);

        auto *relativeVolume = new TagLib::ID3v2::RelativeVolumeFrame();
        relativeVolume->setIdentification(TagLib::String("album", TagLib::String::Latin1));
        relativeVolume->setVolumeAdjustment(replayGainAlbumGain.floatValue,
                                            TagLib::ID3v2::RelativeVolumeFrame::MasterVolume);
        tag->addFrame(relativeVolume);
    }

    if (NSNumber *replayGainAlbumPeak = metadata.replayGainAlbumPeak; replayGainAlbumPeak != nil) {
        auto *frame = new TagLib::ID3v2::UserTextIdentificationFrame();
        frame->setDescription("replaygain_album_peak");
        frame->setText(
                TagLib::StringFromNSString([NSString stringWithFormat:@"%1.8f dB", replayGainAlbumPeak.doubleValue]));
        tag->addFrame(frame);
    }

    // Album art
    tag->removeFrames("APIC");

    if (setAlbumArt) {
        for (SFBAttachedPicture *attachedPicture in metadata.attachedPictures) {
            cg_image_source_unique_ptr imageSource{
                    CGImageSourceCreateWithData((__bridge CFDataRef)attachedPicture.imageData, nullptr)};
            if (!imageSource) {
                continue;
            }

            TagLib::ID3v2::AttachedPictureFrame *frame = new TagLib::ID3v2::AttachedPictureFrame;

            // Convert the image's UTI into a MIME type
            if (CFStringRef typeIdentifier = CGImageSourceGetType(imageSource.get()); typeIdentifier) {
                UTType *type = [UTType typeWithIdentifier:(__bridge NSString *)typeIdentifier];
                if (NSString *mimeType = [type preferredMIMEType]; mimeType != nil) {
                    frame->setMimeType(TagLib::StringFromNSString(mimeType));
                }
            }

            frame->setPicture(TagLib::ByteVector(static_cast<const char *>(attachedPicture.imageData.bytes),
                                                 static_cast<unsigned int>(attachedPicture.imageData.length)));
            frame->setType((TagLib::ID3v2::AttachedPictureFrame::Type)attachedPicture.pictureType);
            if (NSString *pictureDescription = attachedPicture.pictureDescription; pictureDescription != nil) {
                frame->setDescription(TagLib::StringFromNSString(pictureDescription));
            }
            tag->addFrame(frame);
        }
    }
}
