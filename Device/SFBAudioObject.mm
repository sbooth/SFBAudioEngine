/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <map>
#import <memory>
#import <vector>

#import "SFBAudioObject+Internal.h"

#import "SFBAggregateDevice.h"
#import "SFBAudioBox.h"
#import "SFBAudioControl.h"
#import "SFBAudioDevice.h"
#import "SFBAudioPlugIn.h"
#import "SFBAudioStream.h"
#import "SFBAudioTransportManager.h"
#import "SFBBooleanControl.h"
#import "SFBClockDevice.h"
#import "SFBEndpointDevice.h"
#import "SFBLevelControl.h"
#import "SFBSelectorControl.h"
#import "SFBSliderControl.h"
#import "SFBStereoPanControl.h"
#import "SFBSubdevice.h"

#import "SFBCStringForOSType.h"

os_log_t gSFBAudioObjectLog = NULL;

template <>
struct ::std::default_delete<AudioChannelLayout> {
	default_delete() = default;
	template <class U>
	constexpr default_delete(default_delete<U>) noexcept {}
	void operator()(AudioChannelLayout *acl) const noexcept { std::free(acl); }
};

template <>
struct ::std::default_delete<AudioBufferList> {
	default_delete() = default;
	template <class U>
	constexpr default_delete(default_delete<U>) noexcept {}
	void operator()(AudioBufferList *abl) const noexcept
	{
		for(UInt32 bufferIndex = 0; bufferIndex < abl->mNumberBuffers; ++bufferIndex) {
			if(abl->mBuffers[bufferIndex].mData)
				std::free(abl->mBuffers[bufferIndex].mData);
		}
		std::free(abl);
	}
};

template <>
struct ::std::default_delete<AudioHardwareIOProcStreamUsage> {
	default_delete() = default;
	template <class U>
	constexpr default_delete(default_delete<U>) noexcept {}
	void operator()(AudioHardwareIOProcStreamUsage *streamUsage) const noexcept { std::free(streamUsage); }
};

bool operator<(const AudioObjectPropertyAddress& lhs, const AudioObjectPropertyAddress& rhs);
bool operator<(const AudioObjectPropertyAddress& lhs, const AudioObjectPropertyAddress& rhs)
{
	return lhs.mSelector < rhs.mSelector && lhs.mScope < rhs.mScope && lhs.mElement < rhs.mElement;
}

namespace {

	template<bool B, class T = void>
	using enable_if_t = typename std::enable_if<B,T>::type;

	void SFBCreateAudioObjectLog(void) __attribute__ ((constructor));
	void SFBCreateAudioObjectLog()
	{
		static dispatch_once_t onceToken;
		dispatch_once(&onceToken, ^{
			gSFBAudioObjectLog = os_log_create("org.sbooth.AudioEngine", "AudioObject");
		});
	}

	struct SFBAudioObjectPropertyQualifier {
		SFBAudioObjectPropertyQualifier() 								: mData(nullptr), mSize(0) {}
		SFBAudioObjectPropertyQualifier(const void *data, UInt32 size) 	: mData(data), mSize(size) {}

		const void 		*mData;
		const UInt32 	mSize;
	};

#pragma mark AudioBufferList Helpers

	/// Returns the size in bytes of an \c AudioBufferList with the specified number of buffers
	size_t GetBufferListSize(UInt32 numberBuffers)
	{
		return offsetof(AudioBufferList, mBuffers) + (numberBuffers * sizeof(AudioBuffer));
	}

	/// Allocates and returns an \c AudioBufferList with the specified number of buffers
	/// @param dataByteSize The capacity, in bytes, of each buffer
	/// @param numberChannels The number of interleaved channels in each buffer
	std::unique_ptr<AudioBufferList> CreateBufferList(UInt32 numberBuffers, UInt32 dataByteSize = 0, UInt32 numberChannels = 1)
	{
		size_t bufferListSize = GetBufferListSize(numberBuffers);
		AudioBufferList *bufferList = (AudioBufferList *)std::malloc(bufferListSize);
		if(!bufferList)
			return nullptr;

		memset(bufferList, 0, bufferListSize);
		bufferList->mNumberBuffers = numberBuffers;

		if(dataByteSize > 0) {
			for(UInt32 i = 0; i < bufferList->mNumberBuffers; ++i) {
				bufferList->mBuffers[i].mData = std::malloc(dataByteSize);
				if(!bufferList->mBuffers[i].mData) {
					for(UInt32 j = 0; j < i; ++j) {
						std::free(bufferList->mBuffers[j].mData);
					}
					std::free(bufferList);
					return nullptr;
				}
				bufferList->mBuffers[i].mDataByteSize = dataByteSize;
				bufferList->mBuffers[i].mNumberChannels = numberChannels;
			}
		}

		return std::unique_ptr<AudioBufferList>{bufferList};
	}

#pragma mark AudioChannelLayout Helpers

	/// Returns the size in bytes of an \c AudioChannelLayout with the specified number of channel descriptions
	size_t GetChannelLayoutSize(UInt32 numberChannelDescriptions)
	{
		return offsetof(AudioChannelLayout, mChannelDescriptions) + (numberChannelDescriptions * sizeof(AudioChannelDescription));
	}

	 /// Allocates and returns an \c AudioChannelLayout with the specified number of channel descriptions
	std::unique_ptr<AudioChannelLayout> CreateChannelLayout(UInt32 numberChannelDescriptions)
	{
		size_t layoutSize = GetChannelLayoutSize(numberChannelDescriptions);
		AudioChannelLayout *channelLayout = (AudioChannelLayout *)std::malloc(layoutSize);
		if(!channelLayout)
			return nullptr;

		memset(channelLayout, 0, layoutSize);
		channelLayout->mNumberChannelDescriptions = numberChannelDescriptions;

		return std::unique_ptr<AudioChannelLayout>{channelLayout};
	}

	/// Create and returns a deep copy of \c rhs
	std::unique_ptr<AudioChannelLayout> CopyChannelLayout(const AudioChannelLayout& rhs)
	{
		size_t layoutSize = GetChannelLayoutSize(rhs.mNumberChannelDescriptions);
		AudioChannelLayout *channelLayout = (AudioChannelLayout *)std::malloc(layoutSize);
		if(!channelLayout)
			return nullptr;

		memcpy(channelLayout, &rhs, layoutSize);

		return std::unique_ptr<AudioChannelLayout>{channelLayout};
	}

#pragma mark AudioHardwareIOProcStreamUsage Helpers

	/// Returns the size in bytes of an \c AudioHardwareIOProcStreamUsage with the specified number of streams
	size_t GetHardwareIOProcStreamUsageSize(UInt32 numberStreams)
	{
		return offsetof(AudioHardwareIOProcStreamUsage, mNumberStreams) + (numberStreams * sizeof(UInt32));
	}

	/// Allocates and returns an \c AudioHardwareIOProcStreamUsage with the specified number of streams
	std::unique_ptr<AudioHardwareIOProcStreamUsage> CreateHardwareIOProcStreamUsage(UInt32 numberStreams)
	{
		size_t streamUsageSize = GetHardwareIOProcStreamUsageSize(numberStreams);
		AudioHardwareIOProcStreamUsage *streamUsage = (AudioHardwareIOProcStreamUsage *)std::malloc(streamUsageSize);
		if(!streamUsage)
			return nullptr;

		memset(streamUsage, 0, streamUsageSize);
		streamUsage->mNumberStreams = numberStreams;

		return std::unique_ptr<AudioHardwareIOProcStreamUsage>{streamUsage};
	}

#pragma mark - Basic Property Getters

	template <typename T>
	bool GetFixedSizeProperty(AudioObjectID objectID, const AudioObjectPropertyAddress& propertyAddress, T& value, const SFBAudioObjectPropertyQualifier& qualifier = SFBAudioObjectPropertyQualifier(), NSError **error = nullptr)
	{
		NSCParameterAssert(objectID != kAudioObjectUnknown);

		UInt32 dataSize = sizeof(value);
		OSStatus result = AudioObjectGetPropertyData(objectID, &propertyAddress, qualifier.mSize, qualifier.mData, &dataSize, &value);
		if(kAudioHardwareNoError != result) {
			os_log_error(gSFBAudioObjectLog, "AudioObjectGetPropertyData (0x%x, '%{public}.4s', '%{public}.4s', %u) failed: %d '%{public}.4s'", objectID, SFBCStringForOSType(propertyAddress.mSelector), SFBCStringForOSType(propertyAddress.mScope), propertyAddress.mElement, result, SFBCStringForOSType(result));
			if(error)
				*error = [NSError errorWithDomain:NSOSStatusErrorDomain code:result userInfo:nil];
			return false;
		}

		return true;
	}

	template <typename T>
	bool GetVariableSizeProperty(AudioObjectID objectID, const AudioObjectPropertyAddress& propertyAddress, std::unique_ptr<T>& value, const SFBAudioObjectPropertyQualifier& qualifier = SFBAudioObjectPropertyQualifier(), NSError **error = nullptr)
	{
		NSCParameterAssert(objectID != kAudioObjectUnknown);

		UInt32 dataSize = 0;
		OSStatus result = AudioObjectGetPropertyDataSize(objectID, &propertyAddress, qualifier.mSize, qualifier.mData, &dataSize);
		if(result != kAudioHardwareNoError) {
			os_log_error(gSFBAudioObjectLog, "AudioObjectGetPropertyDataSize (0x%x, '%{public}.4s', '%{public}.4s', %u) failed: %d '%{public}.4s'", objectID, SFBCStringForOSType(propertyAddress.mSelector), SFBCStringForOSType(propertyAddress.mScope), propertyAddress.mElement, result, SFBCStringForOSType(result));
			if(error)
				*error = [NSError errorWithDomain:NSOSStatusErrorDomain code:result userInfo:nil];
			return false;
		}

		T *rawValue = (T *)std::malloc(dataSize);
		if(!rawValue) {
			os_log_error(gSFBAudioObjectLog, "Unable to allocate memory");
			if(error)
				*error = [NSError errorWithDomain:NSPOSIXErrorDomain code:ENOMEM userInfo:nil];
			return false;
		}

		result = AudioObjectGetPropertyData(objectID, &propertyAddress, qualifier.mSize, qualifier.mData, &dataSize, rawValue);
		if(kAudioHardwareNoError != result) {
			os_log_error(gSFBAudioObjectLog, "AudioObjectGetPropertyData (0x%x, '%{public}.4s', '%{public}.4s', %u) failed: %d '%{public}.4s'", objectID, SFBCStringForOSType(propertyAddress.mSelector), SFBCStringForOSType(propertyAddress.mScope), propertyAddress.mElement, result, SFBCStringForOSType(result));
			if(error)
				*error = [NSError errorWithDomain:NSOSStatusErrorDomain code:result userInfo:nil];
			free(rawValue);
			return false;
		}

		value = std::unique_ptr<T>{rawValue};

		return true;
	}

	template <typename T>
	bool GetArrayProperty(AudioObjectID objectID, const AudioObjectPropertyAddress& propertyAddress, std::vector<T>& values, const SFBAudioObjectPropertyQualifier& qualifier = SFBAudioObjectPropertyQualifier(), NSError **error = nullptr)
	{
		NSCParameterAssert(objectID != kAudioObjectUnknown);

		UInt32 dataSize = 0;
		OSStatus result = AudioObjectGetPropertyDataSize(objectID, &propertyAddress, qualifier.mSize, qualifier.mData, &dataSize);
		if(result != kAudioHardwareNoError) {
			os_log_error(gSFBAudioObjectLog, "AudioObjectGetPropertyDataSize (0x%x, '%{public}.4s', '%{public}.4s', %u) failed: %d '%{public}.4s'", objectID, SFBCStringForOSType(propertyAddress.mSelector), SFBCStringForOSType(propertyAddress.mScope), propertyAddress.mElement, result, SFBCStringForOSType(result));
			if(error)
				*error = [NSError errorWithDomain:NSOSStatusErrorDomain code:result userInfo:nil];
			return false;
		}

		values.clear();
		values.resize(dataSize / sizeof(T));

		result = AudioObjectGetPropertyData(objectID, &propertyAddress, qualifier.mSize, qualifier.mData, &dataSize, &values[0]);
		if(kAudioHardwareNoError != result) {
			os_log_error(gSFBAudioObjectLog, "AudioObjectGetPropertyData (0x%x, '%{public}.4s', '%{public}.4s', %u) failed: %d '%{public}.4s'", objectID, SFBCStringForOSType(propertyAddress.mSelector), SFBCStringForOSType(propertyAddress.mScope), propertyAddress.mElement, result, SFBCStringForOSType(result));
			if(error)
				*error = [NSError errorWithDomain:NSOSStatusErrorDomain code:result userInfo:nil];
			return false;
		}

		return true;
	}

#pragma mark - Basic Property Setters

	bool SetProperty(AudioObjectID objectID, const AudioObjectPropertyAddress& propertyAddress, const void * _Nonnull value, size_t size, const SFBAudioObjectPropertyQualifier& qualifier = SFBAudioObjectPropertyQualifier(), NSError **error = nullptr)
	{
		NSCParameterAssert(objectID != kAudioObjectUnknown);

		OSStatus result = AudioObjectSetPropertyData(objectID, &propertyAddress, qualifier.mSize, qualifier.mData, (UInt32)size, &value);
		if(kAudioHardwareNoError != result) {
			os_log_error(gSFBAudioObjectLog, "AudioObjectSetPropertyData (0x%x, '%{public}.4s', '%{public}.4s', %u) failed: %d '%{public}.4s'", objectID, SFBCStringForOSType(propertyAddress.mSelector), SFBCStringForOSType(propertyAddress.mScope), propertyAddress.mElement, result, SFBCStringForOSType(result));
			if(error)
				*error = [NSError errorWithDomain:NSOSStatusErrorDomain code:result userInfo:nil];
			return false;
		}

		return true;
	}

#pragma mark - Property Information

	bool HasProperty(AudioObjectID objectID, AudioObjectPropertySelector property, AudioObjectPropertyScope scope = kAudioObjectPropertyScopeGlobal, AudioObjectPropertyElement element = kAudioObjectPropertyElementMaster)
	{
		AudioObjectPropertyAddress propertyAddress = { .mSelector = property, .mScope = scope, .mElement = element };
		return AudioObjectHasProperty(objectID, &propertyAddress);
	}

	NSNumber * _Nullable PropertyIsSettable(AudioObjectID objectID, AudioObjectPropertySelector property, AudioObjectPropertyScope scope = kAudioObjectPropertyScopeGlobal, AudioObjectPropertyElement element = kAudioObjectPropertyElementMaster, NSError **error = nullptr)
	{
		AudioObjectPropertyAddress propertyAddress = { .mSelector = property, .mScope = scope, .mElement = element };
		Boolean isSettable;
		OSStatus result = AudioObjectIsPropertySettable(objectID, &propertyAddress, &isSettable);
		if(result != kAudioHardwareNoError) {
			os_log_error(gSFBAudioObjectLog, "AudioObjectIsPropertySettable ('%{public}.4s', '%{public}.4s', %u) failed: %d '%{public}.4s'", SFBCStringForOSType(propertyAddress.mSelector), SFBCStringForOSType(propertyAddress.mScope), propertyAddress.mElement, result, SFBCStringForOSType(result));
			if(error)
				*error = [NSError errorWithDomain:NSOSStatusErrorDomain code:result userInfo:nil];
			return nil;
		}
		return isSettable ? @YES : @NO;
	}

#pragma mark - Typed Scalar Property Getters

	template <typename T, typename R>
	R * _Nullable ObjectForFixedSizeProperty(AudioObjectID objectID, const AudioObjectPropertyAddress& propertyAddress, const SFBAudioObjectPropertyQualifier& qualifier, NSError **error, R * _Nullable (^transform)(const T& value))
	{
		T value;
		return GetFixedSizeProperty(objectID, propertyAddress, value, qualifier, error) ? transform(value) : nil;
	}

	template <typename T>
	T * _Nullable ObjectForCFTypeProperty(AudioObjectID objectID, const AudioObjectPropertyAddress& propertyAddress, const SFBAudioObjectPropertyQualifier& qualifier = SFBAudioObjectPropertyQualifier(), NSError **error = nullptr)
	{
		T * (^transform)(const CFTypeRef&) = ^(const CFTypeRef& value){
			return (__bridge_transfer T *)value;
		};
		return ObjectForFixedSizeProperty(objectID, propertyAddress, qualifier, error, transform);
	}

	template <typename T, typename std::enable_if_t<std::is_arithmetic<T>::value, bool> = true>
	NSNumber * _Nullable NumberForArithmeticProperty(AudioObjectID objectID, AudioObjectPropertySelector property, AudioObjectPropertyScope scope = kAudioObjectPropertyScopeGlobal, AudioObjectPropertyElement element = kAudioObjectPropertyElementMaster, const SFBAudioObjectPropertyQualifier& qualifier = SFBAudioObjectPropertyQualifier(), NSError **error = nullptr)
	{
		AudioObjectPropertyAddress propertyAddress = { .mSelector = property, .mScope = scope, .mElement = element };
		NSNumber * (^transform)(const T&) = ^(const T& value){
			return @(value);
		};
		return ObjectForFixedSizeProperty(objectID, propertyAddress, qualifier, error, transform);
	}

	NSString * _Nullable StringForProperty(AudioObjectID objectID, AudioObjectPropertySelector property, AudioObjectPropertyScope scope = kAudioObjectPropertyScopeGlobal, AudioObjectPropertyElement element = kAudioObjectPropertyElementMaster, const SFBAudioObjectPropertyQualifier& qualifier = SFBAudioObjectPropertyQualifier(), NSError **error = nullptr)
	{
		AudioObjectPropertyAddress propertyAddress = { .mSelector = property, .mScope = scope, .mElement = element };
		return ObjectForCFTypeProperty<NSString>(objectID, propertyAddress, qualifier, error);
	}

	NSDictionary * _Nullable DictionaryForProperty(AudioObjectID objectID, AudioObjectPropertySelector property, AudioObjectPropertyScope scope = kAudioObjectPropertyScopeGlobal, AudioObjectPropertyElement element = kAudioObjectPropertyElementMaster, const SFBAudioObjectPropertyQualifier& qualifier = SFBAudioObjectPropertyQualifier(), NSError **error = nullptr)
	{
		AudioObjectPropertyAddress propertyAddress = { .mSelector = property, .mScope = scope, .mElement = element };
		return ObjectForCFTypeProperty<NSDictionary>(objectID, propertyAddress, qualifier, error);
	}

	NSArray * _Nullable ArrayForProperty(AudioObjectID objectID, AudioObjectPropertySelector property, AudioObjectPropertyScope scope = kAudioObjectPropertyScopeGlobal, AudioObjectPropertyElement element = kAudioObjectPropertyElementMaster, const SFBAudioObjectPropertyQualifier& qualifier = SFBAudioObjectPropertyQualifier(), NSError **error = nullptr)
	{
		AudioObjectPropertyAddress propertyAddress = { .mSelector = property, .mScope = scope, .mElement = element };
		return ObjectForCFTypeProperty<NSArray>(objectID, propertyAddress, qualifier, error);
	}

	NSURL * _Nullable URLForProperty(AudioObjectID objectID, AudioObjectPropertySelector property, AudioObjectPropertyScope scope = kAudioObjectPropertyScopeGlobal, AudioObjectPropertyElement element = kAudioObjectPropertyElementMaster, const SFBAudioObjectPropertyQualifier& qualifier = SFBAudioObjectPropertyQualifier(), NSError **error = nullptr)
	{
		AudioObjectPropertyAddress propertyAddress = { .mSelector = property, .mScope = scope, .mElement = element };
		return ObjectForCFTypeProperty<NSURL>(objectID, propertyAddress, qualifier, error);
	}

	SFBAudioObject * _Nullable AudioObjectForProperty(AudioObjectID objectID, AudioObjectPropertySelector property, AudioObjectPropertyScope scope = kAudioObjectPropertyScopeGlobal, AudioObjectPropertyElement element = kAudioObjectPropertyElementMaster, const SFBAudioObjectPropertyQualifier& qualifier = SFBAudioObjectPropertyQualifier(), NSError **error = nullptr)
	{
		AudioObjectPropertyAddress propertyAddress = { .mSelector = property, .mScope = scope, .mElement = element };
		SFBAudioObject * (^transform)(const AudioObjectID&) = ^(const AudioObjectID& value){
			return [[SFBAudioObject alloc] initWithAudioObjectID:value];
		};
		return ObjectForFixedSizeProperty(objectID, propertyAddress, qualifier, error, transform);
	}

	NSValue * _Nullable AudioStreamBasicDescriptionForProperty(AudioObjectID objectID, AudioObjectPropertySelector property, AudioObjectPropertyScope scope = kAudioObjectPropertyScopeGlobal, AudioObjectPropertyElement element = kAudioObjectPropertyElementMaster, const SFBAudioObjectPropertyQualifier& qualifier = SFBAudioObjectPropertyQualifier(), NSError **error = nullptr)
	{
		AudioObjectPropertyAddress propertyAddress = { .mSelector = property, .mScope = scope, .mElement = element };
		NSValue * (^transform)(const AudioStreamBasicDescription&) = ^(const AudioStreamBasicDescription& value){
			return [NSValue valueWithAudioStreamBasicDescription:value];
		};
		return ObjectForFixedSizeProperty(objectID, propertyAddress, qualifier, error, transform);
	}

	NSValue * _Nullable AudioValueRangeForProperty(AudioObjectID objectID, AudioObjectPropertySelector property, AudioObjectPropertyScope scope = kAudioObjectPropertyScopeGlobal, AudioObjectPropertyElement element = kAudioObjectPropertyElementMaster, const SFBAudioObjectPropertyQualifier& qualifier = SFBAudioObjectPropertyQualifier(), NSError **error = nullptr)
	{
		AudioObjectPropertyAddress propertyAddress = { .mSelector = property, .mScope = scope, .mElement = element };
		NSValue * (^transform)(const AudioValueRange&) = ^(const AudioValueRange& value){
			return [NSValue valueWithAudioValueRange:value];
		};
		return ObjectForFixedSizeProperty(objectID, propertyAddress, qualifier, error, transform);
	}

	SFBAudioChannelLayoutWrapper * _Nullable AudioChannelLayoutForProperty(AudioObjectID objectID, AudioObjectPropertySelector property, AudioObjectPropertyScope scope = kAudioObjectPropertyScopeGlobal, AudioObjectPropertyElement element = kAudioObjectPropertyElementMaster, const SFBAudioObjectPropertyQualifier& qualifier = SFBAudioObjectPropertyQualifier(), NSError **error = nullptr)
	{
		AudioObjectPropertyAddress propertyAddress = { .mSelector = property, .mScope = scope, .mElement = element };
		std::unique_ptr<AudioChannelLayout> value;
		if(!GetVariableSizeProperty(objectID, propertyAddress, value, qualifier, error))
			return nil;
		return [[SFBAudioChannelLayoutWrapper alloc] initWithAudioChannelLayout:value.release() freeWhenDone:YES];
	}

	SFBAudioBufferListWrapper * _Nullable AudioBufferListForProperty(AudioObjectID objectID, AudioObjectPropertySelector property, AudioObjectPropertyScope scope = kAudioObjectPropertyScopeGlobal, AudioObjectPropertyElement element = kAudioObjectPropertyElementMaster, const SFBAudioObjectPropertyQualifier& qualifier = SFBAudioObjectPropertyQualifier(), NSError **error = nullptr)
	{
		AudioObjectPropertyAddress propertyAddress = { .mSelector = property, .mScope = scope, .mElement = element };
		std::unique_ptr<AudioBufferList> value;
		if(!GetVariableSizeProperty(objectID, propertyAddress, value, qualifier, error))
			return nil;
		return [[SFBAudioBufferListWrapper alloc] initWithAudioBufferList:value.release() freeWhenDone:YES];
	}

	SFBAudioHardwareIOProcStreamUsageWrapper * _Nullable AudioHardwareIOProcStreamUsageForProperty(AudioObjectID objectID, AudioObjectPropertySelector property, AudioObjectPropertyScope scope = kAudioObjectPropertyScopeGlobal, AudioObjectPropertyElement element = kAudioObjectPropertyElementMaster, const SFBAudioObjectPropertyQualifier& qualifier = SFBAudioObjectPropertyQualifier(), NSError **error = nullptr)
	{
		AudioObjectPropertyAddress propertyAddress = { .mSelector = property, .mScope = scope, .mElement = element };
		std::unique_ptr<AudioHardwareIOProcStreamUsage> value;
		if(!GetVariableSizeProperty(objectID, propertyAddress, value, qualifier, error))
			return nil;
		return [[SFBAudioHardwareIOProcStreamUsageWrapper alloc] initWithAudioHardwareIOProcStreamUsage:value.release() freeWhenDone:YES];
	}

#pragma mark - Typed Scalar Property Setters

	template <typename T>
	bool SetCFTypePropertyFromObject(AudioObjectID objectID, const AudioObjectPropertyAddress& propertyAddress, const T *value, const SFBAudioObjectPropertyQualifier& qualifier = SFBAudioObjectPropertyQualifier(), NSError **error = nullptr)
	{
		return SetProperty(objectID, propertyAddress, (__bridge CFTypeRef)value, sizeof(CFTypeRef), qualifier, error);
	}

	template <typename T, typename std::enable_if_t<std::is_arithmetic<T>::value, bool> = true>
	bool SetArithmeticProperty(AudioObjectID objectID, const T& value, AudioObjectPropertySelector property, AudioObjectPropertyScope scope = kAudioObjectPropertyScopeGlobal, AudioObjectPropertyElement element = kAudioObjectPropertyElementMaster, const SFBAudioObjectPropertyQualifier& qualifier = SFBAudioObjectPropertyQualifier(), NSError **error = nullptr)
	{
		AudioObjectPropertyAddress propertyAddress = { .mSelector = property, .mScope = scope, .mElement = element };
		return SetProperty(objectID, propertyAddress, &value, sizeof(value), qualifier, error);
	}

	bool SetStringForProperty(AudioObjectID objectID, NSString *value, AudioObjectPropertySelector property, AudioObjectPropertyScope scope = kAudioObjectPropertyScopeGlobal, AudioObjectPropertyElement element = kAudioObjectPropertyElementMaster, const SFBAudioObjectPropertyQualifier& qualifier = SFBAudioObjectPropertyQualifier(), NSError **error = nullptr)
	{
		NSCParameterAssert(value != nil);
		AudioObjectPropertyAddress propertyAddress = { .mSelector = property, .mScope = scope, .mElement = element };
		return SetCFTypePropertyFromObject(objectID, propertyAddress, value, qualifier, error);
	}

	bool SetAudioStreamBasicDescriptionForProperty(AudioObjectID objectID, const AudioStreamBasicDescription& value, AudioObjectPropertySelector property, AudioObjectPropertyScope scope = kAudioObjectPropertyScopeGlobal, AudioObjectPropertyElement element = kAudioObjectPropertyElementMaster, const SFBAudioObjectPropertyQualifier& qualifier = SFBAudioObjectPropertyQualifier(), NSError **error = nullptr)
	{
		AudioObjectPropertyAddress propertyAddress = { .mSelector = property, .mScope = scope, .mElement = element };
		return SetProperty(objectID, propertyAddress, &value, sizeof(value), qualifier, error);
	}

	bool SetAudioChannelLayoutForProperty(AudioObjectID objectID, SFBAudioChannelLayoutWrapper *value, AudioObjectPropertySelector property, AudioObjectPropertyScope scope = kAudioObjectPropertyScopeGlobal, AudioObjectPropertyElement element = kAudioObjectPropertyElementMaster, const SFBAudioObjectPropertyQualifier& qualifier = SFBAudioObjectPropertyQualifier(), NSError **error = nullptr)
	{
		NSCParameterAssert(value != nil);
		AudioObjectPropertyAddress propertyAddress = { .mSelector = property, .mScope = scope, .mElement = element };
		auto layoutSize = GetChannelLayoutSize(value.audioChannelLayout->mNumberChannelDescriptions);
		return SetProperty(objectID, propertyAddress, value.audioChannelLayout, static_cast<UInt32>(layoutSize), qualifier, error);
	}

	bool SetAudioBufferListForProperty(AudioObjectID objectID, SFBAudioBufferListWrapper *value, AudioObjectPropertySelector property, AudioObjectPropertyScope scope = kAudioObjectPropertyScopeGlobal, AudioObjectPropertyElement element = kAudioObjectPropertyElementMaster, const SFBAudioObjectPropertyQualifier& qualifier = SFBAudioObjectPropertyQualifier(), NSError **error = nullptr)
	{
		NSCParameterAssert(value != nil);
		AudioObjectPropertyAddress propertyAddress = { .mSelector = property, .mScope = scope, .mElement = element };
		auto bufferListSize = GetBufferListSize(value.audioBufferList->mNumberBuffers);
		return SetProperty(objectID, propertyAddress, value.audioBufferList, static_cast<UInt32>(bufferListSize), qualifier, error);
	}

	bool SetAudioHardwareIOProcStreamUsageForProperty(AudioObjectID objectID, SFBAudioHardwareIOProcStreamUsageWrapper *value, AudioObjectPropertySelector property, AudioObjectPropertyScope scope = kAudioObjectPropertyScopeGlobal, AudioObjectPropertyElement element = kAudioObjectPropertyElementMaster, const SFBAudioObjectPropertyQualifier& qualifier = SFBAudioObjectPropertyQualifier(), NSError **error = nullptr)
	{
		NSCParameterAssert(value != nil);
		AudioObjectPropertyAddress propertyAddress = { .mSelector = property, .mScope = scope, .mElement = element };
		auto streamUsageSize = GetHardwareIOProcStreamUsageSize(value.audioHardwareIOProcStreamUsage->mNumberStreams);
		return SetProperty(objectID, propertyAddress, value.audioHardwareIOProcStreamUsage, static_cast<UInt32>(streamUsageSize), qualifier, error);
	}

#pragma mark - Typed Array Property Getters

	template <typename T, typename R>
	NSArray<R *> * _Nullable ObjectArrayForFixedSizeProperty(AudioObjectID objectID, const AudioObjectPropertyAddress& propertyAddress, const SFBAudioObjectPropertyQualifier& qualifier, NSError **error, R * _Nullable (^transform)(const T& value))
	{
		std::vector<T> values;
		if(!GetArrayProperty(objectID, propertyAddress, values, qualifier, error))
			return nil;
		NSMutableArray *result = [NSMutableArray arrayWithCapacity:values.size()];
		for(auto value : values)
			[result addObject:transform(value)];
		return result;
	}

	template <typename T, typename std::enable_if_t<std::is_arithmetic<T>::value, bool> = true>
	NSArray <NSNumber *> * _Nullable NumberArrayForArithmeticProperty(AudioObjectID objectID, AudioObjectPropertySelector property, AudioObjectPropertyScope scope = kAudioObjectPropertyScopeGlobal, AudioObjectPropertyElement element = kAudioObjectPropertyElementMaster, const SFBAudioObjectPropertyQualifier& qualifier = SFBAudioObjectPropertyQualifier(), NSError **error = nullptr)
	{
		AudioObjectPropertyAddress propertyAddress = { .mSelector = property, .mScope = scope, .mElement = element };
		NSNumber * (^transform)(const T&) = ^(const T& value){
			return @(value);
		};
		return ObjectArrayForFixedSizeProperty(objectID, propertyAddress, qualifier, error, transform);
	}

	NSArray<SFBAudioObject *> * _Nullable AudioObjectArrayForProperty(AudioObjectID objectID, AudioObjectPropertySelector property, AudioObjectPropertyScope scope = kAudioObjectPropertyScopeGlobal, AudioObjectPropertyElement element = kAudioObjectPropertyElementMaster, const SFBAudioObjectPropertyQualifier& qualifier = SFBAudioObjectPropertyQualifier(), NSError **error = nullptr)
	{
		AudioObjectPropertyAddress propertyAddress = { .mSelector = property, .mScope = scope, .mElement = element };
		SFBAudioObject * (^transform)(const AudioObjectID&) = ^(const AudioObjectID& value){
			return [[SFBAudioObject alloc] initWithAudioObjectID:value];
		};
		return ObjectArrayForFixedSizeProperty(objectID, propertyAddress, qualifier, error, transform);
	}

	NSArray<NSValue *> * _Nullable AudioStreamRangedDescriptionArrayForProperty(AudioObjectID objectID, AudioObjectPropertySelector property, AudioObjectPropertyScope scope = kAudioObjectPropertyScopeGlobal, AudioObjectPropertyElement element = kAudioObjectPropertyElementMaster, const SFBAudioObjectPropertyQualifier& qualifier = SFBAudioObjectPropertyQualifier(), NSError **error = nullptr)
	{
		AudioObjectPropertyAddress propertyAddress = { .mSelector = property, .mScope = scope, .mElement = element };
		NSValue * (^transform)(const AudioStreamRangedDescription&) = ^(const AudioStreamRangedDescription& value){
			return [NSValue valueWithAudioStreamRangedDescription:value];
		};
		return ObjectArrayForFixedSizeProperty(objectID, propertyAddress, qualifier, error, transform);
	}

	NSArray<NSValue *> * _Nullable AudioValueRangeArrayForProperty(AudioObjectID objectID, AudioObjectPropertySelector property, AudioObjectPropertyScope scope = kAudioObjectPropertyScopeGlobal, AudioObjectPropertyElement element = kAudioObjectPropertyElementMaster, const SFBAudioObjectPropertyQualifier& qualifier = SFBAudioObjectPropertyQualifier(), NSError **error = nullptr)
	{
		AudioObjectPropertyAddress propertyAddress = { .mSelector = property, .mScope = scope, .mElement = element };
		NSValue * (^transform)(const AudioValueRange&) = ^(const AudioValueRange& value){
			return [NSValue valueWithAudioValueRange:value];
		};
		return ObjectArrayForFixedSizeProperty(objectID, propertyAddress, qualifier, error, transform);
	}

#pragma mark - Typed Array Property Setters

	template <typename T, typename std::enable_if_t<std::is_arithmetic<T>::value, bool> = true>
	bool SetArithmeticArrayForProperty(AudioObjectID objectID, NSArray<NSNumber *> *values, AudioObjectPropertySelector property, AudioObjectPropertyScope scope = kAudioObjectPropertyScopeGlobal, AudioObjectPropertyElement element = kAudioObjectPropertyElementMaster, const SFBAudioObjectPropertyQualifier& qualifier = SFBAudioObjectPropertyQualifier(), NSError **error = nullptr)
	{
		NSCParameterAssert(values != nil);
		AudioObjectPropertyAddress propertyAddress = { .mSelector = property, .mScope = scope, .mElement = element };
		std::vector<T> v;
		v.reserve(values.count);
		for(NSNumber *value in values)
			v.push_back(_Generic((T)0,
								 bool:			value.boolValue,
								 char: 			value.charValue, 		unsigned char: 			value.unsignedCharValue,
								 short: 		value.shortValue, 		unsigned short: 		value.unsignedShortValue,
								 int: 			value.intValue, 		unsigned int: 			value.unsignedIntValue,
								 long: 			value.longValue, 		unsigned long: 			value.unsignedLongValue,
								 long long: 	value.longLongValue, 	unsigned long long: 	value.unsignedLongLongValue,
								 float:			value.floatValue,		double:					value.doubleValue));
		return SetProperty(objectID, propertyAddress, &v[0], sizeof(T) * v.size(), qualifier, error);
	}

#pragma mark - Audio Object Helpers

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

#pragma mark - Audio Device Helpers

	bool AudioDeviceHasBuffersInScope(AudioObjectID deviceID, AudioObjectPropertyScope scope)
	{
		AudioObjectPropertyAddress propertyAddress = {
			.mSelector	= kAudioDevicePropertyStreamConfiguration,
			.mScope		= scope,
			.mElement	= kAudioObjectPropertyElementMaster
		};
		std::unique_ptr<AudioBufferList> value;
		return GetVariableSizeProperty(deviceID, propertyAddress, value) ? value->mNumberBuffers > 0 : NO;
	}

#pragma mark - Property Listeners

	bool RemovePropertyListener(AudioObjectID objectID, std::map<AudioObjectPropertyAddress, AudioObjectPropertyListenerBlock>& listenerBlocks, AudioObjectPropertySelector property, AudioObjectPropertyScope scope = kAudioObjectPropertyScopeGlobal, AudioObjectPropertyElement element = kAudioObjectPropertyElementMaster, NSError **error = nullptr)
	{
		NSCParameterAssert(objectID != kAudioObjectUnknown);

		AudioObjectPropertyAddress propertyAddress = { .mSelector = property, .mScope = scope, .mElement = element };

		const auto listenerBlock = listenerBlocks.find(propertyAddress);
		if(listenerBlock != listenerBlocks.end()) {
			os_log_info(gSFBAudioObjectLog, "Removing property listener on object 0x%x for {'%{public}.4s', '%{public}.4s', %u}", objectID, SFBCStringForOSType(property), SFBCStringForOSType(scope), element);

			auto block = listenerBlock->second;
			listenerBlocks.erase(listenerBlock);

			OSStatus result = AudioObjectRemovePropertyListenerBlock(objectID, &propertyAddress, dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0), block);
			if(result != kAudioHardwareNoError) {
				os_log_error(gSFBAudioObjectLog, "AudioObjectRemovePropertyListenerBlock ('%{public}.4s', '%{public}.4s', %u) failed: %d '%{public}.4s'", SFBCStringForOSType(property), SFBCStringForOSType(scope), element, result, SFBCStringForOSType(result));
				if(error)
					*error = [NSError errorWithDomain:NSOSStatusErrorDomain code:result userInfo:nil];
				return false;
			}
		}

		return true;
	}

	bool AddPropertyListener(AudioObjectID objectID, std::map<AudioObjectPropertyAddress, AudioObjectPropertyListenerBlock>& listenerBlocks, dispatch_block_t block, AudioObjectPropertySelector property, AudioObjectPropertyScope scope = kAudioObjectPropertyScopeGlobal, AudioObjectPropertyElement element = kAudioObjectPropertyElementMaster, NSError **error = nullptr)
	{
		NSCParameterAssert(objectID != kAudioObjectUnknown);

		if(!RemovePropertyListener(objectID, listenerBlocks, property, scope, element, error))
			return false;

		if(block) {
			AudioObjectPropertyAddress propertyAddress = { .mSelector = property, .mScope = scope, .mElement = element };

			os_log_info(gSFBAudioObjectLog, "Adding property listener on object 0x%x for {'%{public}.4s', '%{public}.4s', %u}", objectID, SFBCStringForOSType(property), SFBCStringForOSType(scope), element);

			AudioObjectPropertyListenerBlock listenerBlock = ^(UInt32 inNumberAddresses, const AudioObjectPropertyAddress *inAddresses) {
#pragma unused(inNumberAddresses)
#pragma unused(inAddresses)
				block();
			};

			listenerBlocks[propertyAddress] = listenerBlock;

			OSStatus result = AudioObjectAddPropertyListenerBlock(objectID, &propertyAddress, dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0), listenerBlock);
			if(result != kAudioHardwareNoError) {
				os_log_error(gSFBAudioObjectLog, "AudioObjectAddPropertyListenerBlock ('%{public}.4s', '%{public}.4s', %u) failed: %d '%{public}.4s'", SFBCStringForOSType(property), SFBCStringForOSType(scope), element, result, SFBCStringForOSType(result));
				listenerBlocks.erase(propertyAddress);
				if(error)
					*error = [NSError errorWithDomain:NSOSStatusErrorDomain code:result userInfo:nil];
				return false;
			}
		}

		return true;
	}

}

extern "C" {

	BOOL SFBAudioObjectIsClass(AudioObjectID objectID, AudioClassID classID)
	{
		return AudioObjectClass(objectID) == classID;
	}

	BOOL SFBAudioObjectIsClassOrSubclassOf(AudioObjectID objectID, AudioClassID classID)
	{
		return AudioObjectClass(objectID) == classID || AudioObjectBaseClass(objectID) == classID;
	}

	BOOL SFBAudioDeviceSupportsInput(AudioObjectID deviceID) 		{ return AudioDeviceHasBuffersInScope(deviceID, kAudioObjectPropertyScopeInput); }
	BOOL SFBAudioDeviceSupportsOutput(AudioObjectID deviceID) 		{ return AudioDeviceHasBuffersInScope(deviceID, kAudioObjectPropertyScopeOutput); }

}

#pragma mark -

@interface SFBAudioObject ()
{
@private
	/// Registered property listener blocks
	std::map<AudioObjectPropertyAddress, AudioObjectPropertyListenerBlock> _listenerBlocks;
}
@end

@implementation SFBAudioObject

static SFBAudioObject *sSystemObject = nil;

+ (SFBAudioObject *)systemObject
{
	static dispatch_once_t onceToken;
	dispatch_once(&onceToken, ^{
		sSystemObject = [[SFBAudioObject alloc] init];
		sSystemObject->_objectID = kAudioObjectSystemObject;
	});
	return sSystemObject;
}

- (instancetype)initWithAudioObjectID:(AudioObjectID)objectID
{
	if(objectID == kAudioObjectUnknown)
		return nil;

	if(objectID == kAudioObjectSystemObject)
		return [SFBAudioObject systemObject];

	AudioClassID classID = AudioObjectClass(objectID);
	AudioClassID baseClassID = AudioObjectBaseClass(objectID);
	switch(baseClassID) {
		case kAudioObjectClassID:
			switch(classID) {
				case kAudioBoxClassID:			self = [[SFBAudioBox alloc] init];		break;
				case kAudioClockDeviceClassID: 	self = [[SFBClockDevice alloc] init];	break;
				case kAudioControlClassID:		self = [[SFBAudioControl alloc] init]; 	break;
				case kAudioDeviceClassID:		self = [[SFBAudioDevice alloc] init];	break;
				case kAudioPlugInClassID:		self = [[SFBAudioPlugIn alloc] init];	break;
				case kAudioStreamClassID:		self = [[SFBAudioStream alloc] init];	break;
				default:						self = [[SFBAudioObject alloc] init];	break;
			}
			break;

		case kAudioControlClassID:
			switch(classID) {
				case kAudioBooleanControlClassID:		self = [[SFBBooleanControl alloc] init];	break;
				case kAudioLevelControlClassID:			self = [[SFBLevelControl alloc] init];		break;
				case kAudioSelectorControlClassID: 		self = [[SFBSelectorControl alloc] init];	break;
				case kAudioSliderControlClassID:		self = [[SFBSliderControl alloc] init];		break;
				case kAudioStereoPanControlClassID: 	self = [[SFBStereoPanControl alloc] init]; 	break;
				default: 								self = [[SFBAudioControl alloc] init];		break;
			}
			break;

		case kAudioBooleanControlClassID:
			switch(classID) {
				case kAudioMuteControlClassID:
				case kAudioSoloControlClassID:
				case kAudioJackControlClassID:
				case kAudioLFEMuteControlClassID:
				case kAudioPhantomPowerControlClassID:
				case kAudioPhaseInvertControlClassID:
				case kAudioClipLightControlClassID:
				case kAudioTalkbackControlClassID:
				case kAudioListenbackControlClassID: 	self = [[SFBBooleanControl alloc] init]; 	break;
				default:								self = [[SFBBooleanControl alloc] init];	break;
			}
			break;

		case kAudioLevelControlClassID:
			switch(classID) {
				case kAudioVolumeControlClassID:
				case kAudioLFEVolumeControlClassID: 	self = [[SFBLevelControl alloc] init]; 	break;
				default:								self = [[SFBLevelControl alloc] init];	break;
			}
			break;

		case kAudioSelectorControlClassID:
			switch(classID) {
				case kAudioDataSourceControlClassID:
				case kAudioDataDestinationControlClassID:
				case kAudioClockSourceControlClassID:
				case kAudioLineLevelControlClassID:
				case kAudioHighPassFilterControlClassID: 	self = [[SFBSelectorControl alloc] init]; 	break;
				default:									self = [[SFBSelectorControl alloc] init];	break;
			}
			break;

		case kAudioDeviceClassID:
			switch(classID) {
				case kAudioAggregateDeviceClassID: 	self = [[SFBAggregateDevice alloc] init];	break;
				case kAudioEndPointDeviceClassID:	self = [[SFBEndpointDevice alloc] init]; 	break;
				case kAudioSubDeviceClassID:		self = [[SFBSubdevice alloc] init];			break;
				default:							self = [[SFBAudioDevice alloc] init];		break;
			}
			break;

		case kAudioPlugInClassID:
			switch(classID) {
				case kAudioTransportManagerClassID: 	self = [[SFBAudioTransportManager alloc] init]; 	break;
				default:								self = [[SFBAudioPlugIn alloc] init];				break;
			}
			break;

		default:
			self = [[SFBAudioObject alloc] init];
			break;
	}

	if(self)
		_objectID = objectID;
	return self;
}

- (void)dealloc
{
	while(!_listenerBlocks.empty()) {
		auto it = _listenerBlocks.begin();
		RemovePropertyListener(_objectID, _listenerBlocks, it->first.mSelector, it->first.mScope, it->first.mElement);
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

- (NSString *)description
{
	return [NSString stringWithFormat:@"<%@ 0x%x>", self.className, _objectID];
}

@end

@implementation SFBAudioObject (SFBPropertyBasics)

#pragma mark - Property Information

- (BOOL)hasProperty:(SFBAudioObjectPropertySelector)property
{
	return HasProperty(_objectID, property);
}

- (BOOL)hasProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope
{
	return HasProperty(_objectID, property, scope);
}

- (BOOL)hasProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element
{
	return HasProperty(_objectID, property, scope, element);
}

- (NSNumber *)propertyIsSettable:(SFBAudioObjectPropertySelector)property
{
	return PropertyIsSettable(_objectID, property);
}

- (NSNumber *)propertyIsSettable:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope
{
	return PropertyIsSettable(_objectID, property, scope);
}

- (NSNumber *)propertyIsSettable:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element
{
	return PropertyIsSettable(_objectID, property, scope, element);
}

- (NSNumber *)propertyIsSettable:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element error:(NSError **)error
{
	return PropertyIsSettable(_objectID, property, scope, element, error);
}

#pragma mark - Property Observation

- (BOOL)whenPropertyChanges:(SFBAudioObjectPropertySelector)property performBlock:(dispatch_block_t)block
{
	return AddPropertyListener(_objectID, _listenerBlocks, block, property);
}

- (BOOL)whenProperty:(SFBAudioObjectPropertySelector)property changesinScope:(SFBAudioObjectPropertyScope)scope performBlock:(dispatch_block_t)block
{
	return AddPropertyListener(_objectID, _listenerBlocks, block, property, scope);
}

- (BOOL)whenProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope changesOnElement:(SFBAudioObjectPropertyElement)element performBlock:(dispatch_block_t)block
{
	return AddPropertyListener(_objectID, _listenerBlocks, block, property, scope, element);
}

- (BOOL)whenProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope changesOnElement:(SFBAudioObjectPropertyElement)element performBlock:(dispatch_block_t)block error:(NSError **)error
{
	return AddPropertyListener(_objectID, _listenerBlocks, block, property, scope, element, error);
}

@end

@implementation SFBAudioObject (SFBPropertyGetters)

#pragma mark - Property Retrieval

- (NSNumber *)unsignedIntForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element error:(NSError **)error
{
	return NumberForArithmeticProperty<UInt32>(_objectID, property, scope, element, {}, error);
}

- (NSArray *)unsignedIntArrayForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element error:(NSError **)error
{
	return NumberArrayForArithmeticProperty<UInt32>(_objectID, property, scope, element, {}, error);
}

- (NSNumber *)floatForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element error:(NSError **)error
{
	return NumberForArithmeticProperty<Float32>(_objectID, property, scope, element, {}, error);
}

- (NSNumber *)doubleForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element error:(NSError **)error
{
	return NumberForArithmeticProperty<Float64>(_objectID, property, scope, element, {}, error);
}

- (NSString *)stringForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element error:(NSError **)error
{
	return StringForProperty(_objectID, property, scope, element, {}, error);
}

- (NSString *)stringForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element qualifier:(const void *)qualifier qualifierSize:(unsigned int)qualifierSize error:(NSError **)error
{
	return StringForProperty(_objectID, property, scope, element, { qualifier, qualifierSize }, error);
}

- (NSDictionary *)dictionaryForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element error:(NSError **)error
{
	return DictionaryForProperty(_objectID, property, scope, element, {}, error);
}

- (NSArray *)arrayForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element error:(NSError **)error
{
	return ArrayForProperty(_objectID, property, scope, element, {}, error);
}

- (NSURL *)urlForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element error:(NSError **)error
{
	return URLForProperty(_objectID, property, scope, element, {}, error);
}

- (SFBAudioObject *)audioObjectForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element error:(NSError **)error
{
	return AudioObjectForProperty(_objectID, property, scope, element, {}, error);
}

- (SFBAudioObject *)audioObjectForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element qualifier:(const void *)qualifier qualifierSize:(unsigned int)qualifierSize error:(NSError **)error
{
	return AudioObjectForProperty(_objectID, property, scope, element, { qualifier, qualifierSize }, error);
}

- (NSArray *)audioObjectArrayForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element error:(NSError **)error
{
	return AudioObjectArrayForProperty(_objectID, property, scope, element, {}, error);
}

- (NSArray *)audioObjectArrayForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element qualifier:(const void *)qualifier qualifierSize:(unsigned int)qualifierSize error:(NSError **)error
{
	return AudioObjectArrayForProperty(_objectID, property, scope, element, { qualifier, qualifierSize }, error);
}

- (NSValue *)audioStreamBasicDescriptionForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element error:(NSError **)error
{
	return AudioStreamBasicDescriptionForProperty(_objectID, property, scope, element, {}, error);
}

- (NSArray *)audioStreamRangedDescriptionArrayForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element error:(NSError **)error
{
	return AudioStreamRangedDescriptionArrayForProperty(_objectID, property, scope, element, {}, error);
}

- (NSValue *)audioValueRangeForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element error:(NSError **)error
{
	return AudioValueRangeForProperty(_objectID, property, scope, element, {}, error);
}

- (NSArray *)audioValueRangeArrayForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element error:(NSError **)error
{
	return AudioValueRangeArrayForProperty(_objectID, property, scope, element, {}, error);
}

- (SFBAudioChannelLayoutWrapper *)audioChannelLayoutForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element error:(NSError **)error
{
	return AudioChannelLayoutForProperty(_objectID, property, scope, element, {}, error);
}

- (SFBAudioBufferListWrapper *)audioBufferListForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element error:(NSError **)error
{
	return AudioBufferListForProperty(_objectID, property, scope, element, {}, error);
}

- (SFBAudioHardwareIOProcStreamUsageWrapper *)audioHardwareIOProcStreamUsageForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element error:(NSError **)error
{
	return AudioHardwareIOProcStreamUsageForProperty(_objectID, property, scope, element, {}, error);
}

@end

@implementation SFBAudioObject (SFBPropertySetters)

#pragma mark - Property Setting

- (BOOL)setUnsignedInt:(unsigned int)value forProperty:(SFBAudioObjectPropertySelector)property
{
	return SetArithmeticProperty<UInt32>(_objectID, value, property);
}

- (BOOL)setUnsignedInt:(unsigned int)value forProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope
{
	return SetArithmeticProperty<UInt32>(_objectID, value, property, scope);
}

- (BOOL)setUnsignedInt:(unsigned int)value forProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element
{
	return SetArithmeticProperty<UInt32>(_objectID, value, property, scope, element);
}

- (BOOL)setUnsignedInt:(unsigned int)value forProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element error:(NSError **)error
{
	return SetArithmeticProperty<UInt32>(_objectID, value, property, scope, element, {}, error);
}

- (BOOL)setUnsignedIntArray:(NSArray<NSNumber *> *)value forProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element error:(NSError **)error
{
	return SetArithmeticArrayForProperty<UInt32>(_objectID, value, property, scope, element, {}, error);
}

- (BOOL)setFloat:(float)value forProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element error:(NSError **)error
{
	return SetArithmeticProperty<Float32>(_objectID, value, property, scope, element, {}, error);
}

- (BOOL)setDouble:(double)value forProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element error:(NSError **)error
{
	return SetArithmeticProperty<Float64>(_objectID, value, property, scope, element, {}, error);
}

- (BOOL)setString:(NSString *)string forProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element error:(NSError **)error
{
	return SetStringForProperty(_objectID, string, property, scope, element, {}, error);
}

- (BOOL)setAudioStreamBasicDescription:(AudioStreamBasicDescription)value forProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element error:(NSError **)error
{
	return SetAudioStreamBasicDescriptionForProperty(_objectID, value, property, scope, element, {}, error);
}

- (BOOL)setAudioObject:(SFBAudioObject *)value forProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element error:(NSError **)error
{
	return SetArithmeticProperty<AudioObjectID>(_objectID, value.objectID, property, scope, element, {}, error);
}

- (BOOL)setAudioChannelLayout:(SFBAudioChannelLayoutWrapper *)value forProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element error:(NSError **)error
{
	return SetAudioChannelLayoutForProperty(_objectID, value, property, scope, element, {}, error);
}

- (BOOL)setAudioBufferList:(SFBAudioBufferListWrapper *)value forProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element error:(NSError **)error
{
	return SetAudioBufferListForProperty(_objectID, value, property, scope, element, {}, error);
}

- (BOOL)setAudioHardwareIOProcStreamUsage:(SFBAudioHardwareIOProcStreamUsageWrapper *)value forProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element error:(NSError **)error
{
	return SetAudioHardwareIOProcStreamUsageForProperty(_objectID, value, property, scope, element, {}, error);
}

@end

@implementation SFBAudioObject (SFBAudioObjectProperties)

#pragma mark - AudioObject Properties

- (NSNumber *)baseClassID
{
	return [self unsignedIntForProperty:(SFBAudioObjectPropertySelector)kAudioObjectPropertyBaseClass inScope:(SFBAudioObjectPropertyScope)kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster error:NULL];
}

- (NSNumber *)classID
{
	return [self unsignedIntForProperty:(SFBAudioObjectPropertySelector)kAudioObjectPropertyClass inScope:(SFBAudioObjectPropertyScope)kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster error:NULL];
}

- (SFBAudioObject *)owner
{
	return [self audioObjectForProperty:(SFBAudioObjectPropertySelector)kAudioObjectPropertyOwner inScope:(SFBAudioObjectPropertyScope)kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster error:NULL];
}

- (NSString *)name
{
	return [self stringForProperty:(SFBAudioObjectPropertySelector)kAudioObjectPropertyName inScope:(SFBAudioObjectPropertyScope)kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster error:NULL];
}

- (NSString *)modelName
{
	return [self stringForProperty:(SFBAudioObjectPropertySelector)kAudioObjectPropertyModelName inScope:(SFBAudioObjectPropertyScope)kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster error:NULL];
}

- (NSString *)manufacturer
{
	return [self stringForProperty:(SFBAudioObjectPropertySelector)kAudioObjectPropertyManufacturer inScope:(SFBAudioObjectPropertyScope)kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster error:NULL];
}

- (NSString *)nameOfElement:(SFBAudioObjectPropertyElement)element inScope:(SFBAudioObjectPropertyScope)scope
{
	return [self stringForProperty:(SFBAudioObjectPropertySelector)kAudioObjectPropertyElementName inScope:scope onElement:element error:NULL];
}

- (NSString *)categoryNameOfElement:(SFBAudioObjectPropertyElement)element inScope:(SFBAudioObjectPropertyScope)scope
{
	return [self stringForProperty:(SFBAudioObjectPropertySelector)kAudioObjectPropertyElementCategoryName inScope:scope onElement:element error:NULL];
}

- (NSString *)numberNameOfElement:(SFBAudioObjectPropertyElement)element inScope:(SFBAudioObjectPropertyScope)scope
{
	return [self stringForProperty:(SFBAudioObjectPropertySelector)kAudioObjectPropertyElementNumberName inScope:scope onElement:element error:NULL];
}

- (NSArray *)ownedObjects
{
	return [self audioObjectArrayForProperty:(SFBAudioObjectPropertySelector)kAudioObjectPropertyOwnedObjects inScope:(SFBAudioObjectPropertyScope)kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster error:NULL];
}

- (NSArray *)ownedObjectsOfType:(NSArray *)types
{
	NSParameterAssert(types != nil);

	std::vector<AudioClassID> classIDs;
	classIDs.reserve(types.count);
	for(NSNumber *type in types)
		classIDs.push_back(type.unsignedIntValue);

	UInt32 qualifierSize = (UInt32)(sizeof(AudioClassID) * classIDs.size());
	return [self audioObjectArrayForProperty:(SFBAudioObjectPropertySelector)kAudioObjectPropertyOwnedObjects inScope:(SFBAudioObjectPropertyScope)kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster qualifier:&classIDs[0] qualifierSize:qualifierSize error:NULL];
}

- (NSString *)serialNumber
{
	return [self stringForProperty:(SFBAudioObjectPropertySelector)kAudioObjectPropertySerialNumber inScope:(SFBAudioObjectPropertyScope)kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster error:NULL];
}

- (NSString *)firmwareVersion
{
	return [self stringForProperty:(SFBAudioObjectPropertySelector)kAudioObjectPropertyFirmwareVersion inScope:(SFBAudioObjectPropertyScope)kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster error:NULL];
}

@end

#pragma mark -

@implementation NSValue (SFBCoreAudioStructs)

+ (instancetype)valueWithAudioStreamBasicDescription:(AudioStreamBasicDescription)asbd
{
	return [NSValue value:&asbd withObjCType:@encode(AudioStreamBasicDescription)];
}

- (AudioStreamBasicDescription)audioStreamBasicDescriptionValue
{
	AudioStreamBasicDescription asbd;
	[self getValue:&asbd];
	return asbd;
}

+ (instancetype)valueWithAudioStreamRangedDescription:(AudioStreamRangedDescription)asrd
{
	return [NSValue value:&asrd withObjCType:@encode(AudioStreamRangedDescription)];
}

- (AudioStreamRangedDescription)audioStreamRangedDescriptionValue
{
	AudioStreamRangedDescription asrd;
	[self getValue:&asrd];
	return asrd;
}

+ (instancetype)valueWithAudioValueRange:(AudioValueRange)avr
{
	return [NSValue value:&avr withObjCType:@encode(AudioValueRange)];
}

- (AudioValueRange)audioValueRangeValue
{
	AudioValueRange avr;
	[self getValue:&avr];
	return avr;
}

@end

@implementation NSNumber (SFBpid)

+ (instancetype)numberWithPid:(pid_t)pid
{
	static_assert(_Generic((pid_t)0, int: 1, default: 0), "pid_t is not int");
	return [NSNumber numberWithInt:pid];
}

- (pid_t)pidValue
{
	return self.intValue;
}

@end

#pragma mark -

@interface SFBAudioBufferListWrapper ()
{
@private
	AudioBufferList *_bufferList;
	BOOL _freeWhenDone;
}
@end

@implementation SFBAudioBufferListWrapper

- (instancetype)initWithAudioBufferList:(AudioBufferList *)audioBufferList freeWhenDone:(BOOL)freeWhenDone
{
	NSParameterAssert(audioBufferList != NULL);
	if((self = [super init])) {
		_bufferList = audioBufferList;
		_freeWhenDone = freeWhenDone;
	}
	return self;
}

- (instancetype)initWithNumberBuffers:(unsigned int)numberBuffers
{
	auto abl = CreateBufferList(numberBuffers);
	if(!abl)
		return nil;
	return [self initWithAudioBufferList:abl.release() freeWhenDone:YES];
}

- (void)dealloc
{
	if(_freeWhenDone) {
		for(UInt32 i = 0; i < _bufferList->mNumberBuffers; ++i) {
			if(_bufferList->mBuffers[i].mData)
				std::free(_bufferList->mBuffers[i].mData);
		}
		std::free(_bufferList);
	}
}

- (const AudioBufferList *)audioBufferList
{
	return _bufferList;
}

- (UInt32)numberBuffers
{
	return _bufferList->mNumberBuffers;
}

- (const AudioBuffer *)buffers
{
	return _bufferList->mNumberBuffers > 0 ? _bufferList->mBuffers : nullptr;
}

@end

@interface SFBAudioChannelLayoutWrapper ()
{
@private
	AudioChannelLayout *_channelLayout;
	BOOL _freeWhenDone;
}
@end

@implementation SFBAudioChannelLayoutWrapper

- (instancetype)initWithAudioChannelLayout:(AudioChannelLayout *)audioChannelLayout freeWhenDone:(BOOL)freeWhenDone
{
	NSParameterAssert(audioChannelLayout != nullptr);
	if((self = [super init])) {
		_channelLayout = audioChannelLayout;
		_freeWhenDone = freeWhenDone;
	}
	return self;
}

- (instancetype)initWithAudioChannelLayout:(AudioChannelLayout *)audioChannelLayout
{
	NSParameterAssert(audioChannelLayout != nullptr);
	auto acl = CopyChannelLayout(*audioChannelLayout);
	if(!acl)
		return nil;
	return [self initWithAudioChannelLayout:acl.release() freeWhenDone:YES];
}

- (instancetype)initWithNumberChannelDescriptions:(unsigned int)numberChannelDescriptions
{
	auto acl = CreateChannelLayout(numberChannelDescriptions);
	if(!acl)
		return nil;
	return [self initWithAudioChannelLayout:acl.release() freeWhenDone:YES];
}

- (void)dealloc
{
	if(_freeWhenDone)
		std::free(_channelLayout);
}

- (const AudioChannelLayout *)audioChannelLayout
{
	return _channelLayout;
}

- (AudioChannelLayoutTag)tag
{
	return _channelLayout->mChannelLayoutTag;
}

- (AudioChannelBitmap)bitmap
{
	return _channelLayout->mChannelBitmap;
}

- (UInt32)numberChannelDescriptions
{
	return _channelLayout->mNumberChannelDescriptions;
}

- (const AudioChannelDescription *)channelDescriptions
{
	return _channelLayout->mNumberChannelDescriptions > 0 ? _channelLayout->mChannelDescriptions : nullptr;
}

@end

@interface SFBAudioHardwareIOProcStreamUsageWrapper ()
{
@private
	AudioHardwareIOProcStreamUsage *_hardwareIOProcStreamUsage;
	BOOL _freeWhenDone;
}
@end


@implementation SFBAudioHardwareIOProcStreamUsageWrapper

- (instancetype)initWithAudioHardwareIOProcStreamUsage:(AudioHardwareIOProcStreamUsage *)audioHardwareIOProcStreamUsage freeWhenDone:(BOOL)freeWhenDone
{
	NSParameterAssert(audioHardwareIOProcStreamUsage != nullptr);
	if((self = [super init])) {
		_hardwareIOProcStreamUsage = audioHardwareIOProcStreamUsage;
		_freeWhenDone = freeWhenDone;
	}
	return self;
}

- (instancetype)initWithNumberStreams:(unsigned int)numberStreams
{
	auto streamUsage = CreateHardwareIOProcStreamUsage(numberStreams);
	if(!streamUsage)
		return nil;
	return [self initWithAudioHardwareIOProcStreamUsage:streamUsage.release() freeWhenDone:YES];
}

- (void)dealloc
{
	if(_freeWhenDone)
		std::free(_hardwareIOProcStreamUsage);
}

- (const AudioHardwareIOProcStreamUsage *)audioHardwareIOProcStreamUsage
{
	return _hardwareIOProcStreamUsage;
}

- (const void *)ioProc
{
	return _hardwareIOProcStreamUsage->mIOProc;
}

- (UInt32)numberStreams
{
	return _hardwareIOProcStreamUsage->mNumberStreams;
}

- (const UInt32 *)streamIsOn
{
	return _hardwareIOProcStreamUsage->mNumberStreams > 0 ? _hardwareIOProcStreamUsage->mStreamIsOn : nullptr;
}

@end
