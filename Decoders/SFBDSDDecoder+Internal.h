//
// Copyright (c) 2014 - 2023 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import "SFBDSDDecoder.h"

NS_ASSUME_NONNULL_BEGIN

extern os_log_t gSFBDSDDecoderLog;

@interface SFBDSDDecoder ()
{
@package
	SFBInputSource *_inputSource;
@protected
	AVAudioFormat *_sourceFormat;
	AVAudioFormat *_processingFormat;
	NSDictionary *_properties;
}
/// Returns the decoder name
@property (class, nonatomic, readonly) SFBDSDDecoderName decoderName;
@end

#pragma mark - Subclass Registration and Lookup

@interface SFBDSDDecoder (SFBDSDDecoderSubclassRegistration)
/// Register a subclass with the default priority (`0`)
+ (void)registerSubclass:(Class)subclass;
/// Register a subclass with the specified priority
+ (void)registerSubclass:(Class)subclass priority:(int)priority;
@end

@interface SFBDSDDecoder (SFBDSDDecoderSubclassLookup)
/// Returns the appropriate `SFBDSDDecoder` subclass for decoding `url`
+ (nullable Class)subclassForURL:(NSURL *)url;
/// Returns the appropriate `SFBDSDDecoder` subclass for decoding paths with `extension`
+ (nullable Class)subclassForPathExtension:(NSString *)extension;
/// Returns the appropriate `SFBDSDDecoder` subclass for decoding data of `mimeType`
+ (nullable Class)subclassForMIMEType:(NSString *)mimeType;
/// Returns the appropriate `SFBDSDDecoder` subclass corresponding to `decoderName`
+ (nullable Class)subclassForDecoderName:(SFBDSDDecoderName)decoderName;
@end

NS_ASSUME_NONNULL_END
