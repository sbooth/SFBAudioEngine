/*
 *  Copyright (C) 2011, 2012 Stephen F. Booth <me@sbooth.org>
 *  All Rights Reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are
 *  met:
 *
 *    - Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    - Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *    - Neither the name of Stephen F. Booth nor the names of its 
 *      contributors may be used to endorse or promote products derived
 *      from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "SimplePlayer_iOSAppDelegate.h"
#include "AudioDecoder.h"
#include "Logger.h"

#include <libkern/OSAtomic.h>

// ========================================
// Player flags
// ========================================
enum {
	ePlayerFlagRenderingStarted			= 1 << 0,
	ePlayerFlagRenderingFinished		= 1 << 1
};

volatile static uint32_t sPlayerFlags = 0;

@interface SimplePlayer_iOSAppDelegate (Callbacks)
- (void) decodingStarted:(const AudioDecoder *)decoder;
- (void) uiTimerFired:(NSTimer *)timer;
@end

@interface SimplePlayer_iOSAppDelegate (Private)
- (void) updateWindowUI;
@end

static void decodingStarted(void *context, const AudioDecoder *decoder)
{
	[(SimplePlayer_iOSAppDelegate *)context decodingStarted:decoder];
}

// This is called from the realtime rendering thread and as such MUST NOT BLOCK!!
static void renderingStarted(void *context, const AudioDecoder *decoder)
{
#pragma unused(context)
#pragma unused(decoder)
	OSAtomicTestAndSetBarrier(7 /* ePlayerFlagRenderingStarted */, &sPlayerFlags);
}

// This is called from the realtime rendering thread and as such MUST NOT BLOCK!!
static void renderingFinished(void *context, const AudioDecoder *decoder)
{
#pragma unused(context)
#pragma unused(decoder)
	OSAtomicTestAndSetBarrier(6 /* ePlayerFlagRenderingFinished */, &sPlayerFlags);
}

@implementation SimplePlayer_iOSAppDelegate

@synthesize window = _window;
@synthesize slider = _slider;
@synthesize playButton = _playButton;
@synthesize backwardButton = _backwardButton;
@synthesize forwardButton = _forwardButton;
@synthesize elapsed = _elapsed;
@synthesize remaining = _remaining;
@synthesize title = _title;

- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions
{
	asl_add_log_file(nullptr, STDERR_FILENO);
	::logger::SetCurrentLevel(::logger::debug);

	_player = new AudioPlayer();

	// Set up a UI timer that fires 5 times per second
	_uiTimer = [NSTimer scheduledTimerWithTimeInterval:(1.0 / 5.0) target:self selector:@selector(uiTimerFired:) userInfo:nil repeats:YES];
	
	[self.window makeKeyAndVisible];

	// Just play the file
	if(![self playFile:[[NSBundle mainBundle] pathForResource:@"test" ofType:@"flac"]])
		NSLog(@"Couldn't play");

	return YES;
}

- (void)applicationWillResignActive:(UIApplication *)application
{
	_resume = _player->IsPlaying();
	_player->Pause();
}

- (void)applicationDidEnterBackground:(UIApplication *)application
{
	[_uiTimer invalidate], _uiTimer = nil;
}

- (void)applicationWillEnterForeground:(UIApplication *)application
{
	_uiTimer = [NSTimer scheduledTimerWithTimeInterval:(1.0 / 5.0) target:self selector:@selector(uiTimerFired:) userInfo:nil repeats:YES];
}

- (void)applicationDidBecomeActive:(UIApplication *)application
{
	if(_resume)
		_player->Play();
}

- (void)applicationWillTerminate:(UIApplication *)application
{}

- (void)dealloc
{
	delete _player, _player = nullptr;
	
	[_window release], _window = nil;
	[_slider release], _slider = nil;
	[_playButton release], _playButton = nil;
	[_backwardButton release], _backwardButton = nil;
	[_forwardButton release], _forwardButton = nil;

    [super dealloc];
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
		SInt64 desiredFrame = static_cast<SInt64>([(UISlider *)sender value] * totalFrames);
		_player->SeekToFrame(desiredFrame);
	}
}

- (BOOL) playFile:(NSString *)file
{
	if(nil == file)
		return NO;
	
	NSURL *url = [NSURL fileURLWithPath:file];
	
	AudioDecoder *decoder = AudioDecoder::CreateDecoderForURL(reinterpret_cast<CFURLRef>(url));
	if(nullptr == decoder)
		return NO;
	
	_player->Stop();
	
	// Register for rendering started/finished notifications so the UI can be updated properly
	decoder->SetDecodingStartedCallback(decodingStarted, self);
	decoder->SetRenderingStartedCallback(renderingStarted, self);
	decoder->SetRenderingFinishedCallback(renderingFinished, self);
	
	if(!decoder->Open() || !_player->Enqueue(decoder)) {
		delete decoder;
		return NO;
	}
	
	return YES;
}

@end

@implementation SimplePlayer_iOSAppDelegate (Callbacks)

- (void) decodingStarted:(const AudioDecoder *)decoder
{
#pragma unused(decoder)
	_player->Play();
}

- (void) uiTimerFired:(NSTimer *)timer
{
#pragma unused(timer)
	// To avoid blocking the realtime rendering thread, flags are set in the callbacks and subsequently handled here
	if(ePlayerFlagRenderingStarted & sPlayerFlags) {
		OSAtomicTestAndClearBarrier(7 /* ePlayerFlagRenderingStarted */, &sPlayerFlags);
		
		[self updateWindowUI];
		
		return;
	}
	else if(ePlayerFlagRenderingFinished & sPlayerFlags) {
		OSAtomicTestAndClearBarrier(6 /* ePlayerFlagRenderingFinished */, &sPlayerFlags);
		
		[self updateWindowUI];
		
		return;
	}
	
	if(!_player->IsPlaying())
		[_playButton setTitle:@"Resume" forState:UIControlStateNormal];
	else
		[_playButton setTitle:@"Pause" forState:UIControlStateNormal];
	
	SInt64 currentFrame, totalFrames;
	CFTimeInterval currentTime, totalTime;
	
	if(_player->GetPlaybackPositionAndTime(currentFrame, totalFrames, currentTime, totalTime)) {
		float fractionComplete = static_cast<float>(currentFrame) / static_cast<float>(totalFrames);

		[_slider setValue:fractionComplete];
		[_elapsed setText:[NSString stringWithFormat:@"%f", currentTime]];
		[_remaining setText:[NSString stringWithFormat:@"%f", (-1 * (totalTime - currentTime))]];
	}
}

@end

@implementation SimplePlayer_iOSAppDelegate (Private)

- (void) updateWindowUI
{
	NSURL *url = (NSURL *)_player->GetPlayingURL();
	
	// Nothing happening, reset the window
	if(nullptr == url) {
		[_slider setEnabled:NO];
//		[_playButton setState:NSOffState];
		[_playButton setEnabled:NO];
		[_backwardButton setEnabled:NO];
		[_forwardButton setEnabled:NO];
		
		[_elapsed setHidden:YES];
		[_remaining setHidden:YES];
		
		return;
	}
	
	bool seekable = _player->SupportsSeeking();
	
	// Update the window's title and represented file
	[_title setText:[url lastPathComponent]];
	
	// Update the UI
	[_slider setEnabled:seekable];
	[_playButton setEnabled:YES];
	[_backwardButton setEnabled:seekable];
	[_forwardButton setEnabled:seekable];
	
	// Show the times
	[_elapsed setHidden:NO];
	
	SInt64 totalFrames;
	if(_player->GetTotalFrames(totalFrames) && -1 != totalFrames)
		[_remaining setHidden:NO];	
}

@end
