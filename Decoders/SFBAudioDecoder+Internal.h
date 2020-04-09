/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import "SFBAudioDecoder.h"

#define SFB_max(a,b) ({ __typeof__ (a) _a = (a); __typeof__ (b) _b = (b); _a > _b ? _a : _b; })
#define SFB_min(a,b) ({ __typeof__ (a) _a = (a); __typeof__ (b) _b = (b); _a < _b ? _a : _b; })

NS_ASSUME_NONNULL_BEGIN

@interface SFBAudioDecoder ()
{
@package
	SFBInputSource *_inputSource;
	SFBAudioFormat *_sourceFormat;
	SFBAudioFormat *_processingFormat;
	NSInteger _currentFrame;
	NSInteger _totalFrames;
}
@property (nonatomic) SFBAudioFormat *sourceFormat;
@property (nonatomic) SFBAudioFormat *processingFormat;
@end

@interface SFBAudioDecoderSubclassInfo : NSObject
@property (nonatomic) Class klass;
@property (nonatomic) int priority;
@end

@interface SFBAudioDecoder (SFBAudioDecoderSubclassLookup)
+ (nullable Class)subclassForURL:(NSURL *)url;
+ (nullable Class)subclassForPathExtension:(NSString *)extension;
+ (nullable Class)subclassForMIMEType:(NSString *)mimeType;
@end

NS_ASSUME_NONNULL_END
