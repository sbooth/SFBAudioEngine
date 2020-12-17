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
/// Returns the available items or \c nil on error
/// @note This corresponds to \c kAudioSelectorControlPropertyAvailableItems
@property (nonatomic, nullable, readonly) NSArray <NSNumber *> *availableItems NS_REFINED_FOR_SWIFT;
/// Returns the item's name or \c nil on error
/// @note This corresponds to \c kAudioSelectorControlPropertyItemName
@property (nonatomic, nullable, readonly) NSString *itemName NS_REFINED_FOR_SWIFT;
/// Returns the item's kind or \c nil on error
/// @note This corresponds to \c kAudioSelectorControlPropertyItemKind
@property (nonatomic, nullable, readonly) NSNumber *itemKind NS_REFINED_FOR_SWIFT;

@end

NS_ASSUME_NONNULL_END
