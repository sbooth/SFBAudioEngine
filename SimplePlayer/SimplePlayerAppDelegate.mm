/*
 *  Copyright (C) 2009, 2010, 2011 Stephen F. Booth <me@sbooth.org>
 *  All Rights Reserved
 */

#import "SimplePlayerAppDelegate.h"
#import "PlayerWindowController.h"

#include <SFBAudioEngine/AudioDecoder.h>

@implementation SimplePlayerAppDelegate

@synthesize playerWindowController = _playerWindowController;
@synthesize openURLPanel = _openURLPanel;
@synthesize openURLPanelTextField = _openURLPanelTextField;

- (void) dealloc
{
	[_playerWindowController release], _playerWindowController = nil;
	[_openURLPanel release], _openURLPanel = nil;
	[_openURLPanelTextField release], _openURLPanelTextField = nil;
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
	
	return [_playerWindowController playURL:[NSURL fileURLWithPath:filename]];
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
		[_playerWindowController playURL:[NSURL fileURLWithPath:[filenames objectAtIndex:0]]];
	}	

	CFRelease(supportedTypes), supportedTypes = NULL;
}

- (IBAction) openURL:(id)sender
{
	[_openURLPanel center];
	[_openURLPanel makeKeyAndOrderFront:sender];	
}

- (IBAction) openURLPanelOpenAction:(id)sender
{
	[_openURLPanel orderOut:sender];
	NSURL *url = [NSURL URLWithString:[_openURLPanelTextField stringValue]];
	[_playerWindowController playURL:url];
}

- (IBAction) openURLPanelCancelAction:(id)sender
{
	[_openURLPanel orderOut:sender];
}

@end
