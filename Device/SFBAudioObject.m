/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

@import os.log;

#import "SFBAudioObject+Internal.h"

#import "SFBAudioBox.h"
#import "SFBAudioClockDevice.h"
#import "SFBAudioDevice.h"
#import "SFBAggregateAudioDevice.h"
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

BOOL SFBAudioObjectIsDevice(AudioObjectID objectID)
{
	return AudioObjectClass(objectID) == kAudioDeviceClassID || AudioObjectBaseClass(objectID) == kAudioDeviceClassID;
}

BOOL SFBAudioObjectIsBox(AudioObjectID objectID)
{
	return AudioObjectClass(objectID) == kAudioBoxClassID || AudioObjectBaseClass(objectID) == kAudioBoxClassID;
}

BOOL SFBAudioObjectIsClockDevice(AudioObjectID objectID)
{
	return AudioObjectClass(objectID) == kAudioClockDeviceClassID || AudioObjectBaseClass(objectID) == kAudioClockDeviceClassID;
}

#pragma mark - Audio Device Information

BOOL SFBAudioDeviceIsAggregate(AudioObjectID objectID)
{
	return AudioObjectClass(objectID) == kAudioAggregateDeviceClassID;
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

+ (instancetype)audioObjectWithID:(AudioObjectID)objectID
{
	if(objectID == kAudioObjectUnknown)
		return nil;

	AudioClassID classID = AudioObjectClass(objectID);
	switch(classID) {
		case kAudioAggregateDeviceClassID:
			return [[SFBAggregateAudioDevice alloc] initWithAudioObjectID:objectID];
		case kAudioDeviceClassID:
			return [[SFBAudioDevice alloc] initWithAudioObjectID:objectID];
		case kAudioBoxClassID:
			return [[SFBAudioBox alloc] initWithAudioObjectID:objectID];
		case kAudioClockDeviceClassID:
			return [[SFBAudioClockDevice alloc] initWithAudioObjectID:objectID];
		default:
			return [[SFBAudioObject alloc] initWithAudioObjectID:objectID];
	}
}

- (instancetype)initWithAudioObjectID:(AudioObjectID)objectID
{
	NSParameterAssert(objectID != kAudioObjectUnknown);

	if(objectID == kAudioObjectSystemObject)
		return [SFBAudioObject systemObject];

	if((self = [super init])) {
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

- (AudioClassID)baseClassID
{
	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= kAudioObjectPropertyBaseClass,
		.mScope		= kAudioObjectPropertyScopeGlobal,
		.mElement	= kAudioObjectPropertyElementMaster
	};

	AudioClassID baseClassID;
	UInt32 dataSize = sizeof(baseClassID);
	OSStatus result = AudioObjectGetPropertyData(_objectID, &propertyAddress, 0, NULL, &dataSize, &baseClassID);
	if(kAudioHardwareNoError != result) {
		os_log_error(gSFBAudioObjectLog, "AudioObjectGetPropertyData (kAudioObjectPropertyBaseClass) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
		return 0;
	}

	return baseClassID;
}

- (AudioClassID)classID
{
	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= kAudioObjectPropertyClass,
		.mScope		= kAudioObjectPropertyScopeGlobal,
		.mElement	= kAudioObjectPropertyElementMaster
	};

	AudioClassID classID;
	UInt32 dataSize = sizeof(classID);
	OSStatus result = AudioObjectGetPropertyData(_objectID, &propertyAddress, 0, NULL, &dataSize, &classID);
	if(kAudioHardwareNoError != result) {
		os_log_error(gSFBAudioObjectLog, "AudioObjectGetPropertyData (kAudioObjectPropertyClass) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
		return 0;
	}

	return classID;
}

- (NSString *)owner
{
	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= kAudioObjectPropertyOwner,
		.mScope		= kAudioObjectPropertyScopeGlobal,
		.mElement	= kAudioObjectPropertyElementMaster
	};

	CFStringRef owner = NULL;
	UInt32 dataSize = sizeof(owner);
	OSStatus result = AudioObjectGetPropertyData(_objectID, &propertyAddress, 0, NULL, &dataSize, &owner);
	if(result != kAudioHardwareNoError) {
		os_log_error(gSFBAudioObjectLog, "AudioObjectGetPropertyData (kAudioObjectPropertyOwner) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
		return nil;
	}

	return (__bridge_transfer NSString *)owner;
}

- (NSString *)name
{
	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= kAudioObjectPropertyName,
		.mScope		= kAudioObjectPropertyScopeGlobal,
		.mElement	= kAudioObjectPropertyElementMaster
	};

	CFStringRef name = NULL;
	UInt32 dataSize = sizeof(name);
	OSStatus result = AudioObjectGetPropertyData(_objectID, &propertyAddress, 0, NULL, &dataSize, &name);
	if(result != kAudioHardwareNoError) {
		os_log_error(gSFBAudioObjectLog, "AudioObjectGetPropertyData (kAudioObjectPropertyName) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
		return nil;
	}

	return (__bridge_transfer NSString *)name;
}

- (NSString *)modelName
{
	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= kAudioObjectPropertyModelName,
		.mScope		= kAudioObjectPropertyScopeGlobal,
		.mElement	= kAudioObjectPropertyElementMaster
	};

	CFStringRef modelName = NULL;
	UInt32 dataSize = sizeof(modelName);
	OSStatus result = AudioObjectGetPropertyData(_objectID, &propertyAddress, 0, NULL, &dataSize, &modelName);
	if(result != kAudioHardwareNoError) {
		os_log_error(gSFBAudioObjectLog, "AudioObjectGetPropertyData (kAudioObjectPropertyModelName) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
		return nil;
	}

	return (__bridge_transfer NSString *)modelName;
}

- (NSString *)manufacturer
{
	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= kAudioObjectPropertyManufacturer,
		.mScope		= kAudioObjectPropertyScopeGlobal,
		.mElement	= kAudioObjectPropertyElementMaster
	};

	CFStringRef manufacturer = NULL;
	UInt32 dataSize = sizeof(manufacturer);
	OSStatus result = AudioObjectGetPropertyData(_objectID, &propertyAddress, 0, NULL, &dataSize, &manufacturer);
	if(result != kAudioHardwareNoError) {
		os_log_error(gSFBAudioObjectLog, "AudioObjectGetPropertyData (kAudioObjectPropertyManufacturer) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
		return nil;
	}

	return (__bridge_transfer NSString *)manufacturer;
}

- (NSString *)elementName
{
	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= kAudioObjectPropertyElementName,
		.mScope		= kAudioObjectPropertyScopeGlobal,
		.mElement	= kAudioObjectPropertyElementMaster
	};

	CFStringRef elementName = NULL;
	UInt32 dataSize = sizeof(elementName);
	OSStatus result = AudioObjectGetPropertyData(_objectID, &propertyAddress, 0, NULL, &dataSize, &elementName);
	if(result != kAudioHardwareNoError) {
		os_log_error(gSFBAudioObjectLog, "AudioObjectGetPropertyData (kAudioObjectPropertyElementName) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
		return nil;
	}

	return (__bridge_transfer NSString *)elementName;
}

- (NSString *)elementCategoryName
{
	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= kAudioObjectPropertyElementCategoryName,
		.mScope		= kAudioObjectPropertyScopeGlobal,
		.mElement	= kAudioObjectPropertyElementMaster
	};

	CFStringRef elementCategoryName = NULL;
	UInt32 dataSize = sizeof(elementCategoryName);
	OSStatus result = AudioObjectGetPropertyData(_objectID, &propertyAddress, 0, NULL, &dataSize, &elementCategoryName);
	if(result != kAudioHardwareNoError) {
		os_log_error(gSFBAudioObjectLog, "AudioObjectGetPropertyData (kAudioObjectPropertyElementCategoryName) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
		return nil;
	}

	return (__bridge_transfer NSString *)elementCategoryName;
}

- (NSString *)elementNumberName
{
	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= kAudioObjectPropertyElementNumberName,
		.mScope		= kAudioObjectPropertyScopeGlobal,
		.mElement	= kAudioObjectPropertyElementMaster
	};

	CFStringRef elementNumberName = NULL;
	UInt32 dataSize = sizeof(elementNumberName);
	OSStatus result = AudioObjectGetPropertyData(_objectID, &propertyAddress, 0, NULL, &dataSize, &elementNumberName);
	if(result != kAudioHardwareNoError) {
		os_log_error(gSFBAudioObjectLog, "AudioObjectGetPropertyData (kAudioObjectPropertyElementNumberName) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
		return nil;
	}

	return (__bridge_transfer NSString *)elementNumberName;
}

- (NSArray *)ownedObjects
{
	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= kAudioObjectPropertyOwnedObjects,
		.mScope		= kAudioObjectPropertyScopeGlobal,
		.mElement	= kAudioObjectPropertyElementMaster
	};

	UInt32 dataSize = 0;
	OSStatus result = AudioObjectGetPropertyDataSize(_objectID, &propertyAddress, 0, NULL, &dataSize);
	if(result != kAudioHardwareNoError) {
		os_log_error(gSFBAudioObjectLog, "AudioObjectGetPropertyDataSize (kAudioObjectPropertyOwnedObjects) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
		return nil;
	}

	AudioObjectID *objectIDs = (AudioObjectID *)malloc(dataSize);
	if(!objectIDs) {
		os_log_error(gSFBAudioObjectLog, "Unable to allocate memory");
		return nil;
	}

	result = AudioObjectGetPropertyData(kAudioObjectSystemObject, &propertyAddress, 0, NULL, &dataSize, objectIDs);
	if(kAudioHardwareNoError != result) {
		os_log_error(gSFBAudioObjectLog, "AudioObjectGetPropertyData (kAudioObjectPropertyOwnedObjects) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
		free(objectIDs);
		return nil;
	}

	NSMutableArray *objects = [NSMutableArray array];
	for(NSInteger i = 0; i < (NSInteger)(dataSize / sizeof(AudioObjectID)); ++i) {
		SFBAudioObject *object = [SFBAudioObject audioObjectWithID:objectIDs[i]];
		if(object)
			[objects addObject:object];
	}

	free(objectIDs);

	return objects;
}

- (NSString *)serialNumber
{
	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= kAudioObjectPropertySerialNumber,
		.mScope		= kAudioObjectPropertyScopeGlobal,
		.mElement	= kAudioObjectPropertyElementMaster
	};

	CFStringRef serialNumber = NULL;
	UInt32 dataSize = sizeof(serialNumber);
	OSStatus result = AudioObjectGetPropertyData(_objectID, &propertyAddress, 0, NULL, &dataSize, &serialNumber);
	if(result != kAudioHardwareNoError) {
		os_log_error(gSFBAudioObjectLog, "AudioObjectGetPropertyData (kAudioObjectPropertySerialNumber) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
		return nil;
	}

	return (__bridge_transfer NSString *)serialNumber;
}

- (NSString *)firmwareVersion
{
	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= kAudioObjectPropertyFirmwareVersion,
		.mScope		= kAudioObjectPropertyScopeGlobal,
		.mElement	= kAudioObjectPropertyElementMaster
	};

	CFStringRef firmwareVersion = NULL;
	UInt32 dataSize = sizeof(firmwareVersion);
	OSStatus result = AudioObjectGetPropertyData(_objectID, &propertyAddress, 0, NULL, &dataSize, &firmwareVersion);
	if(result != kAudioHardwareNoError) {
		os_log_error(gSFBAudioObjectLog, "AudioObjectGetPropertyData (kAudioObjectPropertyFirmwareVersion) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
		return nil;
	}

	return (__bridge_transfer NSString *)firmwareVersion;
}

#pragma mark - Audio Object Property Observation

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

	return (BOOL)AudioObjectHasProperty(_objectID, &propertyAddress);
}

- (BOOL)propertyIsSettable:(AudioObjectPropertySelector)property
{
	return [self propertyIsSettable:property inScope:kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster];
}

- (BOOL)propertyIsSettable:(AudioObjectPropertySelector)property inScope:(AudioObjectPropertyScope)scope
{
	return [self propertyIsSettable:property inScope:scope onElement:kAudioObjectPropertyElementMaster];
}

- (BOOL)propertyIsSettable:(AudioObjectPropertySelector)property inScope:(AudioObjectPropertyScope)scope onElement:(AudioObjectPropertyElement)element
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
