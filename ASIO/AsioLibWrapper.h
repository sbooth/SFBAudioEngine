// AsioLibWrapper.h

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

#ifndef _ASIOLIBWRAPPER_H_
#define _ASIOLIBWRAPPER_H_


#include "asiosys.h"

#if WINDOWS

#include <windows.h>
#include "iasiodrv.h"
#define DRIVER_TYPE_NAME    IASIO

#else   // Mac and others

#include "asiodrvr.h"
#define DRIVER_TYPE_NAME    AsioDriver

#endif  // WINDOWS


#define ASIO_LIB_ID_CAPACITY              64
#define ASIO_LIB_DISPLAYNAME_CAPACITY     64
#define ASIO_LIB_COMPANY_CAPACITY         64
#define ASIO_LIB_FOLDER_CAPACITY          256
#define ASIO_LIB_ARCHITECTURES_CAPACITY   32

//
// AsioLibInfo struct
//
typedef struct AsioLibInfo
{
	char Id            [ASIO_LIB_ID_CAPACITY];
    int  Number;
	char DisplayName   [ASIO_LIB_DISPLAYNAME_CAPACITY];
    char Company       [ASIO_LIB_COMPANY_CAPACITY];
    char InstallFolder [ASIO_LIB_FOLDER_CAPACITY];
    char Architectures [ASIO_LIB_ARCHITECTURES_CAPACITY];

	bool        ToCString    (char * dest, unsigned int destCapacity, char delimiter);
	static void FromCString  (AsioLibInfo & dest, const char * source, char delimiter);

} AsioLibInfo;


typedef DRIVER_TYPE_NAME AsioDriverType;


//
// AsioLibWrapper class
//
class AsioLibWrapper
{
public:
	AsioLibWrapper(ASIODriverInfo & info);
	~AsioLibWrapper();

	// ASIO discovery
	static int GetAsioLibraryList(AsioLibInfo * buffer, unsigned int bufferCapacity);

	// Library loading / unloading
	static bool LoadLib     (const AsioLibInfo & libInfo);
	static void UnloadLib   ();
	static bool IsLibLoaded ();

    static int CreateInstance(int driverNumber, AsioDriverType ** driver);

};


#endif	// _ASIOLIBWRAPPER_H_
