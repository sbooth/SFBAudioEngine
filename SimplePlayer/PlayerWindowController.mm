/*
 *  Copyright (C) 2009, 2010 Stephen F. Booth <me@sbooth.org>
 *  All Rights Reserved
 */

#import "PlayerWindowController.h"

#define DSP_ENABLED 1

#if DSP_ENABLED
#  include <SFBAudioEngine/DSPAudioPlayer.h>
#else
#  include <SFBAudioEngine/AudioPlayer.h>
#endif /* DSP_ENABLED */
#include <SFBAudioEngine/AudioDecoder.h>

#if DSP_ENABLED
#  define PLAYER (static_cast<DSPAudioPlayer *>(_player))
#else
#  define PLAYER (static_cast<AudioPlayer *>(_player))
#endif /* DSP_ENABLED */

@interface PlayerWindowController (Callbacks)
- (void) renderingStarted:(AudioDecoder *)decoder;
- (void) renderingFinished:(AudioDecoder *)decoder;
- (void) uiTimerFired:(NSTimer *)timer;
@end

@interface PlayerWindowController (Private)
- (void) updateWindowUI;
@end

static void renderingStarted(void *context, const AudioDecoder *decoder)
{
	NSCParameterAssert(NULL != context);
	
	PlayerWindowController *wc = static_cast<PlayerWindowController *>(context);
	[wc renderingStarted:const_cast<AudioDecoder *>(decoder)];
}

static void renderingFinished(void *context, const AudioDecoder *decoder)
{
	NSCParameterAssert(NULL != context);
	
	PlayerWindowController *wc = static_cast<PlayerWindowController *>(context);
	[wc renderingFinished:const_cast<AudioDecoder *>(decoder)];
}

@implementation PlayerWindowController

@synthesize slider = _slider;
@synthesize elapsed = _elapsed;
@synthesize remaining = _remaining;
@synthesize playButton = _playButton;
@synthesize forwardButton = _forwardButton;
@synthesize backwardButton = _backwardButton;

- (id) init
{
	if(nil == (self = [super initWithWindowNibName:@"PlayerWindow"])) {
		[self release];
		return nil;
	}
	
#if DSP_ENABLED
	_player = new DSPAudioPlayer();
#else	
	_player = new AudioPlayer();
#endif

	if(false == PLAYER->SetOutputDeviceUID(CFSTR("AppleUSBAudioEngine:Texas Instruments:Benchmark 1.0:fd111000:1")))
		puts("Couldn't set output device UID");

	// Update the UI 5 times per second in all run loop modes (so menus, etc. don't stop updates)
	_uiTimer = [NSTimer timerWithTimeInterval:(1.0 / 5) target:self selector:@selector(uiTimerFired:) userInfo:nil repeats:YES];

	// addTimer:forMode: will retain _uiTimer
	[[NSRunLoop mainRunLoop] addTimer:_uiTimer forMode:NSRunLoopCommonModes];
	
	return self;
}

- (void) dealloc
{
	[_uiTimer invalidate], _uiTimer = nil;
	
	delete PLAYER, _player = NULL;
	
	[super dealloc];
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
	PLAYER->PlayPause();
}

- (IBAction) seekForward:(id)sender
{
#pragma unused(sender)
	PLAYER->SeekForward();
}

- (IBAction) seekBackward:(id)sender
{
#pragma unused(sender)
	PLAYER->SeekBackward();
}

- (IBAction) seek:(id)sender
{
	NSParameterAssert(nil != sender);
	
	SInt64 desiredFrame = static_cast<SInt64>([sender doubleValue] * PLAYER->GetTotalFrames());
	
	PLAYER->SeekToFrame(desiredFrame);
}

- (BOOL) playFile:(NSString *)file
{
	NSParameterAssert(nil != file);

	NSURL *url = [NSURL fileURLWithPath:file];
	
	AudioDecoder *decoder = AudioDecoder::CreateDecoderForURL(reinterpret_cast<CFURLRef>(url));
	if(NULL == decoder)
		return NO;

	PLAYER->Stop();

	// Register for rendering started/finished notifications so the UI can be updated properly
	decoder->SetRenderingStartedCallback(renderingStarted, self);
	decoder->SetRenderingFinishedCallback(renderingFinished, self);
	
	if(true == PLAYER->Enqueue(decoder)) {
		PLAYER->Play();
		[[NSDocumentController sharedDocumentController] noteNewRecentDocumentURL:url];
	}
	else {
		delete decoder;
		return NO;
	}
	
	return YES;
}

@end

@implementation PlayerWindowController (Callbacks)

// This is called from the real-time rendering thread so it shouldn't do much!
- (void) renderingStarted:(AudioDecoder *)decoder
{
#pragma unused(decoder)
	
	// No autorelease pool is required since no Objective-C objects are created
	
	[self performSelectorOnMainThread:@selector(updateWindowUI) withObject:nil waitUntilDone:NO];
}

// This is also called from the rendering thread
- (void) renderingFinished:(AudioDecoder *)decoder
{
#pragma unused(decoder)
	
	// No autorelease pool is required since no Objective-C objects are created
	
	// If there isn't another decoder queued, the UI should be disabled
	[self performSelectorOnMainThread:@selector(updateWindowUI) withObject:nil waitUntilDone:NO];
}

- (void) uiTimerFired:(NSTimer *)timer
{
#pragma unused(timer)
	if(false == PLAYER->IsPlaying())
		[_playButton setTitle:@"Resume"];
	else
		[_playButton setTitle:@"Pause"];
	
	double fractionComplete = static_cast<double>(PLAYER->GetCurrentFrame()) / static_cast<double>(PLAYER->GetTotalFrames());
	
	[_slider setDoubleValue:fractionComplete];
	[_elapsed setDoubleValue:PLAYER->GetCurrentTime()];
	[_remaining setDoubleValue:(-1 * PLAYER->GetRemainingTime())];
}

@end

@implementation PlayerWindowController (Private)

- (void) updateWindowUI
{
	NSURL *url = reinterpret_cast<const NSURL *>(PLAYER->GetPlayingURL());
	
	// Nothing happening, reset the window
	if(NULL == url) {
		[[self window] setRepresentedURL:nil];
		[[self window] setTitle:@""];
		
		[_slider setEnabled:NO];
		[_playButton setState:NSOffState];
		[_playButton setEnabled:NO];
		[_backwardButton setEnabled:NO];
		[_forwardButton setEnabled:NO];
		
		[_elapsed setHidden:YES];
		[_remaining setHidden:YES];
		
		return;
	}
	
	bool seekable = PLAYER->SupportsSeeking();
	
	// Update the window's title and represented file
	[[self window] setRepresentedURL:url];
	[[self window] setTitle:[[NSFileManager defaultManager] displayNameAtPath:[url path]]];
	
	[[NSDocumentController sharedDocumentController] noteNewRecentDocumentURL:url];
	
	// Update the UI
	[_slider setEnabled:seekable];
	[_playButton setEnabled:YES];
	[_backwardButton setEnabled:seekable];
	[_forwardButton setEnabled:seekable];
	
	// Show the times
	[_elapsed setHidden:NO];
	[_remaining setHidden:NO];	
}

@end
