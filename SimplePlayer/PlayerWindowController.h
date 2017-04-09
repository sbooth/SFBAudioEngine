/*
 * Copyright (c) 2009 - 2017 Stephen F. Booth <me@sbooth.org>
 *
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <Cocoa/Cocoa.h>

@interface PlayerWindowController : NSWindowController

// IB properties
@property (nonatomic, weak) IBOutlet NSSlider *		slider;
@property (nonatomic, weak) IBOutlet NSTextField *	elapsed;
@property (nonatomic, weak) IBOutlet NSTextField *	remaining;
@property (nonatomic, weak) IBOutlet NSButton *		playButton;
@property (nonatomic, weak) IBOutlet NSButton *		forwardButton;
@property (nonatomic, weak) IBOutlet NSButton *		backwardButton;
@property (nonatomic, weak) IBOutlet NSImageView *	albumArt;
@property (nonatomic, weak) IBOutlet NSTextField *	title;
@property (nonatomic, weak) IBOutlet NSTextField *	artist;

// Action methods
- (IBAction) playPause:(id)sender;

- (IBAction) seekForward:(id)sender;
- (IBAction) seekBackward:(id)sender;

- (IBAction) seek:(id)sender;

- (IBAction) skipToNextTrack:(id)sender;

// Attempt to play the specified file- returns YES if successful
- (BOOL) playURL:(NSURL *)url;

// Enqueues the URL on the player
- (BOOL) enqueueURL:(NSURL *)url;

@end
