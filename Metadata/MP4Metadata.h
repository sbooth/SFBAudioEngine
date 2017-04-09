/*
 * Copyright (c) 2006 - 2017 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#pragma once

#include "AudioMetadata.h"

namespace SFB {

	namespace Audio {

		// ========================================
		// A Metadata subclass supporting MPEG-4
		// ========================================
		class MP4Metadata : public Metadata
		{

		public:

			// Data types handled by this class
			static CFArrayRef CreateSupportedFileExtensions();
			static CFArrayRef CreateSupportedMIMETypes();

			static bool HandlesFilesWithExtension(CFStringRef extension);
			static bool HandlesMIMEType(CFStringRef mimeType);

			static Metadata::unique_ptr CreateMetadata(CFURLRef url);

			// Creation
			explicit MP4Metadata(CFURLRef url);

		private:

			// Functionality
			virtual bool _ReadMetadata(CFErrorRef *error);
			virtual bool _WriteMetadata(CFErrorRef *error);
		};

	}
}
