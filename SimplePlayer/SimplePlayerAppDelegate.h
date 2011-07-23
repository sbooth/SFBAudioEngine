/*
 *  Copyright (C) 2009 Stephen F. Booth <me@sbooth.org>
 *  All Rights Reserved
 */

#import <Cocoa/Cocoa.h>

@class PlayerWindowController;

@interface SimplePlayerAppDelegate : NSObject
{
@private
	PlayerWindowController *_playerWindowController;
	NSWindow *_openURLPanel;
	NSTextField *_openURLPanelTextField;
}

@property (assign) IBOutlet PlayerWindowController * playerWindowController;
@property (assign) IBOutlet NSWindow * openURLPanel;
@property (assign) IBOutlet NSTextField * openURLPanelTextField;

- (IBAction) openFile:(id)sender;
- (IBAction) openURL:(id)sender;

- (IBAction) openURLPanelOpenAction:(id)sender;
- (IBAction) openURLPanelCancelAction:(id)sender;

@end
