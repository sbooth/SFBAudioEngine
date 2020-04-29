/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <os/log.h>

#import "SFBAudioDeviceDataSource.h"

#import "SFBAudioDevice.h"
#import "SFBCStringForOSType.h"

extern os_log_t gSFBAudioDeviceLog;

@interface SFBAudioDeviceDataSource ()
{
@private
	SFBAudioDevice *_audioDevice;
	AudioObjectPropertyScope _scope;
	UInt32 _dataSourceID;
}
@end

@implementation SFBAudioDeviceDataSource

- (instancetype)initWithAudioDevice:(SFBAudioDevice *)audioDevice scope:(AudioObjectPropertyScope)scope dataSourceID:(UInt32)dataSourceID
{
	NSParameterAssert(audioDevice != nil);

	if((self = [super init])) {
		_audioDevice = audioDevice;
		_scope = scope;
		_dataSourceID = dataSourceID;
	}
	return self;
}

- (BOOL)isEqual:(id)object
{
	if(![object isKindOfClass:[SFBAudioDeviceDataSource class]])
		return NO;

	SFBAudioDeviceDataSource *other = (SFBAudioDeviceDataSource *)object;
	return [_audioDevice isEqual:other->_audioDevice] && _scope == other->_scope && _dataSourceID == other->_dataSourceID;
}

- (NSUInteger)hash
{
	return _audioDevice.hash ^ _scope ^ _dataSourceID;
}

- (NSString *)name
{
	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= kAudioDevicePropertyDataSourceNameForIDCFString,
		.mScope		= _scope,
		.mElement	= kAudioObjectPropertyElementMaster
	};

	CFStringRef dataSourceName = NULL;
	AudioValueTranslation translation = {
		.mInputData			= &_dataSourceID,
		.mInputDataSize		= sizeof(_dataSourceID),
		.mOutputData		= &dataSourceName,
		.mOutputDataSize	= sizeof(dataSourceName)
	};

	UInt32 dataSize = sizeof(translation);
	OSStatus result = AudioObjectGetPropertyData(_audioDevice.deviceID, &propertyAddress, 0, NULL, &dataSize, &translation);
	if(result != kAudioHardwareNoError) {
		os_log_error(gSFBAudioDeviceLog, "AudioObjectGetPropertyData (kAudioDevicePropertyDataSourceNameForIDCFString) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
		return nil;
	}

	return (__bridge NSString *)dataSourceName;
}

- (UInt32)kind
{
	AudioObjectPropertyAddress propertyAddress = {
		.mSelector	= kAudioDevicePropertyDataSourceKindForID,
		.mScope		= _scope,
		.mElement	= kAudioObjectPropertyElementMaster
	};

	UInt32 dataSourceKind;
	AudioValueTranslation translation = {
		.mInputData			= &_dataSourceID,
		.mInputDataSize		= sizeof(_dataSourceID),
		.mOutputData		= &dataSourceKind,
		.mOutputDataSize	= sizeof(dataSourceKind)
	};

	UInt32 dataSize = sizeof(translation);
	OSStatus result = AudioObjectGetPropertyData(_audioDevice.deviceID, &propertyAddress, 0, NULL, &dataSize, &translation);
	if(result != kAudioHardwareNoError) {
		os_log_error(gSFBAudioDeviceLog, "AudioObjectGetPropertyData (kAudioDevicePropertyDataSourceKindForID) failed: %d '%{public}.4s'", result, SFBCStringForOSType(result));
		return kAudioObjectUnknown;
	}

	return dataSourceKind;

}

- (NSString *)description
{
	return [NSString stringWithFormat:@"<SFBAudioDeviceDataSource '%s', '%s', \"%@\">", SFBCStringForOSType(_dataSourceID), SFBCStringForOSType(_scope), self.name];
}

@end
