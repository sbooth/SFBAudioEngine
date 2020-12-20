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

- (BOOL)isPrivateAggregateInScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element
{
	if(!self.isAggregate)
		return NO;
	return [[(SFBAggregateDevice *)self isPrivateInScope:scope onElement:element] boolValue];
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

- (NSString *)configurationApplicationInScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element
{
	return [self stringForProperty:kAudioDevicePropertyConfigurationApplication inScope:scope onElement:element error:NULL];
}

- (NSString *)deviceUIDInScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element
{
	return [self stringForProperty:kAudioDevicePropertyDeviceUID inScope:scope onElement:element error:NULL];
}

- (NSString *)modelUIDInScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element
{
	return [self stringForProperty:kAudioDevicePropertyModelUID inScope:scope onElement:element error:NULL];
}

- (NSNumber *)transportTypeInScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element
{
	return [self unsignedIntForProperty:kAudioDevicePropertyTransportType inScope:scope onElement:element error:NULL];
}

- (NSArray *)relatedDevicesInScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element
{
	return [self audioObjectArrayForProperty:kAudioDevicePropertyRelatedDevices inScope:scope onElement:element error:NULL];
}

- (NSNumber *)clockDomainInScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element
{
	return [self unsignedIntForProperty:kAudioDevicePropertyClockDomain inScope:scope onElement:element error:NULL];
}

- (NSNumber *)isAliveInScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element
{
	return [self unsignedIntForProperty:kAudioDevicePropertyDeviceIsAlive inScope:scope onElement:element error:NULL];
}

- (NSNumber *)isRunningInScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element
{
	return [self unsignedIntForProperty:kAudioDevicePropertyDeviceIsRunning inScope:scope onElement:element error:NULL];
}

- (NSNumber *)canBeDefaultInScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element
{
	return [self unsignedIntForProperty:kAudioDevicePropertyDeviceCanBeDefaultDevice inScope:scope onElement:element error:NULL];
}

- (NSNumber *)canBeSystemDefaultInScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element
{
	return [self unsignedIntForProperty:kAudioDevicePropertyDeviceCanBeDefaultSystemDevice inScope:scope onElement:element error:NULL];
}

- (NSNumber *)latencyInScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element
{
	return [self unsignedIntForProperty:kAudioDevicePropertyLatency inScope:scope onElement:element error:NULL];
}

- (NSArray *)streamsInScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element
{
	return [self audioObjectArrayForProperty:kAudioDevicePropertyStreams inScope:scope onElement:element error:NULL];
}

- (NSArray *)controlsInScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element
{
	return [self audioObjectArrayForProperty:kAudioObjectPropertyControlList inScope:scope onElement:element error:NULL];
}

- (NSNumber *)safetyOffsetInScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element
{
	return [self unsignedIntForProperty:kAudioDevicePropertySafetyOffset inScope:scope onElement:element error:NULL];
}

- (NSNumber *)sampleRateInScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element
{
	return [self doubleForProperty:kAudioDevicePropertyNominalSampleRate inScope:scope onElement:element error:NULL];
}

- (BOOL)setSampleRate:(double)sampleRate inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element error:(NSError **)error
{
	os_log_info(gSFBAudioObjectLog, "Setting device 0x%x sample rate to %.2f Hz", _objectID, sampleRate);
	return [self setDouble:sampleRate forProperty:kAudioDevicePropertyNominalSampleRate inScope:scope onElement:element error:error];
}

- (NSArray *)availableSampleRatesInScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element
{
	return [self audioValueRangeArrayForProperty:kAudioDevicePropertyAvailableNominalSampleRates inScope:scope onElement:element error:NULL];
}

- (NSURL *)iconInScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element
{
	return [self urlForProperty:kAudioDevicePropertyIcon inScope:scope onElement:element error:NULL];
}

- (NSNumber *)isHiddenInScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element
{
	return [self unsignedIntForProperty:kAudioDevicePropertyIsHidden inScope:scope onElement:element error:NULL];
}

- (NSArray *)preferredStereoChannelsInScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element
{
	return [self unsignedIntArrayForProperty:kAudioDevicePropertyPreferredChannelsForStereo inScope:scope onElement:element error:NULL];
}

- (AVAudioChannelLayout *)preferredChannelLayoutInScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element
{
	return [self audioChannelLayoutForProperty:kAudioDevicePropertyPreferredChannelLayout inScope:scope onElement:element error:NULL];
}

- (BOOL)setPreferredChannelLayout:(AVAudioChannelLayout *)channelLayout inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element error:(NSError **)error
{
	return [self setAudioChannelLayout:channelLayout forProperty:kAudioDevicePropertyPreferredChannelLayout inScope:scope onElement:element error:error];
}

#pragma mark - Device Properties

- (BOOL)isHoggedInScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element
{
	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= kAudioDevicePropertyHogMode,
		.mScope		= scope,
		.mElement	= element
	};

	pid_t hogPID = (pid_t)-1;
	UInt32 dataSize = sizeof(hogPID);

	OSStatus result = AudioObjectGetPropertyData(_objectID, &propertyAddress, 0, NULL, &dataSize, &hogPID);
	if(kAudioHardwareNoError != result) {
		os_log_error(gSFBAudioObjectLog, "AudioObjectGetPropertyData (kAudioDevicePropertyHogMode) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
		return NO;
	}

	return hogPID != (pid_t)-1;
}

- (BOOL)isHogOwnerInScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element
{
	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= kAudioDevicePropertyHogMode,
		.mScope		= scope,
		.mElement	= element
	};

	pid_t hogPID = (pid_t)-1;
	UInt32 dataSize = sizeof(hogPID);

	OSStatus result = AudioObjectGetPropertyData(_objectID, &propertyAddress, 0, NULL, &dataSize, &hogPID);
	if(kAudioHardwareNoError != result) {
		os_log_error(gSFBAudioObjectLog, "AudioObjectGetPropertyData (kAudioDevicePropertyHogMode) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
		return NO;
	}

	return hogPID == getpid();
}

- (BOOL)startHoggingInScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element error:(NSError **)error
{
	os_log_info(gSFBAudioObjectLog, "Taking hog mode for device 0x%x '%{public}.4s'", _objectID, SFBCStringForOSType(scope));

	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= kAudioDevicePropertyHogMode,
		.mScope		= scope,
		.mElement	= element
	};

	pid_t hogPID = (pid_t)-1;
	UInt32 dataSize = sizeof(hogPID);

	OSStatus result = AudioObjectGetPropertyData(_objectID, &propertyAddress, 0, NULL, &dataSize, &hogPID);
	if(kAudioHardwareNoError != result) {
		os_log_error(gSFBAudioObjectLog, "AudioObjectGetPropertyData (kAudioDevicePropertyHogMode) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
		if(error)
			*error = [NSError errorWithDomain:NSOSStatusErrorDomain code:result userInfo:nil];
		return NO;
	}

	// The device is already hogged
	if(hogPID != (pid_t)-1)
		os_log_error(gSFBAudioObjectLog, "Device is already hogged by pid: %d", hogPID);

	hogPID = getpid();

	result = AudioObjectSetPropertyData(_objectID, &propertyAddress, 0, NULL, sizeof(hogPID), &hogPID);
	if(kAudioHardwareNoError != result) {
		os_log_error(gSFBAudioObjectLog, "AudioObjectSetPropertyData (kAudioDevicePropertyHogMode) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
		if(error)
			*error = [NSError errorWithDomain:NSOSStatusErrorDomain code:result userInfo:nil];
		return NO;
	}

	return YES;
}

- (BOOL)stopHoggingInScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element error:(NSError **)error
{
	os_log_info(gSFBAudioObjectLog, "Releasing hog mode for device 0x%x '%{public}.4s'", _objectID, SFBCStringForOSType(scope));

	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= kAudioDevicePropertyHogMode,
		.mScope		= scope,
		.mElement	= element
	};

	pid_t hogPID = (pid_t)-1;
	UInt32 dataSize = sizeof(hogPID);

	OSStatus result = AudioObjectGetPropertyData(_objectID, &propertyAddress, 0, NULL, &dataSize, &hogPID);
	if(kAudioHardwareNoError != result) {
		os_log_error(gSFBAudioObjectLog, "AudioObjectGetPropertyData (kAudioDevicePropertyHogMode) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
		if(error)
			*error = [NSError errorWithDomain:NSOSStatusErrorDomain code:result userInfo:nil];
		return NO;
	}

	// If we don't own hog mode we can't release it
	if(hogPID != getpid())
		os_log_error(gSFBAudioObjectLog, "Device is hogged by pid: %d", hogPID);

	// Release hog mode.
	hogPID = (pid_t)-1;

	result = AudioObjectSetPropertyData(_objectID, &propertyAddress, 0, NULL, sizeof(hogPID), &hogPID);
	if(kAudioHardwareNoError != result) {
		os_log_error(gSFBAudioObjectLog, "AudioObjectSetPropertyData (kAudioDevicePropertyHogMode) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
		if(error)
			*error = [NSError errorWithDomain:NSOSStatusErrorDomain code:result userInfo:nil];
		return NO;
	}

	return YES;
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
		.mElement	= element
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
		.mElement	= element
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
		.mElement	= element
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
		.mElement	= element
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
		.mElement	= element
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
		.mElement	= element
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
		.mElement	= element
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
	[self whenProperty:kAudioDevicePropertyNominalSampleRate inScope:scope changesOnElement:element performBlock:block];
}

- (void)whenDataSourcesChangeInScope:(SFBAudioObjectPropertyScope)scope performBlock:(dispatch_block_t)block
{
	[self whenProperty:kAudioDevicePropertyDataSources inScope:scope changesOnElement:element performBlock:block];
}

- (void)whenActiveDataSourcesChangeInScope:(SFBAudioObjectPropertyScope)scope performBlock:(dispatch_block_t)block
{
	[self whenProperty:kAudioDevicePropertyDataSource inScope:scope changesOnElement:element performBlock:block];
}

- (void)whenMuteChangeInScope:(SFBAudioObjectPropertyScope)scope performBlock:(dispatch_block_t)block
{
	[self whenProperty:kAudioDevicePropertyMute inScope:scope changesOnElement:element performBlock:block];
}

- (void)whenMasterVolumeChangesInScope:(SFBAudioObjectPropertyScope)scope performBlock:(dispatch_block_t)block
{
	[self whenProperty:kAudioDevicePropertyVolumeScalar inScope:scope changesOnElement:element performBlock:block];
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
