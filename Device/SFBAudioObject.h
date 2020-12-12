/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <Foundation/Foundation.h>
#import <CoreAudio/CoreAudio.h>

NS_ASSUME_NONNULL_BEGIN

/// An audio object
NS_SWIFT_NAME(AudioObject) @interface SFBAudioObject : NSObject

/// The system audio object
+ (SFBAudioObject *)systemObject;

/// Returns an initialized \c SFBAudioObject object with the specified audio object ID
/// @note This returns a \c SFBAudioObject subclass based on the audio object's class when possible
/// @param objectID The desired audio object ID
/// @return An initialized \c SFBAudioObject object or \c nil if \c objectID is invalid or unknown
+ (instancetype)audioObjectWithAudioObjectID:(AudioObjectID)objectID;

+ (instancetype)new NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

/// Returns an initialized \c SFBAudioObject object with the specified audio object ID
/// @param objectID The desired audio object ID
/// @return An initialized \c SFBAudioObject object or \c nil if \c objectID is invalid or unknown
- (nullable instancetype)initWithAudioObjectID:(AudioObjectID)objectID NS_DESIGNATED_INITIALIZER;

#pragma mark - Audio Object Information

/// Returns the audio object's ID
@property (nonatomic, readonly) AudioObjectID objectID;

/// Returns the audio object's base class or \c 0 on error
@property (nonatomic, readonly) AudioClassID baseClassID;
/// Returns the audio object's class or \c 0 on error
@property (nonatomic, readonly) AudioClassID classID;
/// Returns the audio object's owning object
@property (nonatomic, readonly) SFBAudioObject * owner;
/// Returns the audio object's name
@property (nonatomic, nullable, readonly) NSString *name;
/// Returns the audio object's model name
@property (nonatomic, nullable, readonly) NSString *modelName;
/// Returns the audio object's manufacturer
@property (nonatomic, nullable, readonly) NSString *manufacturer;
/// Returns the audio object's element name
@property (nonatomic, nullable, readonly) NSString *elementName;
/// Returns the audio object's element category name
@property (nonatomic, nullable, readonly) NSString *elementCategoryName;
/// Returns the audio object's element number name
@property (nonatomic, nullable, readonly) NSString *elementNumberName;

/// Returns the audio objects owned by this object
@property (nonatomic, nullable, readonly) NSArray<SFBAudioObject *> *ownedObjects;

/// Returns the audio object's serial number
@property (nonatomic, nullable, readonly) NSString *serialNumber;
/// Returns the audio object's firmware version
@property (nonatomic, nullable, readonly) NSString *firmwareVersion;

#pragma mark - Audio Object Properties

/// Returns \c YES if the underlying audio object has the specified property
/// @note This queries \c { property, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster }
/// @param property The property to query
/// @return \c YES if the property is supported
- (BOOL)hasProperty:(AudioObjectPropertySelector)property;
/// Returns \c YES if the underlying audio object has the specified property in a scope
/// @note This queries \c { property, scope, kAudioObjectPropertyElementMaster }
/// @param property The property to query
/// @param scope The desired scope
/// @return \c YES if the property is supported
- (BOOL)hasProperty:(AudioObjectPropertySelector)property inScope:(AudioObjectPropertyScope)scope;
/// Returns \c YES if the underlying audio object has the specified property on an element in a scope
/// @param property The property to query
/// @param scope The desired scope
/// @param element The desired element
/// @return \c YES if the property is supported
- (BOOL)hasProperty:(AudioObjectPropertySelector)property inScope:(AudioObjectPropertyScope)scope onElement:(AudioObjectPropertyElement)element;

/// Returns \c YES if the underlying audio object has the specified property and it is settable
/// @note This queries \c { property, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster }
/// @param property The property to query
/// @return \c YES if the property is settable
- (BOOL)propertyIsSettable:(AudioObjectPropertySelector)property;
/// Returns \c YES if the underlying audio object has the specified property in a scope and it is settable
/// @note This queries \c { property, scope, kAudioObjectPropertyElementMaster }
/// @param property The property to query
/// @param scope The desired scope
/// @return \c YES if the property is settable
- (BOOL)propertyIsSettable:(AudioObjectPropertySelector)property inScope:(AudioObjectPropertyScope)scope;
/// Returns \c YES if the underlying audio object has the specified property on an element in a scope and it is settable
/// @param property The property to query
/// @param scope The desired scope
/// @param element The desired element
/// @return \c YES if the property is settable
- (BOOL)propertyIsSettable:(AudioObjectPropertySelector)property inScope:(AudioObjectPropertyScope)scope onElement:(AudioObjectPropertyElement)element;

/// Performs a block when the specified property changes
/// @note This observes \c { property, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster }
/// @param property The property to observe
/// @param block A block to invoke when the property changes or \c nil to remove the previous value
- (void)whenPropertyChanges:(AudioObjectPropertySelector)property performBlock:(_Nullable dispatch_block_t)block;
/// Performs a block when the specified property in a scope changes
/// @note This observes \c { property, scope, kAudioObjectPropertyElementMaster }
/// @param property The property to observe
/// @param scope The desired scope
/// @param block A block to invoke when the property changes or \c nil to remove the previous value
- (void)whenProperty:(AudioObjectPropertySelector)property changesInScope:(AudioObjectPropertyScope)scope performBlock:(_Nullable dispatch_block_t)block;
/// Performs a block when the specified property on an element in a scope changes
/// @param property The property to observe
/// @param scope The desired scope
/// @param element The desired element
/// @param block A block to invoke when the property changes or \c nil to remove the previous value
- (void)whenProperty:(AudioObjectPropertySelector)property inScope:(AudioObjectPropertyScope)scope changesOnElement:(AudioObjectPropertyElement)element performBlock:(_Nullable dispatch_block_t)block;

@end

NS_ASSUME_NONNULL_END
