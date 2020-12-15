/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <os/log.h>

#import <vector>

#import "SFBAudioObject+Internal.h"

#import "SFBAggregateDevice.h"
#import "SFBAudioBox.h"
#import "SFBAudioControl.h"
#import "SFBAudioDevice.h"
#import "SFBAudioPlugIn.h"
#import "SFBAudioStream.h"
#import "SFBAudioTransportManager.h"
#import "SFBClockDevice.h"
#import "SFBEndpointDevice.h"
#import "SFBSubdevice.h"
#import "SFBCStringForOSType.h"

os_log_t gSFBAudioObjectLog = NULL;

template <>
struct ::std::default_delete<AudioBufferList> {
	default_delete() = default;
	template <class U>
	constexpr default_delete(default_delete<U>) noexcept {}
	void operator()(AudioBufferList *abl) const noexcept { std::free(abl); }
};

namespace {

	void SFBCreateAudioObjectLog(void) __attribute__ ((constructor));
	void SFBCreateAudioObjectLog()
	{
		static dispatch_once_t onceToken;
		dispatch_once(&onceToken, ^{
			gSFBAudioObjectLog = os_log_create("org.sbooth.AudioEngine", "AudioObject");
		});
	}

	template <typename T>
	bool GetFixedSizeProperty(AudioObjectID objectID, const AudioObjectPropertyAddress& propertyAddress, T& value, UInt32 qualifierDataSize = 0, const void * qualifierData = nullptr)
	{
		NSCParameterAssert(objectID != kAudioObjectUnknown);

		UInt32 dataSize = sizeof(value);
		OSStatus result = AudioObjectGetPropertyData(objectID, &propertyAddress, qualifierDataSize, qualifierData, &dataSize, &value);
		if(kAudioHardwareNoError != result) {
			os_log_error(gSFBAudioObjectLog, "AudioObjectGetPropertyData (0x%x, '%{public}.4s', '%{public}.4s', %u) failed: %d '%{public}.4s'", objectID, SFBCStringForOSType(propertyAddress.mSelector), SFBCStringForOSType(propertyAddress.mScope), propertyAddress.mElement, result, SFBCStringForOSType(result));
			return false;
		}

		return true;
	}

	template <typename T>
	bool GetDynamicSizeProperty(AudioObjectID objectID, const AudioObjectPropertyAddress& propertyAddress, std::unique_ptr<T>& value, UInt32 qualifierDataSize = 0, const void * qualifierData = nullptr)
	{
		NSCParameterAssert(objectID != kAudioObjectUnknown);

		UInt32 dataSize = 0;
		OSStatus result = AudioObjectGetPropertyDataSize(objectID, &propertyAddress, qualifierDataSize, qualifierData, &dataSize);
		if(result != kAudioHardwareNoError) {
			os_log_error(gSFBAudioObjectLog, "AudioObjectGetPropertyDataSize (0x%x, '%{public}.4s', '%{public}.4s', %u) failed: %d '%{public}.4s'", objectID, SFBCStringForOSType(propertyAddress.mSelector), SFBCStringForOSType(propertyAddress.mScope), propertyAddress.mElement, result, SFBCStringForOSType(result));
			return false;
		}

		T *rawValue = (T *)std::malloc(dataSize);
		if(!rawValue) {
			os_log_error(gSFBAudioObjectLog, "Unable to allocate memory");
			return false;
		}

		result = AudioObjectGetPropertyData(objectID, &propertyAddress, qualifierDataSize, qualifierData, &dataSize, rawValue);
		if(kAudioHardwareNoError != result) {
			os_log_error(gSFBAudioObjectLog, "AudioObjectGetPropertyData (0x%x, '%{public}.4s', '%{public}.4s', %u) failed: %d '%{public}.4s'", objectID, SFBCStringForOSType(propertyAddress.mSelector), SFBCStringForOSType(propertyAddress.mScope), propertyAddress.mElement, result, SFBCStringForOSType(result));
			free(rawValue);
			return false;
		}

		value = std::unique_ptr<T>{rawValue};

		return true;
	}

	template <typename T>
	bool GetArrayProperty(AudioObjectID objectID, const AudioObjectPropertyAddress& propertyAddress, std::vector<T>& values, UInt32 qualifierDataSize = 0, const void * qualifierData = nullptr)
	{
		NSCParameterAssert(objectID != kAudioObjectUnknown);

		UInt32 dataSize = 0;
		OSStatus result = AudioObjectGetPropertyDataSize(objectID, &propertyAddress, qualifierDataSize, qualifierData, &dataSize);
		if(result != kAudioHardwareNoError) {
			os_log_error(gSFBAudioObjectLog, "AudioObjectGetPropertyDataSize (0x%x, '%{public}.4s', '%{public}.4s', %u) failed: %d '%{public}.4s'", objectID, SFBCStringForOSType(propertyAddress.mSelector), SFBCStringForOSType(propertyAddress.mScope), propertyAddress.mElement, result, SFBCStringForOSType(result));
			return false;
		}

		values.clear();
		values.resize(dataSize / sizeof(T));

		result = AudioObjectGetPropertyData(objectID, &propertyAddress, qualifierDataSize, qualifierData, &dataSize, &values[0]);
		if(kAudioHardwareNoError != result) {
			os_log_error(gSFBAudioObjectLog, "AudioObjectGetPropertyData (0x%x, '%{public}.4s', '%{public}.4s', %u) failed: %d '%{public}.4s'", objectID, SFBCStringForOSType(propertyAddress.mSelector), SFBCStringForOSType(propertyAddress.mScope), propertyAddress.mElement, result, SFBCStringForOSType(result));
			return false;
		}

		return true;
	}

	AudioClassID AudioObjectClass(AudioObjectID objectID)
	{
		AudioObjectPropertyAddress propertyAddress = {
			.mSelector	= kAudioObjectPropertyClass,
			.mScope		= kAudioObjectPropertyScopeGlobal,
			.mElement	= kAudioObjectPropertyElementMaster
		};
		AudioClassID classID;
		return GetFixedSizeProperty(objectID, propertyAddress, classID) ? classID : 0;
	}

	AudioClassID AudioObjectBaseClass(AudioObjectID objectID)
	{
		AudioObjectPropertyAddress propertyAddress = {
			.mSelector	= kAudioObjectPropertyBaseClass,
			.mScope		= kAudioObjectPropertyScopeGlobal,
			.mElement	= kAudioObjectPropertyElementMaster
		};
		AudioClassID classID;
		return GetFixedSizeProperty(objectID, propertyAddress, classID) ? classID : 0;
	}

	bool AudioDeviceHasBuffersInScope(AudioObjectID deviceID, AudioObjectPropertyScope scope)
	{
		AudioObjectPropertyAddress propertyAddress = {
			.mSelector	= kAudioDevicePropertyStreamConfiguration,
			.mScope		= scope,
			.mElement	= kAudioObjectPropertyElementMaster
		};
		std::unique_ptr<AudioBufferList> value;
		return GetDynamicSizeProperty(deviceID, propertyAddress, value) ? value->mNumberBuffers > 0 : NO;
	}

}

extern "C" {

#pragma mark - Audio Object Class Determination

	BOOL SFBAudioObjectIsClass(AudioObjectID objectID, AudioClassID classID)
	{
		return AudioObjectClass(objectID) == classID;
	}

	BOOL SFBAudioObjectIsClassOrSubclassOf(AudioObjectID objectID, AudioClassID classID)
	{
		return AudioObjectClass(objectID) == classID || AudioObjectBaseClass(objectID) == classID;
	}

	BOOL SFBAudioObjectIsPlugIn(AudioObjectID objectID) 		{ return SFBAudioObjectIsClassOrSubclassOf(objectID, kAudioPlugInClassID); }
	BOOL SFBAudioObjectIsBox(AudioObjectID objectID) 			{ return SFBAudioObjectIsClassOrSubclassOf(objectID, kAudioBoxClassID); }
	BOOL SFBAudioObjectIsDevice(AudioObjectID objectID) 		{ return SFBAudioObjectIsClassOrSubclassOf(objectID, kAudioDeviceClassID); }
	BOOL SFBAudioObjectIsClockDevice(AudioObjectID objectID) 	{ return SFBAudioObjectIsClassOrSubclassOf(objectID, kAudioClockDeviceClassID); }
	BOOL SFBAudioObjectIsStream(AudioObjectID objectID) 		{ return SFBAudioObjectIsClassOrSubclassOf(objectID, kAudioStreamClassID); }
	BOOL SFBAudioObjectIsControl(AudioObjectID objectID) 		{ return SFBAudioObjectIsClassOrSubclassOf(objectID, kAudioControlClassID); }

#pragma mark - Audio PlugIn Information

	BOOL SFBAudioPlugInIsTransportManager(AudioObjectID objectID) 	{ return SFBAudioObjectIsClass(objectID, kAudioTransportManagerClassID); }

#pragma mark - Audio Device Information

	BOOL SFBAudioDeviceIsAggregate(AudioObjectID objectID) 			{ return SFBAudioObjectIsClass(objectID, kAudioAggregateDeviceClassID); }
	BOOL SFBAudioDeviceIsSubdevice(AudioObjectID objectID) 			{ return SFBAudioObjectIsClass(objectID, kAudioSubDeviceClassID); }
	BOOL SFBAudioDeviceIsEndpointDevice(AudioObjectID objectID) 	{ return SFBAudioObjectIsClass(objectID, kAudioEndPointDeviceClassID); }
	BOOL SFBAudioDeviceIsEndpoint(AudioObjectID objectID) 			{ return SFBAudioObjectIsClass(objectID, kAudioEndPointClassID); }

	BOOL SFBAudioDeviceSupportsInput(AudioObjectID deviceID) 		{ return AudioDeviceHasBuffersInScope(deviceID, kAudioObjectPropertyScopeInput); }
	BOOL SFBAudioDeviceSupportsOutput(AudioObjectID deviceID) 		{ return AudioDeviceHasBuffersInScope(deviceID, kAudioObjectPropertyScopeOutput); }

#pragma mark - Audio Control Information

	BOOL SFBAudioControlIsSlider(AudioObjectID objectID) 		{ return SFBAudioObjectIsClass(objectID, kAudioSliderControlClassID); }
	BOOL SFBAudioControlIsLevel(AudioObjectID objectID) 		{ return SFBAudioObjectIsClassOrSubclassOf(objectID, kAudioLevelControlClassID); }
	BOOL SFBAudioControlIsBoolean(AudioObjectID objectID) 		{ return SFBAudioObjectIsClassOrSubclassOf(objectID, kAudioBooleanControlClassID); }
	BOOL SFBAudioControlIsSelector(AudioObjectID objectID) 		{ return SFBAudioObjectIsClassOrSubclassOf(objectID, kAudioSelectorControlClassID); }
	BOOL SFBAudioControlIsStereoPan(AudioObjectID objectID) 	{ return SFBAudioObjectIsClassOrSubclassOf(objectID, kAudioStereoPanControlClassID); }

#pragma mark - Audio Level Control Information

	BOOL SFBAudioLevelControlIsVolume(AudioObjectID objectID) 		{ return SFBAudioObjectIsClass(objectID, kAudioVolumeControlClassID); }
	BOOL SFBAudioLevelControlIsLFEVolume(AudioObjectID objectID) 	{ return SFBAudioObjectIsClass(objectID, kAudioLFEVolumeControlClassID); }

#pragma mark - Audio Boolean Control Information

	BOOL SFBAudioBooleanControlIsMute(AudioObjectID objectID) 			{ return SFBAudioObjectIsClass(objectID, kAudioMuteControlClassID); }
	BOOL SFBAudioBooleanControlIsSolo(AudioObjectID objectID) 			{ return SFBAudioObjectIsClass(objectID, kAudioSoloControlClassID); }
	BOOL SFBAudioBooleanControlIsJack(AudioObjectID objectID) 			{ return SFBAudioObjectIsClass(objectID, kAudioJackControlClassID); }
	BOOL SFBAudioBooleanControlIsLFEMute(AudioObjectID objectID) 		{ return SFBAudioObjectIsClass(objectID, kAudioLFEMuteControlClassID); }
	BOOL SFBAudioBooleanControlIsPhantomPower(AudioObjectID objectID) 	{ return SFBAudioObjectIsClass(objectID, kAudioPhantomPowerControlClassID); }
	BOOL SFBAudioBooleanControlIsPhaseInvert(AudioObjectID objectID) 	{ return SFBAudioObjectIsClass(objectID, kAudioPhaseInvertControlClassID); }
	BOOL SFBAudioBooleanControlIsClipLight(AudioObjectID objectID) 		{ return SFBAudioObjectIsClass(objectID, kAudioClipLightControlClassID); }
	BOOL SFBAudioBooleanControlIsTalkback(AudioObjectID objectID) 		{ return SFBAudioObjectIsClass(objectID, kAudioTalkbackControlClassID); }
	BOOL SFBAudioBooleanControlIsListenback(AudioObjectID objectID) 	{ return SFBAudioObjectIsClass(objectID, kAudioListenbackControlClassID); }

#pragma mark - Audio Selector Control Information

	BOOL SFBAudioSelectorControlIsDataSource(AudioObjectID objectID) 		{ return SFBAudioObjectIsClass(objectID, kAudioDataSourceControlClassID); }
	BOOL SFBAudioSelectorControlIsDataDestination(AudioObjectID objectID) 	{ return SFBAudioObjectIsClass(objectID, kAudioDataDestinationControlClassID); }
	BOOL SFBAudioSelectorControlIsClockSource(AudioObjectID objectID) 		{ return SFBAudioObjectIsClass(objectID, kAudioClockSourceControlClassID); }
	BOOL SFBAudioSelectorControlIsLevel(AudioObjectID objectID) 			{ return SFBAudioObjectIsClass(objectID, kAudioLineLevelControlClassID); }
	BOOL SFBAudioSelectorControlIsHighpassFilter(AudioObjectID objectID) 	{ return SFBAudioObjectIsClass(objectID, kAudioHighPassFilterControlClassID); }

#pragma mark - Property Support

	NSNumber * SFBUInt32ForProperty(AudioObjectID objectID, const AudioObjectPropertyAddress *propertyAddress)
	{
		NSCParameterAssert(propertyAddress != nullptr);
		UInt32 value;
		return GetFixedSizeProperty(objectID, *propertyAddress, value) ? @(value) : nil;
	}

	NSArray<NSNumber *> * SFBUInt32ArrayForProperty(AudioObjectID objectID, const AudioObjectPropertyAddress *propertyAddress)
	{
		std::vector<UInt32> values;
		if(!GetArrayProperty(objectID, *propertyAddress, values))
			return nil;
		NSMutableArray *result = [NSMutableArray arrayWithCapacity:values.size()];
		for(UInt32 value : values)
			[result addObject:@(value)];
		return result;
	}

	NSNumber * SFBFloat32ForProperty(AudioObjectID objectID, const AudioObjectPropertyAddress *propertyAddress)
	{
		NSCParameterAssert(propertyAddress != nullptr);
		Float32 value;
		return GetFixedSizeProperty(objectID, *propertyAddress, value) ? @(value) : nil;
	}

	NSNumber * SFBFloat64ForProperty(AudioObjectID objectID, const AudioObjectPropertyAddress *propertyAddress)
	{
		NSCParameterAssert(propertyAddress != nullptr);
		Float64 value;
		return GetFixedSizeProperty(objectID, *propertyAddress, value) ? @(value) : nil;
	}

	NSString * SFBStringForProperty(AudioObjectID objectID, const AudioObjectPropertyAddress *propertyAddress)
	{
		NSCParameterAssert(propertyAddress != nullptr);
		CFStringRef value;
		return GetFixedSizeProperty(objectID, *propertyAddress, value) ? (__bridge_transfer NSString *)value : nil;
	}

	NSDictionary * SFBDictionaryForProperty(AudioObjectID objectID, const AudioObjectPropertyAddress *propertyAddress)
	{
		NSCParameterAssert(propertyAddress != nullptr);
		CFDictionaryRef value;
		return GetFixedSizeProperty(objectID, *propertyAddress, value) ? (__bridge_transfer NSDictionary *)value : nil;
	}

	SFBAudioObject * SFBAudioObjectForProperty(AudioObjectID objectID, const AudioObjectPropertyAddress *propertyAddress)
	{
		NSCParameterAssert(propertyAddress != nullptr);
		AudioObjectID value;
		return GetFixedSizeProperty(objectID, *propertyAddress, value) ? [[SFBAudioObject alloc] initWithAudioObjectID:value] : nil;
	}

	NSArray<SFBAudioObject *> * SFBAudioObjectArrayForProperty(AudioObjectID objectID, const AudioObjectPropertyAddress *propertyAddress)
	{
		std::vector<AudioObjectID> values;
		if(!GetArrayProperty(objectID, *propertyAddress, values))
			return nil;
		NSMutableArray *result = [NSMutableArray arrayWithCapacity:values.size()];
		for(UInt32 value : values)
			[result addObject:[[SFBAudioObject alloc] initWithAudioObjectID:value]];
		return result;
	}

}

#pragma mark -

@interface SFBAudioObject ()
{
@private
	/// An array of property listener blocks
	NSMutableDictionary *_listenerBlocks;
}
- (void)addPropertyListenerForPropertyAddress:(const AudioObjectPropertyAddress *)propertyAddress block:(dispatch_block_t)block;
- (void)removePropertyListenerForPropertyAddress:(const AudioObjectPropertyAddress *)propertyAddress;
@end

@implementation SFBAudioObject

static SFBAudioObject *sSystemObject = nil;

+ (SFBAudioObject *)systemObject
{
	static dispatch_once_t onceToken;
	dispatch_once(&onceToken, ^{
		sSystemObject = [[SFBAudioObject alloc] init];
		sSystemObject->_objectID = kAudioObjectSystemObject;
		sSystemObject->_listenerBlocks = [NSMutableDictionary dictionary];
	});
	return sSystemObject;
}

- (instancetype)initWithAudioObjectID:(AudioObjectID)objectID
{
//	NSParameterAssert(objectID != kAudioObjectUnknown);
	if(objectID == kAudioObjectUnknown)
		return nil;

	if(objectID == kAudioObjectSystemObject)
		return [SFBAudioObject systemObject];

	AudioClassID classID = AudioObjectClass(objectID);
	switch(classID) {
		case kAudioBoxClassID:
			self = [[SFBAudioBox alloc] init];
			break;
		case kAudioDeviceClassID:
			self = [[SFBAudioDevice alloc] init];
			break;
		case kAudioEndPointDeviceClassID:
			self = [[SFBEndpointDevice alloc] init];
			break;
		case kAudioAggregateDeviceClassID:
			self = [[SFBAggregateDevice alloc] init];
			break;
		case kAudioSubDeviceClassID:
			self = [[SFBSubdevice alloc] init];
			break;
		case kAudioClockDeviceClassID:
			self = [[SFBClockDevice alloc] init];
			break;
		case kAudioStreamClassID:
			self = [[SFBAudioStream alloc] init];
			break;
		case kAudioPlugInClassID:
			self = [[SFBAudioPlugIn alloc] init];
			break;
		case kAudioTransportManagerClassID:
			self = [[SFBAudioTransportManager alloc] init];
			break;
		case kAudioControlClassID:
			self = [[SFBAudioControl alloc] init];
			break;
		default:
			self = [[SFBAudioObject alloc] init];
			break;
	}

	if(self) {
		_objectID = objectID;
		_listenerBlocks = [NSMutableDictionary dictionary];
	}
	return self;
}

- (void)dealloc
{
	for(NSValue *propertyAddressAsValue in [_listenerBlocks allKeys]) {
		AudioObjectPropertyAddress propertyAddress{};
		[propertyAddressAsValue getValue:&propertyAddress];
		[self removePropertyListenerForPropertyAddress:&propertyAddress];
	}
}

- (BOOL)isEqual:(id)object
{
	if(![object isKindOfClass:[SFBAudioObject class]])
		return NO;

	SFBAudioObject *other = (SFBAudioObject *)object;
	return _objectID == other->_objectID;
}

- (NSUInteger)hash
{
	return _objectID;
}

#pragma mark - Audio Object Properties

- (BOOL)hasProperty:(SFBAudioObjectPropertySelector)property
{
	return [self hasProperty:property inScope:(SFBAudioObjectPropertyScope)kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster];
}

- (BOOL)hasProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope
{
	return [self hasProperty:property inScope:scope onElement:kAudioObjectPropertyElementMaster];
}

- (BOOL)hasProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element
{
	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= property,
		.mScope		= scope,
		.mElement	= element
	};
	return (BOOL)AudioObjectHasProperty(_objectID, &propertyAddress);
}

- (BOOL)propertyIsSettable:(SFBAudioObjectPropertySelector)property
{
	return [self propertyIsSettable:property inScope:(SFBAudioObjectPropertyScope)kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster];
}

- (BOOL)propertyIsSettable:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope
{
	return [self propertyIsSettable:property inScope:scope onElement:kAudioObjectPropertyElementMaster];
}

- (BOOL)propertyIsSettable:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element
{
	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= property,
		.mScope		= scope,
		.mElement	= element
	};

	Boolean isSettable;
	OSStatus result = AudioObjectIsPropertySettable(_objectID, &propertyAddress, &isSettable);
	if(result != kAudioHardwareNoError) {
		os_log_error(gSFBAudioObjectLog, "AudioObjectIsPropertySettable ('%{public}.4s', '%{public}.4s', %u) failed: %d '%{public}.4s'", SFBCStringForOSType(propertyAddress.mSelector), SFBCStringForOSType(propertyAddress.mScope), propertyAddress.mElement, result, SFBCStringForOSType(result));
		return NO;
	}

	return (BOOL)isSettable;
}

- (NSNumber *)uInt32ForProperty:(SFBAudioObjectPropertySelector)property
{
	return [self uInt32ForProperty:property inScope:(SFBAudioObjectPropertyScope)kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster];
}

- (NSNumber *)uInt32ForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope
{
	return [self uInt32ForProperty:property inScope:scope onElement:kAudioObjectPropertyElementMaster];
}

- (NSNumber *)uInt32ForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element
{
	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= property,
		.mScope		= scope,
		.mElement	= element
	};
	return SFBUInt32ForProperty(_objectID, &propertyAddress);
}

- (NSArray *)uInt32ArrayForProperty:(SFBAudioObjectPropertySelector)property
{
	return [self uInt32ArrayForProperty:property inScope:(SFBAudioObjectPropertyScope)kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster];
}

- (NSArray *)uInt32ArrayForProperty:(SFBAudioObjectPropertySelector)property  inScope:(SFBAudioObjectPropertyScope)scope
{
	return [self uInt32ArrayForProperty:property inScope:scope onElement:kAudioObjectPropertyElementMaster];
}

- (NSArray *)uInt32ArrayForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element
{
	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= property,
		.mScope		= scope,
		.mElement	= element
	};
	return SFBUInt32ArrayForProperty(_objectID, &propertyAddress);
}

- (NSNumber *)float32ForProperty:(SFBAudioObjectPropertySelector)property
{
	return [self float32ForProperty:property inScope:(SFBAudioObjectPropertyScope)kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster];
}

- (NSNumber *)float32ForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope
{
	return [self float32ForProperty:property inScope:scope onElement:kAudioObjectPropertyElementMaster];
}

- (NSNumber *)float32ForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element
{
	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= property,
		.mScope		= scope,
		.mElement	= element
	};
	Float32 value;
	return GetFixedSizeProperty(_objectID, propertyAddress, value) ? @(value) : nil;
}

- (NSNumber *)float64ForProperty:(SFBAudioObjectPropertySelector)property
{
	return [self float64ForProperty:property inScope:(SFBAudioObjectPropertyScope)kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster];
}

- (NSNumber *)float64ForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope
{
	return [self float64ForProperty:property inScope:scope onElement:kAudioObjectPropertyElementMaster];
}

- (NSNumber *)float64ForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element
{
	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= property,
		.mScope		= scope,
		.mElement	= element
	};
	Float64 value;
	return GetFixedSizeProperty(_objectID, propertyAddress, value) ? @(value) : nil;
}

- (NSString *)stringForProperty:(SFBAudioObjectPropertySelector)property
{
	return [self stringForProperty:property inScope:(SFBAudioObjectPropertyScope)kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster];
}

- (NSString *)stringForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope
{
	return [self stringForProperty:property inScope:scope onElement:kAudioObjectPropertyElementMaster];
}

- (NSString *)stringForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element
{
	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= property,
		.mScope		= scope,
		.mElement	= element
	};
	return SFBStringForProperty(_objectID, &propertyAddress);
}

- (NSDictionary *)dictionaryForProperty:(SFBAudioObjectPropertySelector)property
{
	return [self dictionaryForProperty:property inScope:(SFBAudioObjectPropertyScope)kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster];
}

- (NSDictionary *)dictionaryForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope
{
	return [self dictionaryForProperty:property inScope:scope onElement:kAudioObjectPropertyElementMaster];
}

- (NSDictionary *)dictionaryForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element
{
	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= property,
		.mScope		= scope,
		.mElement	= element
	};
	return SFBDictionaryForProperty(_objectID, &propertyAddress);
}

- (SFBAudioObject *)audioObjectForProperty:(SFBAudioObjectPropertySelector)property
{
	return [self audioObjectForProperty:property inScope:(SFBAudioObjectPropertyScope)kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster];
}

- (SFBAudioObject *)audioObjectForProperty:(SFBAudioObjectPropertySelector)property  inScope:(SFBAudioObjectPropertyScope)scope
{
	return [self audioObjectForProperty:property inScope:scope onElement:kAudioObjectPropertyElementMaster];
}

- (SFBAudioObject *)audioObjectForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element
{
	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= property,
		.mScope		= scope,
		.mElement	= element
	};
	return SFBAudioObjectForProperty(_objectID, &propertyAddress);
}

- (NSArray *)audioObjectsForProperty:(SFBAudioObjectPropertySelector)property
{
	return [self audioObjectsForProperty:property inScope:(SFBAudioObjectPropertyScope)kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster];
}

- (NSArray *)audioObjectsForProperty:(SFBAudioObjectPropertySelector)property  inScope:(SFBAudioObjectPropertyScope)scope
{
	return [self audioObjectsForProperty:property inScope:scope onElement:kAudioObjectPropertyElementMaster];
}

- (NSArray *)audioObjectsForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element
{
	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= property,
		.mScope		= scope,
		.mElement	= element
	};
	return SFBAudioObjectArrayForProperty(_objectID, &propertyAddress);
}

- (void)whenPropertyChanges:(SFBAudioObjectPropertySelector)property performBlock:(dispatch_block_t)block
{
	[self whenProperty:property inScope:(SFBAudioObjectPropertyScope)kAudioObjectPropertyScopeGlobal changesOnElement:kAudioObjectPropertyElementMaster performBlock:block];
}

- (void)whenProperty:(SFBAudioObjectPropertySelector)property changesinScope:(SFBAudioObjectPropertyScope)scope performBlock:(dispatch_block_t)block
{
	[self whenProperty:property inScope:scope changesOnElement:kAudioObjectPropertyElementMaster performBlock:block];
}

- (void)whenProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope changesOnElement:(SFBAudioObjectPropertyElement)element performBlock:(dispatch_block_t)block
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

#pragma mark - Private Methods

- (void)addPropertyListenerForPropertyAddress:(const AudioObjectPropertyAddress *)propertyAddress block:(dispatch_block_t)block
{
	NSParameterAssert(propertyAddress != nil);
	NSParameterAssert(block != nil);

	os_log_info(gSFBAudioObjectLog, "Adding property listener on object 0x%x for {'%{public}.4s', '%{public}.4s', %u}", _objectID, SFBCStringForOSType(propertyAddress->mSelector), SFBCStringForOSType(propertyAddress->mScope), propertyAddress->mElement);

	NSValue *propertyAddressAsValue = [NSValue value:propertyAddress withObjCType:@encode(AudioObjectPropertyAddress)];

	AudioObjectPropertyListenerBlock listenerBlock = ^(UInt32 inNumberAddresses, const AudioObjectPropertyAddress *inAddresses) {
#pragma unused(inNumberAddresses)
#pragma unused(inAddresses)
		block();
	};

	[_listenerBlocks setObject:listenerBlock forKey:propertyAddressAsValue];

	OSStatus result = AudioObjectAddPropertyListenerBlock(_objectID, propertyAddress, dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0), listenerBlock);
	if(result != kAudioHardwareNoError) {
		os_log_error(gSFBAudioObjectLog, "AudioObjectAddPropertyListenerBlock ('%{public}.4s', '%{public}.4s', %u) failed: %d '%{public}.4s'", SFBCStringForOSType(propertyAddress->mSelector), SFBCStringForOSType(propertyAddress->mScope), propertyAddress->mElement, result, SFBCStringForOSType(result));
		[_listenerBlocks removeObjectForKey:propertyAddressAsValue];
	}
}

- (void)removePropertyListenerForPropertyAddress:(const AudioObjectPropertyAddress *)propertyAddress
{
	NSParameterAssert(propertyAddress != nil);

	NSValue *propertyAddressAsValue = [NSValue value:propertyAddress withObjCType:@encode(AudioObjectPropertyAddress)];
	AudioObjectPropertyListenerBlock listenerBlock = [_listenerBlocks objectForKey:propertyAddressAsValue];
	if(listenerBlock) {
		os_log_info(gSFBAudioObjectLog, "Removing property listener on object 0x%x for {'%{public}.4s', '%{public}.4s', %u}", _objectID, SFBCStringForOSType(propertyAddress->mSelector), SFBCStringForOSType(propertyAddress->mScope), propertyAddress->mElement);

		[_listenerBlocks removeObjectForKey:propertyAddressAsValue];

		OSStatus result = AudioObjectRemovePropertyListenerBlock(_objectID, propertyAddress, dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0), listenerBlock);
		if(result != kAudioHardwareNoError)
			os_log_error(gSFBAudioObjectLog, "AudioObjectRemovePropertyListenerBlock ('%{public}.4s', '%{public}.4s', %u) failed: %d '%{public}.4s'", SFBCStringForOSType(propertyAddress->mSelector), SFBCStringForOSType(propertyAddress->mScope), propertyAddress->mElement, result, SFBCStringForOSType(result));
	}
}

- (NSString *)description
{
	return [NSString stringWithFormat:@"<%@ 0x%x>", self.className, _objectID];
}

@end

@implementation SFBAudioObject (SFBAudioObjectProperties)

- (AudioClassID)baseClassID
{
	return [[self uInt32ForProperty:(SFBAudioObjectPropertySelector)kAudioObjectPropertyBaseClass] unsignedIntValue];
}

- (AudioClassID)classID
{
	return [[self uInt32ForProperty:(SFBAudioObjectPropertySelector)kAudioObjectPropertyClass] unsignedIntValue];
}

- (NSString *)owner
{
	return [self stringForProperty:(SFBAudioObjectPropertySelector)kAudioObjectPropertyOwner];
}

- (NSString *)name
{
	return [self stringForProperty:(SFBAudioObjectPropertySelector)kAudioObjectPropertyName];
}

- (NSString *)modelName
{
	return [self stringForProperty:(SFBAudioObjectPropertySelector)kAudioObjectPropertyModelName];
}

- (NSString *)manufacturer
{
	return [self stringForProperty:(SFBAudioObjectPropertySelector)kAudioObjectPropertyManufacturer];
}

- (NSString *)nameOfElement:(SFBAudioObjectPropertyElement)element
{
	return [self stringForProperty:(SFBAudioObjectPropertySelector)kAudioObjectPropertyElementName inScope:(SFBAudioObjectPropertyScope)kAudioObjectPropertyScopeGlobal onElement:element];
}

- (NSString *)nameOfElement:(SFBAudioObjectPropertyElement)element inScope:(SFBAudioObjectPropertyScope)scope
{
	return [self stringForProperty:(SFBAudioObjectPropertySelector)kAudioObjectPropertyElementName inScope:scope onElement:element];
}

- (NSString *)categoryNameOfElement:(SFBAudioObjectPropertyElement)element
{
	return [self stringForProperty:(SFBAudioObjectPropertySelector)kAudioObjectPropertyElementCategoryName inScope:(SFBAudioObjectPropertyScope)kAudioObjectPropertyScopeGlobal onElement:element];
}

- (NSString *)categoryNameOfElement:(SFBAudioObjectPropertyElement)element inScope:(SFBAudioObjectPropertyScope)scope
{
	return [self stringForProperty:(SFBAudioObjectPropertySelector)kAudioObjectPropertyElementCategoryName inScope:scope onElement:element];
}

- (NSString *)numberNameOfElement:(SFBAudioObjectPropertyElement)element
{
	return [self stringForProperty:(SFBAudioObjectPropertySelector)kAudioObjectPropertyElementNumberName inScope:(SFBAudioObjectPropertyScope)kAudioObjectPropertyScopeGlobal onElement:element];
}

- (NSString *)numberNameOfElement:(SFBAudioObjectPropertyElement)element inScope:(SFBAudioObjectPropertyScope)scope
{
	return [self stringForProperty:(SFBAudioObjectPropertySelector)kAudioObjectPropertyElementNumberName inScope:scope onElement:element];
}

- (NSArray *)ownedObjects
{
	return [self audioObjectsForProperty:(SFBAudioObjectPropertySelector)kAudioObjectPropertyOwnedObjects];
}

- (NSString *)serialNumber
{
	return [self stringForProperty:(SFBAudioObjectPropertySelector)kAudioObjectPropertySerialNumber];
}

- (NSString *)firmwareVersion
{
	return [self stringForProperty:(SFBAudioObjectPropertySelector)kAudioObjectPropertyFirmwareVersion];
}

@end
