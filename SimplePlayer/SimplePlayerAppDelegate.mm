/*
 *  Copyright (C) 2009, 2010 Stephen F. Booth <me@sbooth.org>
 *  All Rights Reserved
 */

#import "SimplePlayerAppDelegate.h"
#import "PlayerWindowController.h"

#include <SFBAudioEngine/AudioDecoder.h>

@implementation SimplePlayerAppDelegate

@synthesize playerWindowController = _playerWindowController;

- (void) dealloc
{
	[_playerWindowController release], _playerWindowController = nil;
	[super dealloc];
}

- (void) applicationDidFinishLaunching:(NSNotification *)aNotification
{
#pragma unused(aNotification)
	[_playerWindowController showWindow:self];
}

- (BOOL) applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)theApplication
{
#pragma unused(theApplication)
	return YES;
}

- (BOOL) application:(NSApplication *)theApplication openFile:(NSString *)filename
{
#pragma unused(theApplication)
	CFArrayRef supportedTypes = AudioDecoder::CreateSupportedFileExtensions();	
	BOOL extensionValid = [(NSArray *)supportedTypes containsObject:[filename pathExtension]];
	CFRelease(supportedTypes), supportedTypes = NULL;
	
	if(NO == extensionValid)
		return NO;
	
	return [_playerWindowController playFile:filename];
}

- (IBAction) openFile:(id)sender
{
#pragma unused(sender)
	NSOpenPanel *openPanel = [NSOpenPanel openPanel];
	
	CFArrayRef supportedTypes = AudioDecoder::CreateSupportedFileExtensions();

	[openPanel setAllowsMultipleSelection:NO];
	[openPanel setCanChooseDirectories:NO];

	if(NSFileHandlingPanelOKButton == [openPanel runModalForTypes:(NSArray *)supportedTypes]) {
		NSArray *filenames = [openPanel filenames];
		[_playerWindowController playFile:[filenames objectAtIndex:0]];	
	}	

	CFRelease(supportedTypes), supportedTypes = NULL;
}

@end
