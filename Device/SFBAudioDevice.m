/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <os/log.h>

#import "SFBAudioDevice.h"

#import "SFBAudioDeviceDataSource.h"
#import "SFBAudioOutputDevice.h"
#import "SFBCStringForOSType.h"

static BOOL DeviceHasBuffersForScope(AudioObjectID deviceID, AudioObjectPropertyScope scope)
{
	NSCParameterAssert(deviceID != kAudioObjectUnknown);

	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= kAudioDevicePropertyStreamConfiguration,
		.mScope		= scope,
		.mElement	= kAudioObjectPropertyElementWildcard
	};

	UInt32 dataSize = 0;
	OSStatus result = AudioObjectGetPropertyDataSize(deviceID, &propertyAddress, 0, NULL, &dataSize);
	if(result != kAudioHardwareNoError) {
		os_log_error(OS_LOG_DEFAULT, "AudioObjectGetPropertyDataSize (kAudioDevicePropertyStreamConfiguration) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
		return NO;
	}

	AudioBufferList *bufferList = malloc(dataSize);
	if(!bufferList) {
		os_log_error(OS_LOG_DEFAULT, "Unable to allocate memory");
		return NO;
	}

	result = AudioObjectGetPropertyData(deviceID, &propertyAddress, 0, NULL, &dataSize, bufferList);
	if(result != kAudioHardwareNoError) {
		os_log_error(OS_LOG_DEFAULT, "AudioObjectGetPropertyData (kAudioDevicePropertyStreamConfiguration) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
		free(bufferList);
		return NO;
	}

	BOOL supportsScope = bufferList->mNumberBuffers > 0;
	free(bufferList);

	return supportsScope;
}

static BOOL DeviceSupportsInput(AudioObjectID deviceID)
{
	return DeviceHasBuffersForScope(deviceID, kAudioObjectPropertyScopeInput);
}

static BOOL DeviceSupportsOutput(AudioObjectID deviceID)
{
	return DeviceHasBuffersForScope(deviceID, kAudioObjectPropertyScopeOutput);
}

@interface SFBAudioDevice ()
{
@private
	AudioObjectID _deviceID;
	NSMutableDictionary *_dataSourcesListenerBlocks;
}
@end

@implementation SFBAudioDevice

+ (NSArray *)allDevices
{
	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= kAudioHardwarePropertyDevices,
		.mScope		= kAudioObjectPropertyScopeGlobal,
		.mElement	= kAudioObjectPropertyElementWildcard
	};

	UInt32 dataSize = 0;
	OSStatus result = AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &propertyAddress, 0, NULL, &dataSize);
	if(result != kAudioHardwareNoError) {
		os_log_error(OS_LOG_DEFAULT, "AudioObjectGetPropertyDataSize (kAudioHardwarePropertyDevices) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
		return nil;
	}

	AudioObjectID *deviceIDs = (AudioObjectID *)malloc(dataSize);
	if(!deviceIDs) {
		os_log_error(OS_LOG_DEFAULT, "Unable to allocate memory");
		return nil;
	}

	result = AudioObjectGetPropertyData(kAudioObjectSystemObject, &propertyAddress, 0, NULL, &dataSize, deviceIDs);
	if(kAudioHardwareNoError != result) {
		os_log_error(OS_LOG_DEFAULT, "AudioObjectGetPropertyData (kAudioHardwarePropertyDevices) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
		free(deviceIDs);
		return nil;
	}

	NSMutableArray *allDevices = [NSMutableArray array];
	for(NSInteger i = 0; i < (NSInteger)(dataSize / sizeof(AudioObjectID)); ++i) {
		SFBAudioDevice *device = [[SFBAudioDevice alloc] initWithAudioObjectID:deviceIDs[i]];
		if(device) {
			[allDevices addObject:device];
		}
	}

	free(deviceIDs);

	return allDevices;
}

+ (NSArray *)outputDevices
{
	NSMutableArray *outputDevices = [NSMutableArray array];

	NSArray *allDevices = [self allDevices];
	for(SFBAudioDevice *device in allDevices) {
		if(device.supportsOutput) {
			SFBAudioOutputDevice *outputDevice = [[SFBAudioOutputDevice alloc] initWithAudioObjectID:device.deviceID];
			if(device)
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
		.mElement	= kAudioObjectPropertyElementWildcard
	};

	AudioObjectID deviceID = kAudioObjectUnknown;
	UInt32 specifierSize = sizeof(deviceID);
	OSStatus result = AudioObjectGetPropertyData(kAudioObjectSystemObject, &propertyAddress, 0, NULL, &specifierSize, &deviceID);
	if(result != kAudioHardwareNoError) {
		os_log_error(OS_LOG_DEFAULT, "AudioObjectGetPropertyData (kAudioHardwarePropertyDefaultOutputDevice) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
		return nil;
	}

	return [[SFBAudioOutputDevice alloc] initWithAudioObjectID:deviceID];
}

- (nullable instancetype)initWithDeviceUID:(NSString *)deviceUID
{
	NSParameterAssert(deviceUID != nil);

	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= kAudioHardwarePropertyTranslateUIDToDevice,
		.mScope		= kAudioObjectPropertyScopeGlobal,
		.mElement	= kAudioObjectPropertyElementWildcard
	};

	AudioObjectID deviceID = kAudioObjectUnknown;
	UInt32 specifierSize = sizeof(deviceID);
	OSStatus result = AudioObjectGetPropertyData(kAudioObjectSystemObject, &propertyAddress, sizeof(deviceUID), &deviceUID, &specifierSize, &deviceID);
	if(result != kAudioHardwareNoError) {
		os_log_error(OS_LOG_DEFAULT, "AudioObjectGetPropertyData (kAudioHardwarePropertyDeviceForUID) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
		return nil;
	}

	if(deviceID == kAudioObjectUnknown) {
		os_log_error(OS_LOG_DEFAULT, "Unknown audio device UID: %{public}@", deviceUID);
		return nil;
	}

	return [self initWithAudioObjectID:deviceID];
}

- (nullable instancetype)initWithAudioObjectID:(AudioObjectID)deviceID
{
	NSParameterAssert(deviceID != kAudioObjectUnknown);

	if((self = [super init]))
		_deviceID = deviceID;
	return self;
}

- (void)dealloc
{
	[_dataSourcesListenerBlocks enumerateKeysAndObjectsUsingBlock:^(NSNumber *scope, AudioObjectPropertyListenerBlock propertyListenerBlock, BOOL *stop) {
#pragma unused(stop)
		AudioObjectPropertyAddress propertyAddress = {
			.mSelector	= kAudioDevicePropertyDataSources,
			.mScope		= (AudioObjectPropertyScope)scope.unsignedIntValue,
			.mElement	= kAudioObjectPropertyElementMaster
		};

		OSStatus result = AudioObjectRemovePropertyListenerBlock(_deviceID, &propertyAddress, dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0), propertyListenerBlock);
		if(result != kAudioHardwareNoError)
			os_log_error(OS_LOG_DEFAULT, "AudioObjectRemovePropertyListenerBlock (kAudioDevicePropertyDataSources) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
	}];
}

- (NSString *)deviceUID
{
	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= kAudioDevicePropertyDeviceUID,
		.mScope		= kAudioObjectPropertyScopeGlobal,
		.mElement	= kAudioObjectPropertyElementWildcard
	};

	CFStringRef deviceUID = NULL;
	UInt32 dataSize = sizeof(deviceUID);
	OSStatus result = AudioObjectGetPropertyData(_deviceID, &propertyAddress, 0, NULL, &dataSize, &deviceUID);
	if(result != kAudioHardwareNoError) {
		os_log_error(OS_LOG_DEFAULT, "AudioObjectGetPropertyData (kAudioDevicePropertyDeviceUID) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
		return nil;
	}

	return (__bridge_transfer NSString *)deviceUID;
}

- (NSString *)name
{
	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= kAudioObjectPropertyName,
		.mScope		= kAudioObjectPropertyScopeGlobal,
		.mElement	= kAudioObjectPropertyElementWildcard
	};

	CFStringRef name = NULL;
	UInt32 dataSize = sizeof(name);
	OSStatus result = AudioObjectGetPropertyData(_deviceID, &propertyAddress, 0, NULL, &dataSize, &name);
	if(result != kAudioHardwareNoError) {
		os_log_error(OS_LOG_DEFAULT, "AudioObjectGetPropertyData (kAudioObjectPropertyName) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
		return nil;
	}

	return (__bridge_transfer NSString *)name;
}

- (NSString *)manufacturer
{
	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= kAudioObjectPropertyManufacturer,
		.mScope		= kAudioObjectPropertyScopeGlobal,
		.mElement	= kAudioObjectPropertyElementWildcard
	};

	CFStringRef manufacturer = NULL;
	UInt32 dataSize = sizeof(manufacturer);
	OSStatus result = AudioObjectGetPropertyData(_deviceID, &propertyAddress, 0, NULL, &dataSize, &manufacturer);
	if(result != kAudioHardwareNoError) {
		os_log_error(OS_LOG_DEFAULT, "AudioObjectGetPropertyData (kAudioObjectPropertyManufacturer) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
		return nil;
	}

	return (__bridge_transfer NSString *)manufacturer;
}

- (BOOL)supportsInput
{
	return DeviceSupportsInput(_deviceID);
}

- (BOOL)supportsOutput
{
	return DeviceSupportsOutput(_deviceID);
}

- (NSArray *)dataSourcesForScope:(AudioObjectPropertyScope)scope
{
	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= kAudioDevicePropertyDataSources,
		.mScope		= scope,
		.mElement	= kAudioObjectPropertyElementMaster
	};

	UInt32 dataSize = 0;
	OSStatus result = AudioObjectGetPropertyDataSize(_deviceID, &propertyAddress, 0, NULL, &dataSize);
	if(result != kAudioHardwareNoError) {
		os_log_error(OS_LOG_DEFAULT, "AudioObjectGetPropertyDataSize (kAudioDevicePropertyDataSources) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
		return nil;
	}

	UInt32 *dataSourceIDs = (UInt32 *)malloc(dataSize);
	if(!dataSourceIDs) {
		os_log_error(OS_LOG_DEFAULT, "Unable to allocate memory");
		return nil;
	}

	result = AudioObjectGetPropertyData(_deviceID, &propertyAddress, 0, NULL, &dataSize, dataSourceIDs);
	if(kAudioHardwareNoError != result) {
		os_log_error(OS_LOG_DEFAULT, "AudioObjectGetPropertyData (kAudioDevicePropertyDataSources) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
		free(dataSourceIDs);
		return nil;
	}

	NSMutableArray *dataSources = [NSMutableArray array];

	// Iterate through all the data sources
	for(NSInteger i = 0; i < (NSInteger)(dataSize / sizeof(UInt32)); ++i) {
		SFBAudioDeviceDataSource *dataSource = [[SFBAudioDeviceDataSource alloc] initWithAudioDevice:self scope:scope dataSourceID:dataSourceIDs[i]];
		if(dataSource)
			[dataSources addObject:dataSource];
	}

	free(dataSourceIDs);

	return dataSources;
}

- (void)whenDataSourcesChangeForScope:(AudioObjectPropertyScope)scope performBlock:(void (^)(void))block
{
	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= kAudioDevicePropertyDataSources,
		.mScope		= scope,
		.mElement	= kAudioObjectPropertyElementMaster
	};

	AudioObjectPropertyListenerBlock currentPropertyListenerBlock = _dataSourcesListenerBlocks[@(scope)];
	if(currentPropertyListenerBlock) {
		[_dataSourcesListenerBlocks removeObjectForKey:@(scope)];
		OSStatus result = AudioObjectRemovePropertyListenerBlock(_deviceID, &propertyAddress, dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0), currentPropertyListenerBlock);
		if(result != kAudioHardwareNoError)
			os_log_error(OS_LOG_DEFAULT, "AudioObjectRemovePropertyListenerBlock (kAudioDevicePropertyDataSources) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
	}

	if(block == nil)
		return;

	AudioObjectPropertyListenerBlock propertyListenerBlock = ^(UInt32 inNumberAddresses, const AudioObjectPropertyAddress *inAddresses) {
#pragma unused(inNumberAddresses)
#pragma unused(inAddresses)
		block();
	};

	_dataSourcesListenerBlocks[@(scope)] = propertyListenerBlock;

	OSStatus result = AudioObjectAddPropertyListenerBlock(_deviceID, &propertyAddress, dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0), propertyListenerBlock);
	if(result != kAudioHardwareNoError) {
		os_log_error(OS_LOG_DEFAULT, "AudioObjectAddPropertyListener (kAudioDevicePropertyDataSources) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
		[_dataSourcesListenerBlocks removeObjectForKey:@(scope)];
	}
}

- (NSArray *)activeDataSourcesForScope:(AudioObjectPropertyScope)scope
{
	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= kAudioDevicePropertyDataSource,
		.mScope		= scope,
		.mElement	= kAudioObjectPropertyElementMaster
	};

	UInt32 dataSize = 0;
	OSStatus result = AudioObjectGetPropertyDataSize(_deviceID, &propertyAddress, 0, NULL, &dataSize);
	if(result != kAudioHardwareNoError) {
		os_log_error(OS_LOG_DEFAULT, "AudioObjectGetPropertyDataSize (kAudioDevicePropertyDataSource) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
		return nil;
	}

	UInt32 *dataSourceIDs = (UInt32 *)malloc(dataSize);
	if(!dataSourceIDs) {
		os_log_error(OS_LOG_DEFAULT, "Unable to allocate memory");
		return nil;
	}

	result = AudioObjectGetPropertyData(_deviceID, &propertyAddress, 0, NULL, &dataSize, dataSourceIDs);
	if(kAudioHardwareNoError != result) {
		os_log_error(OS_LOG_DEFAULT, "AudioObjectGetPropertyData (kAudioDevicePropertyDataSource) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
		free(dataSourceIDs);
		return nil;
	}

	NSMutableArray *dataSources = [NSMutableArray array];

	// Iterate through all the data sources
	for(NSInteger i = 0; i < (NSInteger)(dataSize / sizeof(UInt32)); ++i) {
		SFBAudioDeviceDataSource *dataSource = [[SFBAudioDeviceDataSource alloc] initWithAudioDevice:self scope:scope dataSourceID:dataSourceIDs[i]];
		if(dataSource)
			[dataSources addObject:dataSource];
	}

	free(dataSourceIDs);

	return dataSources;
}

- (void)setActiveDataSources:(NSArray *)activeDataSources forScope:(AudioObjectPropertyScope)scope
{
	NSParameterAssert(activeDataSources != nil);

	if(activeDataSources.count == 0)
		return;

	UInt32 dataSourceIDs [activeDataSources.count];
	for(NSUInteger i = 0; i < activeDataSources.count; ++i) {
		SFBAudioDeviceDataSource *dataSource = [activeDataSources objectAtIndex:i];
		if(dataSource)
			dataSourceIDs[i] = dataSource.dataSourceID;
	}

	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= kAudioDevicePropertyDataSource,
		.mScope		= scope,
		.mElement	= kAudioObjectPropertyElementMaster
	};

	OSStatus result = AudioObjectSetPropertyData(_deviceID, &propertyAddress, 0, NULL, (UInt32)sizeof(dataSourceIDs), dataSourceIDs);
	if(kAudioHardwareNoError != result)
		os_log_error(OS_LOG_DEFAULT, "AudioObjectSetPropertyData (kAudioDevicePropertyDataSource) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
}

- (double)nominalSampleRate
{
	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= kAudioDevicePropertyNominalSampleRate,
		.mScope		= kAudioObjectPropertyScopeGlobal,
		.mElement	= kAudioObjectPropertyElementMaster
	};

	Float64 sampleRate;
	UInt32 dataSize = sizeof(sampleRate);
	OSStatus result = AudioObjectGetPropertyData(_deviceID, &propertyAddress, 0, NULL, &dataSize, &sampleRate);
	if(kAudioHardwareNoError != result) {
		os_log_error(OS_LOG_DEFAULT, "AudioObjectGetPropertyData (kAudioDevicePropertyNominalSampleRate) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
		return -1;
	}

	return sampleRate;
}

- (void)setNominalSampleRate:(double)nominalSampleRate
{
	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= kAudioDevicePropertyNominalSampleRate,
		.mScope		= kAudioObjectPropertyScopeGlobal,
		.mElement	= kAudioObjectPropertyElementMaster
	};

	Float64 sampleRate = nominalSampleRate;
	OSStatus result = AudioObjectSetPropertyData(_deviceID, &propertyAddress, 0, NULL, sizeof(sampleRate), &sampleRate);
	if(kAudioHardwareNoError != result)
		os_log_error(OS_LOG_DEFAULT, "AudioObjectSetPropertyData (kAudioDevicePropertyNominalSampleRate) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
}

- (NSArray *)availableSampleRates
{
	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= kAudioDevicePropertyAvailableNominalSampleRates,
		.mScope		= kAudioObjectPropertyScopeGlobal,
		.mElement	= kAudioObjectPropertyElementMaster
	};

	UInt32 dataSize = 0;
	OSStatus result = AudioObjectGetPropertyDataSize(_deviceID, &propertyAddress, 0, NULL, &dataSize);
	if(result != kAudioHardwareNoError) {
		os_log_error(OS_LOG_DEFAULT, "AudioObjectGetPropertyDataSize (kAudioDevicePropertyAvailableNominalSampleRates) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
		return nil;
	}

	AudioValueRange *availableNominalSampleRates = (AudioValueRange *)malloc(dataSize);
	if(!availableNominalSampleRates) {
		os_log_error(OS_LOG_DEFAULT, "Unable to allocate memory");
		return nil;
	}

	result = AudioObjectGetPropertyData(_deviceID, &propertyAddress, 0, NULL, &dataSize, availableNominalSampleRates);
	if(kAudioHardwareNoError != result) {
		os_log_error(OS_LOG_DEFAULT, "AudioObjectGetPropertyData (kAudioDevicePropertyAvailableNominalSampleRates) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
		free(availableNominalSampleRates);
		return nil;
	}

	NSMutableArray *availablSampleRates = [NSMutableArray array];

	for(NSInteger i = 0; i < (NSInteger)(dataSize / sizeof(AudioValueRange)); ++i) {
		AudioValueRange nominalSampleRate = availableNominalSampleRates[i];
		if(nominalSampleRate.mMinimum == nominalSampleRate.mMaximum)
			[availablSampleRates addObject:@(nominalSampleRate.mMinimum)];
		else
			os_log_error(OS_LOG_DEFAULT, "nominalSampleRate.mMinimum (%.2f Hz) and nominalSampleRate.mMaximum (%.2f Hz) don't match", nominalSampleRate.mMinimum, nominalSampleRate.mMaximum);
	}

	free(availableNominalSampleRates);

	return availablSampleRates;
}

- (NSString *)description
{
	return [NSString stringWithFormat:@"<%@ 0x%x, \"%@\">", self.className, _deviceID, self.deviceUID];
}

@end
