/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <map>

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

os_log_t gSFBAudioObjectLog = NULL;

template <>
struct ::std::default_delete<AudioBufferList> {
	default_delete() = default;
	template <class U>
	constexpr default_delete(default_delete<U>) noexcept {}
	void operator()(AudioBufferList *abl) const noexcept { std::free(abl); }
};

bool operator<(const AudioObjectPropertyAddress& lhs, const AudioObjectPropertyAddress& rhs);
bool operator<(const AudioObjectPropertyAddress& lhs, const AudioObjectPropertyAddress& rhs)
{
	return lhs.mSelector < rhs.mSelector && lhs.mScope < rhs.mScope && lhs.mElement < rhs.mElement;
}

namespace {

	void SFBCreateAudioObjectLog(void) __attribute__ ((constructor));
	void SFBCreateAudioObjectLog()
	{
		static dispatch_once_t onceToken;
		dispatch_once(&onceToken, ^{
			gSFBAudioObjectLog = os_log_create("org.sbooth.AudioEngine", "AudioObject");
		});
	}

	bool RemovePropertyListener(AudioObjectID objectID, std::map<AudioObjectPropertyAddress, AudioObjectPropertyListenerBlock>& listenerBlocks, AudioObjectPropertySelector property, AudioObjectPropertyScope scope = kAudioObjectPropertyScopeGlobal, AudioObjectPropertyElement element = kAudioObjectPropertyElementMaster)
	{
		NSCParameterAssert(objectID != kAudioObjectUnknown);

		AudioObjectPropertyAddress propertyAddress = { .mSelector = property, .mScope = scope, .mElement = element };

		const auto listenerBlock = listenerBlocks.find(propertyAddress);
		if(listenerBlock == listenerBlocks.end())
			return false;

		os_log_info(gSFBAudioObjectLog, "Removing property listener on object 0x%x for {'%{public}.4s', '%{public}.4s', %u}", objectID, SFBCStringForOSType(property), SFBCStringForOSType(scope), element);

		OSStatus result = AudioObjectRemovePropertyListenerBlock(objectID, &propertyAddress, dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0), listenerBlock->second);
		if(result != kAudioHardwareNoError)
			os_log_error(gSFBAudioObjectLog, "AudioObjectRemovePropertyListenerBlock ('%{public}.4s', '%{public}.4s', %u) failed: %d '%{public}.4s'", SFBCStringForOSType(property), SFBCStringForOSType(scope), element, result, SFBCStringForOSType(result));

		listenerBlocks.erase(listenerBlock);

		return true;
	}

	bool AddPropertyListener(AudioObjectID objectID, std::map<AudioObjectPropertyAddress, AudioObjectPropertyListenerBlock>& listenerBlocks, dispatch_block_t block, AudioObjectPropertySelector property, AudioObjectPropertyScope scope = kAudioObjectPropertyScopeGlobal, AudioObjectPropertyElement element = kAudioObjectPropertyElementMaster)
	{
		NSCParameterAssert(objectID != kAudioObjectUnknown);

		RemovePropertyListener(objectID, listenerBlocks, property, scope, element);

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
				return false;
			}
		}

		return true;
	}

}

#pragma mark Property Information

bool SFB::HasProperty(AudioObjectID objectID, AudioObjectPropertySelector property, AudioObjectPropertyScope scope, AudioObjectPropertyElement element)
{
	AudioObjectPropertyAddress propertyAddress = { .mSelector = property, .mScope = scope, .mElement = element };
	return AudioObjectHasProperty(objectID, &propertyAddress);
}

bool SFB::PropertyIsSettable(AudioObjectID objectID, AudioObjectPropertySelector property, AudioObjectPropertyScope scope, AudioObjectPropertyElement element)
{
	AudioObjectPropertyAddress propertyAddress = { .mSelector = property, .mScope = scope, .mElement = element };
	Boolean isSettable;
	OSStatus result = AudioObjectIsPropertySettable(objectID, &propertyAddress, &isSettable);
	if(result != kAudioHardwareNoError) {
		os_log_error(gSFBAudioObjectLog, "AudioObjectIsPropertySettable ('%{public}.4s', '%{public}.4s', %u) failed: %d '%{public}.4s'", SFBCStringForOSType(propertyAddress.mSelector), SFBCStringForOSType(propertyAddress.mScope), propertyAddress.mElement, result, SFBCStringForOSType(result));
		return false;
	}
	return isSettable;
}

#pragma mark Typed Scalar Property Getters

NSNumber * SFB::UInt32ForProperty(AudioObjectID objectID, AudioObjectPropertySelector property, AudioObjectPropertyScope scope, AudioObjectPropertyElement element)
{
	AudioObjectPropertyAddress propertyAddress = { .mSelector = property, .mScope = scope, .mElement = element };
	UInt32 value;
	return GetFixedSizeProperty(objectID, propertyAddress, value) ? @(value) : nil;
}

NSNumber * SFB::Float32ForProperty(AudioObjectID objectID, AudioObjectPropertySelector property, AudioObjectPropertyScope scope, AudioObjectPropertyElement element)
{
	AudioObjectPropertyAddress propertyAddress = { .mSelector = property, .mScope = scope, .mElement = element };
	Float32 value;
	return GetFixedSizeProperty(objectID, propertyAddress, value) ? @(value) : nil;
}

NSNumber * SFB::Float64ForProperty(AudioObjectID objectID, AudioObjectPropertySelector property, AudioObjectPropertyScope scope, AudioObjectPropertyElement element)
{
	AudioObjectPropertyAddress propertyAddress = { .mSelector = property, .mScope = scope, .mElement = element };
	Float64 value;
	return GetFixedSizeProperty(objectID, propertyAddress, value) ? @(value) : nil;
}

NSString * SFB::StringForProperty(AudioObjectID objectID, AudioObjectPropertySelector property, AudioObjectPropertyScope scope, AudioObjectPropertyElement element)
{
	AudioObjectPropertyAddress propertyAddress = { .mSelector = property, .mScope = scope, .mElement = element };
	CFStringRef value;
	return GetFixedSizeProperty(objectID, propertyAddress, value) ? (__bridge_transfer NSString *)value : nil;
}

NSDictionary * SFB::DictionaryForProperty(AudioObjectID objectID, AudioObjectPropertySelector property, AudioObjectPropertyScope scope, AudioObjectPropertyElement element)
{
	AudioObjectPropertyAddress propertyAddress = { .mSelector = property, .mScope = scope, .mElement = element };
	CFDictionaryRef value;
	return GetFixedSizeProperty(objectID, propertyAddress, value) ? (__bridge_transfer NSDictionary *)value : nil;
}

SFBAudioObject * SFB::AudioObjectForProperty(AudioObjectID objectID, AudioObjectPropertySelector property, AudioObjectPropertyScope scope, AudioObjectPropertyElement element)
{
	AudioObjectPropertyAddress propertyAddress = { .mSelector = property, .mScope = scope, .mElement = element };
	AudioObjectID value;
	return GetFixedSizeProperty(objectID, propertyAddress, value) ? [[SFBAudioObject alloc] initWithAudioObjectID:value] : nil;
}

NSValue * SFB::AudioStreamBasicDescriptionForProperty(AudioObjectID objectID, AudioObjectPropertySelector property, AudioObjectPropertyScope scope, AudioObjectPropertyElement element)
{
	AudioObjectPropertyAddress propertyAddress = { .mSelector = property, .mScope = scope, .mElement = element };
	AudioStreamBasicDescription value;
	return GetFixedSizeProperty(objectID, propertyAddress, value) ? [NSValue valueWithAudioStreamBasicDescription:value] : nil;
}

#pragma mark - Typed Array Property Getters

NSArray <NSNumber *> * SFB::UInt32ArrayForProperty(AudioObjectID objectID, AudioObjectPropertySelector property, AudioObjectPropertyScope scope, AudioObjectPropertyElement element)
{
	AudioObjectPropertyAddress propertyAddress = { .mSelector = property, .mScope = scope, .mElement = element };

	std::vector<UInt32> values;
	if(!GetArrayProperty(objectID, propertyAddress, values))
		return nil;
	NSMutableArray *result = [NSMutableArray arrayWithCapacity:values.size()];
	for(auto value : values)
		[result addObject:@(value)];
	return result;
}

NSArray<SFBAudioObject *> * SFB::AudioObjectArrayForProperty(AudioObjectID objectID, AudioObjectPropertySelector property, AudioObjectPropertyScope scope, AudioObjectPropertyElement element)
{
	AudioObjectPropertyAddress propertyAddress = { .mSelector = property, .mScope = scope, .mElement = element };

	std::vector<AudioObjectID> values;
	if(!GetArrayProperty(objectID, propertyAddress, values))
		return nil;
	NSMutableArray *result = [NSMutableArray arrayWithCapacity:values.size()];
	for(auto value : values)
		[result addObject:[[SFBAudioObject alloc] initWithAudioObjectID:value]];
	return result;
}

NSArray<NSValue *> * SFB::AudioStreamRangedDescriptionArrayForProperty(AudioObjectID objectID, AudioObjectPropertySelector property, AudioObjectPropertyScope scope, AudioObjectPropertyElement element)
{
	AudioObjectPropertyAddress propertyAddress = { .mSelector = property, .mScope = scope, .mElement = element };
	std::vector<AudioStreamRangedDescription> values;
	if(!GetArrayProperty(objectID, propertyAddress, values))
		return nil;
	NSMutableArray *result = [NSMutableArray arrayWithCapacity:values.size()];
	for(auto value : values)
		[result addObject:[NSValue valueWithAudioStreamRangedDescription:value]];
	return result;
}

NSArray<NSValue *> * SFB::AudioValueRangeArrayForProperty(AudioObjectID objectID, AudioObjectPropertySelector property, AudioObjectPropertyScope scope, AudioObjectPropertyElement element)
{
	AudioObjectPropertyAddress propertyAddress = { .mSelector = property, .mScope = scope, .mElement = element };
	std::vector<AudioValueRange> values;
	if(!GetArrayProperty(objectID, propertyAddress, values))
		return nil;
	NSMutableArray *result = [NSMutableArray arrayWithCapacity:values.size()];
	for(auto value : values)
		[result addObject:[NSValue valueWithAudioValueRange:value]];
	return result;
}

#pragma mark - Audio Object Helpers

AudioClassID SFB::AudioObjectClass(AudioObjectID objectID)
{
	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= kAudioObjectPropertyClass,
		.mScope		= kAudioObjectPropertyScopeGlobal,
		.mElement	= kAudioObjectPropertyElementMaster
	};
	AudioClassID classID;
	return GetFixedSizeProperty(objectID, propertyAddress, classID) ? classID : 0;
}

AudioClassID SFB::AudioObjectBaseClass(AudioObjectID objectID)
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

bool SFB::AudioDeviceHasBuffersInScope(AudioObjectID deviceID, AudioObjectPropertyScope scope)
{
	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= kAudioDevicePropertyStreamConfiguration,
		.mScope		= scope,
		.mElement	= kAudioObjectPropertyElementMaster
	};
	std::unique_ptr<AudioBufferList> value;
	return GetVariableSizeProperty(deviceID, propertyAddress, value) ? value->mNumberBuffers > 0 : NO;
}

extern "C" {

#pragma mark - Audio Object Class Determination

	BOOL SFBAudioObjectIsClass(AudioObjectID objectID, AudioClassID classID)
	{
		return SFB::AudioObjectClass(objectID) == classID;
	}

	BOOL SFBAudioObjectIsClassOrSubclassOf(AudioObjectID objectID, AudioClassID classID)
	{
		return SFB::AudioObjectClass(objectID) == classID || SFB::AudioObjectBaseClass(objectID) == classID;
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

	BOOL SFBAudioDeviceSupportsInput(AudioObjectID deviceID) 		{ return SFB::AudioDeviceHasBuffersInScope(deviceID, kAudioObjectPropertyScopeInput); }
	BOOL SFBAudioDeviceSupportsOutput(AudioObjectID deviceID) 		{ return SFB::AudioDeviceHasBuffersInScope(deviceID, kAudioObjectPropertyScopeOutput); }

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

	AudioClassID classID = SFB::AudioObjectClass(objectID);
	AudioClassID baseClassID = SFB::AudioObjectBaseClass(objectID);
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

#pragma mark - Audio Object Properties

- (BOOL)hasProperty:(SFBAudioObjectPropertySelector)property
{
	return SFB::HasProperty(_objectID, property);
}

- (BOOL)hasProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope
{
	return SFB::HasProperty(_objectID, property, scope);
}

- (BOOL)hasProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element
{
	return SFB::HasProperty(_objectID, property, scope, element);
}

- (BOOL)propertyIsSettable:(SFBAudioObjectPropertySelector)property
{
	return SFB::PropertyIsSettable(_objectID, property);
}

- (BOOL)propertyIsSettable:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope
{
	return SFB::PropertyIsSettable(_objectID, property, scope);
}

- (BOOL)propertyIsSettable:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element
{
	return SFB::PropertyIsSettable(_objectID, property, element);
}

- (NSNumber *)uintForProperty:(SFBAudioObjectPropertySelector)property
{
	return SFB::UInt32ForProperty(_objectID, property);
}

- (NSNumber *)uintForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope
{
	return SFB::UInt32ForProperty(_objectID, property, scope);
}

- (NSNumber *)uintForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element
{
	return SFB::UInt32ForProperty(_objectID, property, scope, element);
}

- (NSArray *)uintArrayForProperty:(SFBAudioObjectPropertySelector)property
{
	return SFB::UInt32ArrayForProperty(_objectID, property);
}

- (NSArray *)uintArrayForProperty:(SFBAudioObjectPropertySelector)property  inScope:(SFBAudioObjectPropertyScope)scope
{
	return SFB::UInt32ArrayForProperty(_objectID, property, scope);
}

- (NSArray *)uintArrayForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element
{
	return SFB::UInt32ArrayForProperty(_objectID, property, scope, element);
}

- (NSNumber *)floatForProperty:(SFBAudioObjectPropertySelector)property
{
	return SFB::Float32ForProperty(_objectID, property);
}

- (NSNumber *)floatForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope
{
	return SFB::Float32ForProperty(_objectID, property, scope);
}

- (NSNumber *)floatForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element
{
	return SFB::Float32ForProperty(_objectID, property, scope, element);
}

- (NSNumber *)doubleForProperty:(SFBAudioObjectPropertySelector)property
{
	return SFB::Float64ForProperty(_objectID, property);
}

- (NSNumber *)doubleForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope
{
	return SFB::Float64ForProperty(_objectID, property, scope);
}

- (NSNumber *)doubleForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element
{
	return SFB::Float64ForProperty(_objectID, property, scope, element);
}

- (NSString *)stringForProperty:(SFBAudioObjectPropertySelector)property
{
	return SFB::StringForProperty(_objectID, property);
}

- (NSString *)stringForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope
{
	return SFB::StringForProperty(_objectID, property, scope);
}

- (NSString *)stringForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element
{
	return SFB::StringForProperty(_objectID, property, scope, element);
}

- (NSDictionary *)dictionaryForProperty:(SFBAudioObjectPropertySelector)property
{
	return SFB::DictionaryForProperty(_objectID, property);
}

- (NSDictionary *)dictionaryForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope
{
	return SFB::DictionaryForProperty(_objectID, property, scope);
}

- (NSDictionary *)dictionaryForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element
{
	return SFB::DictionaryForProperty(_objectID, property, scope, element);
}

- (SFBAudioObject *)audioObjectForProperty:(SFBAudioObjectPropertySelector)property
{
	return SFB::AudioObjectForProperty(_objectID, property);
}

- (SFBAudioObject *)audioObjectForProperty:(SFBAudioObjectPropertySelector)property  inScope:(SFBAudioObjectPropertyScope)scope
{
	return SFB::AudioObjectForProperty(_objectID, property, scope);
}

- (SFBAudioObject *)audioObjectForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element
{
	return SFB::AudioObjectForProperty(_objectID, property, scope, element);
}

- (NSArray *)audioObjectArrayForProperty:(SFBAudioObjectPropertySelector)property
{
	return SFB::AudioObjectArrayForProperty(_objectID, property);
}

- (NSArray *)audioObjectArrayForProperty:(SFBAudioObjectPropertySelector)property  inScope:(SFBAudioObjectPropertyScope)scope
{
	return SFB::AudioObjectArrayForProperty(_objectID, property, scope);
}

- (NSArray *)audioObjectArrayForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element
{
	return SFB::AudioObjectArrayForProperty(_objectID, property, scope, element);
}

- (NSValue *)audioStreamBasicDescriptionForProperty:(SFBAudioObjectPropertySelector)property
{
	return SFB::AudioStreamBasicDescriptionForProperty(_objectID, property);
}

- (NSValue *)audioStreamBasicDescriptionForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope
{
	return SFB::AudioStreamBasicDescriptionForProperty(_objectID, property, scope);
}

- (NSValue *)audioStreamBasicDescriptionForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element
{
	return SFB::AudioStreamBasicDescriptionForProperty(_objectID, property, scope, element);
}

- (NSArray *)audioStreamRangedDescriptionArrayForProperty:(SFBAudioObjectPropertySelector)property
{
	return SFB::AudioStreamRangedDescriptionArrayForProperty(_objectID, property);
}

- (NSArray *)audioStreamRangedDescriptionArrayForProperty:(SFBAudioObjectPropertySelector)property  inScope:(SFBAudioObjectPropertyScope)scope
{
	return SFB::AudioStreamRangedDescriptionArrayForProperty(_objectID, property, scope);
}

- (NSArray *)audioStreamRangedDescriptionArrayForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element
{
	return SFB::AudioStreamRangedDescriptionArrayForProperty(_objectID, property, scope, element);
}

- (NSArray *)audioValueRangeArrayForProperty:(SFBAudioObjectPropertySelector)property
{
	return SFB::AudioValueRangeArrayForProperty(_objectID, property);
}

- (NSArray *)audioValueRangeArrayForProperty:(SFBAudioObjectPropertySelector)property  inScope:(SFBAudioObjectPropertyScope)scope
{
	return SFB::AudioValueRangeArrayForProperty(_objectID, property, scope);
}

- (NSArray *)audioValueRangeArrayForProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope onElement:(SFBAudioObjectPropertyElement)element
{
	return SFB::AudioValueRangeArrayForProperty(_objectID, property, scope, element);
}

- (void)whenPropertyChanges:(SFBAudioObjectPropertySelector)property performBlock:(dispatch_block_t)block
{
	AddPropertyListener(_objectID, _listenerBlocks, block, property);
}

- (void)whenProperty:(SFBAudioObjectPropertySelector)property changesinScope:(SFBAudioObjectPropertyScope)scope performBlock:(dispatch_block_t)block
{
	AddPropertyListener(_objectID, _listenerBlocks, block, property, scope);
}

- (void)whenProperty:(SFBAudioObjectPropertySelector)property inScope:(SFBAudioObjectPropertyScope)scope changesOnElement:(SFBAudioObjectPropertyElement)element performBlock:(dispatch_block_t)block
{
	AddPropertyListener(_objectID, _listenerBlocks, block, property, scope, element);
}

- (NSString *)description
{
	return [NSString stringWithFormat:@"<%@ 0x%x>", self.className, _objectID];
}

@end

@implementation SFBAudioObject (SFBAudioObjectProperties)

- (AudioClassID)baseClassID
{
	return SFB::NumericTypeForProperty<AudioClassID>(_objectID, kAudioObjectPropertyBaseClass);
}

- (AudioClassID)classID
{
	return SFB::NumericTypeForProperty<AudioClassID>(_objectID, kAudioObjectPropertyClass);
}

- (SFBAudioObject *)owner
{
	return SFB::AudioObjectForProperty(_objectID, kAudioObjectPropertyOwner);
}

- (NSString *)name
{
	return SFB::StringForProperty(_objectID, kAudioObjectPropertyName);
}

- (NSString *)modelName
{
	return SFB::StringForProperty(_objectID, kAudioObjectPropertyModelName);
}

- (NSString *)manufacturer
{
	return SFB::StringForProperty(_objectID, kAudioObjectPropertyManufacturer);
}

- (NSString *)nameOfElement:(SFBAudioObjectPropertyElement)element
{
	return SFB::StringForProperty(_objectID, kAudioObjectPropertyElementName, kAudioObjectPropertyScopeGlobal, element);
}

- (NSString *)nameOfElement:(SFBAudioObjectPropertyElement)element inScope:(SFBAudioObjectPropertyScope)scope
{
	return SFB::StringForProperty(_objectID, kAudioObjectPropertyElementName, scope, element);
}

- (NSString *)categoryNameOfElement:(SFBAudioObjectPropertyElement)element
{
	return SFB::StringForProperty(_objectID, kAudioObjectPropertyElementCategoryName, kAudioObjectPropertyScopeGlobal, element);
}

- (NSString *)categoryNameOfElement:(SFBAudioObjectPropertyElement)element inScope:(SFBAudioObjectPropertyScope)scope
{
	return SFB::StringForProperty(_objectID, kAudioObjectPropertyElementCategoryName, scope, element);
}

- (NSString *)numberNameOfElement:(SFBAudioObjectPropertyElement)element
{
	return SFB::StringForProperty(_objectID, kAudioObjectPropertyElementNumberName, kAudioObjectPropertyScopeGlobal, element);
}

- (NSString *)numberNameOfElement:(SFBAudioObjectPropertyElement)element inScope:(SFBAudioObjectPropertyScope)scope
{
	return SFB::StringForProperty(_objectID, kAudioObjectPropertyElementNumberName, scope, element);
}

- (NSArray *)ownedObjects
{
	return SFB::AudioObjectArrayForProperty(_objectID, kAudioObjectPropertyOwnedObjects);
}

- (NSString *)serialNumber
{
	return SFB::StringForProperty(_objectID, kAudioObjectPropertySerialNumber);
}

- (NSString *)firmwareVersion
{
	return SFB::StringForProperty(_objectID, kAudioObjectPropertyFirmwareVersion);
}

@end

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
