/*
 *  Copyright (C) 2009, 2010, 2011, 2012, 2013 Stephen F. Booth <me@sbooth.org>
 *  All Rights Reserved
 */

#import "PlayerWindowController.h"

#include <libkern/OSAtomic.h>

#include <SFBAudioEngine/AudioPlayer.h>
#include <SFBAudioEngine/AudioDecoder.h>

// ========================================
// Player flags
// ========================================
enum {
	ePlayerFlagRenderingStarted			= 1 << 0,
	ePlayerFlagRenderingFinished		= 1 << 1
};

@interface PlayerWindowController ()
{
@private
	AudioPlayer		*_player;		// The player instance
	uint32_t		_playerFlags;
	NSTimer			*_uiTimer;
	BOOL			_playWhenDecodingStarts;
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
		_player = new AudioPlayer();

		_playerFlags = 0;

		// Once decoding has started, begin playing the track
		_player->SetDecodingStartedBlock(^(const AudioDecoder */*decoder*/){
			if(_playWhenDecodingStarts) {
				_playWhenDecodingStarts = NO;
				_player->Play();
			}
		});

		// This will be called from the realtime rendering thread and as such MUST NOT BLOCK!!
		_player->SetRenderingStartedBlock(^(const AudioDecoder */*decoder*/){
			OSAtomicTestAndSetBarrier(7 /* ePlayerFlagRenderingStarted */, &_playerFlags);
		});

		// This will be called from the realtime rendering thread and as such MUST NOT BLOCK!!
		_player->SetRenderingFinishedBlock(^(const AudioDecoder */*decoder*/){
			OSAtomicTestAndSetBarrier(6 /* ePlayerFlagRenderingFinished */, &_playerFlags);
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

- (BOOL) playURL:(NSURL *)url
{
	NSParameterAssert(nil != url);

	AudioDecoder *decoder = AudioDecoder::CreateDecoderForURL((__bridge CFURLRef)url);
	if(nullptr == decoder)
		return NO;

	_player->Stop();

	_playWhenDecodingStarts = YES;
	if(!decoder->Open() || !_player->Enqueue(decoder)) {
		_playWhenDecodingStarts = NO;
		delete decoder;
		return NO;
	}

	return YES;
}

- (BOOL) enqueueURL:(NSURL *)url
{
	NSParameterAssert(nil != url);

	AudioDecoder *decoder = AudioDecoder::CreateDecoderForURL((__bridge CFURLRef)url);
	if(nullptr == decoder)
		return NO;

	if(!decoder->Open() || !_player->Enqueue(decoder)) {
		delete decoder;
		return NO;
	}

	return YES;
}

@end

@implementation PlayerWindowController (Callbacks)

- (void) uiTimerFired:(NSTimer *)timer
{
#pragma unused(timer)
	// To avoid blocking the realtime rendering thread, flags are set in the callbacks and subsequently handled here
	if(ePlayerFlagRenderingStarted & _playerFlags) {
		OSAtomicTestAndClearBarrier(7 /* ePlayerFlagRenderingStarted */, &_playerFlags);
		
		[self updateWindowUI];
		
		return;
	}
	else if(ePlayerFlagRenderingFinished & _playerFlags) {
		OSAtomicTestAndClearBarrier(6 /* ePlayerFlagRenderingFinished */, &_playerFlags);
		
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
}

@end
