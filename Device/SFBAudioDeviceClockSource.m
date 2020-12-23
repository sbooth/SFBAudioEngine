/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import "SFBAudioDeviceClockSource.h"
#import "SFBAudioObject+Internal.h"

#import "SFBAudioDevice.h"
#import "SFBCStringForOSType.h"

@interface SFBAudioDeviceClockSource ()
{
@private
	SFBAudioDevice *_audioDevice;
	SFBAudioObjectPropertyScope _scope;
	UInt32 _clockSourceID;
}
@end

@implementation SFBAudioDeviceClockSource

- (instancetype)initWithAudioDevice:(SFBAudioDevice *)audioDevice scope:(SFBAudioObjectPropertyScope)scope clockSourceID:(UInt32)clockSourceID
{
	NSParameterAssert(audioDevice != nil);

	if((self = [super init])) {
		_audioDevice = audioDevice;
		_scope = scope;
		_clockSourceID = clockSourceID;
	}
	return self;
}

- (BOOL)isEqual:(id)object
{
	if(![object isKindOfClass:[SFBAudioDeviceClockSource class]])
		return NO;

	SFBAudioDeviceClockSource *other = (SFBAudioDeviceClockSource *)object;
	return [_audioDevice isEqual:other->_audioDevice] && _scope == other->_scope && _clockSourceID == other->_clockSourceID;
}

- (NSUInteger)hash
{
	return _audioDevice.hash ^ _scope ^ _clockSourceID;
}

- (NSString *)name
{
	return [_audioDevice nameOfClockSource:_clockSourceID inScope:_scope];
}

- (NSNumber *)kind
{
	return [_audioDevice kindOfClockSource:_clockSourceID inScope:_scope];
}

- (NSString *)description
{
	return [NSString stringWithFormat:@"<%@ '%s', '%s', \"%@\">", self.className, SFBCStringForOSType(_clockSourceID), SFBCStringForOSType(_scope), self.name];
}

@end
