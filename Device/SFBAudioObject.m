/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

@import os.log;

#import "SFBAudioObject+Internal.h"

#import "SFBAggregateDevice.h"
#import "SFBAudioBox.h"
#import "SFBAudioDevice.h"
#import "SFBAudioPlugIn.h"
#import "SFBAudioStream.h"
#import "SFBAudioTransportManager.h"
#import "SFBClockDevice.h"
#import "SFBEndpointDevice.h"
#import "SFBSubdevice.h"
#import "SFBCStringForOSType.h"

os_log_t gSFBAudioObjectLog = NULL;

static void SFBCreateAudioObjectLog(void) __attribute__ ((constructor));
static void SFBCreateAudioObjectLog()
{
	static dispatch_once_t onceToken;
	dispatch_once(&onceToken, ^{
		gSFBAudioObjectLog = os_log_create("org.sbooth.AudioEngine", "AudioObject");
	});
}

#pragma mark - Audio Object Class Determination

static AudioClassID AudioObjectClass(AudioObjectID objectID)
{
	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= kAudioObjectPropertyClass,
		.mScope		= kAudioObjectPropertyScopeGlobal,
		.mElement	= kAudioObjectPropertyElementMaster
	};

	AudioClassID classID;
	UInt32 dataSize = sizeof(classID);
	OSStatus result = AudioObjectGetPropertyData(objectID, &propertyAddress, 0, NULL, &dataSize, &classID);
	if(kAudioHardwareNoError != result) {
		os_log_error(gSFBAudioObjectLog, "AudioObjectGetPropertyData (kAudioObjectPropertyClass) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
		return 0;
	}

	return classID;
}

static AudioClassID AudioObjectBaseClass(AudioObjectID objectID)
{
	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= kAudioObjectPropertyBaseClass,
		.mScope		= kAudioObjectPropertyScopeGlobal,
		.mElement	= kAudioObjectPropertyElementMaster
	};

	AudioClassID classID;
	UInt32 dataSize = sizeof(classID);
	OSStatus result = AudioObjectGetPropertyData(objectID, &propertyAddress, 0, NULL, &dataSize, &classID);
	if(kAudioHardwareNoError != result) {
		os_log_error(gSFBAudioObjectLog, "AudioObjectGetPropertyData (kAudioObjectPropertyBaseClass) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
		return 0;
	}

	return classID;
}

BOOL SFBAudioObjectIsPlugIn(AudioObjectID objectID)
{
	return AudioObjectClass(objectID) == kAudioPlugInClassID || AudioObjectBaseClass(objectID) == kAudioPlugInClassID;
}

BOOL SFBAudioObjectIsBox(AudioObjectID objectID)
{
	return AudioObjectClass(objectID) == kAudioBoxClassID || AudioObjectBaseClass(objectID) == kAudioBoxClassID;
}

BOOL SFBAudioObjectIsDevice(AudioObjectID objectID)
{
	return AudioObjectClass(objectID) == kAudioDeviceClassID || AudioObjectBaseClass(objectID) == kAudioDeviceClassID;
}

BOOL SFBAudioObjectIsClockDevice(AudioObjectID objectID)
{
	return AudioObjectClass(objectID) == kAudioClockDeviceClassID || AudioObjectBaseClass(objectID) == kAudioClockDeviceClassID;
}

BOOL SFBAudioObjectIsStream(AudioObjectID objectID)
{
	return AudioObjectClass(objectID) == kAudioStreamClassID || AudioObjectBaseClass(objectID) == kAudioStreamClassID;
}

BOOL SFBAudioObjectIsControl(AudioObjectID objectID)
{
	return AudioObjectClass(objectID) == kAudioControlClassID || AudioObjectBaseClass(objectID) == kAudioControlClassID;
}

#pragma mark - Audio PlugIn Information

BOOL SFBAudioPlugInIsTransportManager(AudioObjectID objectID)
{
	return AudioObjectClass(objectID) == kAudioTransportManagerClassID;
}

#pragma mark - Audio Device Information

BOOL SFBAudioDeviceIsAggregate(AudioObjectID objectID)
{
	return AudioObjectClass(objectID) == kAudioAggregateDeviceClassID;
}

BOOL SFBAudioDeviceIsSubdevice(AudioObjectID objectID)
{
	return AudioObjectClass(objectID) == kAudioSubDeviceClassID;
}

BOOL SFBAudioDeviceIsEndpointDevice(AudioObjectID objectID)
{
	return AudioObjectClass(objectID) == kAudioEndPointDeviceClassID;
}

BOOL SFBAudioDeviceIsEndpoint(AudioObjectID objectID)
{
	return AudioObjectClass(objectID) == kAudioEndPointClassID;
}

static BOOL AudioDeviceHasBuffersInScope(AudioObjectID deviceID, AudioObjectPropertyScope scope)
{
	NSCParameterAssert(deviceID != kAudioObjectUnknown);

	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= kAudioDevicePropertyStreamConfiguration,
		.mScope		= scope,
		.mElement	= kAudioObjectPropertyElementMaster
	};

	UInt32 dataSize = 0;
	OSStatus result = AudioObjectGetPropertyDataSize(deviceID, &propertyAddress, 0, NULL, &dataSize);
	if(result != kAudioHardwareNoError) {
		os_log_error(gSFBAudioObjectLog, "AudioObjectGetPropertyDataSize (kAudioDevicePropertyStreamConfiguration) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
		return NO;
	}

	AudioBufferList *bufferList = malloc(dataSize);
	if(!bufferList) {
		os_log_error(gSFBAudioObjectLog, "Unable to allocate memory");
		return NO;
	}

	result = AudioObjectGetPropertyData(deviceID, &propertyAddress, 0, NULL, &dataSize, bufferList);
	if(result != kAudioHardwareNoError) {
		os_log_error(gSFBAudioObjectLog, "AudioObjectGetPropertyData (kAudioDevicePropertyStreamConfiguration) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
		free(bufferList);
		return NO;
	}

	BOOL supportsScope = bufferList->mNumberBuffers > 0;
	free(bufferList);

	return supportsScope;
}

BOOL SFBAudioDeviceSupportsInput(AudioObjectID deviceID)
{
	return AudioDeviceHasBuffersInScope(deviceID, kAudioObjectPropertyScopeInput);
}

BOOL SFBAudioDeviceSupportsOutput(AudioObjectID deviceID)
{
	return AudioDeviceHasBuffersInScope(deviceID, kAudioObjectPropertyScopeOutput);
}

#pragma mark - Property Support

BOOL SFBUInt32ForProperty(AudioObjectID objectID, AudioObjectPropertyAddress *propertyAddress, UInt32 *value)
{
	NSCParameterAssert(objectID != kAudioObjectUnknown);
	NSCParameterAssert(propertyAddress != NULL);
	NSCParameterAssert(value != NULL);

	UInt32 dataSize = sizeof(*value);
	OSStatus result = AudioObjectGetPropertyData(objectID, propertyAddress, 0, NULL, &dataSize, value);
	if(kAudioHardwareNoError != result) {
		os_log_error(gSFBAudioObjectLog, "AudioObjectGetPropertyData (0x%x, '%{public}.4s', '%{public}.4s', %u) failed: %d '%{public}.4s'", objectID, SFBCStringForOSType(propertyAddress->mSelector), SFBCStringForOSType(propertyAddress->mScope), propertyAddress->mElement, result, SFBCStringForOSType(result));
		return NO;
	}

	return YES;
}

BOOL SFBFloat64ForProperty(AudioObjectID objectID, AudioObjectPropertyAddress *propertyAddress, Float64 *value)
{
	NSCParameterAssert(objectID != kAudioObjectUnknown);
	NSCParameterAssert(propertyAddress != NULL);
	NSCParameterAssert(value != NULL);

	UInt32 dataSize = sizeof(*value);
	OSStatus result = AudioObjectGetPropertyData(objectID, propertyAddress, 0, NULL, &dataSize, value);
	if(kAudioHardwareNoError != result) {
		os_log_error(gSFBAudioObjectLog, "AudioObjectGetPropertyData (0x%x, '%{public}.4s', '%{public}.4s', %u) failed: %d '%{public}.4s'", objectID, SFBCStringForOSType(propertyAddress->mSelector), SFBCStringForOSType(propertyAddress->mScope), propertyAddress->mElement, result, SFBCStringForOSType(result));
		return NO;
	}

	return YES;
}

NSString * SFBStringForProperty(AudioObjectID objectID, AudioObjectPropertyAddress *propertyAddress)
{
	NSCParameterAssert(objectID != kAudioObjectUnknown);
	NSCParameterAssert(propertyAddress != NULL);

	CFStringRef string = NULL;
	UInt32 dataSize = sizeof(string);
	OSStatus result = AudioObjectGetPropertyData(objectID, propertyAddress, 0, NULL, &dataSize, &string);
	if(result != kAudioHardwareNoError) {
		os_log_error(gSFBAudioObjectLog, "AudioObjectGetPropertyData (0x%x, '%{public}.4s', '%{public}.4s', %u) failed: %d '%{public}.4s'", objectID, SFBCStringForOSType(propertyAddress->mSelector), SFBCStringForOSType(propertyAddress->mScope), propertyAddress->mElement, result, SFBCStringForOSType(result));
		return nil;
	}

	return (__bridge_transfer NSString *)string;
}

NSDictionary * SFBDictionaryForProperty(AudioObjectID objectID, AudioObjectPropertyAddress *propertyAddress)
{
	NSCParameterAssert(objectID != kAudioObjectUnknown);
	NSCParameterAssert(propertyAddress != NULL);

	CFDictionaryRef dictionary = NULL;
	UInt32 dataSize = sizeof(dictionary);
	OSStatus result = AudioObjectGetPropertyData(objectID, propertyAddress, 0, NULL, &dataSize, &dictionary);
	if(result != kAudioHardwareNoError) {
		os_log_error(gSFBAudioObjectLog, "AudioObjectGetPropertyData (0x%x, '%{public}.4s', '%{public}.4s', %u) failed: %d '%{public}.4s'", objectID, SFBCStringForOSType(propertyAddress->mSelector), SFBCStringForOSType(propertyAddress->mScope), propertyAddress->mElement, result, SFBCStringForOSType(result));
		return nil;
	}

	return (__bridge_transfer NSDictionary *)dictionary;
}

SFBAudioObject * SFBAudioObjectForProperty(AudioObjectID objectID, AudioObjectPropertyAddress *propertyAddress)
{
	NSCParameterAssert(objectID != kAudioObjectUnknown);
	NSCParameterAssert(propertyAddress != NULL);

	AudioObjectID propertyObjectID = kAudioObjectUnknown;
	UInt32 specifierSize = sizeof(propertyObjectID);
	OSStatus result = AudioObjectGetPropertyData(objectID, propertyAddress, 0, NULL, &specifierSize, &propertyObjectID);
	if(result != kAudioHardwareNoError) {
		os_log_error(gSFBAudioObjectLog, "AudioObjectGetPropertyData (0x%x, '%{public}.4s', '%{public}.4s', %u) failed: %d '%{public}.4s'", objectID, SFBCStringForOSType(propertyAddress->mSelector), SFBCStringForOSType(propertyAddress->mScope), propertyAddress->mElement, result, SFBCStringForOSType(result));
		return nil;
	}

	return [[SFBAudioObject alloc] initWithAudioObjectID:propertyObjectID];
}

NSArray <SFBAudioObject *> * SFBAudioObjectArrayForProperty(AudioObjectID objectID, AudioObjectPropertyAddress *propertyAddress)
{
	NSCParameterAssert(objectID != kAudioObjectUnknown);
	NSCParameterAssert(propertyAddress != NULL);

	UInt32 dataSize = 0;
	OSStatus result = AudioObjectGetPropertyDataSize(objectID, propertyAddress, 0, NULL, &dataSize);
	if(result != kAudioHardwareNoError) {
		os_log_error(gSFBAudioObjectLog, "AudioObjectGetPropertyDataSize (0x%x, '%{public}.4s', '%{public}.4s', %u) failed: %d '%{public}.4s'", objectID, SFBCStringForOSType(propertyAddress->mSelector), SFBCStringForOSType(propertyAddress->mScope), propertyAddress->mElement, result, SFBCStringForOSType(result));
		return nil;
	}

	AudioObjectID *objectIDs = (AudioObjectID *)malloc(dataSize);
	if(!objectIDs) {
		os_log_error(gSFBAudioObjectLog, "Unable to allocate memory");
		return nil;
	}

	result = AudioObjectGetPropertyData(objectID, propertyAddress, 0, NULL, &dataSize, objectIDs);
	if(kAudioHardwareNoError != result) {
		os_log_error(gSFBAudioObjectLog, "AudioObjectGetPropertyData (0x%x, '%{public}.4s', '%{public}.4s', %u) failed: %d '%{public}.4s'", objectID, SFBCStringForOSType(propertyAddress->mSelector), SFBCStringForOSType(propertyAddress->mScope), propertyAddress->mElement, result, SFBCStringForOSType(result));
		free(objectIDs);
		return nil;
	}

	NSMutableArray *objects = [NSMutableArray array];
	for(NSInteger i = 0; i < (NSInteger)(dataSize / sizeof(AudioObjectID)); ++i)
		[objects addObject:[[SFBAudioObject alloc] initWithAudioObjectID:objectIDs[i]]];

	free(objectIDs);

	return objects;
}

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
		AudioObjectPropertyAddress propertyAddress = {0};
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

- (BOOL)hasProperty:(AudioObjectPropertySelector)property
{
	return [self hasProperty:property inScope:kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster];
}

- (BOOL)hasProperty:(AudioObjectPropertySelector)property inScope:(SFBCAObjectPropertyScope)scope
{
	return [self hasProperty:property inScope:scope onElement:kAudioObjectPropertyElementMaster];
}

- (BOOL)hasProperty:(AudioObjectPropertySelector)property inScope:(SFBCAObjectPropertyScope)scope onElement:(AudioObjectPropertyElement)element
{
	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= property,
		.mScope		= scope,
		.mElement	= element
	};
	return (BOOL)AudioObjectHasProperty(_objectID, &propertyAddress);
}

- (BOOL)propertyIsSettable:(AudioObjectPropertySelector)property
{
	return [self propertyIsSettable:property inScope:kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster];
}

- (BOOL)propertyIsSettable:(AudioObjectPropertySelector)property inScope:(SFBCAObjectPropertyScope)scope
{
	return [self propertyIsSettable:property inScope:scope onElement:kAudioObjectPropertyElementMaster];
}

- (BOOL)propertyIsSettable:(AudioObjectPropertySelector)property inScope:(SFBCAObjectPropertyScope)scope onElement:(AudioObjectPropertyElement)element
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

- (NSString *)stringForProperty:(AudioObjectPropertySelector)property
{
	return [self stringForProperty:property inScope:kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster];
}

- (NSString *)stringForProperty:(AudioObjectPropertySelector)property inScope:(SFBCAObjectPropertyScope)scope
{
	return [self stringForProperty:property inScope:scope onElement:kAudioObjectPropertyElementMaster];
}

- (NSString *)stringForProperty:(AudioObjectPropertySelector)property inScope:(SFBCAObjectPropertyScope)scope onElement:(AudioObjectPropertyElement)element
{
	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= property,
		.mScope		= scope,
		.mElement	= element
	};
	return SFBStringForProperty(_objectID, &propertyAddress);
}

- (NSDictionary *)dictionaryForProperty:(AudioObjectPropertySelector)property
{
	return [self dictionaryForProperty:property inScope:kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster];
}

- (NSDictionary *)dictionaryForProperty:(AudioObjectPropertySelector)property inScope:(SFBCAObjectPropertyScope)scope
{
	return [self dictionaryForProperty:property inScope:scope onElement:kAudioObjectPropertyElementMaster];
}

- (NSDictionary *)dictionaryForProperty:(AudioObjectPropertySelector)property inScope:(SFBCAObjectPropertyScope)scope onElement:(AudioObjectPropertyElement)element
{
	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= property,
		.mScope		= scope,
		.mElement	= element
	};
	return SFBDictionaryForProperty(_objectID, &propertyAddress);
}

- (SFBAudioObject *)audioObjectForProperty:(AudioObjectPropertySelector)property
{
	return [self audioObjectForProperty:property inScope:kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster];
}

- (SFBAudioObject *)audioObjectForProperty:(AudioObjectPropertySelector)property  inScope:(SFBCAObjectPropertyScope)scope
{
	return [self audioObjectForProperty:property inScope:scope onElement:kAudioObjectPropertyElementMaster];
}

- (SFBAudioObject *)audioObjectForProperty:(AudioObjectPropertySelector)property inScope:(SFBCAObjectPropertyScope)scope onElement:(AudioObjectPropertyElement)element
{
	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= property,
		.mScope		= scope,
		.mElement	= element
	};
	return SFBAudioObjectForProperty(_objectID, &propertyAddress);
}

- (NSArray *)audioObjectsForProperty:(AudioObjectPropertySelector)property
{
	return [self audioObjectsForProperty:property inScope:kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster];
}

- (NSArray *)audioObjectsForProperty:(AudioObjectPropertySelector)property  inScope:(SFBCAObjectPropertyScope)scope
{
	return [self audioObjectsForProperty:property inScope:scope onElement:kAudioObjectPropertyElementMaster];
}

- (NSArray *)audioObjectsForProperty:(AudioObjectPropertySelector)property inScope:(SFBCAObjectPropertyScope)scope onElement:(AudioObjectPropertyElement)element
{
	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= property,
		.mScope		= scope,
		.mElement	= element
	};
	return SFBAudioObjectArrayForProperty(_objectID, &propertyAddress);
}

- (void)whenPropertyChanges:(AudioObjectPropertySelector)property performBlock:(dispatch_block_t)block
{
	[self whenProperty:property inScope:kAudioObjectPropertyScopeGlobal changesOnElement:kAudioObjectPropertyElementMaster performBlock:block];
}

- (void)whenProperty:(AudioObjectPropertySelector)property changesinScope:(SFBCAObjectPropertyScope)scope performBlock:(dispatch_block_t)block
{
	[self whenProperty:property inScope:scope changesOnElement:kAudioObjectPropertyElementMaster performBlock:block];
}

- (void)whenProperty:(AudioObjectPropertySelector)property inScope:(SFBCAObjectPropertyScope)scope changesOnElement:(AudioObjectPropertyElement)element performBlock:(dispatch_block_t)block
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

#pragma mark - Internal Methods

- (UInt32)uInt32ForProperty:(AudioObjectPropertySelector)property
{
	return [self uInt32ForProperty:property inScope:kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster];
}

- (UInt32)uInt32ForProperty:(AudioObjectPropertySelector)property inScope:(SFBCAObjectPropertyScope)scope
{
	return [self uInt32ForProperty:property inScope:scope onElement:kAudioObjectPropertyElementMaster];
}

- (UInt32)uInt32ForProperty:(AudioObjectPropertySelector)property inScope:(SFBCAObjectPropertyScope)scope onElement:(AudioObjectPropertyElement)element
{
	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= property,
		.mScope		= scope,
		.mElement	= element
	};
	UInt32 value;
	return SFBUInt32ForProperty(_objectID, &propertyAddress, &value) ? value : 0;
}

- (Float64)float64ForProperty:(AudioObjectPropertySelector)property
{
	return [self float64ForProperty:property inScope:kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster];
}

- (Float64)float64ForProperty:(AudioObjectPropertySelector)property inScope:(SFBCAObjectPropertyScope)scope
{
	return [self float64ForProperty:property inScope:scope onElement:kAudioObjectPropertyElementMaster];
}

- (Float64)float64ForProperty:(AudioObjectPropertySelector)property inScope:(SFBCAObjectPropertyScope)scope onElement:(AudioObjectPropertyElement)element
{
	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= property,
		.mScope		= scope,
		.mElement	= element
	};
	Float64 value;
	return SFBFloat64ForProperty(_objectID, &propertyAddress, &value) ? value : nan("1");
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
	return [self uInt32ForProperty:kAudioObjectPropertyBaseClass];
}

- (AudioClassID)classID
{
	return [self uInt32ForProperty:kAudioObjectPropertyClass];
}

- (NSString *)owner
{
	return [self stringForProperty:kAudioObjectPropertyOwner];
}

- (NSString *)name
{
	return [self stringForProperty:kAudioObjectPropertyName];
}

- (NSString *)modelName
{
	return [self stringForProperty:kAudioObjectPropertyModelName];
}

- (NSString *)manufacturer
{
	return [self stringForProperty:kAudioObjectPropertyManufacturer];
}

- (NSString *)nameOfElement:(AudioObjectPropertyElement)element
{
	return [self stringForProperty:kAudioObjectPropertyElementName inScope:kAudioObjectPropertyScopeGlobal onElement:element];
}

- (NSString *)nameOfElement:(AudioObjectPropertyElement)element inScope:(SFBCAObjectPropertyScope)scope
{
	return [self stringForProperty:kAudioObjectPropertyElementName inScope:scope onElement:element];
}

- (NSString *)categoryNameOfElement:(AudioObjectPropertyElement)element
{
	return [self stringForProperty:kAudioObjectPropertyElementCategoryName inScope:kAudioObjectPropertyScopeGlobal onElement:element];
}

- (NSString *)categoryNameOfElement:(AudioObjectPropertyElement)element inScope:(SFBCAObjectPropertyScope)scope
{
	return [self stringForProperty:kAudioObjectPropertyElementCategoryName inScope:scope onElement:element];
}

- (NSString *)numberNameOfElement:(AudioObjectPropertyElement)element
{
	return [self stringForProperty:kAudioObjectPropertyElementNumberName inScope:kAudioObjectPropertyScopeGlobal onElement:element];
}

- (NSString *)numberNameOfElement:(AudioObjectPropertyElement)element inScope:(SFBCAObjectPropertyScope)scope
{
	return [self stringForProperty:kAudioObjectPropertyElementNumberName inScope:scope onElement:element];
}

- (NSArray *)ownedObjects
{
	return [self audioObjectsForProperty:kAudioObjectPropertyOwnedObjects];
}

- (NSString *)serialNumber
{
	return [self stringForProperty:kAudioObjectPropertySerialNumber];
}

- (NSString *)firmwareVersion
{
	return [self stringForProperty:kAudioObjectPropertyFirmwareVersion];
}

@end
