/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <SFBAudioEngine/SFBAudioControl.h>

NS_ASSUME_NONNULL_BEGIN

/// An audio selector control
NS_SWIFT_NAME(SelectorControl) @interface SFBSelectorControl : SFBAudioControl

/// Returns the selected items or \c nil on error
/// @note This corresponds to \c kAudioSelectorControlPropertyCurrentItem
@property (nonatomic, nullable, readonly) NSArray <NSNumber *> *currentItem NS_REFINED_FOR_SWIFT;
/// Sets the selected items
/// @note This corresponds to \c kAudioSelectorControlPropertyCurrentItem
/// @param error An optional pointer to an \c NSError object to receive error information
/// @return \c YES if successful
- (BOOL)setCurrentItem:(NSArray <NSNumber *> *)values error:(NSError **)error NS_REFINED_FOR_SWIFT;

/// Returns the available items or \c nil on error
/// @note This corresponds to \c kAudioSelectorControlPropertyAvailableItems
@property (nonatomic, nullable, readonly) NSArray <NSNumber *> *availableItems NS_REFINED_FOR_SWIFT;

/// Returns the item's name or \c nil on error
/// @note This corresponds to \c kAudioSelectorControlPropertyItemName
- (nullable NSString *)nameOfItem:(UInt32)itemID NS_REFINED_FOR_SWIFT;
/// Returns the item's name or \c nil on error
/// @note This corresponds to \c kAudioSelectorControlPropertyItemKind
- (nullable NSString *)kindOfItem:(UInt32)itemID NS_REFINED_FOR_SWIFT;

@end

NS_ASSUME_NONNULL_END

/// An audio data source control
NS_SWIFT_NAME(DataSourceControl) @interface SFBDataSourceControl : SFBSelectorControl
@end

/// An audio data destination control
NS_SWIFT_NAME(DataDestinationControl) @interface SFBDataDestinationControl : SFBSelectorControl
@end

/// An audio clock source control
NS_SWIFT_NAME(ClockSourceControl) @interface SFBClockSourceControl : SFBSelectorControl
@end

/// An audio line level control
NS_SWIFT_NAME(LineLevelControl) @interface SFBLineLevelControl : SFBSelectorControl
@end

/// An audio high pass filter control
NS_SWIFT_NAME(HighPassFilterControl) @interface SFBHighPassFilterControl : SFBSelectorControl
@end
