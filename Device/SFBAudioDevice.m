/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import "SFBAudioDevice.h"
#import "SFBAudioObject+Internal.h"

#import "SFBAggregateDevice.h"
#import "SFBEndpointDevice.h"
#import "SFBSubdevice.h"
#import "SFBAudioDeviceDataSource.h"
#import "SFBAudioDeviceNotifier.h"
#import "SFBCStringForOSType.h"

@implementation SFBAudioDevice

static SFBAudioDeviceNotifier *sAudioDeviceNotifier = nil;

+ (void)load
{
	static dispatch_once_t onceToken;
	dispatch_once(&onceToken, ^{
		sAudioDeviceNotifier = [SFBAudioDeviceNotifier instance];
	});
}

+ (NSArray *)devices
{
	return [[SFBAudioObject systemObject] audioObjectArrayForProperty:kAudioHardwarePropertyDevices inScope:kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster error:NULL];
}

+ (SFBAudioDevice *)defaultInputDevice
{
	return (SFBAudioDevice *)[[SFBAudioObject systemObject] audioObjectForProperty:kAudioHardwarePropertyDefaultInputDevice inScope:kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster error:NULL];
}

+ (SFBAudioDevice *)defaultOutputDevice
{
	return (SFBAudioDevice *)[[SFBAudioObject systemObject] audioObjectForProperty:kAudioHardwarePropertyDefaultOutputDevice inScope:kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster error:NULL];
}

+ (SFBAudioDevice *)defaultSystemOutputDevice
{
	return (SFBAudioDevice *)[[SFBAudioObject systemObject] audioObjectForProperty:kAudioHardwarePropertyDefaultSystemOutputDevice inScope:kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster error:NULL];
}

+ (NSArray *)inputDevices
{
	NSArray *devices = [SFBAudioDevice devices];
	return [devices objectsAtIndexes:[devices indexesOfObjectsPassingTest:^BOOL(id obj, NSUInteger idx, BOOL *stop) {
#pragma unused(idx)
#pragma unused(stop)
		return [obj supportsInput];
	}]];
}

+ (NSArray *)outputDevices
{
	NSArray *devices = [SFBAudioDevice devices];
	return [devices objectsAtIndexes:[devices indexesOfObjectsPassingTest:^BOOL(id obj, NSUInteger idx, BOOL *stop) {
#pragma unused(idx)
#pragma unused(stop)
		return [obj supportsOutput];
	}]];
}

- (instancetype)initWithAudioObjectID:(AudioObjectID)objectID
{
	NSParameterAssert(SFBAudioObjectIsClassOrSubclassOf(objectID, kAudioDeviceClassID));
	return [super initWithAudioObjectID:objectID];
}

- (instancetype)initWithDeviceUID:(NSString *)deviceUID
{
	NSParameterAssert(deviceUID != nil);

	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= kAudioHardwarePropertyTranslateUIDToDevice,
		.mScope		= kAudioObjectPropertyScopeGlobal,
		.mElement	= kAudioObjectPropertyElementMaster
	};

	AudioObjectID objectID = kAudioObjectUnknown;
	UInt32 specifierSize = sizeof(objectID);
	OSStatus result = AudioObjectGetPropertyData(kAudioObjectSystemObject, &propertyAddress, sizeof(deviceUID), &deviceUID, &specifierSize, &objectID);
	if(result != kAudioHardwareNoError) {
		os_log_error(gSFBAudioObjectLog, "AudioObjectGetPropertyData (kAudioHardwarePropertyDeviceForUID) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
		return nil;
	}

	if(objectID == kAudioObjectUnknown) {
		os_log_error(gSFBAudioObjectLog, "Unknown audio device UID: %{public}@", deviceUID);
		return nil;
	}

	return [self initWithAudioObjectID:objectID];
}

- (BOOL)supportsInput
{
	return SFBAudioDeviceSupportsInput(_objectID);
}

- (BOOL)supportsOutput
{
	return SFBAudioDeviceSupportsOutput(_objectID);
}

- (BOOL)isAggregate
{
	BOOL isClass = SFBAudioObjectIsClass(_objectID, kAudioAggregateDeviceClassID);
	NSAssert(isClass == [self isKindOfClass:[SFBAggregateDevice class]], @"Aggregate device instantiated with incorrect class: %@", self.className);
	return isClass;
}

- (BOOL)isPrivateAggregate
{
	return self.isAggregate && [(SFBAggregateDevice *)self isPrivate];
}

- (BOOL)isEndpointDevice
{
	BOOL isClass = SFBAudioObjectIsClass(_objectID, kAudioEndPointDeviceClassID);
	NSAssert(isClass == [self isKindOfClass:[SFBEndpointDevice class]], @"Endpoint device instantiated with incorrect class: %@", self.className);
	return isClass;
}

- (BOOL)isEndpoint
{
	return SFBAudioObjectIsClass(_objectID, kAudioEndPointClassID);
}

- (BOOL)isSubdevice
{
	BOOL isClass = SFBAudioObjectIsClass(_objectID, kAudioSubDeviceClassID);
	NSAssert(isClass == [self isKindOfClass:[SFBSubdevice class]], @"Subdevice instantiated with incorrect class: %@", self.className);
	return isClass;
}

#pragma mark - Device Base Properties

- (NSString *)configurationApplication
{
	return [self stringForProperty:kAudioDevicePropertyConfigurationApplication inScope:kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster error:NULL];
}

- (NSString *)deviceUID
{
	return [self stringForProperty:kAudioDevicePropertyDeviceUID inScope:kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster error:NULL];
}

- (NSString *)modelUID
{
	return [self stringForProperty:kAudioDevicePropertyModelUID inScope:kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster error:NULL];
}

- (NSNumber *)transportType
{
	return [self unsignedIntForProperty:kAudioDevicePropertyTransportType inScope:kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster error:NULL];
}

- (NSArray *)relatedDevicesInScope:(SFBAudioObjectPropertyScope)scope
{
	return [self audioObjectArrayForProperty:kAudioDevicePropertyRelatedDevices inScope:scope onElement:kAudioObjectPropertyElementMaster error:NULL];
}

- (NSNumber *)clockDomain
{
	return [self unsignedIntForProperty:kAudioDevicePropertyClockDomain inScope:kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster error:NULL];
}

- (NSNumber *)isAlive
{
	return [self unsignedIntForProperty:kAudioDevicePropertyDeviceIsAlive inScope:kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster error:NULL];
}

- (NSNumber *)isRunning
{
	return [self unsignedIntForProperty:kAudioDevicePropertyDeviceIsRunning inScope:kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster error:NULL];
}

- (BOOL)setIsRunning:(BOOL)value error:(NSError **)error
{
	return [self setUnsignedInt:(value != 0) forProperty:kAudioDevicePropertyDeviceIsRunning inScope:kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster error:error];
}

- (NSNumber *)canBeDefaultInScope:(SFBAudioObjectPropertyScope)scope
{
	return [self unsignedIntForProperty:kAudioDevicePropertyDeviceCanBeDefaultDevice inScope:scope onElement:kAudioObjectPropertyElementMaster error:NULL];
}

- (NSNumber *)canBeSystemDefaultInScope:(SFBAudioObjectPropertyScope)scope
{
	return [self unsignedIntForProperty:kAudioDevicePropertyDeviceCanBeDefaultSystemDevice inScope:scope onElement:kAudioObjectPropertyElementMaster error:NULL];
}

- (NSNumber *)latencyInScope:(SFBAudioObjectPropertyScope)scope
{
	return [self unsignedIntForProperty:kAudioDevicePropertyLatency inScope:scope onElement:kAudioObjectPropertyElementMaster error:NULL];
}

- (NSArray *)streamsInScope:(SFBAudioObjectPropertyScope)scope
{
	return [self audioObjectArrayForProperty:kAudioDevicePropertyStreams inScope:scope onElement:kAudioObjectPropertyElementMaster error:NULL];
}

- (NSArray *)controlsInScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element
{
	return [self audioObjectArrayForProperty:kAudioObjectPropertyControlList inScope:scope onElement:element error:NULL];
}

- (NSNumber *)safetyOffsetInScope:(SFBAudioObjectPropertyScope)scope
{
	return [self unsignedIntForProperty:kAudioDevicePropertySafetyOffset inScope:scope onElement:kAudioObjectPropertyElementMaster error:NULL];
}

- (NSNumber *)sampleRate
{
	return [self doubleForProperty:kAudioDevicePropertyNominalSampleRate inScope:kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster error:NULL];
}

- (BOOL)setSampleRate:(double)sampleRate error:(NSError **)error
{
	os_log_info(gSFBAudioObjectLog, "Setting device 0x%x sample rate to %.2f Hz", _objectID, sampleRate);
	return [self setDouble:sampleRate forProperty:kAudioDevicePropertyNominalSampleRate inScope:kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster error:error];
}

- (NSArray *)availableSampleRates
{
	return [self audioValueRangeArrayForProperty:kAudioDevicePropertyAvailableNominalSampleRates inScope:kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster error:NULL];
}

- (NSURL *)icon
{
	return [self urlForProperty:kAudioDevicePropertyIcon inScope:kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster error:NULL];
}

- (NSNumber *)isHidden
{
	return [self unsignedIntForProperty:kAudioDevicePropertyIsHidden inScope:kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster error:NULL];
}

- (NSArray *)preferredStereoChannelsInScope:(SFBAudioObjectPropertyScope)scope
{
	return [self unsignedIntArrayForProperty:kAudioDevicePropertyPreferredChannelsForStereo inScope:scope onElement:kAudioObjectPropertyElementMaster error:NULL];
}

- (SFBAudioChannelLayoutWrapper *)preferredChannelLayoutInScope:(SFBAudioObjectPropertyScope)scope
{
	return [self audioChannelLayoutForProperty:kAudioDevicePropertyPreferredChannelLayout inScope:scope onElement:kAudioObjectPropertyElementMaster error:NULL];
}

- (BOOL)setPreferredChannelLayout:(SFBAudioChannelLayoutWrapper *)channelLayout inScope:(SFBAudioObjectPropertyScope)scope error:(NSError **)error
{
	return [self setAudioChannelLayout:channelLayout forProperty:kAudioDevicePropertyPreferredChannelLayout inScope:scope onElement:kAudioObjectPropertyElementMaster error:error];
}

#pragma mark - Device Properties

- (NSNumber *)plugInInScope:(SFBAudioObjectPropertyScope)scope
{
	return [self unsignedIntForProperty:kAudioDevicePropertyPlugIn inScope:scope onElement:kAudioObjectPropertyElementMaster error:NULL];
}


- (NSNumber *)isRunningSomewhere
{
	return [self unsignedIntForProperty:kAudioDevicePropertyDeviceIsRunningSomewhere inScope:kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster error:NULL];
}

- (NSNumber *)hogModeInScope:(SFBAudioObjectPropertyScope)scope
{
	return [self unsignedIntForProperty:kAudioDevicePropertyHogMode inScope:scope onElement:kAudioObjectPropertyElementMaster error:NULL];
}

- (BOOL)setHogMode:(pid_t)value inScope:(SFBAudioObjectPropertyScope)scope error:(NSError **)error
{
	return [self setUnsignedInt:(unsigned int)value forProperty:kAudioDevicePropertyHogMode inScope:scope onElement:kAudioObjectPropertyElementMaster error:error];
}

// Hog mode helpers
- (NSNumber *)isHoggedInScope:(SFBAudioObjectPropertyScope)scope
{
	NSNumber *hogpid = [self hogModeInScope:scope];
	if(!hogpid)
		return nil;
	return @(hogpid.pidValue != (pid_t)-1);
}

- (NSNumber *)isHogOwnerInScope:(SFBAudioObjectPropertyScope)scope
{
	NSNumber *hogpid = [self hogModeInScope:scope];
	if(!hogpid)
		return nil;
	return @(hogpid.pidValue == getpid());
}

- (BOOL)startHoggingInScope:(SFBAudioObjectPropertyScope)scope error:(NSError **)error
{
	os_log_info(gSFBAudioObjectLog, "Taking hog mode for device 0x%x '%{public}.4s'", _objectID, SFBCStringForOSType(scope));

	NSNumber *hogpid = [self hogModeInScope:scope];
	if(hogpid && hogpid.pidValue != (pid_t)-1)
		os_log_error(gSFBAudioObjectLog, "Device is already hogged by pid: %d", hogpid.pidValue);

	return [self setHogMode:getpid() inScope:scope error:error];
}

- (BOOL)stopHoggingInScope:(SFBAudioObjectPropertyScope)scope error:(NSError **)error
{
	os_log_info(gSFBAudioObjectLog, "Releasing hog mode for device 0x%x '%{public}.4s'", _objectID, SFBCStringForOSType(scope));

	NSNumber *hogpid = [self hogModeInScope:scope];
	if(hogpid && hogpid.pidValue != getpid())
		os_log_error(gSFBAudioObjectLog, "Device is hogged by pid: %d", hogpid.pidValue);

	return [self setHogMode:(pid_t)-1 inScope:scope error:error];
}

- (NSNumber *)bufferFrameSizeInScope:(SFBAudioObjectPropertyScope)scope
{
	return [self unsignedIntForProperty:kAudioDevicePropertyBufferFrameSize inScope:scope onElement:kAudioObjectPropertyElementMaster error:NULL];
}

- (BOOL)setBufferFrameSize:(UInt32)value inScope:(SFBAudioObjectPropertyScope)scope error:(NSError **)error
{
	return [self setUnsignedInt:value forProperty:value inScope:value onElement:kAudioObjectPropertyElementMaster error:error];
}

- (NSValue *)bufferFrameSizeRangeInScope:(SFBAudioObjectPropertyScope)scope
{
	return [self audioValueRangeForProperty:kAudioDevicePropertyBufferFrameSizeRange inScope:scope onElement:kAudioObjectPropertyElementMaster error:NULL];
}

- (NSNumber *)usesVariableBufferFrameSizesInScope:(SFBAudioObjectPropertyScope)scope
{
	return [self unsignedIntForProperty:kAudioDevicePropertyUsesVariableBufferFrameSizes inScope:scope onElement:kAudioObjectPropertyElementMaster error:NULL];
}

- (NSNumber *)ioCycleUsageInScope:(SFBAudioObjectPropertyScope)scope
{
	return [self floatForProperty:kAudioDevicePropertyIOCycleUsage inScope:scope onElement:kAudioObjectPropertyElementMaster error:NULL];
}

- (SFBAudioBufferListWrapper *)streamConfigurationInScope:(SFBAudioObjectPropertyScope)scope
{
	return [self audioBufferListForProperty:kAudioDevicePropertyStreamConfiguration inScope:scope onElement:kAudioObjectPropertyElementMaster error:NULL];
}

- (NSValue *)ioProcStreamUsageInScope:(SFBAudioObjectPropertyScope)scope
{
//	kAudioDevicePropertyIOProcStreamUsage
//	AudioHardwareIOProcStreamUsage
	return nil;
}

- (NSNumber *)actualSampleRate
{
	return [self doubleForProperty:kAudioDevicePropertyActualSampleRate inScope:kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster error:NULL];
}

- (NSString *)clockDeviceInScope:(SFBAudioObjectPropertyScope)scope
{
	return [self stringForProperty:kAudioDevicePropertyClockDevice inScope:scope onElement:kAudioObjectPropertyElementMaster error:NULL];
}

- (NSNumber *)ioThreadOSWorkgroupInScope:(SFBAudioObjectPropertyScope)scope
{
//	kAudioDevicePropertyIOThreadOSWorkgroup
//	os_workgroup_t;
	return nil;
}

#pragma mark - Audio Controls

- (float)volumeForChannel:(SFBAudioObjectPropertyElement)channel inScope:(SFBAudioObjectPropertyScope)scope
{
	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= kAudioDevicePropertyVolumeScalar,
		.mScope		= scope,
		.mElement	= channel
	};

	Float32 volume;
	UInt32 dataSize = sizeof(volume);
	OSStatus result = AudioObjectGetPropertyData(_objectID, &propertyAddress, 0, NULL, &dataSize, &volume);
	if(result != kAudioHardwareNoError) {
		os_log_error(gSFBAudioObjectLog, "AudioObjectGetPropertyData (kAudioDevicePropertyVolumeScalar, '%{public}.4s', %u) failed: %d", SFBCStringForOSType(scope), channel, result);
		return nanf("1");
	}
	return volume;
}

- (BOOL)setVolume:(float)volume forChannel:(SFBAudioObjectPropertyElement)channel inScope:(SFBAudioObjectPropertyScope)scope error:(NSError **)error
{
	os_log_info(gSFBAudioObjectLog, "Setting device 0x%x '%{public}.4s' channel %u volume scalar to %f", _objectID, SFBCStringForOSType(scope), channel, volume);

	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= kAudioDevicePropertyVolumeScalar,
		.mScope		= scope,
		.mElement	= channel
	};

	if(!AudioObjectHasProperty(_objectID, &propertyAddress)) {
		os_log_debug(gSFBAudioObjectLog, "AudioObjectHasProperty (kAudioDevicePropertyVolumeScalar, '%{public}.4s', %u) is false", SFBCStringForOSType(scope), channel);
		return NO;
	}

	OSStatus result = AudioObjectSetPropertyData(_objectID, &propertyAddress, 0, NULL, sizeof(volume), &volume);
	if(result != kAudioHardwareNoError) {
		os_log_error(gSFBAudioObjectLog, "AudioObjectSetPropertyData (kAudioDevicePropertyVolumeScalar, '%{public}.4s', %u) failed: %d", SFBCStringForOSType(scope), channel, result);
		if(error)
			*error = [NSError errorWithDomain:NSOSStatusErrorDomain code:result userInfo:nil];
		return NO;
	}

	return YES;
}

- (float)volumeInDecibelsForChannel:(SFBAudioObjectPropertyElement)channel inScope:(SFBAudioObjectPropertyScope)scope
{
	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= kAudioDevicePropertyVolumeDecibels,
		.mScope		= scope,
		.mElement	= channel
	};

	Float32 volume;
	UInt32 dataSize = sizeof(volume);
	OSStatus result = AudioObjectGetPropertyData(_objectID, &propertyAddress, 0, NULL, &dataSize, &volume);
	if(result != kAudioHardwareNoError) {
		os_log_error(gSFBAudioObjectLog, "AudioObjectGetPropertyData (kAudioDevicePropertyVolumeDecibels, '%{public}.4s', %u) failed: %d", SFBCStringForOSType(scope), channel, result);
		return nanf("1");
	}
	return volume;
}

- (BOOL)setVolumeInDecibels:(float)volume forChannel:(SFBAudioObjectPropertyElement)channel inScope:(SFBAudioObjectPropertyScope)scope error:(NSError **)error
{
	os_log_info(gSFBAudioObjectLog, "Setting device 0x%x '%{public}.4s' channel %u volume dB to %f", _objectID, SFBCStringForOSType(scope), channel, volume);

	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= kAudioDevicePropertyVolumeDecibels,
		.mScope		= scope,
		.mElement	= channel
	};

	if(!AudioObjectHasProperty(_objectID, &propertyAddress)) {
		os_log_debug(gSFBAudioObjectLog, "AudioObjectHasProperty (kAudioDevicePropertyVolumeDecibels, '%{public}.4s', %u) is false", SFBCStringForOSType(scope), channel);
		return NO;
	}

	OSStatus result = AudioObjectSetPropertyData(_objectID, &propertyAddress, 0, NULL, sizeof(volume), &volume);
	if(result != kAudioHardwareNoError) {
		os_log_error(gSFBAudioObjectLog, "AudioObjectSetPropertyData (kAudioDevicePropertyVolumeDecibels, '%{public}.4s', %u) failed: %d", SFBCStringForOSType(scope), channel, result);
		if(error)
			*error = [NSError errorWithDomain:NSOSStatusErrorDomain code:result userInfo:nil];
		return NO;
	}

	return YES;
}

- (float)convertVolumeScalar:(float)volumeScalar toDecibelsInScope:(SFBAudioObjectPropertyScope)scope
{
	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= kAudioDevicePropertyVolumeScalarToDecibels,
		.mScope		= scope,
		.mElement	= kAudioObjectPropertyElementMaster
	};

	Float32 volume = volumeScalar;
	UInt32 dataSize = sizeof(volume);
	OSStatus result = AudioObjectGetPropertyData(_objectID, &propertyAddress, 0, NULL, &dataSize, &volume);
	if(result != noErr) {
		os_log_error(gSFBAudioObjectLog, "AudioObjectGetPropertyData (kAudioDevicePropertyVolumeScalarToDecibels, '%{public}.4s') failed: %d '%{public}.4s'", SFBCStringForOSType(scope), result, SFBCStringForOSType(result));
		return nanf("1");
	}

	return volume;
}

- (float)convertDecibels:(float)decibels toVolumeScalarInScope:(SFBAudioObjectPropertyScope)scope
{
	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= kAudioDevicePropertyVolumeDecibelsToScalar,
		.mScope		= scope,
		.mElement	= kAudioObjectPropertyElementMaster
	};

	Float32 volume = decibels;
	UInt32 dataSize = sizeof(volume);
	OSStatus result = AudioObjectGetPropertyData(_objectID, &propertyAddress, 0, NULL, &dataSize, &volume);
	if(result != noErr) {
		os_log_error(gSFBAudioObjectLog, "AudioObjectGetPropertyData (kAudioDevicePropertyVolumeDecibelsToScalar, '%{public}.4s') failed: %d '%{public}.4s'", SFBCStringForOSType(scope), result, SFBCStringForOSType(result));
		return nanf("1");
	}

	return volume;
}

- (BOOL)isMutedInScope:(SFBAudioObjectPropertyScope)scope
{
	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= kAudioDevicePropertyMute,
		.mScope		= scope,
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

- (BOOL)setMute:(BOOL)mute inScope:(SFBAudioObjectPropertyScope)scope error:(NSError **)error
{
	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= kAudioDevicePropertyMute,
		.mScope		= scope,
		.mElement	= kAudioObjectPropertyElementMaster
	};

	UInt32 muted = (UInt32)mute;
	OSStatus result = AudioObjectSetPropertyData(_objectID, &propertyAddress, 0, NULL, sizeof(muted), &muted);
	if(result != kAudioHardwareNoError) {
		os_log_error(gSFBAudioObjectLog, "AudioObjectSetPropertyData (kAudioDevicePropertyMute) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
		if(error)
			*error = [NSError errorWithDomain:NSOSStatusErrorDomain code:result userInfo:nil];
		return NO;
	}

	return YES;
}

- (NSArray *)dataSourcesInScope:(SFBAudioObjectPropertyScope)scope
{
	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= kAudioDevicePropertyDataSources,
		.mScope		= scope,
		.mElement	= kAudioObjectPropertyElementMaster
	};

	UInt32 dataSize = 0;
	OSStatus result = AudioObjectGetPropertyDataSize(_objectID, &propertyAddress, 0, NULL, &dataSize);
	if(result != kAudioHardwareNoError) {
		os_log_error(gSFBAudioObjectLog, "AudioObjectGetPropertyDataSize (kAudioDevicePropertyDataSources) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
		return nil;
	}

	UInt32 *dataSourceIDs = (UInt32 *)malloc(dataSize);
	if(!dataSourceIDs) {
		os_log_error(gSFBAudioObjectLog, "Unable to allocate memory");
		return nil;
	}

	result = AudioObjectGetPropertyData(_objectID, &propertyAddress, 0, NULL, &dataSize, dataSourceIDs);
	if(kAudioHardwareNoError != result) {
		os_log_error(gSFBAudioObjectLog, "AudioObjectGetPropertyData (kAudioDevicePropertyDataSources) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
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

- (NSArray *)activeDataSourcesInScope:(SFBAudioObjectPropertyScope)scope
{
	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= kAudioDevicePropertyDataSource,
		.mScope		= scope,
		.mElement	= kAudioObjectPropertyElementMaster
	};

	UInt32 dataSize = 0;
	OSStatus result = AudioObjectGetPropertyDataSize(_objectID, &propertyAddress, 0, NULL, &dataSize);
	if(result != kAudioHardwareNoError) {
		os_log_error(gSFBAudioObjectLog, "AudioObjectGetPropertyDataSize (kAudioDevicePropertyDataSource) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
		return nil;
	}

	UInt32 *dataSourceIDs = (UInt32 *)malloc(dataSize);
	if(!dataSourceIDs) {
		os_log_error(gSFBAudioObjectLog, "Unable to allocate memory");
		return nil;
	}

	result = AudioObjectGetPropertyData(_objectID, &propertyAddress, 0, NULL, &dataSize, dataSourceIDs);
	if(kAudioHardwareNoError != result) {
		os_log_error(gSFBAudioObjectLog, "AudioObjectGetPropertyData (kAudioDevicePropertyDataSource) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
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

- (BOOL)setActiveDataSources:(NSArray *)activeDataSources inScope:(SFBAudioObjectPropertyScope)scope error:(NSError **)error
{
	NSParameterAssert(activeDataSources != nil);

	os_log_info(gSFBAudioObjectLog, "Setting device 0x%x '%{public}.4s' active data sources to %{public}@", _objectID, SFBCStringForOSType(scope), activeDataSources);

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

	OSStatus result = AudioObjectSetPropertyData(_objectID, &propertyAddress, 0, NULL, (UInt32)sizeof(dataSourceIDs), dataSourceIDs);
	if(kAudioHardwareNoError != result) {
		os_log_error(gSFBAudioObjectLog, "AudioObjectSetPropertyData (kAudioDevicePropertyDataSource) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
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

- (void)whenDataSourcesChangeInScope:(SFBAudioObjectPropertyScope)scope performBlock:(dispatch_block_t)block
{
	[self whenProperty:kAudioDevicePropertyDataSources inScope:scope changesOnElement:kAudioObjectPropertyElementMaster performBlock:block];
}

- (void)whenActiveDataSourcesChangeInScope:(SFBAudioObjectPropertyScope)scope performBlock:(dispatch_block_t)block
{
	[self whenProperty:kAudioDevicePropertyDataSource inScope:scope changesOnElement:kAudioObjectPropertyElementMaster performBlock:block];
}

- (void)whenMuteChangeInScope:(SFBAudioObjectPropertyScope)scope performBlock:(dispatch_block_t)block
{
	[self whenProperty:kAudioDevicePropertyMute inScope:scope changesOnElement:kAudioObjectPropertyElementMaster performBlock:block];
}

- (void)whenMasterVolumeChangesInScope:(SFBAudioObjectPropertyScope)scope performBlock:(dispatch_block_t)block
{
	[self whenProperty:kAudioDevicePropertyVolumeScalar inScope:scope changesOnElement:kAudioObjectPropertyElementMaster performBlock:block];
}

- (void)whenVolumeChangesForChannel:(AudioObjectPropertyElement)channel inScope:(SFBAudioObjectPropertyScope)scope performBlock:(dispatch_block_t)block
{
	[self whenProperty:kAudioDevicePropertyVolumeScalar inScope:scope changesOnElement:channel performBlock:block];
}

- (NSString *)description
{
	return [NSString stringWithFormat:@"<%@ 0x%x, \"%@\">", self.className, _objectID, self.deviceUID];
}

@end
