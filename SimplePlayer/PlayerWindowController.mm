/*
 *  Copyright (C) 2009, 2010, 2011, 2012, 2013 Stephen F. Booth <me@sbooth.org>
 *  All Rights Reserved
 */

#import "PlayerWindowController.h"

#include <atomic>

#include <SFBAudioEngine/AudioPlayer.h>
#include <SFBAudioEngine/AudioDecoder.h>
#include <SFBAudioEngine/AudioMetadata.h>

// ========================================
// Player flags
// ========================================
enum ePlayerFlags : unsigned int {
	ePlayerFlagRenderingStarted			= 1u << 0,
	ePlayerFlagRenderingFinished		= 1u << 1
};

@interface PlayerWindowController ()
{
@private
	SFB::Audio::Player	*_player;		// The player instance
	std::atomic_uint	_playerFlags;
	NSTimer				*_uiTimer;
}
@end

@interface PlayerWindowController (Callbacks)
- (void) uiTimerFired:(NSTimer *)timer;
@end

@interface PlayerWindowController (Private)
- (void) updateWindowUI;
@end

@implementation PlayerWindowController

- (id) init
{
	if((self = [super initWithWindowNibName:@"PlayerWindow"])) {
		_player = new SFB::Audio::Player();

		_playerFlags = 0;

		// This will be called from the realtime rendering thread and as such MUST NOT BLOCK!!
		_player->SetRenderingStartedBlock(^(const SFB::Audio::Decoder& /*decoder*/){
			_playerFlags.fetch_or(ePlayerFlagRenderingStarted);
		});

		// This will be called from the realtime rendering thread and as such MUST NOT BLOCK!!
		_player->SetRenderingFinishedBlock(^(const SFB::Audio::Decoder& /*decoder*/){
			_playerFlags.fetch_or(ePlayerFlagRenderingFinished);
		});

		// Update the UI 5 times per second in all run loop modes (so menus, etc. don't stop updates)
		_uiTimer = [NSTimer timerWithTimeInterval:(1.0 / 5) target:self selector:@selector(uiTimerFired:) userInfo:nil repeats:YES];

		// addTimer:forMode: will keep a reference _uiTimer
		[[NSRunLoop mainRunLoop] addTimer:_uiTimer forMode:NSRunLoopCommonModes];
	}

	return self;
}

- (void) dealloc
{
	[_uiTimer invalidate], _uiTimer = nil;

	delete _player, _player = nullptr;
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

- (IBAction) playPause:(id)sender
{
#pragma unused(sender)
	_player->PlayPause();
}

- (IBAction) seekForward:(id)sender
{
#pragma unused(sender)
	_player->SeekForward();
}

- (IBAction) seekBackward:(id)sender
{
#pragma unused(sender)
	_player->SeekBackward();
}

- (IBAction) seek:(id)sender
{
	NSParameterAssert(nil != sender);
	
	SInt64 totalFrames;
	if(_player->GetTotalFrames(totalFrames)) {
		SInt64 desiredFrame = static_cast<SInt64>([sender doubleValue] * totalFrames);
		_player->SeekToFrame(desiredFrame);
	}
}

- (IBAction) skipToNextTrack:(id)sender
{
	_player->SkipToNextTrack();
}

- (BOOL) playURL:(NSURL *)url
{
	NSParameterAssert(nil != url);

	return _player->Play((__bridge CFURLRef)url);
}

- (BOOL) enqueueURL:(NSURL *)url
{
	NSParameterAssert(nil != url);

	return _player->Enqueue((__bridge CFURLRef)url);
}

@end

@implementation PlayerWindowController (Callbacks)

- (void) uiTimerFired:(NSTimer *)timer
{
#pragma unused(timer)
	// To avoid blocking the realtime rendering thread, flags are set in the callbacks and subsequently handled here
	auto flags = _playerFlags.load();

	if(ePlayerFlagRenderingStarted & flags) {
		_playerFlags.fetch_and(~ePlayerFlagRenderingStarted);

		[self updateWindowUI];
		
		return;
	}
	else if(ePlayerFlagRenderingFinished & flags) {
		_playerFlags.fetch_and(~ePlayerFlagRenderingFinished);

		[self updateWindowUI];
		
		return;
	}

	if(!_player->IsPlaying())
		[self.playButton setTitle:@"Resume"];
	else
		[self.playButton setTitle:@"Pause"];

	SInt64 currentFrame, totalFrames;
	CFTimeInterval currentTime, totalTime;

	if(_player->GetPlaybackPositionAndTime(currentFrame, totalFrames, currentTime, totalTime)) {
		double fractionComplete = static_cast<double>(currentFrame) / static_cast<double>(totalFrames);
		
		[self.slider setDoubleValue:fractionComplete];
		[self.elapsed setDoubleValue:currentTime];
		[self.remaining setDoubleValue:(-1 * (totalTime - currentTime))];
	}
}

@end

@implementation PlayerWindowController (Private)

- (void) updateWindowUI
{
	NSURL *url = (__bridge NSURL *)_player->GetPlayingURL();
	
	// Nothing happening, reset the window
	if(nullptr == url) {
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
	
	bool seekable = _player->SupportsSeeking();
	
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

	SInt64 totalFrames;
	if(_player->GetTotalFrames(totalFrames) && -1 != totalFrames)
		[self.remaining setHidden:NO];

	// Load and display some metadata.  Normally the metadata would be read and stored in the background,
	// but for simplicity's sake it is done here.
	auto metadata = SFB::Audio::Metadata::CreateMetadataForURL((__bridge CFURLRef)url);
	if(metadata) {
		auto pictures = metadata->GetAttachedPictures();
		if(!pictures.empty())
			[self.albumArt setImage:[[NSImage alloc] initWithData:(__bridge NSData *)pictures.front()->GetData()]];
		else
			[self.albumArt setImage:nil];

		if(metadata->GetTitle())
			[self.title setStringValue:(__bridge NSString *)metadata->GetTitle()];
		else
			[self.title setStringValue:@""];

		if(metadata->GetArtist())
			[self.artist setStringValue:(__bridge NSString *)metadata->GetArtist()];
		else
			[self.artist setStringValue:@""];
	}
	else {
		[self.albumArt setImage:[NSImage imageNamed:@"NSApplicationIcon"]];
		[self.title setStringValue:@""];
		[self.artist setStringValue:@""];
	}
}

@end
