/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <os/log.h>

#import "SFBAudioObject.h"

NS_ASSUME_NONNULL_BEGIN

extern os_log_t gSFBAudioObjectLog;

@interface SFBAudioObject ()
{
@protected
	/// The underlying audio object identifier
	AudioObjectID _objectID;
}
@end

#ifdef __cplusplus
extern "C" {
#endif

	BOOL SFBAudioObjectIsClass(AudioObjectID objectID, AudioClassID classID);
	BOOL SFBAudioObjectIsClassOrSubclassOf(AudioObjectID objectID, AudioClassID classID);

	BOOL SFBAudioDeviceSupportsInput(AudioObjectID deviceID);
	BOOL SFBAudioDeviceSupportsOutput(AudioObjectID deviceID);

#ifdef __cplusplus
}
#endif

NS_ASSUME_NONNULL_END
