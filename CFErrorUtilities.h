/*
 *  Copyright (C) 2012, 2013, 2014 Stephen F. Booth <me@sbooth.org>
 *  All Rights Reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are
 *  met:
 *
 *    - Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    - Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *    - Neither the name of Stephen F. Booth nor the names of its 
 *      contributors may be used to endorse or promote products derived
 *      from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <CoreFoundation/CoreFoundation.h>

/*! @file CFErrorUtilities.h @brief Utility functions simplifying the boilerplate creation of CFErrors */

/*! @brief \c SFBAudioEngine's encompassing namespace */
namespace SFB {

	/*!
	 * @brief Create a \c CFErrorRef
	 * @note The returned error must be released by the caller
	 * @param domain The \c CFErrorRef error domain
	 * @param code The \c CFErrorRef error code
	 * @param description The value for userinfo dictionary \c kCFErrorLocalizedDescriptionKey
	 * @param failureReason The value for userinfo dictionary \c kCFErrorLocalizedFailureReasonKey
	 * @param recoverySuggestion The value for userinfo dictionary \c kCFErrorLocalizedRecoverySuggestionKey
	 * @return A \c CFErrorRef or \c nullptr on failure
	 */
	CFErrorRef CreateError(CFStringRef domain, CFIndex code, CFStringRef description, CFStringRef failureReason, CFStringRef recoverySuggestion);

	/*!
	 * @brief Create a \c CFErrorRef
	 * @note The returned error must be released by the caller
	 * @param domain The \c CFErrorRef error domain
	 * @param code The \c CFErrorRef error code
	 * @param descriptionFormatStringForURL The value for userinfo dictionary \c kCFErrorLocalizedDescriptionKey.  The display name of \c url will be substituted for the first occurrence of \c %@ in this string.
	 * @param url The URL
	 * @param failureReason The value for userinfo dictionary \c kCFErrorLocalizedFailureReasonKey
	 * @param recoverySuggestion The value for userinfo dictionary \c kCFErrorLocalizedRecoverySuggestionKey
	 * @return A \c CFErrorRef or \c nullptr on failure
	 */
	CFErrorRef CreateErrorForURL(CFStringRef domain, CFIndex code, CFStringRef descriptionFormatStringForURL, CFURLRef url, CFStringRef failureReason, CFStringRef recoverySuggestion);

}
