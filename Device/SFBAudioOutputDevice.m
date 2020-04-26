/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <os/log.h>

#import "SFBAudioOutputDevice.h"

#import "SFBAudioDeviceDataSource.h"
#import "SFBCStringForOSType.h"

@implementation SFBAudioOutputDevice

- (BOOL)isMuted
{
	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= kAudioDevicePropertyMute,
		.mScope		= kAudioObjectPropertyScopeOutput,
		.mElement	= kAudioObjectPropertyElementMaster
	};

	UInt32 isMuted = 0;
	UInt32 dataSize = sizeof(isMuted);
	OSStatus result = AudioObjectGetPropertyData(self.deviceID, &propertyAddress, 0, NULL, &dataSize, &isMuted);
	if(result != kAudioHardwareNoError) {
		os_log_error(OS_LOG_DEFAULT, "AudioObjectGetPropertyData (kAudioDevicePropertyMute) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
		return NO;
	}

	return isMuted ? YES : NO;
}

- (void)setMute:(BOOL)mute
{
	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= kAudioDevicePropertyMute,
		.mScope		= kAudioObjectPropertyScopeOutput,
		.mElement	= kAudioObjectPropertyElementMaster
	};

	UInt32 muted = (UInt32)mute;
	OSStatus result = AudioObjectSetPropertyData(self.deviceID, &propertyAddress, 0, NULL, sizeof(muted), &muted);
	if(result != kAudioHardwareNoError)
		os_log_error(OS_LOG_DEFAULT, "AudioObjectSetPropertyData (kAudioDevicePropertyMute) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
}

- (float)masterVolume
{
	return [self volumeForChannel:kAudioObjectPropertyElementMaster];
}

- (void)setMasterVolume:(float)masterVolume
{
	[self setVolume:masterVolume forChannel:kAudioObjectPropertyElementMaster];
}

- (float)volumeForChannel:(AudioObjectPropertyElement)channel
{
	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= kAudioDevicePropertyVolumeScalar,
		.mScope		= kAudioObjectPropertyScopeOutput,
		.mElement	= channel
	};

	Float32 volume;
	UInt32 dataSize = sizeof(volume);
	OSStatus result = AudioObjectGetPropertyData(self.deviceID, &propertyAddress, 0, NULL, &dataSize, &volume);
	if(result != kAudioHardwareNoError) {
		os_log_error(OS_LOG_DEFAULT, "AudioObjectGetPropertyData (kAudioDevicePropertyVolumeScalar, kAudioObjectPropertyScopeOutput, %u) failed: %d", channel, result);
		return -1;
	}
	return volume;
}

- (void)setVolume:(float)volume forChannel:(AudioObjectPropertyElement)channel
{
	os_log_info(OS_LOG_DEFAULT, "Setting device 0x%x channel %u volume to %f", self.deviceID, channel, volume);

	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= kAudioDevicePropertyVolumeScalar,
		.mScope		= kAudioObjectPropertyScopeOutput,
		.mElement	= channel
	};

	if(!AudioObjectHasProperty(self.deviceID, &propertyAddress)) {
		os_log_info(OS_LOG_DEFAULT, "AudioObjectHasProperty (kAudioDevicePropertyVolumeScalar, kAudioObjectPropertyScopeOutput, %u) is false", channel);
		return;
	}

	OSStatus result = AudioObjectSetPropertyData(self.deviceID, &propertyAddress, 0, NULL, sizeof(volume), &volume);
	if(result != kAudioHardwareNoError)
		os_log_error(OS_LOG_DEFAULT, "AudioObjectSetPropertyData (kAudioDevicePropertyVolumeScalar, kAudioObjectPropertyScopeOutput, %u) failed: %d", channel, result);
}

- (NSArray *)dataSources
{
	return [super dataSourcesInScope:kAudioObjectPropertyScopeOutput];
}

- (NSArray *)activeDataSources
{
	return [super activeDataSourcesInScope:kAudioObjectPropertyScopeOutput];
}

- (void)setActiveDataSources:(NSArray *)activeDataSources
{
	NSParameterAssert(activeDataSources != nil);
	[super setActiveDataSources:activeDataSources inScope:kAudioObjectPropertyScopeOutput];
}

@end
