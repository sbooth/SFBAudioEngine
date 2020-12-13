/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

@import os.log;

#import "SFBAudioDevice.h"
#import "SFBAudioObject+Internal.h"

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
	return [[SFBAudioObject systemObject] audioObjectArrayForProperty:kAudioHardwarePropertyDevices];
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

+ (SFBAudioDevice *)defaultInputDevice
{
	return (SFBAudioDevice *)[[SFBAudioObject systemObject] audioObjectForProperty:kAudioHardwarePropertyDefaultInputDevice];
}

+ (SFBAudioDevice *)defaultOutputDevice
{
	return (SFBAudioDevice *)[[SFBAudioObject systemObject] audioObjectForProperty:kAudioHardwarePropertyDefaultOutputDevice];
}

+ (SFBAudioDevice *)defaultSystemOutputDevice
{
	return (SFBAudioDevice *)[[SFBAudioObject systemObject] audioObjectForProperty:kAudioHardwarePropertyDefaultSystemOutputDevice];
}

- (instancetype)initWithAudioObjectID:(AudioObjectID)objectID
{
	NSParameterAssert(SFBAudioObjectIsDevice(objectID));
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

	AudioObjectID deviceID = kAudioObjectUnknown;
	UInt32 specifierSize = sizeof(deviceID);
	OSStatus result = AudioObjectGetPropertyData(kAudioObjectSystemObject, &propertyAddress, sizeof(deviceUID), &deviceUID, &specifierSize, &deviceID);
	if(result != kAudioHardwareNoError) {
		os_log_error(gSFBAudioObjectLog, "AudioObjectGetPropertyData (kAudioHardwarePropertyDeviceForUID) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
		return nil;
	}

	if(deviceID == kAudioObjectUnknown) {
		os_log_error(gSFBAudioObjectLog, "Unknown audio device UID: %{public}@", deviceUID);
		return nil;
	}

	return [self initWithAudioObjectID:deviceID];
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
	return SFBAudioDeviceIsAggregate(_objectID);
}

- (BOOL)isPrivateAggregate
{
	if(!SFBAudioDeviceIsAggregate(_objectID))
		return NO;
	return [[[self dictionaryForProperty:kAudioAggregateDevicePropertyComposition] objectForKey:@ kAudioAggregateDeviceIsPrivateKey] boolValue];
}

- (BOOL)isEndPointDevice
{
	return SFBAudioDeviceIsEndpointDevice(_objectID);
}

- (BOOL)isEndPoint
{
	return SFBAudioDeviceIsEndpoint(_objectID);
}

#pragma mark - Device Properties

- (NSString *)configurationApplication
{
	return [self stringForProperty:kAudioDevicePropertyConfigurationApplication];
}

- (NSString *)deviceUID
{
	return [self stringForProperty:kAudioDevicePropertyDeviceUID];
}

- (NSString *)modelUID
{
	return [self stringForProperty:kAudioDevicePropertyModelUID];
}

- (UInt32)transportType
{
	return [self uInt32ForProperty:kAudioDevicePropertyTransportType];
}

- (NSArray *)relatedDevices
{
	return [self audioObjectArrayForProperty:kAudioDevicePropertyRelatedDevices];
}

- (UInt32)clockDomain
{
	return [self uInt32ForProperty:kAudioDevicePropertyClockDomain];
}

- (BOOL)isAlive
{
	return (BOOL)[self uInt32ForProperty:kAudioDevicePropertyDeviceIsAlive];
}

- (BOOL)isRunning
{
	return (BOOL)[self uInt32ForProperty:kAudioDevicePropertyDeviceIsRunning];
}

- (BOOL)canBeDefault
{
	return (BOOL)[self uInt32ForProperty:kAudioDevicePropertyDeviceCanBeDefaultDevice];
}

- (BOOL)canBeSystemDefault
{
	return (BOOL)[self uInt32ForProperty:kAudioDevicePropertyDeviceCanBeDefaultSystemDevice];
}

- (UInt32)latency
{
	return [self uInt32ForProperty:kAudioDevicePropertyLatency];
}

- (NSArray *)streams
{
	return [self audioObjectArrayForProperty:kAudioDevicePropertyStreams];
}

- (NSArray *)controls
{
	return [self audioObjectArrayForProperty:kAudioObjectPropertyControlList];
}

- (UInt32)safetyOffset
{
	return [self uInt32ForProperty:kAudioDevicePropertySafetyOffset];
}

- (double)sampleRate
{
	return [self float64ForProperty:kAudioDevicePropertyNominalSampleRate];
}

- (BOOL)setSampleRate:(double)sampleRate error:(NSError **)error
{
	os_log_info(gSFBAudioObjectLog, "Setting device 0x%x sample rate to %.2f Hz", _objectID, sampleRate);

	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= kAudioDevicePropertyNominalSampleRate,
		.mScope		= kAudioObjectPropertyScopeGlobal,
		.mElement	= kAudioObjectPropertyElementMaster
	};

	Float64 nominalSampleRate = sampleRate;
	OSStatus result = AudioObjectSetPropertyData(_objectID, &propertyAddress, 0, NULL, sizeof(nominalSampleRate), &nominalSampleRate);
	if(kAudioHardwareNoError != result) {
		os_log_error(gSFBAudioObjectLog, "AudioObjectSetPropertyData (kAudioDevicePropertyNominalSampleRate) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
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
	OSStatus result = AudioObjectGetPropertyDataSize(_objectID, &propertyAddress, 0, NULL, &dataSize);
	if(result != kAudioHardwareNoError) {
		os_log_error(gSFBAudioObjectLog, "AudioObjectGetPropertyDataSize (kAudioDevicePropertyAvailableNominalSampleRates) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
		return nil;
	}

	AudioValueRange *availableNominalSampleRates = (AudioValueRange *)malloc(dataSize);
	if(!availableNominalSampleRates) {
		os_log_error(gSFBAudioObjectLog, "Unable to allocate memory");
		return nil;
	}

	result = AudioObjectGetPropertyData(_objectID, &propertyAddress, 0, NULL, &dataSize, availableNominalSampleRates);
	if(kAudioHardwareNoError != result) {
		os_log_error(gSFBAudioObjectLog, "AudioObjectGetPropertyData (kAudioDevicePropertyAvailableNominalSampleRates) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
		free(availableNominalSampleRates);
		return nil;
	}

	NSMutableArray *availablSampleRates = [NSMutableArray array];

	for(NSInteger i = 0; i < (NSInteger)(dataSize / sizeof(AudioValueRange)); ++i) {
		AudioValueRange nominalSampleRate = availableNominalSampleRates[i];
		if(nominalSampleRate.mMinimum == nominalSampleRate.mMaximum)
			[availablSampleRates addObject:@(nominalSampleRate.mMinimum)];
		else
			os_log_error(gSFBAudioObjectLog, "nominalSampleRate.mMinimum (%.2f Hz) and nominalSampleRate.mMaximum (%.2f Hz) don't match", nominalSampleRate.mMinimum, nominalSampleRate.mMaximum);
	}

	free(availableNominalSampleRates);

	return availablSampleRates;
}

- (NSURL *)icon
{
	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= kAudioDevicePropertyIcon,
		.mScope		= kAudioObjectPropertyScopeGlobal,
		.mElement	= kAudioObjectPropertyElementMaster
	};

	CFURLRef url = NULL;
	UInt32 dataSize = sizeof(url);
	OSStatus result = AudioObjectGetPropertyData(_objectID, &propertyAddress, 0, NULL, &dataSize, &url);
	if(result != kAudioHardwareNoError) {
		os_log_error(gSFBAudioObjectLog, "AudioObjectGetPropertyData (kAudioDevicePropertyIcon) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
		return nil;
	}

	return (__bridge_transfer NSURL *)url;
}

- (BOOL)isHidden
{
	return (BOOL)[self uInt32ForProperty:kAudioDevicePropertyIsHidden];
}

- (NSArray *)preferredStereoChannelsInScope:(AudioObjectPropertyScope)scope
{
	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= kAudioDevicePropertyPreferredChannelsForStereo,
		.mScope		= scope,
		.mElement	= kAudioObjectPropertyElementMaster
	};

	if(!AudioObjectHasProperty(_objectID, &propertyAddress)) {
		os_log_debug(gSFBAudioObjectLog, "AudioObjectHasProperty (kAudioDevicePropertyPreferredChannelsForStereo, '%{public}.4s') is false", SFBCStringForOSType(scope));
		return nil;
	}

	UInt32 preferredChannels [2];
	UInt32 dataSize = sizeof(preferredChannels);
	OSStatus result = AudioObjectGetPropertyData(_objectID, &propertyAddress, 0, NULL, &dataSize, &preferredChannels);
	if(kAudioHardwareNoError != result) {
		os_log_debug(gSFBAudioObjectLog, "AudioObjectGetPropertyData (kAudioDevicePropertyPreferredChannelsForStereo, '%{public}.4s') failed: %d", SFBCStringForOSType(scope), result);
		return nil;
	}

	return @[@(preferredChannels[0]), @(preferredChannels[1])];
}

- (AVAudioChannelLayout *)preferredChannelLayoutInScope:(AudioObjectPropertyScope)scope
{
	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= kAudioDevicePropertyPreferredChannelLayout,
		.mScope		= scope,
		.mElement	= kAudioObjectPropertyElementMaster
	};

	if(!AudioObjectHasProperty(_objectID, &propertyAddress)) {
		os_log_debug(gSFBAudioObjectLog, "AudioObjectHasProperty (kAudioDevicePropertyPreferredChannelLayout, '%{public}.4s') is false", SFBCStringForOSType(scope));
		return nil;
	}

	UInt32 dataSize = 0;
	OSStatus result = AudioObjectGetPropertyDataSize(_objectID, &propertyAddress, 0, NULL, &dataSize);
	if(kAudioHardwareNoError != result) {
		os_log_debug(gSFBAudioObjectLog, "AudioObjectGetPropertyDataSize (kAudioDevicePropertyPreferredChannelLayout, '%{public}.4s') failed: %d", SFBCStringForOSType(scope), result);
		return nil;
	}

	AudioChannelLayout *preferredChannelLayout = malloc(dataSize);
	result = AudioObjectGetPropertyData(_objectID, &propertyAddress, 0, NULL, &dataSize, preferredChannelLayout);
	if(kAudioHardwareNoError != result) {
		os_log_debug(gSFBAudioObjectLog, "AudioObjectGetPropertyData (kAudioDevicePropertyPreferredChannelLayout, '%{public}.4s') failed: %d", SFBCStringForOSType(scope), result);
		free(preferredChannelLayout);
		return nil;
	}

	AVAudioChannelLayout *channelLayout = [AVAudioChannelLayout layoutWithLayout:preferredChannelLayout];
	free(preferredChannelLayout);

	return channelLayout;
}

- (BOOL)isHoggedInScope:(AudioObjectPropertyScope)scope
{
	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= kAudioDevicePropertyHogMode,
		.mScope		= scope,
		.mElement	= kAudioObjectPropertyElementMaster
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

- (BOOL)isHogOwnerInScope:(AudioObjectPropertyScope)scope
{
	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= kAudioDevicePropertyHogMode,
		.mScope		= scope,
		.mElement	= kAudioObjectPropertyElementMaster
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

- (BOOL)startHoggingInScope:(AudioObjectPropertyScope)scope error:(NSError **)error
{
	os_log_info(gSFBAudioObjectLog, "Taking hog mode for device 0x%x '%{public}.4s'", _objectID, SFBCStringForOSType(scope));

	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= kAudioDevicePropertyHogMode,
		.mScope		= scope,
		.mElement	= kAudioObjectPropertyElementMaster
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

- (BOOL)stopHoggingInScope:(AudioObjectPropertyScope)scope error:(NSError **)error
{
	os_log_info(gSFBAudioObjectLog, "Releasing hog mode for device 0x%x '%{public}.4s'", _objectID, SFBCStringForOSType(scope));

	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= kAudioDevicePropertyHogMode,
		.mScope		= scope,
		.mElement	= kAudioObjectPropertyElementMaster
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

- (float)volumeForChannel:(AudioObjectPropertyElement)channel inScope:(AudioObjectPropertyScope)scope
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

- (BOOL)setVolume:(float)volume forChannel:(AudioObjectPropertyElement)channel inScope:(AudioObjectPropertyScope)scope error:(NSError **)error
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

- (float)volumeInDecibelsForChannel:(AudioObjectPropertyElement)channel inScope:(AudioObjectPropertyScope)scope
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

- (BOOL)setVolumeInDecibels:(float)volume forChannel:(AudioObjectPropertyElement)channel inScope:(AudioObjectPropertyScope)scope error:(NSError **)error
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

- (float)convertVolumeScalar:(float)volumeScalar toDecibelsInScope:(AudioObjectPropertyScope)scope
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

- (float)convertDecibels:(float)decibels toVolumeScalarInScope:(AudioObjectPropertyScope)scope
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

- (BOOL)isMutedInScope:(AudioObjectPropertyScope)scope
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

- (BOOL)setMute:(BOOL)mute inScope:(AudioObjectPropertyScope)scope error:(NSError **)error
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

- (NSArray *)dataSourcesInScope:(AudioObjectPropertyScope)scope
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

- (NSArray *)activeDataSourcesInScope:(AudioObjectPropertyScope)scope
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

- (BOOL)setActiveDataSources:(NSArray *)activeDataSources inScope:(AudioObjectPropertyScope)scope error:(NSError **)error
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

- (void)whenDataSourcesChangeInScope:(AudioObjectPropertyScope)scope performBlock:(dispatch_block_t)block
{
	[self whenProperty:kAudioDevicePropertyDataSources inScope:scope changesOnElement:kAudioObjectPropertyElementMaster performBlock:block];
}

- (void)whenActiveDataSourcesChangeInScope:(AudioObjectPropertyScope)scope performBlock:(dispatch_block_t)block
{
	[self whenProperty:kAudioDevicePropertyDataSource inScope:scope changesOnElement:kAudioObjectPropertyElementMaster performBlock:block];
}

- (void)whenMuteChangeInScope:(AudioObjectPropertyScope)scope performBlock:(dispatch_block_t)block
{
	[self whenProperty:kAudioDevicePropertyMute inScope:scope changesOnElement:kAudioObjectPropertyElementMaster performBlock:block];
}

- (void)whenMasterVolumeChangesInScope:(AudioObjectPropertyScope)scope performBlock:(dispatch_block_t)block
{
	[self whenProperty:kAudioDevicePropertyVolumeScalar inScope:scope changesOnElement:kAudioObjectPropertyElementMaster performBlock:block];
}

- (void)whenVolumeChangesForChannel:(AudioObjectPropertyElement)channel inScope:(AudioObjectPropertyScope)scope performBlock:(dispatch_block_t)block
{
	[self whenProperty:kAudioDevicePropertyVolumeScalar inScope:scope changesOnElement:channel performBlock:block];
}

- (NSString *)description
{
	return [NSString stringWithFormat:@"<%@ 0x%x, \"%@\">", self.className, _objectID, self.deviceUID];
}

@end
