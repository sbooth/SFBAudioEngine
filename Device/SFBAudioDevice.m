/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import "SFBAudioDevice.h"
#import "SFBAudioObject+Internal.h"

#import "NSArray+SFBFunctional.h"
#import "SFBAggregateDevice.h"
#import "SFBAudioDeviceDataSource.h"
#import "SFBAudioDeviceClockSource.h"
#import "SFBSystemAudioObject.h"
#import "SFBEndpointDevice.h"
#import "SFBSubdevice.h"

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
	return [[SFBSystemAudioObject sharedInstance] audioObjectArrayForProperty:kAudioHardwarePropertyDevices inScope:kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster error:NULL];
}

+ (SFBAudioDevice *)defaultInputDevice
{
	return (SFBAudioDevice *)[[SFBSystemAudioObject sharedInstance] audioObjectForProperty:kAudioHardwarePropertyDefaultInputDevice inScope:kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster error:NULL];
}

+ (SFBAudioDevice *)defaultOutputDevice
{
	return (SFBAudioDevice *)[[SFBSystemAudioObject sharedInstance] audioObjectForProperty:kAudioHardwarePropertyDefaultOutputDevice inScope:kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster error:NULL];
}

+ (SFBAudioDevice *)defaultSystemOutputDevice
{
	return (SFBAudioDevice *)[[SFBSystemAudioObject sharedInstance] audioObjectForProperty:kAudioHardwarePropertyDefaultSystemOutputDevice inScope:kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster error:NULL];
}

+ (NSArray *)inputDevices
{
	return [[SFBAudioDevice devices] filteredArrayUsingBlock:^BOOL(SFBAudioDevice *obj) {
		return [obj supportsInput];
	}];
}

+ (NSArray *)outputDevices
{
	return [[SFBAudioDevice devices] filteredArrayUsingBlock:^BOOL(SFBAudioDevice *obj) {
		return [obj supportsOutput];
	}];
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
		os_log_error(gSFBAudioObjectLog, "AudioObjectGetPropertyData (kAudioHardwarePropertyTranslateUIDToDevice) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
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

- (NSArray *)relatedDevices
{
	return [self audioObjectArrayForProperty:kAudioDevicePropertyRelatedDevices inScope:kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster error:NULL];
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

- (BOOL)setPreferredStereoChannels:(NSArray<NSNumber *> *)stereoChannels inScope:(SFBAudioObjectPropertyScope)scope error:(NSError **)error
{
	NSParameterAssert(stereoChannels.count == 2);
	return [self setUnsignedIntArray:stereoChannels forProperty:kAudioDevicePropertyPreferredChannelsForStereo inScope:scope onElement:kAudioObjectPropertyElementMaster error:error];
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

- (NSNumber *)plugIn
{
	return [self unsignedIntForProperty:kAudioDevicePropertyPlugIn inScope:kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster error:NULL];
}

- (NSNumber *)isRunningSomewhere
{
	return [self unsignedIntForProperty:kAudioDevicePropertyDeviceIsRunningSomewhere inScope:kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster error:NULL];
}

- (NSNumber *)hogMode
{
	return [self unsignedIntForProperty:kAudioDevicePropertyHogMode inScope:kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster error:NULL];
}

- (BOOL)setHogMode:(pid_t)value error:(NSError **)error
{
	return [self setUnsignedInt:(UInt32)value forProperty:kAudioDevicePropertyHogMode inScope:kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster error:NULL];
}

// Hog mode helpers
- (NSNumber *)isHogged
{
	NSNumber *hogpid = self.hogMode;
	if(!hogpid)
		return nil;
	return @(hogpid.pidValue != (pid_t)-1);
}

- (NSNumber *)isHogOwner
{
	NSNumber *hogpid = self.hogMode;
	if(!hogpid)
		return nil;
	return @(hogpid.pidValue == getpid());
}

- (BOOL)startHoggingReturningError:(NSError **)error
{
	os_log_info(gSFBAudioObjectLog, "Taking hog mode for device 0x%x", _objectID);

	NSNumber *hogpid = self.hogMode;
	if(hogpid && hogpid.pidValue != (pid_t)-1)
		os_log_error(gSFBAudioObjectLog, "Device is already hogged by pid: %d", hogpid.pidValue);

	return [self setHogMode:getpid() error:error];
}

- (BOOL)stopHoggingReturningError:(NSError **)error
{
	os_log_info(gSFBAudioObjectLog, "Releasing hog mode for device 0x%x", _objectID);

	NSNumber *hogpid = self.hogMode;
	if(hogpid && hogpid.pidValue != getpid())
		os_log_error(gSFBAudioObjectLog, "Device is hogged by pid: %d", hogpid.pidValue);

	return [self setHogMode:(pid_t)-1 error:error];
}

- (NSNumber *)bufferFrameSize
{
	return [self unsignedIntForProperty:kAudioDevicePropertyBufferFrameSize inScope:kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster error:NULL];
}

- (BOOL)setBufferFrameSize:(UInt32)value error:(NSError **)error
{
	return [self setUnsignedInt:value forProperty:value inScope:kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster error:error];
}

- (NSValue *)bufferFrameSizeRange
{
	return [self audioValueRangeForProperty:kAudioDevicePropertyBufferFrameSizeRange inScope:kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster error:NULL];
}

- (NSNumber *)usesVariableBufferFrameSizes
{
	return [self unsignedIntForProperty:kAudioDevicePropertyUsesVariableBufferFrameSizes inScope:kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster error:NULL];
}

- (NSNumber *)ioCycleUsage
{
	return [self floatForProperty:kAudioDevicePropertyIOCycleUsage inScope:kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster error:NULL];
}

- (SFBAudioBufferListWrapper *)streamConfigurationInScope:(SFBAudioObjectPropertyScope)scope
{
	return [self audioBufferListForProperty:kAudioDevicePropertyStreamConfiguration inScope:scope onElement:kAudioObjectPropertyElementMaster error:NULL];
}

- (SFBAudioHardwareIOProcStreamUsageWrapper *)ioProcStreamUsageInScope:(SFBAudioObjectPropertyScope)scope
{
	return [self audioHardwareIOProcStreamUsageForProperty:kAudioDevicePropertyIOProcStreamUsage inScope:scope onElement:kAudioObjectPropertyElementMaster error:NULL];
}

- (BOOL)setIOProcStreamUsage:(SFBAudioHardwareIOProcStreamUsageWrapper *)value inScope:(SFBAudioObjectPropertyScope)scope error:(NSError **)error
{
	return [self setAudioHardwareIOProcStreamUsage:value forProperty:kAudioDevicePropertyIOProcStreamUsage inScope:kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster error:error];
}

- (NSNumber *)actualSampleRate
{
	return [self doubleForProperty:kAudioDevicePropertyActualSampleRate inScope:kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster error:NULL];
}

- (NSString *)clockDevice
{
	return [self stringForProperty:kAudioDevicePropertyClockDevice inScope:kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster error:NULL];
}

- (os_workgroup_t)ioThreadOSWorkgroup API_AVAILABLE(macos(11.0))
{
	return [self osWorkgroupForProperty:kAudioDevicePropertyIOThreadOSWorkgroup inScope:kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster error:NULL];
}

#pragma mark - Device Properties Implemented by Audio Controls

- (NSNumber *)jackIsConnectedToElement:(SFBAudioObjectPropertyElement)element inScope:(SFBAudioObjectPropertyScope)scope
{
	return [self unsignedIntForProperty:kAudioDevicePropertyJackIsConnected inScope:scope onElement:element error:NULL];
}

- (NSNumber *)volumeScalarForChannel:(SFBAudioObjectPropertyElement)channel inScope:(SFBAudioObjectPropertyScope)scope
{
	return [self floatForProperty:kAudioDevicePropertyVolumeScalar inScope:scope onElement:channel error:NULL];
}

- (BOOL)setVolumeScalar:(float)scalar forChannel:(SFBAudioObjectPropertyElement)channel inScope:(SFBAudioObjectPropertyScope)scope error:(NSError **)error
{
	os_log_info(gSFBAudioObjectLog, "Setting device 0x%x '%{public}.4s' channel %u volume scalar to %f", _objectID, SFBCStringForOSType(scope), channel, scalar);
	return [self setFloat:scalar forProperty:kAudioDevicePropertyVolumeScalar inScope:scope onElement:channel error:error];
}

- (NSNumber *)volumeDecibelsForChannel:(SFBAudioObjectPropertyElement)channel inScope:(SFBAudioObjectPropertyScope)scope
{
	return [self floatForProperty:kAudioDevicePropertyVolumeDecibels inScope:scope onElement:channel error:NULL];
}

- (BOOL)setVolumeDecibels:(float)decibels forChannel:(SFBAudioObjectPropertyElement)channel inScope:(SFBAudioObjectPropertyScope)scope error:(NSError **)error
{
	os_log_info(gSFBAudioObjectLog, "Setting device 0x%x '%{public}.4s' channel %u volume dB to %f", _objectID, SFBCStringForOSType(scope), channel, decibels);
	return [self setFloat:decibels forProperty:kAudioDevicePropertyVolumeDecibels inScope:scope onElement:channel error:error];
}

- (NSValue *)volumeRangeDecibelsForChannel:(SFBAudioObjectPropertyElement)channel inScope:(SFBAudioObjectPropertyScope)scope
{
	return [self audioValueRangeForProperty:kAudioDevicePropertyVolumeRangeDecibels inScope:scope onElement:channel error:NULL];
}

- (NSNumber *)convertVolumeToDecibelsFromScalar:(float)scalar inScope:(SFBAudioObjectPropertyScope)scope error:(NSError **)error
{
	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= kAudioDevicePropertyVolumeScalarToDecibels,
		.mScope		= scope,
		.mElement	= kAudioObjectPropertyElementMaster
	};

	Float32 value = scalar;
	UInt32 dataSize = sizeof(value);
	OSStatus result = AudioObjectGetPropertyData(_objectID, &propertyAddress, 0, NULL, &dataSize, &value);
	if(result != kAudioHardwareNoError) {
		os_log_error(gSFBAudioObjectLog, "AudioObjectGetPropertyData (kAudioDevicePropertyVolumeScalarToDecibels) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
		if(error)
			*error = [NSError errorWithDomain:NSOSStatusErrorDomain code:result userInfo:nil];
		return nil;
	}

	return @(value);
}

- (NSNumber *)convertVolumeToScalarFromDecibels:(float)decibels inScope:(SFBAudioObjectPropertyScope)scope error:(NSError **)error
{
	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= kAudioDevicePropertyVolumeDecibelsToScalar,
		.mScope		= scope,
		.mElement	= kAudioObjectPropertyElementMaster
	};

	Float32 value = decibels;
	UInt32 dataSize = sizeof(value);
	OSStatus result = AudioObjectGetPropertyData(_objectID, &propertyAddress, 0, NULL, &dataSize, &value);
	if(result != kAudioHardwareNoError) {
		os_log_error(gSFBAudioObjectLog, "AudioObjectGetPropertyData (kAudioDevicePropertyVolumeDecibelsToScalar) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
		if(error)
			*error = [NSError errorWithDomain:NSOSStatusErrorDomain code:result userInfo:nil];
		return nil;
	}

	return @(value);
}

- (NSNumber *)stereoPanInScope:(SFBAudioObjectPropertyScope)scope
{
	return [self floatForProperty:kAudioDevicePropertyStereoPan inScope:scope onElement:kAudioObjectPropertyElementMaster error:NULL];
}

- (NSArray *)stereoPanChannelsInScope:(SFBAudioObjectPropertyScope)scope
{
	return [self unsignedIntArrayForProperty:kAudioDevicePropertyStereoPanChannels inScope:scope onElement:kAudioObjectPropertyElementMaster error:NULL];
}

- (BOOL)setStereoPanChannels:(NSArray *)panChannels inScope:(SFBAudioObjectPropertyScope)scope error:(NSError **)error
{
	NSParameterAssert(panChannels.count == 2);
	return [self setUnsignedIntArray:panChannels forProperty:kAudioDevicePropertyStereoPanChannels inScope:scope onElement:kAudioObjectPropertyElementMaster error:error];
}

- (NSNumber *)muteInScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element
{
	return [self unsignedIntForProperty:kAudioDevicePropertyMute inScope:scope onElement:element error:NULL];
}

- (BOOL)setMute:(BOOL)value inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element error:(NSError **)error
{
	return [self setUnsignedInt:(value != 0) forProperty:kAudioDevicePropertyMute inScope:scope onElement:element error:error];
}

- (NSNumber *)soloInScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element
{
	return [self unsignedIntForProperty:kAudioDevicePropertySolo inScope:scope onElement:element error:NULL];
}

- (BOOL)setSolo:(BOOL)value inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element error:(NSError **)error
{
	return [self setUnsignedInt:(value != 0) forProperty:kAudioDevicePropertySolo inScope:scope onElement:element error:error];
}

- (NSNumber *)phantomPowerInScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element
{
	return [self unsignedIntForProperty:kAudioDevicePropertyPhantomPower inScope:scope onElement:element error:NULL];
}

- (BOOL)setPhantomPower:(BOOL)value inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element error:(NSError **)error
{
	return [self setUnsignedInt:(value != 0) forProperty:kAudioDevicePropertyPhantomPower inScope:scope onElement:element error:error];
}

- (NSNumber *)phaseInvertInScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element
{
	return [self unsignedIntForProperty:kAudioDevicePropertyPhaseInvert inScope:scope onElement:element error:NULL];
}

- (BOOL)setPhaseInvert:(BOOL)value inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element error:(NSError **)error
{
	return [self setUnsignedInt:(value != 0) forProperty:kAudioDevicePropertyPhaseInvert inScope:scope onElement:element error:error];
}

- (NSNumber *)clipLightInScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element
{
	return [self unsignedIntForProperty:kAudioDevicePropertyClipLight inScope:scope onElement:element error:NULL];
}

- (BOOL)setClipLight:(BOOL)value inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element error:(NSError **)error
{
	return [self setUnsignedInt:(value != 0) forProperty:kAudioDevicePropertyClipLight inScope:scope onElement:element error:error];
}

- (NSNumber *)talkbackInScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element
{
	return [self unsignedIntForProperty:kAudioDevicePropertyTalkback inScope:scope onElement:element error:NULL];
}

- (BOOL)setTalkback:(BOOL)value inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element error:(NSError **)error
{
	return [self setUnsignedInt:(value != 0) forProperty:kAudioDevicePropertyTalkback inScope:scope onElement:element error:error];
}

- (NSNumber *)listenbackInScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element
{
	return [self unsignedIntForProperty:kAudioDevicePropertyListenback inScope:scope onElement:element error:NULL];
}

- (BOOL)setListenback:(BOOL)value inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element error:(NSError **)error
{
	return [self setUnsignedInt:(value != 0) forProperty:kAudioDevicePropertyListenback inScope:scope onElement:element error:error];
}

- (NSArray *)dataSourceInScope:(SFBAudioObjectPropertyScope)scope
{
	return [self unsignedIntArrayForProperty:kAudioDevicePropertyDataSource inScope:scope onElement:kAudioObjectPropertyElementMaster error:NULL];
}

- (BOOL)setDataSource:(NSArray<NSNumber *> *)value inScope:(SFBAudioObjectPropertyScope)scope error:(NSError **)error
{
	NSParameterAssert(value != nil);
	return [self setUnsignedIntArray:value forProperty:kAudioDevicePropertyDataSource inScope:scope onElement:kAudioObjectPropertyElementMaster error:error];
}

- (NSArray *)dataSourcesInScope:(SFBAudioObjectPropertyScope)scope
{
	return [self unsignedIntArrayForProperty:kAudioDevicePropertyDataSources inScope:scope onElement:kAudioObjectPropertyElementMaster error:NULL];
}

- (NSString *)nameOfDataSource:(UInt32)dataSource inScope:(SFBAudioObjectPropertyScope)scope
{
	return [self translateToStringFromUnsignedInteger:dataSource usingProperty:kAudioDevicePropertyDataSourceNameForIDCFString inScope:scope onElement:kAudioObjectPropertyElementMaster error:NULL];
}

- (NSNumber *)kindOfDataSource:(UInt32)dataSource inScope:(SFBAudioObjectPropertyScope)scope
{
	return [self translateToUnsignedIntegerFromUnsignedInteger:dataSource usingProperty:kAudioDevicePropertyDataSourceKindForID inScope:scope onElement:kAudioObjectPropertyElementMaster error:NULL];
}

- (NSArray *)availableDataSourcesInScope:(SFBAudioObjectPropertyScope)scope
{
	return [[self dataSourcesInScope:scope] mappedArrayUsingBlock:^(NSNumber *obj) {
		return [[SFBAudioDeviceDataSource alloc] initWithAudioDevice:self scope:scope dataSourceID:obj.unsignedIntValue];
	}];
}

- (NSArray *)activeDataSourcesInScope:(SFBAudioObjectPropertyScope)scope
{
	return [[self dataSourceInScope:scope] mappedArrayUsingBlock:^(NSNumber *obj) {
		return [[SFBAudioDeviceDataSource alloc] initWithAudioDevice:self scope:scope dataSourceID:obj.unsignedIntValue];
	}];
}

- (BOOL)setActiveDataSources:(NSArray<SFBAudioDeviceDataSource *> *)value inScope:(SFBAudioObjectPropertyScope)scope error:(NSError **)error
{
	NSParameterAssert(value != nil);
	NSArray *dataSourceIDs = [value mappedArrayUsingBlock:^id(SFBAudioDeviceDataSource *obj) {
		NSAssert(obj.scope == scope, @"Mismatched scopes");
		return @(obj.dataSourceID);
	}];
	return [self setDataSource:dataSourceIDs inScope:scope error:error];
}

- (NSArray *)clockSourceInScope:(SFBAudioObjectPropertyScope)scope
{
	return [self unsignedIntArrayForProperty:kAudioDevicePropertyClockSource inScope:scope onElement:kAudioObjectPropertyElementMaster error:NULL];
}

- (BOOL)setClockSource:(NSArray<NSNumber *> *)value inScope:(SFBAudioObjectPropertyScope)scope error:(NSError **)error
{
	NSParameterAssert(value != nil);
	return [self setUnsignedIntArray:value forProperty:kAudioDevicePropertyClockSource inScope:scope onElement:kAudioObjectPropertyElementMaster error:error];
}

- (NSArray *)clockSourcesInScope:(SFBAudioObjectPropertyScope)scope
{
	return [self unsignedIntArrayForProperty:kAudioDevicePropertyClockSources inScope:scope onElement:kAudioObjectPropertyElementMaster error:NULL];
}

- (NSString *)nameOfClockSource:(UInt32)clockSource inScope:(SFBAudioObjectPropertyScope)scope
{
	return [self translateToStringFromUnsignedInteger:clockSource usingProperty:kAudioDevicePropertyClockSourceNameForIDCFString inScope:scope onElement:kAudioObjectPropertyElementMaster error:NULL];
}

- (NSNumber *)kindOfClockSource:(UInt32)clockSource inScope:(SFBAudioObjectPropertyScope)scope
{
	return [self translateToUnsignedIntegerFromUnsignedInteger:clockSource usingProperty:kAudioDevicePropertyClockSourceKindForID inScope:scope onElement:kAudioObjectPropertyElementMaster error:NULL];
}

- (NSArray *)availableClockSourcesInScope:(SFBAudioObjectPropertyScope)scope
{
	return [[self clockSourcesInScope:scope] mappedArrayUsingBlock:^(NSNumber *obj) {
		return [[SFBAudioDeviceClockSource alloc] initWithAudioDevice:self scope:scope clockSourceID:obj.unsignedIntValue];
	}];
}

- (NSArray *)activeClockSourcesInScope:(SFBAudioObjectPropertyScope)scope
{
	return [[self clockSourceInScope:scope] mappedArrayUsingBlock:^(NSNumber *obj) {
		return [[SFBAudioDeviceClockSource alloc] initWithAudioDevice:self scope:scope clockSourceID:obj.unsignedIntValue];
	}];
}

- (BOOL)setActiveClockSources:(NSArray<SFBAudioDeviceClockSource *> *)value inScope:(SFBAudioObjectPropertyScope)scope error:(NSError **)error
{
	NSParameterAssert(value != nil);
	NSArray *clockSourceIDs = [value mappedArrayUsingBlock:^id(SFBAudioDeviceClockSource *obj) {
		NSAssert(obj.scope == scope, @"Mismatched scopes");
		return @(obj.clockSourceID);
	}];
	return [self setClockSource:clockSourceIDs inScope:scope error:error];
}

- (NSString *)description
{
	return [NSString stringWithFormat:@"<%@ 0x%x, \"%@\">", self.className, _objectID, self.deviceUID];
}

@end

@implementation SFBAudioEndpoint

- (instancetype)initWithAudioObjectID:(AudioObjectID)objectID
{
	NSParameterAssert(SFBAudioObjectIsClassOrSubclassOf(objectID, kAudioEndPointClassID));
	return [super initWithAudioObjectID:objectID];
}

@end
