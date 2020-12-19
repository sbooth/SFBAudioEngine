/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import "SFBAudioTransportManager.h"
#import "SFBAudioObject+Internal.h"

#import "SFBCStringForOSType.h"

@implementation SFBAudioTransportManager

+ (NSArray *)transportManagers
{
	return [[SFBAudioObject systemObject] audioObjectArrayForProperty:kAudioHardwarePropertyTransportManagerList inScope:kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster error:NULL];
}

- (instancetype)initWithAudioObjectID:(AudioObjectID)objectID
{
	NSParameterAssert(SFBAudioObjectIsClass(objectID, kAudioTransportManagerClassID));
	return [super initWithAudioObjectID:objectID];
}

- (instancetype)initWithBundleID:(NSString *)bundleID
{
	NSParameterAssert(bundleID != nil);

	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= kAudioHardwarePropertyTranslateBundleIDToTransportManager,
		.mScope		= kAudioObjectPropertyScopeGlobal,
		.mElement	= kAudioObjectPropertyElementMaster
	};

	AudioObjectID objectID = kAudioObjectUnknown;
	UInt32 specifierSize = sizeof(objectID);
	OSStatus result = AudioObjectGetPropertyData(kAudioObjectSystemObject, &propertyAddress, sizeof(bundleID), &bundleID, &specifierSize, &objectID);
	if(result != kAudioHardwareNoError) {
		os_log_error(gSFBAudioObjectLog, "AudioObjectGetPropertyData (kAudioHardwarePropertyTranslateBundleIDToTransportManager) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
		return nil;
	}

	if(objectID == kAudioObjectUnknown) {
		os_log_error(gSFBAudioObjectLog, "Unknown audio transport manager bundle ID: %{public}@", bundleID);
		return nil;
	}

	return [self initWithAudioObjectID:objectID];
}

- (NSArray *)endPoints
{
	return [self audioObjectArrayForProperty:kAudioTransportManagerPropertyEndPointList inScope:kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster error:NULL];
}

- (SFBAudioObject *)endpointForUID:(NSString *)endpointUID
{
	NSParameterAssert(endpointUID != nil);
	CFStringRef qualifier = (__bridge CFStringRef)endpointUID;
	return [self audioObjectForProperty:kAudioTransportManagerPropertyTranslateUIDToEndPoint inScope:kAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster qualifier:qualifier qualifierSize:sizeof(qualifier) error:NULL];
}

- (NSNumber *)transportType
{
	return [self unsignedIntForProperty:kAudioTransportManagerPropertyTransportType inScope:SFBAudioObjectPropertyScopeGlobal onElement:kAudioObjectPropertyElementMaster error:NULL];
}

@end
