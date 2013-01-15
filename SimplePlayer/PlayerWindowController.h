/*
 *  Copyright (C) 2009, 2010, 2011, 2012, 2013 Stephen F. Booth <me@sbooth.org>
 *  All Rights Reserved
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

// Action methods
- (IBAction) playPause:(id)sender;

- (IBAction) seekForward:(id)sender;
- (IBAction) seekBackward:(id)sender;

- (IBAction) seek:(id)sender;

// Attempt to play the specified file- returns YES if successful
- (BOOL) playURL:(NSURL *)url;

// Enqueues the URL on the player
- (BOOL) enqueueURL:(NSURL *)url;

@end
