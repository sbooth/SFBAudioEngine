/*
 * Copyright (c) 2009 - 2017 Stephen F. Booth <me@sbooth.org>
 *
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import "SimplePlayerAppDelegate.h"
#import "PlayerWindowController.h"

#include <SFBAudioEngine/AudioDecoder.h>
#include <SFBAudioEngine/Logger.h>

@implementation SimplePlayerAppDelegate

- (void) applicationDidFinishLaunching:(NSNotification *)aNotification
{
	// Enable verbose logging to stderr
	asl_add_log_file(nullptr, STDERR_FILENO);
	::SFB::Logger::SetCurrentLevel(::SFB::Logger::debug);

	// Show the player window
	[self.playerWindowController showWindow:self];
}

- (BOOL) applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)theApplication
{
	return YES;
}

- (BOOL) application:(NSApplication *)theApplication openFile:(NSString *)filename
{
	return [self.playerWindowController playURL:[NSURL fileURLWithPath:filename]];
}

- (IBAction) openFile:(id)sender
{
	NSOpenPanel *openPanel = [NSOpenPanel openPanel];

	[openPanel setAllowsMultipleSelection:NO];
	[openPanel setCanChooseDirectories:NO];
	[openPanel setAllowedFileTypes:(__bridge_transfer NSArray *)SFB::Audio::Decoder::CreateSupportedFileExtensions()];

	if(NSFileHandlingPanelOKButton == [openPanel runModal]) {
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
	NSOpenPanel *openPanel = [NSOpenPanel openPanel];

	[openPanel setAllowsMultipleSelection:YES];
	[openPanel setCanChooseDirectories:NO];
	[openPanel setAllowedFileTypes:(__bridge_transfer NSArray *)SFB::Audio::Decoder::CreateSupportedFileExtensions()];

	if(NSFileHandlingPanelOKButton == [openPanel runModal]) {
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
