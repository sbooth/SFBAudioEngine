/*
 *  Copyright (C) 2009, 2010, 2011, 2012, 2013, 2014, 2015 Stephen F. Booth <me@sbooth.org>
 *  All Rights Reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are
 *  met:
 *
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
