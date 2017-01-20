// AsioLibWrapperMac.cpp

/*
 *  Copyright (C) 2013, 2014 exaSound Audio Design <contact@exaSound.com>
 *  All Rights Reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are
 *  met:
 *
 *  - Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  - Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  - Neither the exaSound brand and logo nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
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

#include "AsioLibWrapper.h"

#include <string.h>
#include <dlfcn.h>
#include <CoreFoundation/CoreFoundation.h>


static void * _libHandle = 0;
static char   _libName[ASIO_LIB_ID_CAPACITY + ASIO_LIB_FOLDER_CAPACITY] = "";


// ASIOInit
typedef int (*PtrToCreateInstance)(int, AsioDriver**);
static PtrToCreateInstance _pCreateInstance = 0;



// function prototypes
CFURLEnumeratorRef CreateDirectoryEnumerator(CFStringRef dirPath);
bool HasExtension(CFURLRef fileUrl, CFStringRef ext);
bool LoadAsioLibInfo(CFURLRef asioLibUrl, AsioLibInfo & buffer);


//-----------------------------------------------------------------------------
int AsioLibWrapper::GetAsioLibraryList(AsioLibInfo *buffer, unsigned int bufferCapacity)
{

// NOTE: Folder for plist files was selected according to recomendations in following document from Apple (see Table 1-1):
//		https://developer.apple.com/library/mac/documentation/General/Conceptual/MOSXAppProgrammingGuide/AppRuntime/AppRuntime.html

    unsigned int          cnt;
    CFURLEnumeratorRef    dirEnum;
    CFURLRef              fileUrl;
    CFURLEnumeratorResult res;
    bool                  ok;

    dirEnum = CreateDirectoryEnumerator(CFSTR("/Library/Application Support/ASIO"));
    if ( ! dirEnum ) {
        return -1;
    }

    cnt = 0;
    if (( ! buffer ) || (bufferCapacity == 0) ) {
        // calculate the number of ASIO libraries
        do {
            res = CFURLEnumeratorGetNextURL(dirEnum, &fileUrl, NULL);
            if (res == kCFURLEnumeratorSuccess) {
                if ( HasExtension(fileUrl, CFSTR("plist")) ) {
                    cnt++;
                }
            }
        } while (res != kCFURLEnumeratorEnd);
    }
    else {
        // get actual data
        do {
            res = CFURLEnumeratorGetNextURL(dirEnum, &fileUrl, NULL);
            if (res == kCFURLEnumeratorSuccess) {
                if ( HasExtension(fileUrl, CFSTR("plist")) ) {
                    if (cnt < bufferCapacity) {
                        ok = LoadAsioLibInfo(fileUrl, buffer[cnt]);
                        if (ok) {
                            cnt++;
                        }
                    }
                }
            }
        } while (res != kCFURLEnumeratorEnd);
    }

    if (dirEnum) {
        CFRelease(dirEnum);
    }

    return (int)cnt;
}
//-----------------------------------------------------------------------------
bool AsioLibWrapper::LoadLib(const AsioLibInfo & libInfo)
{
    int mode;
    char path[ASIO_LIB_ID_CAPACITY + ASIO_LIB_FOLDER_CAPACITY];

    if (strlen(libInfo.InstallFolder) > 0) {
        strcpy(path, libInfo.InstallFolder);
        if (path[strlen(path) - 1] != '/') {
            strcat(path, "/");
        }
        strcat(path, libInfo.Id);
    }
    else {
        strcpy(path, libInfo.Id);
    }

    if (AsioLibWrapper::IsLibLoaded()) {
        return (strcasecmp(path, _libName) == 0);
    }

    mode = RTLD_LOCAL | RTLD_LAZY;
    _libHandle = dlopen(path, mode);
    if ( ! _libHandle ) {
        return false;
    }

    strcpy(_libName, path);

    // CreateInstance
    _pCreateInstance = (PtrToCreateInstance) dlsym(_libHandle, "CreateInstance");
    if ( ! _pCreateInstance ) {
        AsioLibWrapper::UnloadLib();
        return false;
    }


    return true;
}

//-----------------------------------------------------------------------------
void AsioLibWrapper::UnloadLib()
{
    if (_libHandle) {
        dlclose(_libHandle);
        _libHandle       = 0;
        _pCreateInstance = 0;
        _libName[0]       = '\0';
    }
}

//-----------------------------------------------------------------------------
bool AsioLibWrapper::IsLibLoaded()
{
    return (_libHandle != 0);
}

//-----------------------------------------------------------------------------
int AsioLibWrapper::CreateInstance(int driverNumber, AsioDriverType **driver)
{
    if ( ! _pCreateInstance )
        return 1;
    return _pCreateInstance(driverNumber, driver);
}

//-----------------------------------------------------------------------------
CFURLEnumeratorRef CreateDirectoryEnumerator(CFStringRef dirPath)
{
    CFURLRef           dirUrl  = NULL;
    CFURLEnumeratorRef dirEnum = NULL;

    dirUrl = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, dirPath, kCFURLPOSIXPathStyle, true);
    if (dirUrl ) {
        dirEnum = CFURLEnumeratorCreateForDirectoryURL(kCFAllocatorDefault, dirUrl, kCFURLEnumeratorDefaultBehavior, NULL);
    }
    if (dirUrl) {
        CFRelease(dirUrl);
    }
    return dirEnum;
}

//-----------------------------------------------------------------------------
bool HasExtension(CFURLRef fileUrl, CFStringRef ext)
{
    CFStringRef fileExt;
    bool        ok;

    fileExt = CFURLCopyPathExtension(fileUrl);
    if (fileExt) {
        ok = (kCFCompareEqualTo == CFStringCompare(fileExt, ext, kCFCompareCaseInsensitive));
        CFRelease(fileExt);
        return ok;
    }
    return false;
}

//-----------------------------------------------------------------------------
bool LoadAsioLibInfo(CFURLRef asioLibUrl, AsioLibInfo & buffer)
{
    CFDataRef           resourceData;
    SInt32              errorCode;
    Boolean             status;
    CFErrorRef          errorRef;
    CFPropertyListRef   propertyList;
    Boolean             ok;
    const void        * val;

    buffer.Number = 0;
	memset(buffer.Id,            '\0', ASIO_LIB_ID_CAPACITY);
	memset(buffer.DisplayName,   '\0', ASIO_LIB_DISPLAYNAME_CAPACITY);
    memset(buffer.Company,       '\0', ASIO_LIB_COMPANY_CAPACITY);
	memset(buffer.InstallFolder, '\0', ASIO_LIB_FOLDER_CAPACITY);
	memset(buffer.Architectures, '\0', ASIO_LIB_ARCHITECTURES_CAPACITY);

    status = CFURLCreateDataAndPropertiesFromResource(kCFAllocatorDefault, asioLibUrl, &resourceData, NULL, NULL, &errorCode);
    if ( ! status ) {
        return false;
    }

    errorRef     = NULL;
    propertyList = CFPropertyListCreateWithData(kCFAllocatorDefault, resourceData, kCFPropertyListImmutable, NULL, &errorRef);

    if (propertyList) {
        // name
        ok = CFDictionaryGetValueIfPresent((CFDictionaryRef)propertyList, CFSTR("Name"), &val);
        if (ok) {
            if (val) {
                /*ok =*/ CFStringGetCString((CFStringRef)val, buffer.Id, ASIO_LIB_ID_CAPACITY, kCFStringEncodingASCII);
            }
        }
        ok = CFDictionaryGetValueIfPresent((CFDictionaryRef)propertyList, CFSTR("Number"), &val);
        if (ok) {
            if (val) {
                /*ok =*/ CFNumberGetValue((CFNumberRef)val, kCFNumberSInt32Type, &buffer.Number);
            }
        }
        // number
        // display name
        ok = CFDictionaryGetValueIfPresent((CFDictionaryRef)propertyList, CFSTR("DisplayName"), &val);
        if (ok) {
            if (val) {
                /*ok =*/ CFStringGetCString((CFStringRef)val, buffer.DisplayName, ASIO_LIB_DISPLAYNAME_CAPACITY, kCFStringEncodingASCII);
            }
        }
        // company
        ok = CFDictionaryGetValueIfPresent((CFDictionaryRef)propertyList, CFSTR("Company"), &val);
        if (ok) {
            if (val) {
                /*ok =*/ CFStringGetCString((CFStringRef)val, buffer.Company, ASIO_LIB_COMPANY_CAPACITY, kCFStringEncodingASCII);
            }
        }
        // installation folder
        ok = CFDictionaryGetValueIfPresent((CFDictionaryRef)propertyList, CFSTR("InstallationFolder"), &val);
        if (ok) {
            if (val) {
                /*ok =*/ CFStringGetCString((CFStringRef)val, buffer.InstallFolder, ASIO_LIB_FOLDER_CAPACITY, kCFStringEncodingASCII);
            }
        }
        // build architectures
        ok = CFDictionaryGetValueIfPresent((CFDictionaryRef)propertyList, CFSTR("Architectures"), &val);
        if (ok) {
            if (val) {
                /*ok =*/ CFStringGetCString((CFStringRef)val, buffer.Architectures, ASIO_LIB_ARCHITECTURES_CAPACITY, kCFStringEncodingASCII);
            }
        }
    }

    // cleanup
    CFRelease(resourceData);
    if (errorRef) {
        CFRelease(errorRef);
    }
    if (propertyList) {
        CFRelease(propertyList);
    }

    // Id and DisplayName are mandatory, other fields are optional
    return (strlen(buffer.Id) > 0) && (strlen(buffer.DisplayName) > 0);
}

//-----------------------------------------------------------------------------
bool AsioLibInfo::ToCString(char * dest, unsigned int destCapacity, char delimiter)
{
	if ( ! dest )
		return false;
	if (destCapacity < strlen(Id) + 1 + strlen(DisplayName) + 1 + strlen(Company) + 1 +
                       strlen(InstallFolder) + 1 + strlen(Architectures) + 1)
		return false;
	if (delimiter == '\0')
		return false;

	strcpy  (dest, Id);
	strncat (dest, &delimiter, 1);

    char numChars[12];
    memset(numChars, 0, sizeof(numChars));
    sprintf(numChars, "%d", Number);
    strcat(dest, numChars);
    strncat (dest, &delimiter, 1);

	strcat  (dest, DisplayName);
	strncat (dest, &delimiter, 1);
	strcat  (dest, Company);
	strncat (dest, &delimiter, 1);
	strcat  (dest, InstallFolder);
	strncat (dest, &delimiter, 1);
	strcat  (dest, Architectures);

	return true;
}

//-----------------------------------------------------------------------------
void AsioLibInfo::FromCString(AsioLibInfo & dest, const char * source, char delimiter)
{
	const char * p1;
	const char * p2;

    dest.Number = 0;
	memset(dest.Id,            '\0', ASIO_LIB_ID_CAPACITY);
	memset(dest.DisplayName,   '\0', ASIO_LIB_DISPLAYNAME_CAPACITY);
    memset(dest.Company,       '\0', ASIO_LIB_COMPANY_CAPACITY);
	memset(dest.InstallFolder, '\0', ASIO_LIB_FOLDER_CAPACITY);
	memset(dest.Architectures, '\0', ASIO_LIB_ARCHITECTURES_CAPACITY);

	// Id
	p1 = source;
	p2 = strchr(p1, delimiter);
	if (p2) {
		strncpy(dest.Id, p1, (size_t)(p2 - p1));
	}
	else {
		strcpy(dest.Id, p1);
	}

    // Number
    char numChars[12];
    memset(numChars, 0, sizeof(numChars));
	if ( ! p2 )
		return;
	p1 = p2 + 1;
	p2 = strchr(p1, delimiter);
	if (p2) {
		strncpy(numChars, p1, (size_t)(p2 - p1));
	}
    if (strlen(numChars)) {
        sscanf(numChars, "%d", &dest.Number);
    }

	// DisplayName
	if ( ! p2 )
		return;
	p1 = p2 + 1;
	p2 = strchr(p1, delimiter);
	if (p2) {
		strncpy(dest.DisplayName, p1, (size_t)(p2 - p1));
	}
	else {
		strcpy(dest.DisplayName, p1);
	}

    // Company
	if ( ! p2 )
		return;
	p1 = p2 + 1;
	p2 = strchr(p1, delimiter);
	if (p2) {
		strncpy(dest.Company, p1, (size_t)(p2 - p1));
	}
	else {
		strcpy(dest.Company, p1);
	}

	// InstallFolder
	if ( ! p2 )
		return;
	p1 = p2 + 1;
	p2 = strchr(p1, delimiter);
	if (p2) {
		strncpy(dest.InstallFolder, p1, (size_t)(p2 - p1));
	}
	else {
		strcpy(dest.InstallFolder, p1);
	}

	// Architectures
	if ( ! p2 )
		return;
	p1 = p2 + 1;
	p2 = strchr(p1, delimiter);
	if (p2) {
		strncpy(dest.Architectures, p1, (size_t)(p2 - p1));
	}
	else {
		strcpy(dest.Architectures, p1);
	}

}

//-----------------------------------------------------------------------------
