/*
 *  Copyright (C) 2009 Stephen F. Booth <me@sbooth.org>
 *  All Rights Reserved
 */

#import "SimplePlayerAppDelegate.h"
#import "PlayerWindowController.h"

#include <SFBAudioEngine/AudioDecoder.h>

@implementation SimplePlayerAppDelegate

@synthesize playerWindowController = _playerWindowController;

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
	BOOL extensionValid = [reinterpret_cast<const NSArray *>(supportedTypes) containsObject:[filename pathExtension]];
	CFRelease(supportedTypes), supportedTypes = NULL;
	
	if(NO == extensionValid)
		return NO;
	
	return [_playerWindowController playFile:filename];
}

- (IBAction) openFile:(id)sender
{
#pragma unused(sender)
	NSOpenPanel *openPanel = [NSOpenPanel openPanel];
	
	[openPanel setAllowsMultipleSelection:NO];
	[openPanel setCanChooseDirectories:NO];
	
	CFArrayRef supportedTypes = AudioDecoder::CreateSupportedFileExtensions();
	
	if(NSCancelButton == [openPanel runModalForDirectory:nil file:nil types:reinterpret_cast<const NSArray *>(supportedTypes)])
		return;
	
	CFRelease(supportedTypes), supportedTypes = NULL;
	
	NSArray *filenames = [openPanel filenames];
	
	[_playerWindowController playFile:[filenames objectAtIndex:0]];	
}

@end
