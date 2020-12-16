/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <os/log.h>

#import "SFBAudioObject.h"

#ifdef __cplusplus
#import <memory>
#import <vector>

#import "SFBCStringForOSType.h"
#endif

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
namespace SFB {

#pragma mark - Basic Property Getters

	template <typename T>
	bool GetFixedSizeProperty(AudioObjectID objectID, const AudioObjectPropertyAddress& propertyAddress, T& value, UInt32 qualifierDataSize = 0, const void * _Nullable qualifierData = nullptr)
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
	bool GetVariableSizeProperty(AudioObjectID objectID, const AudioObjectPropertyAddress& propertyAddress, std::unique_ptr<T>& value, UInt32 qualifierDataSize = 0, const void * _Nullable qualifierData = nullptr)
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
	bool GetArrayProperty(AudioObjectID objectID, const AudioObjectPropertyAddress& propertyAddress, std::vector<T>& values, UInt32 qualifierDataSize = 0, const void * _Nullable qualifierData = nullptr)
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

#pragma mark - Numeric Properties

	template <typename T, T DefaultValue = std::numeric_limits<T>::quiet_NaN()>
	typename std::enable_if<std::is_arithmetic<T>::value, bool>::type NumericTypeForProperty(AudioObjectID objectID, AudioObjectPropertySelector property, AudioObjectPropertyScope scope = kAudioObjectPropertyScopeGlobal, AudioObjectPropertyElement element = kAudioObjectPropertyElementMaster)
	{
		AudioObjectPropertyAddress propertyAddress = { .mSelector = property, .mScope = scope, .mElement = element };
		T value;
		return GetFixedSizeProperty(objectID, propertyAddress, value) ? value : DefaultValue;
	}

#pragma mark Property Information

	bool HasProperty(AudioObjectID objectID, AudioObjectPropertySelector property, AudioObjectPropertyScope scope = kAudioObjectPropertyScopeGlobal, AudioObjectPropertyElement element = kAudioObjectPropertyElementMaster);

	bool PropertyIsSettable(AudioObjectID objectID, AudioObjectPropertySelector property, AudioObjectPropertyScope scope = kAudioObjectPropertyScopeGlobal, AudioObjectPropertyElement element = kAudioObjectPropertyElementMaster);

#pragma mark Typed Property Getters

	NSNumber * _Nullable UInt32ForProperty(AudioObjectID objectID, AudioObjectPropertySelector property, AudioObjectPropertyScope scope = kAudioObjectPropertyScopeGlobal, AudioObjectPropertyElement element = kAudioObjectPropertyElementMaster);

	NSNumber * _Nullable Float32ForProperty(AudioObjectID objectID, AudioObjectPropertySelector property, AudioObjectPropertyScope scope = kAudioObjectPropertyScopeGlobal, AudioObjectPropertyElement element = kAudioObjectPropertyElementMaster);

	NSNumber * _Nullable Float64ForProperty(AudioObjectID objectID, AudioObjectPropertySelector property, AudioObjectPropertyScope scope = kAudioObjectPropertyScopeGlobal, AudioObjectPropertyElement element = kAudioObjectPropertyElementMaster);

	NSString * _Nullable StringForProperty(AudioObjectID objectID, AudioObjectPropertySelector property, AudioObjectPropertyScope scope = kAudioObjectPropertyScopeGlobal, AudioObjectPropertyElement element = kAudioObjectPropertyElementMaster);

	NSDictionary * _Nullable DictionaryForProperty(AudioObjectID objectID, AudioObjectPropertySelector property, AudioObjectPropertyScope scope = kAudioObjectPropertyScopeGlobal, AudioObjectPropertyElement element = kAudioObjectPropertyElementMaster);

	SFBAudioObject * _Nullable AudioObjectForProperty(AudioObjectID objectID, AudioObjectPropertySelector property, AudioObjectPropertyScope scope = kAudioObjectPropertyScopeGlobal, AudioObjectPropertyElement element = kAudioObjectPropertyElementMaster);


	NSArray <NSNumber *> * _Nullable UInt32ArrayForProperty(AudioObjectID objectID, AudioObjectPropertySelector property, AudioObjectPropertyScope scope = kAudioObjectPropertyScopeGlobal, AudioObjectPropertyElement element = kAudioObjectPropertyElementMaster);

	NSArray<SFBAudioObject *> * _Nullable AudioObjectArrayForProperty(AudioObjectID objectID, AudioObjectPropertySelector property, AudioObjectPropertyScope scope = kAudioObjectPropertyScopeGlobal, AudioObjectPropertyElement element = kAudioObjectPropertyElementMaster);

#pragma mark - Audio Object Helpers

	AudioClassID AudioObjectClass(AudioObjectID objectID);
	AudioClassID AudioObjectBaseClass(AudioObjectID objectID);

#pragma mark - Audio Device Helpers

	bool AudioDeviceHasBuffersInScope(AudioObjectID deviceID, AudioObjectPropertyScope scope);

}

extern "C" {
#endif

#pragma mark - Audio Object Class Determination

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
