/*
 *  Copyright (C) 2009, 2010, 2011, 2012 Stephen F. Booth <me@sbooth.org>
 *  All Rights Reserved
 */

#import "SimplePlayerAppDelegate.h"
#import "PlayerWindowController.h"

#include <SFBAudioEngine/AudioDecoder.h>
#include <SFBAudioEngine/Logger.h>

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
	asl_add_log_file(nullptr, STDERR_FILENO);
	::logger::SetCurrentLevel(::logger::debug);
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
	CFRelease(supportedTypes), supportedTypes = nullptr;
	
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
	[openPanel setAllowedFileTypes:(NSArray *)supportedTypes];

	if(NSFileHandlingPanelOKButton == [openPanel runModal]) {
		NSArray *URLs = [openPanel URLs];
		[_playerWindowController playURL:[URLs objectAtIndex:0]];
	}	

	CFRelease(supportedTypes), supportedTypes = nullptr;
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
