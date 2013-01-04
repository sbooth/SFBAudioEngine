/*
 *  Copyright (C) 2009, 2010, 2011, 2012 Stephen F. Booth <me@sbooth.org>
 *  All Rights Reserved
 */

#import "SimplePlayerAppDelegate.h"
#import "PlayerWindowController.h"

#include <SFBAudioEngine/AudioDecoder.h>
#include <SFBAudioEngine/Logger.h>

@implementation SimplePlayerAppDelegate

@synthesize playerWindowController, openURLPanel, openURLPanelTextField;

- (void) applicationDidFinishLaunching:(NSNotification *)aNotification
{
#pragma unused(aNotification)
	// Enable verbose logging to stderr
	asl_add_log_file(nullptr, STDERR_FILENO);
	::logger::SetCurrentLevel(::logger::debug);

	// Show the player window
	[playerWindowController showWindow:self];
}

- (BOOL) applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)theApplication
{
#pragma unused(theApplication)
	return YES;
}

- (BOOL) application:(NSApplication *)theApplication openFile:(NSString *)filename
{
#pragma unused(theApplication)
	NSArray *supportedTypes = (__bridge_transfer NSArray *)AudioDecoder::CreateSupportedFileExtensions();
	if(![supportedTypes containsObject:[filename pathExtension]])
		return NO;
	
	return [playerWindowController playURL:[NSURL fileURLWithPath:filename]];
}

- (IBAction) openFile:(id)sender
{
#pragma unused(sender)
	NSOpenPanel *openPanel = [NSOpenPanel openPanel];
	
	[openPanel setAllowsMultipleSelection:NO];
	[openPanel setCanChooseDirectories:NO];
	[openPanel setAllowedFileTypes:(__bridge_transfer NSArray *)AudioDecoder::CreateSupportedFileExtensions()];

	if(NSFileHandlingPanelOKButton == [openPanel runModal]) {
		NSArray *URLs = [openPanel URLs];
		[playerWindowController playURL:[URLs objectAtIndex:0]];
	}	
}

- (IBAction) openURL:(id)sender
{
	[openURLPanel center];
	[openURLPanel makeKeyAndOrderFront:sender];	
}

- (IBAction) openURLPanelOpenAction:(id)sender
{
	[openURLPanel orderOut:sender];
	NSURL *url = [NSURL URLWithString:[openURLPanelTextField stringValue]];
	[playerWindowController playURL:url];
}

- (IBAction) openURLPanelCancelAction:(id)sender
{
	[openURLPanel orderOut:sender];
}

@end
