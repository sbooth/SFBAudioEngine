/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <os/log.h>

#import <CoreAudio/CoreAudio.h>

#import "SFBAudioDeviceNotifier.h"

#import "SFBCStringForOSType.h"

@interface SFBAudioDeviceNotifier ()
{
@private
	AudioObjectPropertyListenerBlock _listenerBlock;
	NSHashTable *_blocks;
	dispatch_queue_t _blockQueue;
}
@end

@implementation SFBAudioDeviceNotifier

static SFBAudioDeviceNotifier *sInstance = nil;

+ (SFBAudioDeviceNotifier *)instance
{
	static dispatch_once_t onceToken;
	dispatch_once(&onceToken, ^{
		sInstance = [[SFBAudioDeviceNotifier alloc] init];

		sInstance->_blockQueue = dispatch_queue_create("org.sbooth.AudioEngine.AudioDeviceNotifier.IsolationQueue", DISPATCH_QUEUE_SERIAL);
		sInstance->_blocks = [NSHashTable weakObjectsHashTable];

		AudioObjectPropertyAddress propertyAddress = {
			.mSelector	= kAudioHardwarePropertyDevices,
			.mScope		= kAudioObjectPropertyScopeGlobal,
			.mElement	= kAudioObjectPropertyElementMaster
		};

		sInstance->_listenerBlock = ^(UInt32 inNumberAddresses, const AudioObjectPropertyAddress *inAddresses) {
#pragma unused(inNumberAddresses)
#pragma unused(inAddresses)
			// Make a local copy of _blocks to avoid dispatching the callbacks on _blockQueue
			__block NSArray *blocks = nil;
			dispatch_sync(sInstance->_blockQueue, ^{
				blocks = sInstance->_blocks.allObjects;
			});

			for(void(^block)(void) in blocks)
				block();
		};

		OSStatus result = AudioObjectAddPropertyListenerBlock(kAudioObjectSystemObject, &propertyAddress, dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0), sInstance->_listenerBlock);
		if(result != kAudioHardwareNoError)
			os_log_error(OS_LOG_DEFAULT, "AudioObjectAddPropertyListenerBlock (kAudioHardwarePropertyDevices) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
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
			os_log_error(OS_LOG_DEFAULT, "AudioObjectRemovePropertyListenerBlock (kAudioHardwarePropertyDevices) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
	}
}

- (void)addDevicesChangedCallback:(void (^)(void))block
{
	dispatch_sync(_blockQueue, ^{
		[_blocks addObject:block];
	});
}

- (void)removeDevicesChangedCallback:(void (^)(void))block
{
	dispatch_sync(_blockQueue, ^{
		[_blocks removeObject:block];
	});
}

@end
