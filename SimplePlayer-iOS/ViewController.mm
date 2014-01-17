/*
 *  Copyright (C) 2011, 2012, 2013 Stephen F. Booth <me@sbooth.org>
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

#import "ViewController.h"

#include <atomic>

#include "AudioPlayer.h"
#include "AudioDecoder.h"

#include "Logger.h"

// ========================================
// Player flags
// ========================================
enum ePlayerFlags : unsigned int {
	ePlayerFlagRenderingStarted			= 1u << 0,
	ePlayerFlagRenderingFinished		= 1u << 1
};

@interface ViewController ()
{
@private
	SFB::Audio::Player	*_player;		// The player instance
	std::atomic_uint	_playerFlags;
	NSTimer				*_userInterfaceTimer;
	BOOL				_resume;
}
@end

@interface ViewController (Callbacks)
- (void)applicationWillResignActive:(UIApplication *)application;
- (void)applicationDidEnterBackground:(UIApplication *)application;
- (void)applicationWillEnterForeground:(UIApplication *)application;
- (void)applicationDidBecomeActive:(UIApplication *)application;
- (void) userInterfaceTimerFired:(NSTimer *)timer;
@end

@interface ViewController (Private)
- (void) updateUserInterface;
@end

@implementation ViewController

- (void)viewDidLoad
{
    [super viewDidLoad];

	[[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(applicationWillResignActive:) name:UIApplicationWillEnterForegroundNotification object:nil];
	[[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(applicationDidEnterBackground:) name:UIApplicationDidEnterBackgroundNotification object:nil];
	[[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(applicationWillEnterForeground:) name:UIApplicationWillEnterForegroundNotification object:nil];
	[[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(applicationDidBecomeActive:) name:UIApplicationDidBecomeActiveNotification object:nil];

	try {
		_player = new SFB::Audio::Player();
	}

	catch(std::exception& e) {
		LOGGER_CRIT("org.sbooth.SimplePlayer-iOS", "Unable to create an AudioPlayer: " << e.what());
	}

	_playerFlags = 0;

	// This will be called from the realtime rendering thread and as such MUST NOT BLOCK!!
	_player->SetRenderingStartedBlock(^(const SFB::Audio::Decoder& /*decoder*/){
		_playerFlags.fetch_or(ePlayerFlagRenderingStarted);
	});

	// This will be called from the realtime rendering thread and as such MUST NOT BLOCK!!
	_player->SetRenderingFinishedBlock(^(const SFB::Audio::Decoder& /*decoder*/){
		_playerFlags.fetch_or(ePlayerFlagRenderingFinished);
	});

	// Set up a UI timer that fires 5 times per second
	_userInterfaceTimer = [NSTimer scheduledTimerWithTimeInterval:(1.0 / 5.0) target:self selector:@selector(userInterfaceTimerFired:) userInfo:nil repeats:YES];

	[self updateUserInterface];
}

- (void)didReceiveMemoryWarning
{
    [super didReceiveMemoryWarning];
    // Dispose of any resources that can be recreated.
}

- (void)dealloc
{
	delete _player, _player = nullptr;
}

- (IBAction) playPause:(id)sender
{
	_player->PlayPause();
}

- (IBAction) seekForward:(id)sender
{
	_player->SeekForward();
}

- (IBAction) seekBackward:(id)sender
{
	_player->SeekBackward();
}

- (IBAction) seek:(id)sender
{
	NSParameterAssert(nil != sender);

	SInt64 totalFrames;
	if(_player->GetTotalFrames(totalFrames)) {
		SInt64 desiredFrame = (SInt64)([(UISlider *)sender value] * totalFrames);
		_player->SeekToFrame(desiredFrame);
	}
}

- (IBAction) playTestTrack:(id)sender
{
	if(![self playURL:[[NSBundle mainBundle] URLForResource:@"tone16bit" withExtension:@"flac"]])
		LOGGER_ERR("org.sbooth.SimplePlayer-iOS", "Could not play");
}

- (BOOL) playURL:(NSURL *)url;
{
	if(nil == url)
		return NO;

	return _player->Play((__bridge CFURLRef)url);
}

@end

@implementation ViewController (Callbacks)

- (void)applicationWillResignActive:(UIApplication *)application
{
	_resume = _player->IsPlaying();
	_player->Pause();
}

- (void)applicationDidEnterBackground:(UIApplication *)application
{
	[_userInterfaceTimer invalidate], _userInterfaceTimer = nil;
}

- (void)applicationWillEnterForeground:(UIApplication *)application
{
	_userInterfaceTimer = [NSTimer scheduledTimerWithTimeInterval:(1.0 / 5.0) target:self selector:@selector(userInterfaceTimerFired:) userInfo:nil repeats:YES];
}

- (void)applicationDidBecomeActive:(UIApplication *)application
{
	if(_resume)
		_player->Play();
}

- (void) userInterfaceTimerFired:(NSTimer *)timer
{
	// To avoid blocking the realtime rendering thread, flags are set in the callbacks and subsequently handled here
	auto flags = _playerFlags.load();

	if(ePlayerFlagRenderingStarted & flags) {
		_playerFlags.fetch_and(~ePlayerFlagRenderingStarted);

		[self updateUserInterface];

		return;
	}
	else if(ePlayerFlagRenderingFinished & flags) {
		_playerFlags.fetch_and(~ePlayerFlagRenderingFinished);

		[self updateUserInterface];

		return;
	}

	if(!_player->IsPlaying())
		[self.playButton setTitle:@"Resume" forState:UIControlStateNormal];
	else
		[self.playButton setTitle:@"Pause" forState:UIControlStateNormal];

	SInt64 currentFrame, totalFrames;
	CFTimeInterval currentTime, totalTime;

	if(_player->GetPlaybackPositionAndTime(currentFrame, totalFrames, currentTime, totalTime)) {
		float fractionComplete = (float)currentFrame / (float)totalFrames;

		[self.slider setValue:fractionComplete];
		[self.elapsed setText:[NSString stringWithFormat:@"%f", currentTime]];
		[self.remaining setText:[NSString stringWithFormat:@"%f", (-1 * (totalTime - currentTime))]];
	}
}

@end

@implementation ViewController (Private)

- (void) updateUserInterface
{
	NSURL *url = (__bridge NSURL *)_player->GetPlayingURL();

	// Nothing happening, reset the window
	if(nullptr == url) {
		[self.slider setEnabled:NO];
		[self.playButton setEnabled:NO];
		[self.backwardButton setEnabled:NO];
		[self.forwardButton setEnabled:NO];

		[self.elapsed setHidden:YES];
		[self.remaining setHidden:YES];

		[self.trackTitle setText:@""];

		return;
	}

	bool seekable = _player->SupportsSeeking();

	// Update the window's title and represented file
	[self.trackTitle setText:[url lastPathComponent]];

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
