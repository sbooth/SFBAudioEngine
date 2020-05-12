/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

@import os.log;

#import "SFBAudioDevice.h"

#import "SFBAudioDeviceDataSource.h"
#import "SFBAudioDeviceNotifier.h"
#import "SFBAudioOutputDevice.h"
#import "SFBCStringForOSType.h"

os_log_t gSFBAudioDeviceLog = NULL;

static void SFBCreateAudioDeviceLog(void) __attribute__ ((constructor));
static void SFBCreateAudioDeviceLog()
{
	static dispatch_once_t onceToken;
	dispatch_once(&onceToken, ^{
		gSFBAudioDeviceLog = os_log_create("org.sbooth.AudioEngine", "AudioDevice");
	});
}

static BOOL DeviceHasBuffersInScope(AudioObjectID deviceID, AudioObjectPropertyScope scope)
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
		os_log_error(gSFBAudioDeviceLog, "AudioObjectGetPropertyDataSize (kAudioDevicePropertyStreamConfiguration) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
		return NO;
	}

	AudioBufferList *bufferList = malloc(dataSize);
	if(!bufferList) {
		os_log_error(gSFBAudioDeviceLog, "Unable to allocate memory");
		return NO;
	}

	result = AudioObjectGetPropertyData(deviceID, &propertyAddress, 0, NULL, &dataSize, bufferList);
	if(result != kAudioHardwareNoError) {
		os_log_error(gSFBAudioDeviceLog, "AudioObjectGetPropertyData (kAudioDevicePropertyStreamConfiguration) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
		free(bufferList);
		return NO;
	}

	BOOL supportsScope = bufferList->mNumberBuffers > 0;
	free(bufferList);

	return supportsScope;
}

static BOOL DeviceSupportsInput(AudioObjectID deviceID)
{
	return DeviceHasBuffersInScope(deviceID, kAudioObjectPropertyScopeInput);
}

static BOOL DeviceSupportsOutput(AudioObjectID deviceID)
{
	return DeviceHasBuffersInScope(deviceID, kAudioObjectPropertyScopeOutput);
}

@interface SFBAudioDevice ()
{
@private
	AudioObjectID _deviceID;
	NSMutableDictionary *_listenerBlocks;
}
- (void)addPropertyListenerForPropertyAddress:(const AudioObjectPropertyAddress *)propertyAddress block:(dispatch_block_t)block;
- (void)removePropertyListenerForPropertyAddress:(const AudioObjectPropertyAddress *)propertyAddress;
@end

@implementation SFBAudioDevice

static SFBAudioDeviceNotifier *sAudioDeviceNotifier = nil;

+ (void)load
{
	static dispatch_once_t onceToken;
	dispatch_once(&onceToken, ^{
		sAudioDeviceNotifier = [SFBAudioDeviceNotifier instance];
	});
}

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
		os_log_error(gSFBAudioDeviceLog, "AudioObjectGetPropertyDataSize (kAudioHardwarePropertyDevices) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
		return nil;
	}

	AudioObjectID *deviceIDs = (AudioObjectID *)malloc(dataSize);
	if(!deviceIDs) {
		os_log_error(gSFBAudioDeviceLog, "Unable to allocate memory");
		return nil;
	}

	result = AudioObjectGetPropertyData(kAudioObjectSystemObject, &propertyAddress, 0, NULL, &dataSize, deviceIDs);
	if(kAudioHardwareNoError != result) {
		os_log_error(gSFBAudioDeviceLog, "AudioObjectGetPropertyData (kAudioHardwarePropertyDevices) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
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
		os_log_error(gSFBAudioDeviceLog, "AudioObjectGetPropertyData (kAudioHardwarePropertyDefaultOutputDevice) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
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
		os_log_error(gSFBAudioDeviceLog, "AudioObjectGetPropertyData (kAudioHardwarePropertyDeviceForUID) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
		return nil;
	}

	if(deviceID == kAudioObjectUnknown) {
		os_log_error(gSFBAudioDeviceLog, "Unknown audio device UID: %{public}@", deviceUID);
		return nil;
	}

	return [self initWithAudioObjectID:deviceID];
}

- (nullable instancetype)initWithAudioObjectID:(AudioObjectID)audioObjectID
{
	NSParameterAssert(audioObjectID != kAudioObjectUnknown);

	if((self = [super init])) {
		_deviceID = audioObjectID;
		_listenerBlocks = [NSMutableDictionary dictionary];
	}
	return self;
}

- (void)dealloc
{
	for(NSValue *propertyAddressAsValue in [_listenerBlocks allKeys]) {
		AudioObjectPropertyAddress propertyAddress = {0};
		[propertyAddressAsValue getValue:&propertyAddress];
		[self removePropertyListenerForPropertyAddress:&propertyAddress];
	}
}

- (BOOL)isEqual:(id)object
{
	if(![object isKindOfClass:[SFBAudioDevice class]])
		return NO;

	SFBAudioDevice *other = (SFBAudioDevice *)object;
	return _deviceID == other->_deviceID;
}

- (NSUInteger)hash
{
	return _deviceID;
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
		os_log_error(gSFBAudioDeviceLog, "AudioObjectGetPropertyData (kAudioDevicePropertyDeviceUID) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
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
		os_log_error(gSFBAudioDeviceLog, "AudioObjectGetPropertyData (kAudioObjectPropertyName) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
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
		os_log_error(gSFBAudioDeviceLog, "AudioObjectGetPropertyData (kAudioObjectPropertyManufacturer) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
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

#pragma mark - Device Properties

- (double)sampleRate
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
		os_log_error(gSFBAudioDeviceLog, "AudioObjectGetPropertyData (kAudioDevicePropertyNominalSampleRate) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
		return nan("1");
	}

	return sampleRate;
}

- (BOOL)setSampleRate:(double)sampleRate error:(NSError **)error
{
	os_log_info(gSFBAudioDeviceLog, "Setting device 0x%x sample rate to %.2f Hz", _deviceID, sampleRate);

	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= kAudioDevicePropertyNominalSampleRate,
		.mScope		= kAudioObjectPropertyScopeGlobal,
		.mElement	= kAudioObjectPropertyElementMaster
	};

	Float64 nominalSampleRate = sampleRate;
	OSStatus result = AudioObjectSetPropertyData(_deviceID, &propertyAddress, 0, NULL, sizeof(nominalSampleRate), &nominalSampleRate);
	if(kAudioHardwareNoError != result) {
		os_log_error(gSFBAudioDeviceLog, "AudioObjectSetPropertyData (kAudioDevicePropertyNominalSampleRate) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
		if(error)
			*error = [NSError errorWithDomain:NSOSStatusErrorDomain code:result userInfo:nil];
		return NO;
	}

	return YES;
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
		os_log_error(gSFBAudioDeviceLog, "AudioObjectGetPropertyDataSize (kAudioDevicePropertyAvailableNominalSampleRates) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
		return nil;
	}

	AudioValueRange *availableNominalSampleRates = (AudioValueRange *)malloc(dataSize);
	if(!availableNominalSampleRates) {
		os_log_error(gSFBAudioDeviceLog, "Unable to allocate memory");
		return nil;
	}

	result = AudioObjectGetPropertyData(_deviceID, &propertyAddress, 0, NULL, &dataSize, availableNominalSampleRates);
	if(kAudioHardwareNoError != result) {
		os_log_error(gSFBAudioDeviceLog, "AudioObjectGetPropertyData (kAudioDevicePropertyAvailableNominalSampleRates) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
		free(availableNominalSampleRates);
		return nil;
	}

	NSMutableArray *availablSampleRates = [NSMutableArray array];

	for(NSInteger i = 0; i < (NSInteger)(dataSize / sizeof(AudioValueRange)); ++i) {
		AudioValueRange nominalSampleRate = availableNominalSampleRates[i];
		if(nominalSampleRate.mMinimum == nominalSampleRate.mMaximum)
			[availablSampleRates addObject:@(nominalSampleRate.mMinimum)];
		else
			os_log_error(gSFBAudioDeviceLog, "nominalSampleRate.mMinimum (%.2f Hz) and nominalSampleRate.mMaximum (%.2f Hz) don't match", nominalSampleRate.mMinimum, nominalSampleRate.mMaximum);
	}

	free(availableNominalSampleRates);

	return availablSampleRates;
}

- (float)volumeForChannel:(AudioObjectPropertyElement)channel inScope:(AudioObjectPropertyScope)scope
{
	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= kAudioDevicePropertyVolumeScalar,
		.mScope		= scope,
		.mElement	= channel
	};

	Float32 volume;
	UInt32 dataSize = sizeof(volume);
	OSStatus result = AudioObjectGetPropertyData(_deviceID, &propertyAddress, 0, NULL, &dataSize, &volume);
	if(result != kAudioHardwareNoError) {
		os_log_error(gSFBAudioDeviceLog, "AudioObjectGetPropertyData (kAudioDevicePropertyVolumeScalar, '%{public}.4s', %u) failed: %d", SFBCStringForOSType(scope), channel, result);
		return nanf("1");
	}
	return volume;
}

- (BOOL)setVolume:(float)volume forChannel:(AudioObjectPropertyElement)channel inScope:(AudioObjectPropertyScope)scope error:(NSError **)error
{
	os_log_info(gSFBAudioDeviceLog, "Setting device 0x%x '%{public}.4s' channel %u volume scalar to %f", _deviceID, SFBCStringForOSType(scope), channel, volume);

	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= kAudioDevicePropertyVolumeScalar,
		.mScope		= scope,
		.mElement	= channel
	};

	if(!AudioObjectHasProperty(_deviceID, &propertyAddress)) {
		os_log_debug(gSFBAudioDeviceLog, "AudioObjectHasProperty (kAudioDevicePropertyVolumeScalar, '%{public}.4s', %u) is false", SFBCStringForOSType(scope), channel);
		return NO;
	}

	OSStatus result = AudioObjectSetPropertyData(_deviceID, &propertyAddress, 0, NULL, sizeof(volume), &volume);
	if(result != kAudioHardwareNoError) {
		os_log_error(gSFBAudioDeviceLog, "AudioObjectSetPropertyData (kAudioDevicePropertyVolumeScalar, '%{public}.4s', %u) failed: %d", SFBCStringForOSType(scope), channel, result);
		if(error)
			*error = [NSError errorWithDomain:NSOSStatusErrorDomain code:result userInfo:nil];
		return NO;
	}

	return YES;
}

- (float)volumeInDecibelsForChannel:(AudioObjectPropertyElement)channel inScope:(AudioObjectPropertyScope)scope
{
	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= kAudioDevicePropertyVolumeDecibels,
		.mScope		= scope,
		.mElement	= channel
	};

	Float32 volume;
	UInt32 dataSize = sizeof(volume);
	OSStatus result = AudioObjectGetPropertyData(_deviceID, &propertyAddress, 0, NULL, &dataSize, &volume);
	if(result != kAudioHardwareNoError) {
		os_log_error(gSFBAudioDeviceLog, "AudioObjectGetPropertyData (kAudioDevicePropertyVolumeDecibels, '%{public}.4s', %u) failed: %d", SFBCStringForOSType(scope), channel, result);
		return nanf("1");
	}
	return volume;
}

- (BOOL)setVolumeInDecibels:(float)volume forChannel:(AudioObjectPropertyElement)channel inScope:(AudioObjectPropertyScope)scope error:(NSError **)error
{
	os_log_info(gSFBAudioDeviceLog, "Setting device 0x%x '%{public}.4s' channel %u volume dB to %f", _deviceID, SFBCStringForOSType(scope), channel, volume);

	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= kAudioDevicePropertyVolumeDecibels,
		.mScope		= scope,
		.mElement	= channel
	};

	if(!AudioObjectHasProperty(_deviceID, &propertyAddress)) {
		os_log_debug(gSFBAudioDeviceLog, "AudioObjectHasProperty (kAudioDevicePropertyVolumeDecibels, '%{public}.4s', %u) is false", SFBCStringForOSType(scope), channel);
		return NO;
	}

	OSStatus result = AudioObjectSetPropertyData(_deviceID, &propertyAddress, 0, NULL, sizeof(volume), &volume);
	if(result != kAudioHardwareNoError) {
		os_log_error(gSFBAudioDeviceLog, "AudioObjectSetPropertyData (kAudioDevicePropertyVolumeDecibels, '%{public}.4s', %u) failed: %d", SFBCStringForOSType(scope), channel, result);
		if(error)
			*error = [NSError errorWithDomain:NSOSStatusErrorDomain code:result userInfo:nil];
		return NO;
	}

	return YES;
}

- (float)convertVolumeScalar:(float)volumeScalar toDecibelsInScope:(AudioObjectPropertyScope)scope
{
	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= kAudioDevicePropertyVolumeScalarToDecibels,
		.mScope		= scope,
		.mElement	= kAudioObjectPropertyElementMaster
	};

	Float32 volume = volumeScalar;
	UInt32 dataSize = sizeof(volume);
	OSStatus result = AudioObjectGetPropertyData(_deviceID, &propertyAddress, 0, NULL, &dataSize, &volume);
	if(result != noErr) {
		os_log_error(gSFBAudioDeviceLog, "AudioObjectGetPropertyData (kAudioDevicePropertyVolumeScalarToDecibels, '%{public}.4s') failed: %d '%{public}.4s'", SFBCStringForOSType(scope), result, SFBCStringForOSType(result));
		return nanf("1");
	}

	return volume;
}

- (float)convertDecibels:(float)decibels toVolumeScalarInScope:(AudioObjectPropertyScope)scope
{
	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= kAudioDevicePropertyVolumeDecibelsToScalar,
		.mScope		= scope,
		.mElement	= kAudioObjectPropertyElementMaster
	};

	Float32 volume = decibels;
	UInt32 dataSize = sizeof(volume);
	OSStatus result = AudioObjectGetPropertyData(_deviceID, &propertyAddress, 0, NULL, &dataSize, &volume);
	if(result != noErr) {
		os_log_error(gSFBAudioDeviceLog, "AudioObjectGetPropertyData (kAudioDevicePropertyVolumeDecibelsToScalar, '%{public}.4s') failed: %d '%{public}.4s'", SFBCStringForOSType(scope), result, SFBCStringForOSType(result));
		return nanf("1");
	}

	return volume;
}

- (NSArray *)dataSourcesInScope:(AudioObjectPropertyScope)scope
{
	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= kAudioDevicePropertyDataSources,
		.mScope		= scope,
		.mElement	= kAudioObjectPropertyElementMaster
	};

	UInt32 dataSize = 0;
	OSStatus result = AudioObjectGetPropertyDataSize(_deviceID, &propertyAddress, 0, NULL, &dataSize);
	if(result != kAudioHardwareNoError) {
		os_log_error(gSFBAudioDeviceLog, "AudioObjectGetPropertyDataSize (kAudioDevicePropertyDataSources) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
		return nil;
	}

	UInt32 *dataSourceIDs = (UInt32 *)malloc(dataSize);
	if(!dataSourceIDs) {
		os_log_error(gSFBAudioDeviceLog, "Unable to allocate memory");
		return nil;
	}

	result = AudioObjectGetPropertyData(_deviceID, &propertyAddress, 0, NULL, &dataSize, dataSourceIDs);
	if(kAudioHardwareNoError != result) {
		os_log_error(gSFBAudioDeviceLog, "AudioObjectGetPropertyData (kAudioDevicePropertyDataSources) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
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

- (NSArray *)activeDataSourcesInScope:(AudioObjectPropertyScope)scope
{
	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= kAudioDevicePropertyDataSource,
		.mScope		= scope,
		.mElement	= kAudioObjectPropertyElementMaster
	};

	UInt32 dataSize = 0;
	OSStatus result = AudioObjectGetPropertyDataSize(_deviceID, &propertyAddress, 0, NULL, &dataSize);
	if(result != kAudioHardwareNoError) {
		os_log_error(gSFBAudioDeviceLog, "AudioObjectGetPropertyDataSize (kAudioDevicePropertyDataSource) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
		return nil;
	}

	UInt32 *dataSourceIDs = (UInt32 *)malloc(dataSize);
	if(!dataSourceIDs) {
		os_log_error(gSFBAudioDeviceLog, "Unable to allocate memory");
		return nil;
	}

	result = AudioObjectGetPropertyData(_deviceID, &propertyAddress, 0, NULL, &dataSize, dataSourceIDs);
	if(kAudioHardwareNoError != result) {
		os_log_error(gSFBAudioDeviceLog, "AudioObjectGetPropertyData (kAudioDevicePropertyDataSource) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
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

- (BOOL)setActiveDataSources:(NSArray *)activeDataSources inScope:(AudioObjectPropertyScope)scope error:(NSError **)error
{
	NSParameterAssert(activeDataSources != nil);

	os_log_info(gSFBAudioDeviceLog, "Setting device 0x%x '%{public}.4s' active data sources to %{public}@", _deviceID, SFBCStringForOSType(scope), activeDataSources);

//	if(activeDataSources.count == 0)
//		return NO;

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
	if(kAudioHardwareNoError != result) {
		os_log_error(gSFBAudioDeviceLog, "AudioObjectSetPropertyData (kAudioDevicePropertyDataSource) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
		if(error)
			*error = [NSError errorWithDomain:NSOSStatusErrorDomain code:result userInfo:nil];
		return NO;
	}

	return YES;
}

#pragma mark - Device Property Observation

- (void)whenSampleRateChangesPerformBlock:(dispatch_block_t)block
{
	[self whenProperty:kAudioDevicePropertyNominalSampleRate inScope:kAudioObjectPropertyScopeGlobal changesOnElement:kAudioObjectPropertyElementMaster performBlock:block];
}

- (void)whenDataSourcesChangeInScope:(AudioObjectPropertyScope)scope performBlock:(dispatch_block_t)block
{
	[self whenProperty:kAudioDevicePropertyDataSources inScope:scope changesOnElement:kAudioObjectPropertyElementMaster performBlock:block];
}

- (void)whenActiveDataSourcesChangeInScope:(AudioObjectPropertyScope)scope performBlock:(dispatch_block_t)block
{
	[self whenProperty:kAudioDevicePropertyDataSource inScope:scope changesOnElement:kAudioObjectPropertyElementMaster performBlock:block];
}

- (BOOL)hasProperty:(AudioObjectPropertySelector)property
{
	return [self hasProperty:property inScope:kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster];
}

- (BOOL)hasProperty:(AudioObjectPropertySelector)property inScope:(AudioObjectPropertyScope)scope
{
	return [self hasProperty:property inScope:scope onElement:kAudioObjectPropertyElementMaster];
}

- (BOOL)hasProperty:(AudioObjectPropertySelector)property inScope:(AudioObjectPropertyScope)scope onElement:(AudioObjectPropertyElement)element
{
	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= property,
		.mScope		= scope,
		.mElement	= element
	};

	return (BOOL)AudioObjectHasProperty(_deviceID, &propertyAddress);
}

- (void)whenPropertyChanges:(AudioObjectPropertySelector)property performBlock:(dispatch_block_t)block
{
	[self whenProperty:property inScope:kAudioObjectPropertyScopeGlobal changesOnElement:kAudioObjectPropertyElementMaster performBlock:block];
}

- (void)whenProperty:(AudioObjectPropertySelector)property changesInScope:(AudioObjectPropertyScope)scope performBlock:(dispatch_block_t)block
{
	[self whenProperty:property inScope:scope changesOnElement:kAudioObjectPropertyElementMaster performBlock:block];
}

- (void)whenProperty:(AudioObjectPropertySelector)property inScope:(AudioObjectPropertyScope)scope changesOnElement:(AudioObjectPropertyElement)element performBlock:(dispatch_block_t)block
{
	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= property,
		.mScope		= scope,
		.mElement	= element
	};

	[self removePropertyListenerForPropertyAddress:&propertyAddress];
	if(block)
		[self addPropertyListenerForPropertyAddress:&propertyAddress block:block];
}

- (void)addPropertyListenerForPropertyAddress:(const AudioObjectPropertyAddress *)propertyAddress block:(dispatch_block_t)block
{
	NSParameterAssert(propertyAddress != nil);
	NSParameterAssert(block != nil);

	os_log_info(gSFBAudioDeviceLog, "Adding property listener on device 0x%x for {'%{public}.4s', '%{public}.4s', %u}", _deviceID, SFBCStringForOSType(propertyAddress->mSelector), SFBCStringForOSType(propertyAddress->mScope), propertyAddress->mElement);

	NSValue *propertyAddressAsValue = [NSValue value:propertyAddress withObjCType:@encode(AudioObjectPropertyAddress)];

	AudioObjectPropertyListenerBlock listenerBlock = ^(UInt32 inNumberAddresses, const AudioObjectPropertyAddress *inAddresses) {
#pragma unused(inNumberAddresses)
#pragma unused(inAddresses)
		block();
	};

	[_listenerBlocks setObject:listenerBlock forKey:propertyAddressAsValue];

	OSStatus result = AudioObjectAddPropertyListenerBlock(_deviceID, propertyAddress, dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0), listenerBlock);
	if(result != kAudioHardwareNoError) {
		os_log_error(gSFBAudioDeviceLog, "AudioObjectAddPropertyListenerBlock ('%{public}.4s', '%{public}.4s', %u) failed: %d '%{public}.4s'", SFBCStringForOSType(propertyAddress->mSelector), SFBCStringForOSType(propertyAddress->mScope), propertyAddress->mElement, result, SFBCStringForOSType(result));
		[_listenerBlocks removeObjectForKey:propertyAddressAsValue];
	}
}

- (void)removePropertyListenerForPropertyAddress:(const AudioObjectPropertyAddress *)propertyAddress
{
	NSParameterAssert(propertyAddress != nil);

	NSValue *propertyAddressAsValue = [NSValue value:propertyAddress withObjCType:@encode(AudioObjectPropertyAddress)];
	AudioObjectPropertyListenerBlock listenerBlock = [_listenerBlocks objectForKey:propertyAddressAsValue];
	if(listenerBlock) {
		os_log_info(gSFBAudioDeviceLog, "Removing property listener on device 0x%x for {'%{public}.4s', '%{public}.4s', %u}", _deviceID, SFBCStringForOSType(propertyAddress->mSelector), SFBCStringForOSType(propertyAddress->mScope), propertyAddress->mElement);

		[_listenerBlocks removeObjectForKey:propertyAddressAsValue];

		OSStatus result = AudioObjectRemovePropertyListenerBlock(_deviceID, propertyAddress, dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0), listenerBlock);
		if(result != kAudioHardwareNoError)
			os_log_error(gSFBAudioDeviceLog, "AudioObjectRemovePropertyListenerBlock ('%{public}.4s', '%{public}.4s', %u) failed: %d '%{public}.4s'", SFBCStringForOSType(propertyAddress->mSelector), SFBCStringForOSType(propertyAddress->mScope), propertyAddress->mElement, result, SFBCStringForOSType(result));
	}
}

- (NSString *)description
{
	return [NSString stringWithFormat:@"<%@ 0x%x, \"%@\">", self.className, _deviceID, self.deviceUID];
}

@end
