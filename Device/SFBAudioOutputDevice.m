/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

@import os.log;

#import "SFBAudioOutputDevice.h"
#import "SFBAudioObject+Internal.h"

#import "SFBAudioDeviceDataSource.h"
#import "SFBCStringForOSType.h"

@implementation SFBAudioOutputDevice

+ (NSArray *)outputDevices
{
	NSMutableArray *outputDevices = [NSMutableArray array];

	NSArray *devices = [SFBAudioDevice devices];
	for(SFBAudioDevice *device in devices) {
		if(device.supportsOutput) {
			SFBAudioOutputDevice *outputDevice = [[SFBAudioOutputDevice alloc] initWithAudioObjectID:device.deviceID];
			if(outputDevice)
				[outputDevices addObject:outputDevice];
		}
	}

	return outputDevices;
}

+ (SFBAudioOutputDevice *)defaultOutputDevice
{
	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= kAudioHardwarePropertyDefaultOutputDevice,
		.mScope		= kAudioObjectPropertyScopeGlobal,
		.mElement	= kAudioObjectPropertyElementMaster
	};

	AudioObjectID deviceID = kAudioObjectUnknown;
	UInt32 specifierSize = sizeof(deviceID);
	OSStatus result = AudioObjectGetPropertyData(kAudioObjectSystemObject, &propertyAddress, 0, NULL, &specifierSize, &deviceID);
	if(result != kAudioHardwareNoError) {
		os_log_error(gSFBAudioObjectLog, "AudioObjectGetPropertyData (kAudioHardwarePropertyDefaultOutputDevice) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
		return nil;
	}

	return [[SFBAudioOutputDevice alloc] initWithAudioObjectID:deviceID];
}

- (instancetype)initWithAudioObjectID:(AudioObjectID)audioObjectID
{
	NSParameterAssert(audioObjectID != kAudioObjectUnknown);
	NSParameterAssert(SFBAudioDeviceSupportsOutput(audioObjectID));

	return [super initWithAudioObjectID:audioObjectID];
}

#pragma mark - Device Properties

- (BOOL)isMuted
{
	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= kAudioDevicePropertyMute,
		.mScope		= kAudioObjectPropertyScopeOutput,
		.mElement	= kAudioObjectPropertyElementMaster
	};

	UInt32 isMuted = 0;
	UInt32 dataSize = sizeof(isMuted);
	OSStatus result = AudioObjectGetPropertyData(_objectID, &propertyAddress, 0, NULL, &dataSize, &isMuted);
	if(result != kAudioHardwareNoError) {
		os_log_error(gSFBAudioObjectLog, "AudioObjectGetPropertyData (kAudioDevicePropertyMute) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
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
	OSStatus result = AudioObjectSetPropertyData(_objectID, &propertyAddress, 0, NULL, sizeof(muted), &muted);
	if(result != kAudioHardwareNoError)
		os_log_error(gSFBAudioObjectLog, "AudioObjectSetPropertyData (kAudioDevicePropertyMute) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
}

- (BOOL)hasMasterVolume
{
	return [self hasProperty:kAudioDevicePropertyVolumeScalar inScope:kAudioObjectPropertyScopeOutput onElement:kAudioObjectPropertyElementMaster];
}

- (float)masterVolume
{
	return [self volumeForChannel:kAudioObjectPropertyElementMaster];
}

- (BOOL)setMasterVolume:(float)masterVolume error:(NSError **)error
{
	return [self setVolume:masterVolume forChannel:kAudioObjectPropertyElementMaster error:error];
}

- (float)volumeForChannel:(AudioObjectPropertyElement)channel
{
	return [super volumeForChannel:channel inScope:kAudioObjectPropertyScopeOutput];
}

- (BOOL)setVolume:(float)volume forChannel:(AudioObjectPropertyElement)channel error:(NSError **)error
{
	return [super setVolume:volume forChannel:channel inScope:kAudioObjectPropertyScopeOutput error:error];
}

- (NSArray *)preferredStereoChannels
{
	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= kAudioDevicePropertyPreferredChannelsForStereo,
		.mScope		= kAudioObjectPropertyScopeOutput,
		.mElement	= kAudioObjectPropertyElementMaster
	};

	if(!AudioObjectHasProperty(_objectID, &propertyAddress)) {
		os_log_debug(gSFBAudioObjectLog, "AudioObjectHasProperty (kAudioDevicePropertyPreferredChannelsForStereo, kAudioObjectPropertyScopeOutput) is false");
		return nil;
	}

	UInt32 preferredChannels [2];
	UInt32 dataSize = sizeof(preferredChannels);
	OSStatus result = AudioObjectGetPropertyData(_objectID, &propertyAddress, 0, NULL, &dataSize, &preferredChannels);
	if(kAudioHardwareNoError != result) {
		os_log_debug(gSFBAudioObjectLog, "AudioObjectGetPropertyData (kAudioDevicePropertyPreferredChannelsForStereo, kAudioObjectPropertyScopeOutput) failed: %d", result);
		return nil;
	}

	return @[@(preferredChannels[0]), @(preferredChannels[1])];
}

- (AVAudioChannelLayout *)preferredChannelLayout
{
	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= kAudioDevicePropertyPreferredChannelLayout,
		.mScope		= kAudioObjectPropertyScopeOutput,
		.mElement	= kAudioObjectPropertyElementMaster
	};

	if(!AudioObjectHasProperty(_objectID, &propertyAddress)) {
		os_log_debug(gSFBAudioObjectLog, "AudioObjectHasProperty (kAudioDevicePropertyPreferredChannelLayout, kAudioObjectPropertyScopeOutput) is false");
		return nil;
	}

	UInt32 dataSize = 0;
	OSStatus result = AudioObjectGetPropertyDataSize(_objectID, &propertyAddress, 0, NULL, &dataSize);
	if(kAudioHardwareNoError != result) {
		os_log_debug(gSFBAudioObjectLog, "AudioObjectGetPropertyDataSize (kAudioDevicePropertyPreferredChannelLayout, kAudioObjectPropertyScopeOutput) failed: %d", result);
		return nil;
	}

	AudioChannelLayout *preferredChannelLayout = malloc(dataSize);
	result = AudioObjectGetPropertyData(_objectID, &propertyAddress, 0, NULL, &dataSize, preferredChannelLayout);
	if(kAudioHardwareNoError != result) {
		os_log_debug(gSFBAudioObjectLog, "AudioObjectGetPropertyData (kAudioDevicePropertyPreferredChannelLayout, kAudioObjectPropertyScopeOutput) failed: %d", result);
		free(preferredChannelLayout);
		return nil;
	}

	AVAudioChannelLayout *channelLayout = [AVAudioChannelLayout layoutWithLayout:preferredChannelLayout];
	free(preferredChannelLayout);

	return channelLayout;
}

- (NSArray *)dataSources
{
	return [super dataSourcesInScope:kAudioObjectPropertyScopeOutput];
}

- (NSArray *)activeDataSources
{
	return [super activeDataSourcesInScope:kAudioObjectPropertyScopeOutput];
}

- (BOOL)setActiveDataSources:(NSArray *)activeDataSources error:(NSError **)error
{
	NSParameterAssert(activeDataSources != nil);
	return [super setActiveDataSources:activeDataSources inScope:kAudioObjectPropertyScopeOutput error:error];
}

#pragma mark - Device Property Observation

- (void)whenMuteChangesPerformBlock:(dispatch_block_t)block
{
	[self whenProperty:kAudioDevicePropertyMute inScope:kAudioObjectPropertyScopeOutput changesOnElement:kAudioObjectPropertyElementMaster performBlock:block];
}

- (void)whenMasterVolumeChangesPerformBlock:(dispatch_block_t)block
{
	[self whenProperty:kAudioDevicePropertyVolumeScalar inScope:kAudioObjectPropertyScopeOutput changesOnElement:kAudioObjectPropertyElementMaster performBlock:block];
}

- (void)whenVolumeChangesForChannel:(AudioObjectPropertyElement)channel performBlock:(dispatch_block_t)block
{
	[self whenProperty:kAudioDevicePropertyVolumeScalar inScope:kAudioObjectPropertyScopeOutput changesOnElement:channel performBlock:block];
}

- (void)whenDataSourcesChangePerformBlock:(dispatch_block_t)block
{
	[self whenDataSourcesChangeInScope:kAudioObjectPropertyScopeOutput performBlock:block];
}

- (void)whenActiveDataSourcesChangePerformBlock:(dispatch_block_t)block
{
	[self whenActiveDataSourcesChangeInScope:kAudioObjectPropertyScopeOutput performBlock:block];
}

@end
