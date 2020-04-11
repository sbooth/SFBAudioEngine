/*
 * Copyright (c) 2009 - 2020 Stephen F. Booth <me@sbooth.org>
 *
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import "PlayerWindowController.h"

#import <SFBAudioEngine/SFBAudioDecoder.h>
#import <SFBAudioEngine/SFBAudioFile.h>
#import <SFBAudioEngine/SFBAudioPlayer.h>

@interface SFBAttachedPicture (ImageCreation)
@property (nonatomic, nullable, readonly) NSImage *image;
@end

@implementation SFBAttachedPicture (ImageCreation)
- (NSImage *)image { return [[NSImage alloc] initWithData:self.imageData]; }
@end

@interface PlayerWindowController ()
{
@private
	SFBAudioPlayer 		*_player;
	dispatch_source_t	_timer;
}
@end

@interface PlayerWindowController (Private)
- (void) updateWindowUI;
@end

@implementation PlayerWindowController

- (id) init
{
	if((self = [super initWithWindowNibName:@"PlayerWindow"])) {
		_player = [[SFBAudioPlayer alloc] init];

		__weak typeof(self) weakSelf = self;

		[_player setRenderingStartedNotificationHandler:^(SFBAudioDecoder * _Nonnull decoder) {
			NSURL *url = decoder.inputSource.url;
			dispatch_async(dispatch_get_main_queue(), ^{
				[weakSelf updateWindowUI];
				if([url isFileURL])
					[[NSDocumentController sharedDocumentController] noteNewRecentDocumentURL:url];
			});
		}];

		[_player setRenderingFinishedNotificationHandler:^(SFBAudioDecoder * _Nonnull decoder) {
#pragma unused(decoder)
			dispatch_async(dispatch_get_main_queue(), ^{
				[weakSelf updateWindowUI];
			});
		}];

		[_player setErrorNotificationHandler:^(NSError * _Nonnull error) {
			dispatch_async(dispatch_get_main_queue(), ^{
				[NSApp presentError:error];
			});
		}];

		// Update the UI 5 times per second
		_timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, dispatch_get_main_queue());
		dispatch_source_set_timer(_timer, DISPATCH_TIME_NOW, NSEC_PER_SEC / 5, NSEC_PER_SEC / 3);

		dispatch_source_set_event_handler(_timer, ^{
			if(!self->_player.isPlaying)
				[self.playButton setTitle:@"Resume"];
			else
				[self.playButton setTitle:@"Pause"];

			SFBAudioPlayerPlaybackPosition playbackPosition;
			SFBAudioPlayerPlaybackTime playbackTime;
			if([self->_player getPlaybackPosition:&playbackPosition andTime:&playbackTime]) {
				double fractionComplete = (double)playbackPosition.currentFrame / playbackPosition.totalFrames;
				[self.slider setDoubleValue:fractionComplete];
				[self.elapsed setDoubleValue:playbackTime.currentTime];
				[self.remaining setDoubleValue:(-1 * (playbackTime.totalTime - playbackTime.currentTime))];
			}
		});

		// Start the timer
		dispatch_resume(_timer);
	}

	return self;
}

- (void) awakeFromNib
{
	// Disable the UI since no file is loaded
	[self updateWindowUI];
}

- (NSString *) windowFrameAutosaveName
{
	return @"Player Window";
}

- (void) windowWillClose:(NSNotification *)notification
{
#pragma unused(notification)
	[_player stopReturningError:nil];
}

- (IBAction) playPause:(id)sender
{
#pragma unused(sender)
	[_player playPauseReturningError:nil];
}

- (IBAction) seekForward:(id)sender
{
#pragma unused(sender)
	[_player seekForward];
}

- (IBAction) seekBackward:(id)sender
{
#pragma unused(sender)
	[_player seekBackward];
}

- (IBAction) seek:(id)sender
{
#pragma unused(sender)
	[_player seekToPosition:[sender floatValue]];
}

- (IBAction) skipToNextTrack:(id)sender
{
#pragma unused(sender)
	[_player skipToNext];
}

- (BOOL)playURL:(NSURL *)url
{
	return [_player playURL:url error:nil];
}

- (BOOL)enqueueURL:(NSURL *)url
{
	return [_player enqueueURL:url error:nil];
}

@end

@implementation PlayerWindowController (Private)

- (void)updateWindowUI
{
	NSURL *url = _player.url;

	// Nothing happening, reset the window
	if(!url) {
		[[self window] setRepresentedURL:nil];
		[[self window] setTitle:@""];

		[self.slider setEnabled:NO];
		[self.playButton setState:NSOffState];
		[self.playButton setEnabled:NO];
		[self.backwardButton setEnabled:NO];
		[self.forwardButton setEnabled:NO];

		[self.elapsed setHidden:YES];
		[self.remaining setHidden:YES];

		[self.albumArt setImage:[NSImage imageNamed:@"NSApplicationIcon"]];
		[self.title setStringValue:@""];
		[self.artist setStringValue:@""];

		return;
	}

	BOOL seekable = _player.supportsSeeking;

	// Update the window's title and represented file
	[[self window] setRepresentedURL:url];
	[[self window] setTitle:[[NSFileManager defaultManager] displayNameAtPath:[url path]]];

	[[NSDocumentController sharedDocumentController] noteNewRecentDocumentURL:url];

	// Update the UI
	[self.slider setEnabled:seekable];
	[self.playButton setEnabled:YES];
	[self.backwardButton setEnabled:seekable];
	[self.forwardButton setEnabled:seekable];

	// Show the times
	[self.elapsed setHidden:NO];

	if(_player.totalFrames != -1)
		[self.remaining setHidden:NO];

	// Load and display some metadata.  Normally the metadata would be read and stored in the background,
	// but for simplicity's sake it is done here.
	SFBAudioFile *audioFile = [SFBAudioFile audioFileWithURL:url error:nil];
	if(audioFile) {
		SFBAudioMetadata *metadata = audioFile.metadata;
		NSSet *pictures = metadata.attachedPictures;
		if([pictures count] > 0)
			[self.albumArt setImage:[[pictures anyObject] image]];
		else
			[self.albumArt setImage:nil];

		[self.title setStringValue:metadata.title ?: @""];
		[self.artist setStringValue:metadata.artist ?: @""];
	}
	else {
		[self.albumArt setImage:[NSImage imageNamed:@"NSApplicationIcon"]];
		[self.title setStringValue:@""];
		[self.artist setStringValue:@""];
	}
}

@end
