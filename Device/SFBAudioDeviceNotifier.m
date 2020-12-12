/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

@import os.log;

@import CoreAudio;

#import "SFBAudioDeviceNotifier.h"

#import "SFBCStringForOSType.h"

const NSNotificationName SFBAudioDevicesChangedNotification = @"org.sbooth.AudioEngine.AudioDeviceNotifier.ChangedNotification";

extern os_log_t gSFBAudioObjectLog;

@interface SFBAudioDeviceNotifier ()
{
@private
	AudioObjectPropertyListenerBlock _listenerBlock;
}
@end

@implementation SFBAudioDeviceNotifier

static SFBAudioDeviceNotifier *sInstance = nil;

+ (SFBAudioDeviceNotifier *)instance
{
	static dispatch_once_t onceToken;
	dispatch_once(&onceToken, ^{
		sInstance = [[SFBAudioDeviceNotifier alloc] init];

		AudioObjectPropertyAddress propertyAddress = {
			.mSelector	= kAudioHardwarePropertyDevices,
			.mScope		= kAudioObjectPropertyScopeGlobal,
			.mElement	= kAudioObjectPropertyElementMaster
		};

		sInstance->_listenerBlock = ^(UInt32 inNumberAddresses, const AudioObjectPropertyAddress *inAddresses) {
#pragma unused(inNumberAddresses)
#pragma unused(inAddresses)
			[[NSNotificationCenter defaultCenter] postNotificationName:SFBAudioDevicesChangedNotification object:nil];
		};

		OSStatus result = AudioObjectAddPropertyListenerBlock(kAudioObjectSystemObject, &propertyAddress, dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0), sInstance->_listenerBlock);
		if(result != kAudioHardwareNoError)
			os_log_error(gSFBAudioObjectLog, "AudioObjectAddPropertyListener (kAudioDevicePropertyDataSources) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
	});

	return sInstance;
}

- (void)dealloc
{
	if(_listenerBlock) {
		AudioObjectPropertyAddress propertyAddress = {
			.mSelector	= kAudioHardwarePropertyDevices,
			.mScope		= kAudioObjectPropertyScopeGlobal,
			.mElement	= kAudioObjectPropertyElementMaster
		};

		OSStatus result = AudioObjectRemovePropertyListenerBlock(kAudioObjectSystemObject, &propertyAddress, dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0), _listenerBlock);
		if(result != kAudioHardwareNoError)
			os_log_error(gSFBAudioObjectLog, "AudioObjectRemovePropertyListenerBlock (kAudioHardwarePropertyDevices) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
	}
}

@end
