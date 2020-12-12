/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <Foundation/Foundation.h>

/// Project version number for SFBAudioEngine.
FOUNDATION_EXPORT double SFBAudioEngineVersionNumber;

/// Project version string for SFBAudioEngine.
FOUNDATION_EXPORT const unsigned char SFBAudioEngineVersionString [];

#import <SFBAudioEngine/SFBAudioEngineTypes.h>

#import <SFBAudioEngine/SFBInputSource.h>

#import <SFBAudioEngine/SFBAudioDecoding.h>
#import <SFBAudioEngine/SFBPCMDecoding.h>
#import <SFBAudioEngine/SFBAudioDecoder.h>
#import <SFBAudioEngine/SFBDSDDecoding.h>
#import <SFBAudioEngine/SFBDSDDecoder.h>
#import <SFBAudioEngine/SFBDSDPCMDecoder.h>
#import <SFBAudioEngine/SFBDoPDecoder.h>
#import <SFBAudioEngine/SFBLoopableRegionDecoder.h>

#import <SFBAudioEngine/SFBOutputSource.h>

#import <SFBAudioEngine/SFBAudioEncoding.h>
#import <SFBAudioEngine/SFBPCMEncoding.h>
#import <SFBAudioEngine/SFBAudioEncoder.h>

#if TARGET_OS_OSX

#import <SFBAudioEngine/SFBAudioObject.h>
#import <SFBAudioEngine/SFBAudioDevice.h>
#import <SFBAudioEngine/SFBAudioDeviceDataSource.h>
#import <SFBAudioEngine/SFBAudioOutputDevice.h>
#import <SFBAudioEngine/SFBAggregateAudioDevice.h>
#import <SFBAudioEngine/SFBAudioBox.h>
#import <SFBAudioEngine/SFBAudioClockDevice.h>

#endif

#import <SFBAudioEngine/SFBAudioPlayerNode.h>
#import <SFBAudioEngine/SFBAudioPlayer.h>

#import <SFBAudioEngine/SFBAudioProperties.h>
#import <SFBAudioEngine/SFBAttachedPicture.h>
#import <SFBAudioEngine/SFBAudioMetadata.h>
#import <SFBAudioEngine/SFBAudioFile.h>

#import <SFBAudioEngine/SFBReplayGainAnalyzer.h>

#import <SFBAudioEngine/SFBAudioExporter.h>
#import <SFBAudioEngine/SFBAudioConverter.h>
