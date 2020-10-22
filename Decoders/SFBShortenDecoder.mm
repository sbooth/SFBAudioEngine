/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <os/log.h>

#import <algorithm>

#import "SFBShortenDecoder.h"

#import "AVAudioPCMBuffer+SFBBufferUtilities.h"
#import "NSError+SFBURLPresentation.h"

//#define MAGIC                 "ajkg"
//#define FORMAT_VERSION        2
#define MIN_SUPPORTED_VERSION 1
#define MAX_SUPPORTED_VERSION 3
//#define MAX_VERSION           7

//#define UNDEFINED_UINT     -1
#define DEFAULT_BLOCK_SIZE  256
#define DEFAULT_V0NMEAN     0
#define DEFAULT_V2NMEAN     4
#define DEFAULT_MAXNLPC     0
#define DEFAULT_NCHAN       1
#define DEFAULT_NSKIP       0
#define DEFAULT_NDISCARD    0
#define NBITPERLONG         32
#define DEFAULT_MINSNR      256
#define DEFAULT_MAXRESNSTR  "32.0"
#define DEFAULT_QUANTERROR  0
#define MINBITRATE          2.5

#define MAX_LPC_ORDER 64
#define CHANSIZE      0
#define ENERGYSIZE    3
#define BITSHIFTSIZE  2
#define NWRAP         3

#define FNSIZE       2
#define FN_DIFF0     0
#define FN_DIFF1     1
#define FN_DIFF2     2
#define FN_DIFF3     3
#define FN_QUIT      4
#define FN_BLOCKSIZE 5
#define FN_BITSHIFT  6
#define FN_QLPC      7
#define FN_ZERO      8
#define FN_VERBATIM  9

#define VERBATIM_CKSIZE_SIZE 5   /* a var_put code size */
#define VERBATIM_BYTE_SIZE   8   /* code size 8 on single bytes means no compression at all */
#define VERBATIM_CHUNK_MAX   256 /* max. size of a FN_VERBATIM chunk */

#define ULONGSIZE 2
#define NSKIPSIZE 1
#define LPCQSIZE  2
#define LPCQUANT  5
#define XBYTESIZE 7

#define TYPESIZE            4
#define TYPE_AU1            0  /* original lossless ulaw                    */
#define TYPE_S8             1  /* signed 8 bit characters                   */
#define TYPE_U8             2  /* unsigned 8 bit characters                 */
#define TYPE_S16HL          3  /* signed 16 bit shorts: high-low            */
#define TYPE_U16HL          4  /* unsigned 16 bit shorts: high-low          */
#define TYPE_S16LH          5  /* signed 16 bit shorts: low-high            */
#define TYPE_U16LH          6  /* unsigned 16 bit shorts: low-high          */
#define TYPE_ULAW           7  /* lossy ulaw: internal conversion to linear */
#define TYPE_AU2            8  /* new ulaw with zero mapping                */
#define TYPE_AU3            9  /* lossless alaw                             */
#define TYPE_ALAW          10  /* lossy alaw: internal conversion to linear */
//#define TYPE_RIFF_WAVE     11  /* Microsoft .WAV files                      */
//#define TYPE_AIFF          12  /* Apple .AIFF files                         */
//#define TYPE_EOF           13
//#define TYPE_GENERIC_ULAW 128
//#define TYPE_GENERIC_ALAW 129

//#define POSITIVE_ULAW_ZERO 0xff
#define NEGATIVE_ULAW_ZERO 0x7f

#define SEEK_TABLE_REVISION 1

#define SEEK_HEADER_SIZE  12
#define SEEK_TRAILER_SIZE 12
#define SEEK_ENTRY_SIZE   80

#define V2LPCQOFFSET (1 << LPCQUANT)

#define MAX_CHANNELS 8
#define MAX_BLOCKSIZE 65535

#define CANONICAL_HEADER_SIZE 44

#define WAVE_FORMAT_PCM 0x0001

#define ROUNDEDSHIFTDOWN(x, n) (((n) == 0) ? (x) : ((x) >> ((n) - 1)) >> 1)

namespace {
	static const uint32_t sMaskTable [] = {
		0x0,
		0x1,		0x3,		0x7,		0xf,
		0x1f,		0x3f,		0x7f,		0xff,
		0x1ff,		0x3ff,		0x7ff,		0xfff,
		0x1fff,		0x3fff,		0x7fff,		0xffff,
		0x1ffff,	0x3ffff,	0x7ffff,	0xfffff,
		0x1fffff,	0x3fffff,	0x7fffff,	0xffffff,
		0x1ffffff,	0x3ffffff,	0x7ffffff,	0xfffffff,
		0x1fffffff,	0x3fffffff,	0x7fffffff,	0xffffffff
	};

	static uint8_t ulaw_outward[13][256] = {
		{127,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,64,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,80,81,82,83,84,85,86,87,88,89,90,91,92,93,94,95,96,97,98,99,100,101,102,103,104,105,106,107,108,109,110,111,112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,255,254,253,252,251,250,249,248,247,246,245,244,243,242,241,240,239,238,237,236,235,234,233,232,231,230,229,228,227,226,225,224,223,222,221,220,219,218,217,216,215,214,213,212,211,210,209,208,207,206,205,204,203,202,201,200,199,198,197,196,195,194,193,192,191,190,189,188,187,186,185,184,183,182,181,180,179,178,177,176,175,174,173,172,171,170,169,168,167,166,165,164,163,162,161,160,159,158,157,156,155,154,153,152,151,150,149,148,147,146,145,144,143,142,141,140,139,138,137,136,135,134,133,132,131,130,129,128},
		{112,114,116,118,120,122,124,126,127,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,64,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,80,81,82,83,84,85,86,87,88,89,90,91,92,93,94,95,96,97,98,99,100,101,102,103,104,105,106,107,108,109,110,111,113,115,117,119,121,123,125,255,253,251,249,247,245,243,241,239,238,237,236,235,234,233,232,231,230,229,228,227,226,225,224,223,222,221,220,219,218,217,216,215,214,213,212,211,210,209,208,207,206,205,204,203,202,201,200,199,198,197,196,195,194,193,192,191,190,189,188,187,186,185,184,183,182,181,180,179,178,177,176,175,174,173,172,171,170,169,168,167,166,165,164,163,162,161,160,159,158,157,156,155,154,153,152,151,150,149,148,147,146,145,144,143,142,141,140,139,138,137,136,135,134,133,132,131,130,129,128,254,252,250,248,246,244,242,240},
		{96,98,100,102,104,106,108,110,112,113,114,116,117,118,120,121,122,124,125,126,127,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,64,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,80,81,82,83,84,85,86,87,88,89,90,91,92,93,94,95,97,99,101,103,105,107,109,111,115,119,123,255,251,247,243,239,237,235,233,231,229,227,225,223,222,221,220,219,218,217,216,215,214,213,212,211,210,209,208,207,206,205,204,203,202,201,200,199,198,197,196,195,194,193,192,191,190,189,188,187,186,185,184,183,182,181,180,179,178,177,176,175,174,173,172,171,170,169,168,167,166,165,164,163,162,161,160,159,158,157,156,155,154,153,152,151,150,149,148,147,146,145,144,143,142,141,140,139,138,137,136,135,134,133,132,131,130,129,128,254,253,252,250,249,248,246,245,244,242,241,240,238,236,234,232,230,228,226,224},
		{80,82,84,86,88,90,92,94,96,97,98,100,101,102,104,105,106,108,109,110,112,113,114,115,116,117,118,120,121,122,123,124,125,126,127,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,64,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,81,83,85,87,89,91,93,95,99,103,107,111,119,255,247,239,235,231,227,223,221,219,217,215,213,211,209,207,206,205,204,203,202,201,200,199,198,197,196,195,194,193,192,191,190,189,188,187,186,185,184,183,182,181,180,179,178,177,176,175,174,173,172,171,170,169,168,167,166,165,164,163,162,161,160,159,158,157,156,155,154,153,152,151,150,149,148,147,146,145,144,143,142,141,140,139,138,137,136,135,134,133,132,131,130,129,128,254,253,252,251,250,249,248,246,245,244,243,242,241,240,238,237,236,234,233,232,230,229,228,226,225,224,222,220,218,216,214,212,210,208},
		{64,66,68,70,72,74,76,78,80,81,82,84,85,86,88,89,90,92,93,94,96,97,98,99,100,101,102,104,105,106,107,108,109,110,112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,127,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,65,67,69,71,73,75,77,79,83,87,91,95,103,111,255,239,231,223,219,215,211,207,205,203,201,199,197,195,193,191,190,189,188,187,186,185,184,183,182,181,180,179,178,177,176,175,174,173,172,171,170,169,168,167,166,165,164,163,162,161,160,159,158,157,156,155,154,153,152,151,150,149,148,147,146,145,144,143,142,141,140,139,138,137,136,135,134,133,132,131,130,129,128,254,253,252,251,250,249,248,247,246,245,244,243,242,241,240,238,237,236,235,234,233,232,230,229,228,227,226,225,224,222,221,220,218,217,216,214,213,212,210,209,208,206,204,202,200,198,196,194,192},
		{49,51,53,55,57,59,61,63,64,66,67,68,70,71,72,74,75,76,78,79,80,81,82,84,85,86,87,88,89,90,92,93,94,95,96,97,98,99,100,101,102,104,105,106,107,108,109,110,111,112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,127,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,50,52,54,56,58,60,62,65,69,73,77,83,91,103,255,231,219,211,205,201,197,193,190,188,186,184,182,180,178,176,175,174,173,172,171,170,169,168,167,166,165,164,163,162,161,160,159,158,157,156,155,154,153,152,151,150,149,148,147,146,145,144,143,142,141,140,139,138,137,136,135,134,133,132,131,130,129,128,254,253,252,251,250,249,248,247,246,245,244,243,242,241,240,239,238,237,236,235,234,233,232,230,229,228,227,226,225,224,223,222,221,220,218,217,216,215,214,213,212,210,209,208,207,206,204,203,202,200,199,198,196,195,194,192,191,189,187,185,183,181,179,177},
		{32,34,36,38,40,42,44,46,48,49,51,52,53,55,56,57,59,60,61,63,64,65,66,67,68,70,71,72,73,74,75,76,78,79,80,81,82,83,84,85,86,87,88,89,90,92,93,94,95,96,97,98,99,100,101,102,103,104,105,106,107,108,109,110,111,112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,127,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,33,35,37,39,41,43,45,47,50,54,58,62,69,77,91,255,219,205,197,190,186,182,178,175,173,171,169,167,165,163,161,159,158,157,156,155,154,153,152,151,150,149,148,147,146,145,144,143,142,141,140,139,138,137,136,135,134,133,132,131,130,129,128,254,253,252,251,250,249,248,247,246,245,244,243,242,241,240,239,238,237,236,235,234,233,232,231,230,229,228,227,226,225,224,223,222,221,220,218,217,216,215,214,213,212,211,210,209,208,207,206,204,203,202,201,200,199,198,196,195,194,193,192,191,189,188,187,185,184,183,181,180,179,177,176,174,172,170,168,166,164,162,160},
		{16,18,20,22,24,26,28,30,32,33,34,36,37,38,40,41,42,44,45,46,48,49,50,51,52,53,55,56,57,58,59,60,61,63,64,65,66,67,68,69,70,71,72,73,74,75,76,78,79,80,81,82,83,84,85,86,87,88,89,90,91,92,93,94,95,96,97,98,99,100,101,102,103,104,105,106,107,108,109,110,111,112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,127,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,17,19,21,23,25,27,29,31,35,39,43,47,54,62,77,255,205,190,182,175,171,167,163,159,157,155,153,151,149,147,145,143,142,141,140,139,138,137,136,135,134,133,132,131,130,129,128,254,253,252,251,250,249,248,247,246,245,244,243,242,241,240,239,238,237,236,235,234,233,232,231,230,229,228,227,226,225,224,223,222,221,220,219,218,217,216,215,214,213,212,211,210,209,208,207,206,204,203,202,201,200,199,198,197,196,195,194,193,192,191,189,188,187,186,185,184,183,181,180,179,178,177,176,174,173,172,170,169,168,166,165,164,162,161,160,158,156,154,152,150,148,146,144},
		{2,4,6,8,10,12,14,16,17,18,20,21,22,24,25,26,28,29,30,32,33,34,35,36,37,38,40,41,42,43,44,45,46,48,49,50,51,52,53,54,55,56,57,58,59,60,61,63,64,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,80,81,82,83,84,85,86,87,88,89,90,91,92,93,94,95,96,97,98,99,100,101,102,103,104,105,106,107,108,109,110,111,112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,127,0,1,3,5,7,9,11,13,15,19,23,27,31,39,47,62,255,190,175,167,159,155,151,147,143,141,139,137,135,133,131,129,254,253,252,251,250,249,248,247,246,245,244,243,242,241,240,239,238,237,236,235,234,233,232,231,230,229,228,227,226,225,224,223,222,221,220,219,218,217,216,215,214,213,212,211,210,209,208,207,206,205,204,203,202,201,200,199,198,197,196,195,194,193,192,191,189,188,187,186,185,184,183,182,181,180,179,178,177,176,174,173,172,171,170,169,168,166,165,164,163,162,161,160,158,157,156,154,153,152,150,149,148,146,145,144,142,140,138,136,134,132,130,128},
		{1,2,4,5,6,8,9,10,12,13,14,16,17,18,19,20,21,22,24,25,26,27,28,29,30,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,64,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,80,81,82,83,84,85,86,87,88,89,90,91,92,93,94,95,96,97,98,99,100,101,102,103,104,105,106,107,108,109,110,111,112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,127,0,3,7,11,15,23,31,47,255,175,159,151,143,139,135,131,254,253,252,251,250,249,248,247,246,245,244,243,242,241,240,239,238,237,236,235,234,233,232,231,230,229,228,227,226,225,224,223,222,221,220,219,218,217,216,215,214,213,212,211,210,209,208,207,206,205,204,203,202,201,200,199,198,197,196,195,194,193,192,191,190,189,188,187,186,185,184,183,182,181,180,179,178,177,176,174,173,172,171,170,169,168,167,166,165,164,163,162,161,160,158,157,156,155,154,153,152,150,149,148,147,146,145,144,142,141,140,138,137,136,134,133,132,130,129,128},
		{1,2,3,4,5,6,8,9,10,11,12,13,14,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,64,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,80,81,82,83,84,85,86,87,88,89,90,91,92,93,94,95,96,97,98,99,100,101,102,103,104,105,106,107,108,109,110,111,112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,127,0,7,15,31,255,159,143,135,254,253,252,251,250,249,248,247,246,245,244,243,242,241,240,239,238,237,236,235,234,233,232,231,230,229,228,227,226,225,224,223,222,221,220,219,218,217,216,215,214,213,212,211,210,209,208,207,206,205,204,203,202,201,200,199,198,197,196,195,194,193,192,191,190,189,188,187,186,185,184,183,182,181,180,179,178,177,176,175,174,173,172,171,170,169,168,167,166,165,164,163,162,161,160,158,157,156,155,154,153,152,151,150,149,148,147,146,145,144,142,141,140,139,138,137,136,134,133,132,131,130,129,128},
		{1,2,3,4,5,6,7,8,9,10,11,12,13,14,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,64,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,80,81,82,83,84,85,86,87,88,89,90,91,92,93,94,95,96,97,98,99,100,101,102,103,104,105,106,107,108,109,110,111,112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,127,0,15,255,143,254,253,252,251,250,249,248,247,246,245,244,243,242,241,240,239,238,237,236,235,234,233,232,231,230,229,228,227,226,225,224,223,222,221,220,219,218,217,216,215,214,213,212,211,210,209,208,207,206,205,204,203,202,201,200,199,198,197,196,195,194,193,192,191,190,189,188,187,186,185,184,183,182,181,180,179,178,177,176,175,174,173,172,171,170,169,168,167,166,165,164,163,162,161,160,159,158,157,156,155,154,153,152,151,150,149,148,147,146,145,144,142,141,140,139,138,137,136,135,134,133,132,131,130,129,128},
		{1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,64,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,80,81,82,83,84,85,86,87,88,89,90,91,92,93,94,95,96,97,98,99,100,101,102,103,104,105,106,107,108,109,110,111,112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,127,0,255,254,253,252,251,250,249,248,247,246,245,244,243,242,241,240,239,238,237,236,235,234,233,232,231,230,229,228,227,226,225,224,223,222,221,220,219,218,217,216,215,214,213,212,211,210,209,208,207,206,205,204,203,202,201,200,199,198,197,196,195,194,193,192,191,190,189,188,187,186,185,184,183,182,181,180,179,178,177,176,175,174,173,172,171,170,169,168,167,166,165,164,163,162,161,160,159,158,157,156,155,154,153,152,151,150,149,148,147,146,145,144,143,142,141,140,139,138,137,136,135,134,133,132,131,130,129,128}
	};

	/// Returns a two-dimensional \c rows x \c cols array using one allocation from \c malloc
	template <typename T>
	T ** AllocateContiguous2DArray(size_t rows, size_t cols)
	{
		T **result = (T **)malloc((rows * sizeof(T *)) + (rows * cols * sizeof(T)));
		T *tmp = (T *)(result + rows);
		for(size_t i = 0; i < rows; ++i) {
			result[i] = tmp + i * cols;
		}
		return result;
	}

	/// Clips values to the interval [lower, upper]
	template <typename T>
	T clip(const T& n, const T& lower, const T& upper) {
		return std::max(lower, std::min(n, upper));
	}

	void fix_bitshift(int32_t *buffer, int nitem, int bitshift, int ftype)
	{
		int i;

		if(ftype == TYPE_AU1) {
			for(i = 0; i < nitem; i++) {
				buffer[i] = ulaw_outward[bitshift][buffer[i] + 128];
			}
		}
		else if(ftype == TYPE_AU2)
			for(i = 0; i < nitem; i++) {
				if(buffer[i] >= 0)
					buffer[i] = ulaw_outward[bitshift][buffer[i] + 128];
				else if(buffer[i] == -1)
					buffer[i] =  NEGATIVE_ULAW_ZERO;
				else
					buffer[i] = ulaw_outward[bitshift][buffer[i] + 129];
			}
		else
			if(bitshift != 0)
				for(i = 0; i < nitem; i++) {
					buffer[i] <<= bitshift;
				}
	}

	/// Variable-length input using Golomb-Rice coding
	class VariableLengthInput {
	public:
		/// Creates a new \c VariableLengthInput object with an internal buffer of the specified size
		VariableLengthInput(size_t size = 512)
			: mInputBlock(nil), mSize(size), mBytesAvailable(0), mBuffer(0), mBitsAvailable(0)
		{
			mBuf = new uint8_t [(size_t)mSize];
			mPos = mBuf;
		}

		~VariableLengthInput()
		{
			delete [] mBuf;
		}

		/// Input callback type
		using InputBlock = bool(^)(void *buf, size_t len, size_t& read);

		/// Sets the input callback
		void SetInputCallback(InputBlock block)
		{
			mInputBlock = block;
		}

		/// Reads a single unsigned value from the specified bin
		bool uvar_get(int32_t& i32, size_t bin)
		{
			if(mBitsAvailable == 0) {
				if(!word_get(mBuffer))
					return false;
				mBitsAvailable = 32;
			}

			int32_t result;
			for(result = 0; !(mBuffer & (1L << --mBitsAvailable)); ++result) {
				if(mBitsAvailable == 0) {
					if(!word_get(mBuffer))
						return false;
					mBitsAvailable = 32;
				}
			}

			while(bin != 0) {
				if(mBitsAvailable >= bin) {
					result = (result << bin) | (int32_t)((mBuffer >> (mBitsAvailable - bin)) & sMaskTable[bin]);
					mBitsAvailable -= bin;
					bin = 0;
				}
				else {
					result = (result << mBitsAvailable) | (int32_t)(mBuffer & sMaskTable[mBitsAvailable]);
					bin -= mBitsAvailable;
					if(!word_get(mBuffer))
						return false;
					mBitsAvailable = 32;
				}
			}

			i32 = result;
			return true;
		}

		/// Reads a single signed value from the specified bin
		bool var_get(int32_t& i32, size_t bin)
		{
			int32_t var;
			if(!uvar_get(var, bin + 1))
				return false;

			uint32_t uvar = (uint32_t)var;
			if(uvar & 1)
				i32 = ~(uvar >> 1);
			else
				i32 = (uvar >> 1);
			return true;
		}

		/// Reads the unsigned Golomb-Rice code
		bool ulong_get(uint32_t& ui32)
		{
			int32_t bitcount;
			if(!uvar_get(bitcount, ULONGSIZE))
				return false;

			int32_t i32;
			if(!uvar_get(i32, (uint32_t)bitcount))
				return false;

			ui32 = (uint32_t)i32;
			return true;
		}

		bool uint_get(uint32_t& ui32, int version, size_t bin)
		{
			if(version == 0) {
				int32_t i32;
				if(!uvar_get(i32, bin))
					return false;
				ui32 = (uint32_t)i32;
				return true;
			}
			else
				return ulong_get(ui32);
		}

	private:
		/// Input callback
		InputBlock mInputBlock;
		/// Size of \c mBuf in bytes
		size_t mSize;
		/// Byte buffer
		uint8_t *mBuf;
		/// Current position in mBuf
		uint8_t *mPos;
		/// Bytes available in \c mPos
		size_t mBytesAvailable;
		/// Bit buffer
		uint32_t mBuffer;
		/// Bits available in \c mBuffer
		size_t mBitsAvailable;

		/// Reads a single \c uint32_t from the byte buffer, refilling if necessary
		bool word_get(uint32_t& ui32)
		{
			if(mBytesAvailable < 4) {
				size_t bytesRead = 0;
				if(!mInputBlock || !mInputBlock(mBuf, mSize, bytesRead) || bytesRead < 4)
					return false;
				mBytesAvailable += bytesRead;
				mPos = mBuf;
			}

			ui32 = (uint32_t)((((int32_t)mPos[0]) << 24) | (((int32_t)mPos[1]) << 16) | (((int32_t)mPos[2]) << 8) | ((int32_t)mPos[3]));

			mPos += 4;
			mBytesAvailable -= 4;

			return true;
		}

	};

	class ByteStream {
	public:
		ByteStream(const void *buf, size_t len)
			: mBuf(buf), mLen(len), mPos(0)
		{}

		bool ReadLE16(uint16_t& ui16)
		{
			if((mPos + 2) > mLen)
				return false;
			ui16 = OSReadLittleInt16(mBuf, mPos);
			mPos += 2;
			return true;
		}

		uint16_t ReadLE16()
		{
			uint16_t result = 0;
			if(!ReadLE16(result))
				mPos = mLen;
			return result;
		}

		bool ReadLE32(uint32_t& ui32)
		{
			if((mPos + 4) > mLen)
				return false;
			ui32 = OSReadLittleInt32(mBuf, mPos);
			mPos += 4;
			return true;
		}

		uint32_t ReadLE32()
		{
			uint32_t result = 0;
			if(!ReadLE32(result))
				mPos = mLen;
			return result;
		}

		bool ReadLE64(uint64_t& ui64)
		{
			if((mPos + 8) > mLen)
				return false;
			ui64 = OSReadLittleInt64(mBuf, mPos);
			mPos += 8;
			return true;
		}

		uint64_t ReadLE64()
		{
			uint64_t result = 0;
			if(!ReadLE64(result))
				mPos = mLen;
			return result;
		}

		bool ReadBE16(uint16_t& ui16)
		{
			if((mPos + 2) > mLen)
				return false;
			ui16 = OSReadBigInt16(mBuf, mPos);
			mPos += 2;
			return true;
		}

		uint16_t ReadBE16()
		{
			uint16_t result = 0;
			if(!ReadBE16(result))
				mPos = mLen;
			return result;
		}

		bool ReadBE32(uint32_t& ui32)
		{
			if((mPos + 4) > mLen)
				return false;
			ui32 = OSReadBigInt32(mBuf, mPos);
			mPos += 4;
			return true;
		}

		uint32_t ReadBE32()
		{
			uint32_t result = 0;
			if(!ReadBE32(result))
				mPos = mLen;
			return result;
		}

		bool ReadBE64(uint64_t& ui64)
		{
			if((mPos + 8) > mLen)
				return false;
			ui64 = OSReadBigInt64(mBuf, mPos);
			mPos += 8;
			return true;
		}

		uint64_t ReadBE64()
		{
			uint64_t result = 0;
			if(!ReadBE64(result))
				mPos = mLen;
			return result;
		}

		bool Skip(size_t count)
		{
			if((mPos + count) > mLen)
				return false;
			mPos += count;
			return true;
		}

		inline size_t Remaining() const
		{
			return mLen - mPos;
		}

	private:
		const void *mBuf;
		size_t mLen;
		size_t mPos;
	};
}

@interface SFBShortenDecoder ()
{
@private
	VariableLengthInput _input;
	int _version;
	int32_t _lpcqoffset;
	int _internal_ftype;
	int _nchan;
	int _nmean;
	int _blocksize;
	int _maxnlpc;
	int _nwrap;

	uint32_t _sampleRate;
	uint32_t _bitsPerSample;

	int32_t **_buffer;
	int32_t **_offset;
	int *_qlpc;
	int _bitshift;

	bool _eos;

	AVAudioPCMBuffer *_frameBuffer;
	AVAudioFramePosition _framePosition;
	AVAudioFramePosition _frameLength;
}
- (BOOL)parseShortenHeaderReturningError:(NSError **)error;
- (BOOL)parseRIFFChunk:(ByteStream&)chunkData error:(NSError **)error;
- (BOOL)parseFORMChunk:(ByteStream&)chunkData error:(NSError **)error;
@end

@implementation SFBShortenDecoder

+ (void)load
{
	[SFBAudioDecoder registerSubclass:[self class]];
}

+ (NSSet *)supportedPathExtensions
{
	return [NSSet setWithObject:@"shn"];
}

+ (NSSet *)supportedMIMETypes
{
	return [NSSet setWithObject:@"audio/x-shorten"];
}

- (BOOL)openReturningError:(NSError **)error
{
	if(![super openReturningError:error] || ![self parseShortenHeaderReturningError:error])
		return NO;

	// Sanity checks
	if(_bitsPerSample != 8 && _bitsPerSample != 16) {
		os_log_error(gSFBAudioDecoderLog, "Unsupported bit depth: %u", _bitsPerSample);
		if(error)
			*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
											 code:SFBAudioDecoderErrorCodeInputOutput
					descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a supported Shorten file.", @"")
											  url:_inputSource.url
									failureReason:NSLocalizedString(@"Unsupported bit depth", @"")
							   recoverySuggestion:NSLocalizedString(@"The file's bit depth is not supported.", @"")];
		return NO;
	}

	if((_bitsPerSample == 8 && (_internal_ftype != TYPE_U8 && _internal_ftype != TYPE_S8)) || (_bitsPerSample == 16 && (_internal_ftype != TYPE_U16HL && _internal_ftype != TYPE_U16LH && _internal_ftype != TYPE_S16HL && _internal_ftype != TYPE_S16LH))) {
		os_log_error(gSFBAudioDecoderLog, "Unsupported bit depth/audio type combination: %u, %u", _bitsPerSample, _internal_ftype);
		if(error)
			*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
											 code:SFBAudioDecoderErrorCodeInputOutput
					descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a supported Shorten file.", @"")
											  url:_inputSource.url
									failureReason:NSLocalizedString(@"Unsupported bit depth/audio type combination", @"")
							   recoverySuggestion:NSLocalizedString(@"The file's bit depth and audio type is not supported.", @"")];
		return NO;
	}

	// Set up the processing format
	AudioStreamBasicDescription processingStreamDescription;

	processingStreamDescription.mFormatID			= kAudioFormatLinearPCM;
	processingStreamDescription.mFormatFlags		= kAudioFormatFlagIsNonInterleaved | kAudioFormatFlagIsPacked;
	if(_internal_ftype == TYPE_U16HL || _internal_ftype == TYPE_S16HL)
		processingStreamDescription.mFormatFlags	|= kAudioFormatFlagIsBigEndian;
	if(_internal_ftype == TYPE_S8 || _internal_ftype == TYPE_S16HL || _internal_ftype == TYPE_S16LH)
		processingStreamDescription.mFormatFlags	|= kAudioFormatFlagIsSignedInteger;

	processingStreamDescription.mSampleRate			= _sampleRate;
	processingStreamDescription.mChannelsPerFrame	= (UInt32)_nchan;
	processingStreamDescription.mBitsPerChannel		= _bitsPerSample;

	processingStreamDescription.mBytesPerPacket		= (_bitsPerSample + 7) / 8;
	processingStreamDescription.mFramesPerPacket	= 1;
	processingStreamDescription.mBytesPerFrame		= processingStreamDescription.mBytesPerPacket * processingStreamDescription.mFramesPerPacket;

	AVAudioChannelLayout *channelLayout = nil;
	switch(_nchan) {
		case 1:		channelLayout = [AVAudioChannelLayout layoutWithLayoutTag:kAudioChannelLayoutTag_Mono];				break;
		case 2:		channelLayout = [AVAudioChannelLayout layoutWithLayoutTag:kAudioChannelLayoutTag_Stereo];			break;
		case 3:		channelLayout = [AVAudioChannelLayout layoutWithLayoutTag:kAudioChannelLayoutTag_MPEG_3_0_A];		break;
		case 4:		channelLayout = [AVAudioChannelLayout layoutWithLayoutTag:kAudioChannelLayoutTag_Quadraphonic];		break;
		case 5:		channelLayout = [AVAudioChannelLayout layoutWithLayoutTag:kAudioChannelLayoutTag_MPEG_5_0_A];		break;
		case 6:		channelLayout = [AVAudioChannelLayout layoutWithLayoutTag:kAudioChannelLayoutTag_MPEG_5_1_A];		break;
		case 7:		channelLayout = [AVAudioChannelLayout layoutWithLayoutTag:kAudioChannelLayoutTag_MPEG_6_1_A];		break;
		case 8:		channelLayout = [AVAudioChannelLayout layoutWithLayoutTag:kAudioChannelLayoutTag_MPEG_7_1_A];		break;
	}

	_processingFormat = [[AVAudioFormat alloc] initWithStreamDescription:&processingStreamDescription channelLayout:channelLayout];

	// Set up the source format
	AudioStreamBasicDescription sourceStreamDescription;

	sourceStreamDescription.mFormatID			= SFBAudioFormatIDShorten;

	sourceStreamDescription.mSampleRate			= _sampleRate;
	sourceStreamDescription.mChannelsPerFrame	= (UInt32)_nchan;
	sourceStreamDescription.mBitsPerChannel		= _bitsPerSample;

	sourceStreamDescription.mFramesPerPacket	= (UInt32)_blocksize;

	_sourceFormat = [[AVAudioFormat alloc] initWithStreamDescription:&sourceStreamDescription];

	_frameBuffer = [[AVAudioPCMBuffer alloc] initWithPCMFormat:_processingFormat frameCapacity:(AVAudioFrameCount)_blocksize];

	// Allocate decoding buffers
	_buffer = AllocateContiguous2DArray<int32_t>((size_t)_nchan, (size_t)(_blocksize + _nwrap));
	_offset = AllocateContiguous2DArray<int32_t>((size_t)_nchan, (size_t)std::max(1, _nmean));

	for(auto i = 0; i < _nchan; ++i) {
		for(auto j = 0; j < _nwrap; ++j) {
			_buffer[i][j] = 0;
		}
		_buffer[i] += _nwrap;
	}

	if(_maxnlpc > 0)
		_qlpc = new int [(size_t)_maxnlpc];

	// Initialize offset
	int32_t mean = 0;
	switch(_internal_ftype) {
//		case TYPE_AU1:
		case TYPE_S8:
		case TYPE_S16HL:
		case TYPE_S16LH:
//		case TYPE_ULAW:
//		case TYPE_AU2:
//		case TYPE_AU3:
//		case TYPE_ALAW:
			mean = 0;
			break;
		case TYPE_U8:
			mean = 0x80;
			break;
		case TYPE_U16HL:
		case TYPE_U16LH:
			mean = 0x8000;
			break;
		default:
			os_log_error(gSFBAudioDecoderLog, "Unsupported audio type: %u", _internal_ftype);
			return NO;
	}

	for(auto chan = 0; chan < _nchan; ++chan) {
		for(auto i = 0; i < std::max(1, _nmean); ++i) {
			_offset[chan][i] = mean;
		}
	}

	return YES;
}

- (BOOL)closeReturningError:(NSError **)error
{
	if(_buffer) {
		free(_buffer);
		_buffer = nullptr;
	}
	if(_offset) {
		free(_offset);
		_offset = nullptr;
	}
	if(_qlpc) {
		delete [] _qlpc;
		_qlpc = nullptr;
	}

	return [super closeReturningError:error];
}

- (BOOL)isOpen
{
	return _buffer != nullptr;
}

- (AVAudioFramePosition)framePosition
{
	return _framePosition;
}

- (AVAudioFramePosition)frameLength
{
	return _frameLength;
}

- (BOOL)decodeIntoBuffer:(AVAudioPCMBuffer *)buffer frameLength:(AVAudioFrameCount)frameLength error:(NSError **)error
{
	NSParameterAssert(buffer != nil);

	// Reset output buffer data size
	buffer.frameLength = 0;

	if(![buffer.format isEqual:_processingFormat]) {
		os_log_debug(gSFBAudioDecoderLog, "-decodeAudio:frameLength:error: called with invalid parameters");
		return NO;
	}

	if(frameLength > buffer.frameCapacity)
		frameLength = buffer.frameCapacity;

	AVAudioFrameCount framesProcessed = 0;

	for(;;) {
		AVAudioFrameCount framesRemaining = frameLength - framesProcessed;
		AVAudioFrameCount framesCopied = [buffer appendContentsOfBuffer:_frameBuffer readOffset:0 frameLength:framesRemaining];
		[_frameBuffer trimAtOffset:0 frameLength:framesCopied];

		framesProcessed += framesCopied;

		// All requested frames were read or EOS reached
		if(framesProcessed == frameLength || _eos)
			break;

		// Grab the next frame
		if(![self decodeFrameReturningError:error])
			os_log_error(gSFBAudioDecoderLog, "Error decoding Shorten frame");
	}

	_framePosition += framesProcessed;

	return YES;
}

- (BOOL)supportsSeeking
{
	// FIXME: Seek table support
	return NO;
}

- (BOOL)parseShortenHeaderReturningError:(NSError **)error
{
	// Read magic number
	uint32_t magic;
	if(![_inputSource readUInt32BigEndian:&magic error:nil] || magic != 'ajkg') {
		if(error)
			*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
											 code:SFBAudioDecoderErrorCodeInputOutput
					descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid Shorten file.", @"")
											  url:_inputSource.url
									failureReason:NSLocalizedString(@"Not a valid Shorten file", @"")
							   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
		return NO;
	}

	// Read file version
	uint8_t version;
	if(![_inputSource readUInt8:&version error:nil] || version < MIN_SUPPORTED_VERSION || version > MAX_SUPPORTED_VERSION) {
		os_log_error(gSFBAudioDecoderLog, "Unsupported version: %u", version);
		if(error)
			*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
											 code:SFBAudioDecoderErrorCodeInputOutput
					descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a supported Shorten file.", @"")
											  url:_inputSource.url
									failureReason:NSLocalizedString(@"Version not supported", @"")
							   recoverySuggestion:NSLocalizedString(@"The file's version is not supported.", @"")];
		return NO;
	}
	_version = version;

	// Default nmean
	_nmean = _version < 2 ? DEFAULT_V0NMEAN : DEFAULT_V2NMEAN;

	// Set up variable length reading callback
	_input.SetInputCallback(^bool(void *buf, size_t len, size_t &read) {
		NSInteger bytesRead;
		if(![self->_inputSource readBytes:buf length:(NSInteger)len bytesRead:&bytesRead error:nil])
			return false;
		read = (size_t)bytesRead;
		return true;
	});

	// Read internal file type
	uint32_t ftype;
	if(!_input.uint_get(ftype, _version, TYPESIZE)) {
		if(error)
			*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
											 code:SFBAudioDecoderErrorCodeInputOutput
					descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid Shorten file.", @"")
											  url:_inputSource.url
									failureReason:NSLocalizedString(@"Not a valid Shorten file", @"")
							   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
		return NO;
	}
	if(ftype != TYPE_U8 && ftype != TYPE_S8 && ftype != TYPE_U16HL && ftype != TYPE_U16LH && ftype != TYPE_S16HL && ftype != TYPE_S16LH) {
		os_log_error(gSFBAudioDecoderLog, "Unsupported audio type: %u", ftype);
		if(error)
			*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
											 code:SFBAudioDecoderErrorCodeInputOutput
					descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid Shorten file.", @"")
											  url:_inputSource.url
									failureReason:NSLocalizedString(@"Invalid or unsupported audio type", @"")
							   recoverySuggestion:NSLocalizedString(@"The file contains an invalid or unsupported audio type.", @"")];
		return NO;
	}
	_internal_ftype = (int)ftype;

	// Read number of channels
	uint32_t nchan;
	if(!_input.uint_get(nchan, _version, CHANSIZE) || nchan == 0 || nchan > MAX_CHANNELS) {
		os_log_error(gSFBAudioDecoderLog, "Invalid or unsupported channel count: %u", nchan);
		if(error)
			*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
											 code:SFBAudioDecoderErrorCodeInputOutput
					descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid Shorten file.", @"")
											  url:_inputSource.url
									failureReason:NSLocalizedString(@"Invalid or unsupported number of channels", @"")
							   recoverySuggestion:NSLocalizedString(@"The file contains an invalid or unsupported number of channels.", @"")];
		return NO;
	}
	_nchan = (int)nchan;

	// Read blocksize if version > 0
	if(_version > 0) {
		uint32_t blocksize;
		if(!_input.uint_get(blocksize, _version, (size_t)log2(DEFAULT_BLOCK_SIZE)) || blocksize == 0 || blocksize > MAX_BLOCKSIZE) {
			os_log_error(gSFBAudioDecoderLog, "Invalid or unsupported block size: %u", blocksize);
			if(error)
				*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
												 code:SFBAudioDecoderErrorCodeInputOutput
						descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid Shorten file.", @"")
												  url:_inputSource.url
										failureReason:NSLocalizedString(@"Invalid or unsupported block size", @"")
								   recoverySuggestion:NSLocalizedString(@"The file contains an invalid or unsupported block size.", @"")];
			return NO;
		}
		_blocksize = (int)blocksize;

		uint32_t maxnlpc = 0;
		if(!_input.uint_get(maxnlpc, _version, LPCQSIZE) || maxnlpc > 1024) {
			os_log_error(gSFBAudioDecoderLog, "Invalid maxnlpc: %u", maxnlpc);
			if(error)
				*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
												 code:SFBAudioDecoderErrorCodeInputOutput
						descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid Shorten file.", @"")
												  url:_inputSource.url
										failureReason:NSLocalizedString(@"Not a valid Shorten file", @"")
								   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
			return NO;
		}
		_maxnlpc = (int)maxnlpc;

		uint32_t nmean;
		if(!_input.uint_get(nmean, _version, 0) || nmean > 32768) {
			os_log_error(gSFBAudioDecoderLog, "Invalid nmean: %u", nmean);
			if(error)
				*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
												 code:SFBAudioDecoderErrorCodeInputOutput
						descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid Shorten file.", @"")
												  url:_inputSource.url
										failureReason:NSLocalizedString(@"Not a valid Shorten file", @"")
								   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
			return NO;
		}
		_nmean = (int)nmean;

		uint32_t nskip;
		if(!_input.uint_get(nskip, _version, NSKIPSIZE) /* || nskip > bits_remaining_in_input */) {
			if(error)
				*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
												 code:SFBAudioDecoderErrorCodeInputOutput
						descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid Shorten file.", @"")
												  url:_inputSource.url
										failureReason:NSLocalizedString(@"Not a valid Shorten file", @"")
								   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
			return NO;
		}

		for(uint32_t i = 0; i < nskip; ++i) {
			uint32_t dummy;
			if(!_input.uint_get(dummy, _version, XBYTESIZE)) {
				if(error)
					*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
													 code:SFBAudioDecoderErrorCodeInputOutput
							descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid Shorten file.", @"")
													  url:_inputSource.url
											failureReason:NSLocalizedString(@"Not a valid Shorten file", @"")
									   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
				return NO;
			}
		}
	}
	else {
		_blocksize = DEFAULT_BLOCK_SIZE;
		_maxnlpc = DEFAULT_MAXNLPC;
	}

	_nwrap = std::max(NWRAP, (int)_maxnlpc);

	if(_version > 1)
		_lpcqoffset = V2LPCQOFFSET;

	// Parse the WAVE or AIFF header in the verbatim section

	int32_t fn;
	if(!_input.uvar_get(fn, FNSIZE) || fn != FN_VERBATIM) {
		os_log_error(gSFBAudioDecoderLog, "Missing initial verbatim section");
		if(error)
			*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
											 code:SFBAudioDecoderErrorCodeInputOutput
					descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a supported Shorten file.", @"")
											  url:_inputSource.url
									failureReason:NSLocalizedString(@"Missing initial verbatim section", @"")
							   recoverySuggestion:NSLocalizedString(@"The file is missing the initial verbatim section.", @"")];
		return NO;
	}

	int32_t header_size;
	if(!_input.uvar_get(header_size, VERBATIM_CKSIZE_SIZE) || header_size < CANONICAL_HEADER_SIZE || header_size > VERBATIM_CHUNK_MAX) {
		os_log_error(gSFBAudioDecoderLog, "Incorrect header size: %u", header_size);
		if(error)
			*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
											 code:SFBAudioDecoderErrorCodeInputOutput
					descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid Shorten file.", @"")
											  url:_inputSource.url
									failureReason:NSLocalizedString(@"Not a valid Shorten file", @"")
							   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
		return NO;
	}

	int8_t header_bytes [header_size];
	for(int32_t i = 0; i < header_size; ++i) {
		int32_t byte;
		if(!_input.uvar_get(byte, VERBATIM_BYTE_SIZE)) {
			if(error)
				*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
												 code:SFBAudioDecoderErrorCodeInputOutput
						descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid Shorten file.", @"")
												  url:_inputSource.url
										failureReason:NSLocalizedString(@"Not a valid Shorten file", @"")
								   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
			return NO;
		}

		header_bytes[i] = (int8_t)byte;
	}

	ByteStream chunkData{header_bytes, (size_t)header_size};
	auto chunkID = chunkData.ReadBE32();

	// Skip chunk size
	chunkData.Skip(4);

	// WAVE
	if(chunkID == 'RIFF') {
		if(![self parseRIFFChunk:chunkData error:error])
			return NO;
	}
	// AIFF
	else if(chunkID == 'FORM') {
		if(![self parseFORMChunk:chunkData error:error])
			return NO;
	}
	else {
		os_log_error(gSFBAudioDecoderLog, "Unsupported data format: %u", chunkID);
		if(error)
			*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
											 code:SFBAudioDecoderErrorCodeInputOutput
					descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a supported Shorten file.", @"")
											  url:_inputSource.url
									failureReason:NSLocalizedString(@"Unsupported data format", @"")
							   recoverySuggestion:NSLocalizedString(@"The file's data format is not supported.", @"")];
		return NO;
	}

	return YES;
}

- (BOOL)parseRIFFChunk:(ByteStream&)chunkData error:(NSError **)error
{
	auto chunkID = chunkData.ReadBE32();
	if(chunkID != 'WAVE') {
		os_log_error(gSFBAudioDecoderLog, "Missing 'WAVE' in 'RIFF' chunk");
		if(error)
			*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
											 code:SFBAudioDecoderErrorCodeInputOutput
					descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid Shorten file.", @"")
											  url:_inputSource.url
									failureReason:NSLocalizedString(@"Not a valid Shorten file", @"")
							   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
		return NO;
	}

	// Skip unknown chunks, looking for 'fmt '
	while((chunkID = chunkData.ReadBE32()) != 'fmt ') {
		auto len = chunkData.ReadLE32();
		chunkData.Skip(len);
		if(chunkData.Remaining() < 16) {
			os_log_error(gSFBAudioDecoderLog, "Missing 'fmt ' chunk");
			if(error)
				*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
												 code:SFBAudioDecoderErrorCodeInputOutput
						descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid Shorten file.", @"")
												  url:_inputSource.url
										failureReason:NSLocalizedString(@"Not a valid Shorten file", @"")
								   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
			return NO;
		}
	}

	auto len = chunkData.ReadLE32();
	if(len < 16) {
		os_log_error(gSFBAudioDecoderLog, "'fmt ' chunk is too small (%u bytes)", len);
		if(error)
			*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
											 code:SFBAudioDecoderErrorCodeInputOutput
					descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid Shorten file.", @"")
											  url:_inputSource.url
									failureReason:NSLocalizedString(@"Not a valid Shorten file", @"")
							   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
		return NO;
	}

	auto format_tag = chunkData.ReadLE16();
	if(format_tag != WAVE_FORMAT_PCM) {
		os_log_error(gSFBAudioDecoderLog, "Unsupported WAVE format tag: %x", format_tag);
		if(error)
			*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
											 code:SFBAudioDecoderErrorCodeInputOutput
					descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a supported Shorten file.", @"")
											  url:_inputSource.url
									failureReason:NSLocalizedString(@"Unsupported WAVE format tag", @"")
							   recoverySuggestion:NSLocalizedString(@"The file's WAVE format tag is not supported.", @"")];
		return NO;
	}

	auto channels = chunkData.ReadLE16();
	if(_nchan != channels)
		os_log_info(gSFBAudioDecoderLog, "Channel count mismatch between Shorten (%d) and 'fmt ' chunk (%u)", _nchan, channels);
	_sampleRate = chunkData.ReadLE32();
	chunkData.Skip(4); // average bytes per second
	chunkData.Skip(2); // block align
	_bitsPerSample = chunkData.ReadLE16();

	if(len > 16)
		os_log_info(gSFBAudioDecoderLog, "%u bytes in 'fmt ' chunk not parsed", len - 16);

	return YES;
}

- (BOOL)parseFORMChunk:(ByteStream&)chunkData error:(NSError **)error
{
	auto chunkID = chunkData.ReadBE32();
	if(chunkID != 'AIFF' && chunkID != 'AIFC') {
		os_log_error(gSFBAudioDecoderLog, "Missing 'AIFF' or 'AIFC' in 'FORM' chunk");
		if(error)
			*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
											 code:SFBAudioDecoderErrorCodeInputOutput
					descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid Shorten file.", @"")
											  url:_inputSource.url
									failureReason:NSLocalizedString(@"Not a valid Shorten file", @"")
							   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
		return NO;
	}

	// Skip unknown chunks, looking for 'COMM'
	while((chunkID = chunkData.ReadBE32()) != 'COMM') {
		auto len = chunkData.ReadBE32();
		// pad byte not included in ckLen
		if((int32_t)len < 0 || chunkData.Remaining() < 18 + len + (len & 1)) {
			os_log_error(gSFBAudioDecoderLog, "Missing 'COMM' chunk");
			if(error)
				*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
												 code:SFBAudioDecoderErrorCodeInputOutput
						descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid Shorten file.", @"")
												  url:_inputSource.url
										failureReason:NSLocalizedString(@"Not a valid Shorten file", @"")
								   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
			return NO;
		}
		chunkData.Skip(len + (len & 1));
	}

	auto len = chunkData.ReadBE32();
	if((int32_t)len < 18) {
		os_log_error(gSFBAudioDecoderLog, "'COMM' chunk is too small (%u bytes)", len);
		if(error)
			*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
											 code:SFBAudioDecoderErrorCodeInputOutput
					descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid Shorten file.", @"")
											  url:_inputSource.url
									failureReason:NSLocalizedString(@"Not a valid Shorten file", @"")
							   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
		return NO;
	}

	auto channels = chunkData.ReadBE16();
	if(_nchan != channels)
		os_log_info(gSFBAudioDecoderLog, "Channel count mismatch between Shorten (%d) and 'COMM' chunk (%u)", _nchan, channels);

	chunkData.Skip(4); // numSampleFrames

	_bitsPerSample = chunkData.ReadBE16();

	// sample rate is IEEE 754 80-bit extended float (16-bit exponent, 1-bit integer part, 63-bit fraction)
	auto exp = (int16_t)chunkData.ReadBE16() - 16383 - 63;
	if(exp < -63 || exp > 63) {
		os_log_error(gSFBAudioDecoderLog, "exp out of range: %d", exp);
		if(error)
			*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
											 code:SFBAudioDecoderErrorCodeInputOutput
					descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid Shorten file.", @"")
											  url:_inputSource.url
									failureReason:NSLocalizedString(@"Not a valid Shorten file", @"")
							   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
		return NO;
	}

	auto frac = chunkData.ReadBE64();
	if(exp >= 0)
		_sampleRate = (uint32_t)(frac << exp);
	else
		_sampleRate = (uint32_t)((frac + (1 << (-frac - 1))) >> -frac);

	if(len > 18)
		os_log_info(gSFBAudioDecoderLog, "%u bytes in 'COMM' chunk not parsed", len - 16);

	return YES;
}

- (BOOL)decodeFrameReturningError:(NSError **)error
{
	int chan = 0;
	for(;;) {
		int32_t cmd;
		if(!_input.uvar_get(cmd, FNSIZE)) {
			if(error)
				*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
												 code:SFBAudioDecoderErrorCodeInputOutput
						descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid Shorten file.", @"")
												  url:_inputSource.url
										failureReason:NSLocalizedString(@"Not a valid Shorten file", @"")
								   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
			return NO;
		}

		if(cmd == FN_QUIT) {
			_eos = true;
			return YES;
		}

		switch(cmd) {
			case FN_ZERO:
			case FN_DIFF0:
			case FN_DIFF1:
			case FN_DIFF2:
			case FN_DIFF3:
			case FN_QLPC:
			{
				int32_t coffset, *cbuffer = _buffer[chan];
				int resn = 0, nlpc;

				if(cmd != FN_ZERO) {
					if(!_input.uvar_get(resn, ENERGYSIZE)) {
						if(error)
							*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
															 code:SFBAudioDecoderErrorCodeInputOutput
									descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid Shorten file.", @"")
															  url:_inputSource.url
													failureReason:NSLocalizedString(@"Not a valid Shorten file", @"")
											   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
						return NO;
					}
					/* this is a hack as version 0 differed in definition of var_get */
					if(_version == 0)
						resn--;
				}

				/* find mean offset : N.B. this code duplicated */
				if(_nmean == 0)
					coffset = _offset[chan][0];
				else
				{
					int32_t sum = (_version < 2) ? 0 : _nmean / 2;
					for(auto i = 0; i < _nmean; i++) {
						sum += _offset[chan][i];
					}
					if(_version < 2)
						coffset = sum / _nmean;
					else
						coffset = ROUNDEDSHIFTDOWN(sum / _nmean, _bitshift);
				}

				switch(cmd)
				{
					case FN_ZERO:
						for(auto i = 0; i < _blocksize; ++i) {
							cbuffer[i] = 0;
						}
						break;
					case FN_DIFF0:
						for(auto i = 0; i < _blocksize; ++i) {
							int32_t var;
							if(!_input.var_get(var, (size_t)resn)) {
								if(error)
									*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
																	 code:SFBAudioDecoderErrorCodeInputOutput
											descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid Shorten file.", @"")
																	  url:_inputSource.url
															failureReason:NSLocalizedString(@"Not a valid Shorten file", @"")
													   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
								return NO;
							}
							cbuffer[i] = var + coffset;
						}
						break;
					case FN_DIFF1:
						for(auto i = 0; i < _blocksize; ++i) {
							int32_t var;
							if(!_input.var_get(var, (size_t)resn)) {
								if(error)
									*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
																	 code:SFBAudioDecoderErrorCodeInputOutput
											descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid Shorten file.", @"")
																	  url:_inputSource.url
															failureReason:NSLocalizedString(@"Not a valid Shorten file", @"")
													   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
								return NO;
							}
							cbuffer[i] = var + cbuffer[i - 1];
						}
						break;
					case FN_DIFF2:
						for(auto i = 0; i < _blocksize; ++i) {
							int32_t var;
							if(!_input.var_get(var, (size_t)resn)) {
								if(error)
									*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
																	 code:SFBAudioDecoderErrorCodeInputOutput
											descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid Shorten file.", @"")
																	  url:_inputSource.url
															failureReason:NSLocalizedString(@"Not a valid Shorten file", @"")
													   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
								return NO;
							}
							cbuffer[i] = var + (2 * cbuffer[i - 1] - cbuffer[i - 2]);
						}
						break;
					case FN_DIFF3:
						for(auto i = 0; i < _blocksize; ++i) {
							int32_t var;
							if(!_input.var_get(var, (size_t)resn)) {
								if(error)
									*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
																	 code:SFBAudioDecoderErrorCodeInputOutput
											descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid Shorten file.", @"")
																	  url:_inputSource.url
															failureReason:NSLocalizedString(@"Not a valid Shorten file", @"")
													   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
								return NO;
							}
							cbuffer[i] = var + 3 * (cbuffer[i - 1] -  cbuffer[i - 2]) + cbuffer[i - 3];
						}
						break;
					case FN_QLPC:
						if(!_input.uvar_get(nlpc, LPCQSIZE)) {
							if(error)
								*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
																 code:SFBAudioDecoderErrorCodeInputOutput
										descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid Shorten file.", @"")
																  url:_inputSource.url
														failureReason:NSLocalizedString(@"Not a valid Shorten file", @"")
												   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
							return NO;
						}

						for(auto i = 0; i < nlpc; ++i) {
							if(!_input.var_get(_qlpc[i], LPCQUANT)) {
								if(error)
									*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
																	 code:SFBAudioDecoderErrorCodeInputOutput
											descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid Shorten file.", @"")
																	  url:_inputSource.url
															failureReason:NSLocalizedString(@"Not a valid Shorten file", @"")
													   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
								return NO;
							}
						}
						for(auto i = 0; i < nlpc; ++i) {
							cbuffer[i - nlpc] -= coffset;
						}
						for(auto i = 0; i < _blocksize; ++i) {
							int32_t sum = _lpcqoffset;

							for(auto j = 0; j < nlpc; ++j) {
								sum += _qlpc[j] * cbuffer[i - j - 1];
							}
							int32_t var;
							if(!_input.var_get(var, (size_t)resn)) {
								if(error)
									*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
																	 code:SFBAudioDecoderErrorCodeInputOutput
											descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid Shorten file.", @"")
																	  url:_inputSource.url
															failureReason:NSLocalizedString(@"Not a valid Shorten file", @"")
													   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
								return NO;
							}
							cbuffer[i] = var + (sum >> LPCQUANT);
						}
						if(coffset != 0) {
							for(auto i = 0; i < _blocksize; ++i) {
								cbuffer[i] += coffset;
							}
						}
						break;
				}

				/* store mean value if appropriate : N.B. Duplicated code */
				if(_nmean > 0) {
					int32_t sum = (_version < 2) ? 0 : _blocksize / 2;

					for(auto i = 0; i < _blocksize; ++i) {
						sum += cbuffer[i];
					}

					for(auto i = 1; i < _nmean; ++i) {
						_offset[chan][i - 1] = _offset[chan][i];
					}
					if(_version < 2)
						_offset[chan][_nmean - 1] = sum / _blocksize;
					else
						_offset[chan][_nmean - 1] = (sum / _blocksize) << _bitshift;
				}

				/* do the wrap */
				for(auto i = -_nwrap; i < 0; i++) {
					cbuffer[i] = cbuffer[i + _blocksize];
				}

				fix_bitshift(cbuffer, _blocksize, _bitshift, _internal_ftype);

				if(chan == _nchan - 1) {
					switch(_internal_ftype) {
						case TYPE_U8:
						{
							auto abl = _frameBuffer.audioBufferList;
							for(auto channel = 0; channel < _nchan; ++channel) {
								auto channel_buf = (uint8_t *)abl->mBuffers[channel].mData;
								for(auto sample = 0; sample < _blocksize; ++sample) {
									channel_buf[sample] = (uint8_t)clip(_buffer[channel][sample], 0, UINT8_MAX);
								}
							}
							_frameBuffer.frameLength = (AVAudioFrameCount)_blocksize;
							break;
						}
						case TYPE_S8:
						{
							auto abl = _frameBuffer.audioBufferList;
							for(auto channel = 0; channel < _nchan; ++channel) {
								auto channel_buf = (int8_t *)abl->mBuffers[channel].mData;
								for(auto sample = 0; sample < _blocksize; ++sample) {
									channel_buf[sample] = (int8_t)clip(_buffer[channel][sample], INT16_MIN, INT16_MAX);
								}
							}
							_frameBuffer.frameLength = (AVAudioFrameCount)_blocksize;
							break;
						}
						case TYPE_U16HL:
						case TYPE_U16LH:
						{
							auto abl = _frameBuffer.audioBufferList;
							for(auto channel = 0; channel < _nchan; ++channel) {
								auto channel_buf = (uint16_t *)abl->mBuffers[channel].mData;
								for(auto sample = 0; sample < _blocksize; ++sample) {
									channel_buf[sample] = (uint16_t)clip(_buffer[channel][sample], 0, UINT16_MAX);
								}
							}
							_frameBuffer.frameLength = (AVAudioFrameCount)_blocksize;
							break;
						}
						case TYPE_S16HL:
						case TYPE_S16LH:
						{
							auto abl = _frameBuffer.audioBufferList;
							for(auto channel = 0; channel < _nchan; ++channel) {
								auto channel_buf = (int16_t *)abl->mBuffers[channel].mData;
								for(auto sample = 0; sample < _blocksize; ++sample) {
									channel_buf[sample] = (int16_t)clip(_buffer[channel][sample], INT16_MIN, INT16_MAX);
								}
							}
							_frameBuffer.frameLength = (AVAudioFrameCount)_blocksize;
							break;
						}
					}

					return YES;
				}
				chan = (chan + 1) % _nchan;
				break;
			}

			case FN_BLOCKSIZE:
			{
				uint32_t uint;
				if(!_input.uint_get(uint, _version, (size_t)log2(_blocksize)) || uint == 0 || uint > MAX_BLOCKSIZE || (int)uint > _blocksize) {
					os_log_error(gSFBAudioDecoderLog, "Invalid or unsupported block size: %u", uint);
					if(error)
						*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
														 code:SFBAudioDecoderErrorCodeInputOutput
								descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid Shorten file.", @"")
														  url:_inputSource.url
												failureReason:NSLocalizedString(@"Invalid or unsupported block size", @"")
										   recoverySuggestion:NSLocalizedString(@"The file contains an invalid or unsupported block size.", @"")];
					return NO;
				}
				_blocksize = (int)uint;
				break;
			}
			case FN_BITSHIFT:
				if(!_input.uvar_get(_bitshift, BITSHIFTSIZE) || _bitshift > 32) {
					os_log_error(gSFBAudioDecoderLog, "Invald or unsupported bitshift: %u", _bitshift);
					if(error)
						*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
														 code:SFBAudioDecoderErrorCodeInputOutput
								descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid Shorten file.", @"")
														  url:_inputSource.url
												failureReason:NSLocalizedString(@"Invalid or unsupported bitshift", @"")
										   recoverySuggestion:NSLocalizedString(@"The file contains an invalid or unsupported bitshift.", @"")];
					return NO;
				}
				break;
			case FN_VERBATIM:
			{
				int32_t chunk_len;
				if(!_input.uvar_get(chunk_len, VERBATIM_CKSIZE_SIZE) || chunk_len < 0 || chunk_len > VERBATIM_CHUNK_MAX) {
					os_log_error(gSFBAudioDecoderLog, "Invald verbatim length: %u", chunk_len);
					if(error)
						*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
														 code:SFBAudioDecoderErrorCodeInputOutput
								descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid Shorten file.", @"")
														  url:_inputSource.url
												failureReason:NSLocalizedString(@"Not a valid Shorten file", @"")
										   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
					return NO;
				}
				while(chunk_len--) {
					int32_t dummy;
					if(!_input.uvar_get(dummy, VERBATIM_BYTE_SIZE)) {
						if(error)
							*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
															 code:SFBAudioDecoderErrorCodeInputOutput
									descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid Shorten file.", @"")
															  url:_inputSource.url
													failureReason:NSLocalizedString(@"Not a valid Shorten file", @"")
											   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
						return NO;
					}
				}
				break;
			}

			default:
				os_log_error(gSFBAudioDecoderLog, "sanity check failed for function: %d", cmd);
				if(error)
					*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
													 code:SFBAudioDecoderErrorCodeInputOutput
							descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid Shorten file.", @"")
													  url:_inputSource.url
											failureReason:NSLocalizedString(@"Not a valid Shorten file", @"")
									   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
				return NO;
		}
	}

	return YES;
}

@end
