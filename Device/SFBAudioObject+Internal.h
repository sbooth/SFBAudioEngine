/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

@import os.log;

#import "SFBAudioObject.h"

NS_ASSUME_NONNULL_BEGIN

extern os_log_t gSFBAudioObjectLog;

BOOL SFBAudioObjectIsPlugIn(AudioObjectID objectID);
BOOL SFBAudioObjectIsBox(AudioObjectID objectID);
BOOL SFBAudioObjectIsDevice(AudioObjectID objectID);
BOOL SFBAudioObjectIsClockDevice(AudioObjectID objectID);
BOOL SFBAudioObjectIsStream(AudioObjectID objectID);

BOOL SFBAudioPlugInIsTransportManager(AudioObjectID objectID);

BOOL SFBAudioDeviceIsAggregate(AudioObjectID objectID);
BOOL SFBAudioDeviceIsEndPoint(AudioObjectID objectID);

BOOL SFBAudioDeviceSupportsInput(AudioObjectID deviceID);
BOOL SFBAudioDeviceSupportsOutput(AudioObjectID deviceID);

@interface SFBAudioObject ()
{
@protected
	/// The underlying audio object identifier
	AudioObjectID _objectID;
}
@end



NS_ASSUME_NONNULL_END
