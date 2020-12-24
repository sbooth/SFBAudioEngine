/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <SFBAudioEngine/SFBAudioControl.h>

NS_ASSUME_NONNULL_BEGIN

/// An audio boolean control
NS_SWIFT_NAME(BooleanControl) @interface SFBBooleanControl : SFBAudioControl

/// Returns the control's value or \c nil on error
/// @note This corresponds to \c kAudioBooleanControlPropertyValue
@property (nonatomic, nullable, readonly) NSNumber *value NS_REFINED_FOR_SWIFT;
/// Sets the control's value
/// @note This corresponds to \c kAudioBooleanControlPropertyValue
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return \c YES if successful
- (BOOL)setValue:(BOOL)value error:(NSError **)error NS_SWIFT_NAME(setValue(_:));

@end

/// An audio mute control
NS_SWIFT_NAME(MuteControl) @interface SFBMuteControl : SFBBooleanControl
@end

/// An audio solo control
NS_SWIFT_NAME(SoloControl) @interface SFBSoloControl : SFBBooleanControl
@end

/// An audio jack control
NS_SWIFT_NAME(JackControl) @interface SFBJackControl : SFBBooleanControl
@end

/// An audio LFE mute control
NS_SWIFT_NAME(LFEMuteControl) @interface SFBLFEMuteControl : SFBBooleanControl
@end

/// An audio phantom power control
NS_SWIFT_NAME(PhantomPowerControl) @interface SFBPhantomPowerControl : SFBBooleanControl
@end

/// An audio phase invert control
NS_SWIFT_NAME(PhaseInvertControl) @interface SFBPhaseInvertControl : SFBBooleanControl
@end

/// An audio clip light control
NS_SWIFT_NAME(ClipLightControl) @interface SFBClipLightControl : SFBBooleanControl
@end

/// An audio talkback control
NS_SWIFT_NAME(TalkbackControl) @interface SFBTalkbackControl : SFBBooleanControl
@end

/// An audio listenback control
NS_SWIFT_NAME(ListenbackControl) @interface SFBListenbackControl : SFBBooleanControl
@end

NS_ASSUME_NONNULL_END
