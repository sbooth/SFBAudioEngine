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
}

@property (assign) IBOutlet PlayerWindowController * playerWindowController;

- (IBAction) openFile:(id)sender;

@end
