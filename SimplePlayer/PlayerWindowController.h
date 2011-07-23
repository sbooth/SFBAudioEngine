/*
 *  Copyright (C) 2009 Stephen F. Booth <me@sbooth.org>
 *  All Rights Reserved
 */

#import <Cocoa/Cocoa.h>

@interface PlayerWindowController : NSWindowController
{
@private
	void			*_player;				// An instance of AudioPlayer
	NSTimer			*_uiTimer;				// User interface update timer

	NSSlider		*_slider;				// UI elements
	NSTextField		*_elapsed;
	NSTextField		*_remaining;
	NSButton		*_playButton;
	NSButton		*_forwardButton;
	NSButton		*_backwardButton;
}

// IB properties
@property (assign) IBOutlet NSSlider *		slider;
@property (assign) IBOutlet NSTextField *	elapsed;
@property (assign) IBOutlet NSTextField *	remaining;
@property (assign) IBOutlet NSButton *		playButton;
@property (assign) IBOutlet NSButton *		forwardButton;
@property (assign) IBOutlet NSButton *		backwardButton;

// Action methods
- (IBAction) playPause:(id)sender;

- (IBAction) seekForward:(id)sender;
- (IBAction) seekBackward:(id)sender;

- (IBAction) seek:(id)sender;

// Attempt to play the specified file- returns YES if successful
- (BOOL) playURL:(NSURL *)url;

@end
