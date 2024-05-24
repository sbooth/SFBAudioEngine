//
// Copyright (c) 2006 - 2023 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import <os/log.h>

#import "SFBAudioDecoder.h"

NS_ASSUME_NONNULL_BEGIN

extern os_log_t gSFBAudioDecoderLog;

@interface SFBAudioDecoder ()
{
@package
	SFBInputSource *_inputSource;
@protected
	AVAudioFormat *_sourceFormat;
	AVAudioFormat *_processingFormat;
	NSDictionary *_properties;
}
/// Returns the decoder name
@property (class, nonatomic, readonly) SFBAudioDecoderName decoderName;
@end

#pragma mark - Subclass Registration and Lookup

@interface SFBAudioDecoder (SFBAudioDecoderSubclassRegistration)
/// Register a subclass with the default priority (`0`)
+ (void)registerSubclass:(Class)subclass;
/// Register a subclass with the specified priority
+ (void)registerSubclass:(Class)subclass priority:(int)priority;
@end

@interface SFBAudioDecoder (SFBAudioDecoderSubclassLookup)
/// Returns the appropriate `SFBAudioDecoder` subclass for decoding `url`
+ (nullable Class)subclassForURL:(NSURL *)url;
/// Returns the appropriate `SFBAudioDecoder` subclass for decoding paths with `extension`
+ (nullable Class)subclassForPathExtension:(NSString *)extension;
/// Returns the appropriate `SFBAudioDecoder` subclass for decoding data of `mimeType`
+ (nullable Class)subclassForMIMEType:(NSString *)mimeType;
/// Returns the appropriate `SFBAudioDecoder` subclass corresponding to `decoderName`
+ (nullable Class)subclassForDecoderName:(SFBAudioDecoderName)decoderName;
@end

NS_ASSUME_NONNULL_END
