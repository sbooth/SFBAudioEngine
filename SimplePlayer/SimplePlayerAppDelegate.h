/*
 * Copyright (c) 2009 - 2017 Stephen F. Booth <me@sbooth.org>
 *
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <Cocoa/Cocoa.h>

@class PlayerWindowController;

@interface SimplePlayerAppDelegate : NSObject <NSApplicationDelegate>
{}

// IB outlets
@property (nonatomic, weak) IBOutlet PlayerWindowController *	playerWindowController;
@property (nonatomic, weak) IBOutlet NSWindow *					openURLPanel;
@property (nonatomic, weak) IBOutlet NSTextField *				openURLPanelTextField;

- (IBAction) openFile:(id)sender;
- (IBAction) openURL:(id)sender;

- (IBAction) enqueueFiles:(id)sender;

- (IBAction) openURLPanelOpenAction:(id)sender;
- (IBAction) openURLPanelCancelAction:(id)sender;

@end
