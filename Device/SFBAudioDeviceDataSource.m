/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import "SFBAudioDeviceDataSource.h"
#import "SFBAudioObject+Internal.h"

#import "SFBAudioDevice.h"
#import "SFBCStringForOSType.h"

@interface SFBAudioDeviceDataSource ()
{
@private
	SFBAudioDevice *_audioDevice;
	SFBAudioObjectPropertyScope _scope;
	UInt32 _dataSourceID;
}
@end

@implementation SFBAudioDeviceDataSource

- (instancetype)initWithAudioDevice:(SFBAudioDevice *)audioDevice scope:(SFBAudioObjectPropertyScope)scope dataSourceID:(UInt32)dataSourceID
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
	return [_audioDevice nameOfDataSource:_dataSourceID inScope:_scope];
}

- (NSNumber *)kind
{
	return [_audioDevice kindOfDataSource:_dataSourceID inScope:_scope];
}

- (NSString *)description
{
	return [NSString stringWithFormat:@"<SFBAudioDeviceDataSource '%s', '%s', \"%@\">", SFBCStringForOSType(_dataSourceID), SFBCStringForOSType(_scope), self.name];
}

@end
