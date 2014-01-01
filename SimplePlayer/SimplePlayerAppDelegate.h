/*
 *  Copyright (C) 2009, 2010, 2011, 2012, 2013, 2014 Stephen F. Booth <me@sbooth.org>
 *  All Rights Reserved
 */

#import <Cocoa/Cocoa.h>

@class PlayerWindowController;

@interface SimplePlayerAppDelegate : NSObject
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
