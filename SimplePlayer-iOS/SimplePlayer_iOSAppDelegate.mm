//
//  SimplePlayer_iOSAppDelegate.m
//  SimplePlayer-iOS
//
//  Created by Stephen F. Booth on 7/9/11.
//  Copyright 2011 __MyCompanyName__. All rights reserved.
//

#import "SimplePlayer_iOSAppDelegate.h"

#include <log4cxx/basicconfigurator.h>

@implementation SimplePlayer_iOSAppDelegate


@synthesize window = _window;

- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions
{
	// Set up a simple configuration that logs on the console.
	log4cxx::BasicConfigurator::configure();

	// Override point for customization after application launch.
	_player = new iOSAudioPlayer();

	[self.window makeKeyAndVisible];

	NSURL *u = [NSURL fileURLWithPath:[[NSBundle mainBundle] pathForResource:@"test" ofType:@"aiff"]];
	if(_player->Enqueue((CFURLRef)u)) {
		if(!_player->Play())
			NSLog(@"couldn't play");
	}
	else
		NSLog(@"couldn't enqueue");
	

    return YES;
}

- (void)applicationWillResignActive:(UIApplication *)application
{
	/*
	 Sent when the application is about to move from active to inactive state. This can occur for certain types of temporary interruptions (such as an incoming phone call or SMS message) or when the user quits the application and it begins the transition to the background state.
	 Use this method to pause ongoing tasks, disable timers, and throttle down OpenGL ES frame rates. Games should use this method to pause the game.
	 */
	_resume = _player->IsPlaying();
	_player->Pause();
}

- (void)applicationDidEnterBackground:(UIApplication *)application
{
	/*
	 Use this method to release shared resources, save user data, invalidate timers, and store enough application state information to restore your application to its current state in case it is terminated later. 
	 If your application supports background execution, this method is called instead of applicationWillTerminate: when the user quits.
	 */
}

- (void)applicationWillEnterForeground:(UIApplication *)application
{
	/*
	 Called as part of the transition from the background to the inactive state; here you can undo many of the changes made on entering the background.
	 */
}

- (void)applicationDidBecomeActive:(UIApplication *)application
{
	/*
	 Restart any tasks that were paused (or not yet started) while the application was inactive. If the application was previously in the background, optionally refresh the user interface.
	 */
	if(_resume)
		_player->Play();
}

- (void)applicationWillTerminate:(UIApplication *)application
{
	/*
	 Called when the application is about to terminate.
	 Save data if appropriate.
	 See also applicationDidEnterBackground:.
	 */
}

- (void)dealloc
{
	delete _player, _player = NULL;
	[_window release];
    [super dealloc];
}

@end
