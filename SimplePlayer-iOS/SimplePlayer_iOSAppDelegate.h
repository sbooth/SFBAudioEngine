//
//  SimplePlayer_iOSAppDelegate.h
//  SimplePlayer-iOS
//
//  Created by Stephen F. Booth on 7/9/11.
//  Copyright 2011 __MyCompanyName__. All rights reserved.
//

#import <UIKit/UIKit.h>

#ifdef __cplusplus
# include "iOSAudioPlayer.h"
#endif

@interface SimplePlayer_iOSAppDelegate : NSObject <UIApplicationDelegate>
{
#ifdef __cplusplus
	iOSAudioPlayer *_player;
#else
	void *_player;
#endif
	BOOL _resume;
}

@property (nonatomic, retain) IBOutlet UIWindow *window;

@end
