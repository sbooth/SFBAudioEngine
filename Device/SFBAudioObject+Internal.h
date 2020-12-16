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

#pragma mark - Audio object class determination

BOOL SFBAudioObjectIsClass(AudioObjectID objectID, AudioClassID classID);
BOOL SFBAudioObjectIsClassOrSubclassOf(AudioObjectID objectID, AudioClassID classID);

BOOL SFBAudioObjectIsPlugIn(AudioObjectID objectID);
BOOL SFBAudioObjectIsBox(AudioObjectID objectID);
BOOL SFBAudioObjectIsDevice(AudioObjectID objectID);
BOOL SFBAudioObjectIsClockDevice(AudioObjectID objectID);
BOOL SFBAudioObjectIsStream(AudioObjectID objectID);
BOOL SFBAudioObjectIsControl(AudioObjectID objectID);

#pragma mark - Audio PlugIn Information

BOOL SFBAudioPlugInIsTransportManager(AudioObjectID objectID);

#pragma mark - Audio Device Information

BOOL SFBAudioDeviceIsAggregate(AudioObjectID objectID);
BOOL SFBAudioDeviceIsSubdevice(AudioObjectID objectID);
BOOL SFBAudioDeviceIsEndpointDevice(AudioObjectID objectID);
BOOL SFBAudioDeviceIsEndpoint(AudioObjectID objectID);

BOOL SFBAudioDeviceSupportsInput(AudioObjectID deviceID);
BOOL SFBAudioDeviceSupportsOutput(AudioObjectID deviceID);

#pragma mark - Audio Control Information

BOOL SFBAudioControlIsSlider(AudioObjectID objectID);
BOOL SFBAudioControlIsLevel(AudioObjectID objectID);
BOOL SFBAudioControlIsBoolean(AudioObjectID objectID);
BOOL SFBAudioControlIsSelector(AudioObjectID objectID);
BOOL SFBAudioControlIsStereoPan(AudioObjectID objectID);

#pragma mark - Audio Level Control Information

BOOL SFBAudioLevelControlIsVolume(AudioObjectID objectID);
BOOL SFBAudioLevelControlIsLFEVolume(AudioObjectID objectID);

#pragma mark - Audio Boolean Control Information

BOOL SFBAudioBooleanControlIsMute(AudioObjectID objectID);
BOOL SFBAudioBooleanControlIsSolo(AudioObjectID objectID);
BOOL SFBAudioBooleanControlIsJack(AudioObjectID objectID);
BOOL SFBAudioBooleanControlIsLFEMute(AudioObjectID objectID);
BOOL SFBAudioBooleanControlIsPhantomPower(AudioObjectID objectID);
BOOL SFBAudioBooleanControlIsPhaseInvert(AudioObjectID objectID);
BOOL SFBAudioBooleanControlIsClipLight(AudioObjectID objectID);
BOOL SFBAudioBooleanControlIsTalkback(AudioObjectID objectID);
BOOL SFBAudioBooleanControlIsListenback(AudioObjectID objectID);

#pragma mark - Audio Selector Control Information

BOOL SFBAudioSelectorControlIsDataSource(AudioObjectID objectID);
BOOL SFBAudioSelectorControlIsDataDestination(AudioObjectID objectID);
BOOL SFBAudioSelectorControlIsClockSource(AudioObjectID objectID);
BOOL SFBAudioSelectorControlIsLevel(AudioObjectID objectID);
BOOL SFBAudioSelectorControlIsHighpassFilter(AudioObjectID objectID);

#ifdef __cplusplus
}
#endif

NS_ASSUME_NONNULL_END
