/*
 * Copyright (c) 2009 - 2020 Stephen F. Booth <me@sbooth.org>
 *
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import "SimplePlayerAppDelegate.h"
#import "PlayerWindowController.h"

#include <SFBAudioEngine/AudioDecoder.h>

@implementation SimplePlayerAppDelegate

- (void) applicationDidFinishLaunching:(NSNotification *)notification
{
#pragma unused(notification)
	// Show the player window
	[self.playerWindowController showWindow:self];
}

- (BOOL) applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender
{
#pragma unused(sender)
	return YES;
}

- (BOOL) application:(NSApplication *)sender openFile:(NSString *)filename
{
#pragma unused(sender)
	return [self.playerWindowController playURL:[NSURL fileURLWithPath:filename]];
}

- (IBAction) openFile:(id)sender
{
#pragma unused(sender)
	NSOpenPanel *openPanel = [NSOpenPanel openPanel];

	[openPanel setAllowsMultipleSelection:NO];
	[openPanel setCanChooseDirectories:NO];
	[openPanel setAllowedFileTypes:(__bridge_transfer NSArray *)SFB::Audio::Decoder::CreateSupportedFileExtensions()];

	if(NSModalResponseOK == [openPanel runModal]) {
		NSArray *URLs = [openPanel URLs];
		[self.playerWindowController playURL:[URLs objectAtIndex:0]];
	}
}

- (IBAction) openURL:(id)sender
{
	[self.openURLPanel center];
	[self.openURLPanel makeKeyAndOrderFront:sender];
}

- (IBAction) enqueueFiles:(id)sender
{
#pragma unused(sender)
	NSOpenPanel *openPanel = [NSOpenPanel openPanel];

	[openPanel setAllowsMultipleSelection:YES];
	[openPanel setCanChooseDirectories:NO];
	[openPanel setAllowedFileTypes:(__bridge_transfer NSArray *)SFB::Audio::Decoder::CreateSupportedFileExtensions()];

	if(NSModalResponseOK == [openPanel runModal]) {
		for(NSURL *url in [openPanel URLs])
			[self.playerWindowController enqueueURL:url];
	}
}

- (IBAction) openURLPanelOpenAction:(id)sender
{
	[self.openURLPanel orderOut:sender];
	NSURL *url = [NSURL URLWithString:[self.openURLPanelTextField stringValue]];
	[self.playerWindowController playURL:url];
}

- (IBAction) openURLPanelCancelAction:(id)sender
{
	[self.openURLPanel orderOut:sender];
}

@end
