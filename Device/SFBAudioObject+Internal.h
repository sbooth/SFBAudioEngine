/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <os/log.h>

#import "SFBAudioObject.h"

NS_ASSUME_NONNULL_BEGIN

/// The log for \c SFBAudioObject and subclasses
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

	/// Returns \c YES if the class of \c objectID is \c classID
	BOOL SFBAudioObjectIsClass(AudioObjectID objectID, AudioClassID classID);
	/// Returns \c YES if the class or base class of \c objectID is \c classID
	BOOL SFBAudioObjectIsClassOrSubclassOf(AudioObjectID objectID, AudioClassID classID);

	/// Returns \c YES if \c deviceID has audio buffers in the input scope
	BOOL SFBAudioDeviceSupportsInput(AudioObjectID deviceID);
	/// Returns \c YES if \c deviceID has audio buffers in the output scope
	BOOL SFBAudioDeviceSupportsOutput(AudioObjectID deviceID);

#ifdef __cplusplus
}
#endif

NS_ASSUME_NONNULL_END
