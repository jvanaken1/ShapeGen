/*
  Copyright (C) 2022 Jerry R. VanAken

  This software is provided 'as-is', without any express or implied
  warranty. In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.

  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.

  3. This notice may not be removed or altered from any source distribution.
*/
//---------------------------------------------------------------------
//
// bmpfile.cpp:
//   This file contains the code for a bitmap file reader (for image
//   files with .bmp filename extension). The BmpReader class inherits
//   from the base class ImageReader that defined to supply image data
//   to TiledPattern objects, defined in render.h.
//
//---------------------------------------------------------------------

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "demo.h"

//---------------------------------------------------------------------
//
// The following structures are defined for .bmp files
//
//---------------------------------------------------------------------

typedef unsigned short WORD;
typedef unsigned int DWORD;
typedef int LONG;

// Values for biCompression field
const int BI_RGB = 0;
const int BI_RLE8 = 1;
const int BI_RLE4 = 2;
const int BI_BITFIELDS = 3;

// Make sure that packing alignment setting for structs
// enables bfSize field to immediately follow bfType
#pragma pack(push,2)
struct BITMAPFILEHEADER {
    WORD  bfType;        // bitmap file magic 'BM'
    DWORD bfSize;        // file size (in bytes)
    WORD  bfReserved1;   //
    WORD  bfReserved2;   //
    DWORD bfOffBits;     // byte offset to pixel data
};
#pragma pack(pop)

struct BITMAPINFOHEADER {
    DWORD   biSize;           // size of this header (in bytes)
    LONG    biWidth;          // width of bitmap (in pixels)
    LONG    biHeight;         // height of bitmap (in pixels)
    WORD    biPlanes;         // number of planes (set to 1)
    WORD    biBitCount;       // bits per pixel
    DWORD   biCompression;    // pixel format (BI_RGB, etc.)
    DWORD   biSizeImage;      // 0 (if uncompressed)
    LONG    biXPelsPerMeter;  // horizontal pixels per meter
    LONG    biYPelsPerMeter;  // vertical pixels per meter
    DWORD   biClrUsed;        // color table, number of colors
    DWORD   biClrImportant;   // color table, important colors
};

typedef int FXPT2DOT30;

struct CIEXYZ {
    FXPT2DOT30 ciexyzX;
    FXPT2DOT30 ciexyzY;
    FXPT2DOT30 ciexyzZ;
};

struct CIEXYZTRIPLE {
    CIEXYZ ciexyzRed;
    CIEXYZ ciexyzGreen;
    CIEXYZ ciexyzBlue;
};

struct BITMAPV4HEADER {
    DWORD    biSize;
    LONG     biWidth;            
    LONG     biHeight;           
    WORD     biPlanes;           
    WORD     biBitCount;         
    DWORD    biCompression;     
    DWORD    biSizeImage;        
    LONG     biXPelsPerMeter;    
    LONG     biYPelsPerMeter;    
    DWORD    biClrUsed;          
    DWORD    biClrImportant;  // <-- BITMAPINFOHEADER ends here   
    DWORD    biRedMask;
    DWORD    biGreenMask;
    DWORD    biBlueMask;
    DWORD    biAlphaMask;
    DWORD    biCSType;
    CIEXYZTRIPLE  biEndpoints;
    DWORD    biGammaRed;
    DWORD    biGammaGreen;
    DWORD    biGammaBlue;
};

struct BITMAPV5HEADER {
    DWORD    biSize;
    LONG     biWidth;
    LONG     biHeight;
    WORD     biPlanes;
    WORD     biBitCount;
    DWORD    biCompression;
    DWORD    biSizeImage;
    LONG     biXPelsPerMeter;
    LONG     biYPelsPerMeter;
    DWORD    biClrUsed;
    DWORD    biClrImportant;  // <-- BITMAPINFOHEADER ends here
    DWORD    biRedMask;
    DWORD    biGreenMask;
    DWORD    biBlueMask;
    DWORD    biAlphaMask;
    DWORD    biCSType;
    CIEXYZTRIPLE  biEndpoints;
    DWORD    biGammaRed;
    DWORD    biGammaGreen;
    DWORD    biGammaBlue;  // <-- BITMAPV4HEADER ends here
    DWORD    biIntent;
    DWORD    biProfileData;
    DWORD    biProfileSize;
    DWORD    biReserved;
};

//---------------------------------------------------------------------
//
// Class BmpReader functions:
//   Used to read pixel data serially from a .bmp file. See the
//   BmpReader class definition in demo.h.
//
//   A BmpReader object always converts the pixel data from the .bmp
//   file to either a 32-bit RGBA (i.e., 0xaabbggrr) or a 32-bit BGRA
//   (0xaarrggbb) format, as is required by the TiledPattern class
//   defined in renderer.h. This file reader can handle only .bmp
//   files that use 24-bit or 32-bit uncompressed formats for pixel
//   data.
//
//---------------------------------------------------------------------

BmpReader::BmpReader(const char *pszFile, UserMessage *umsg)
                     : _umsg(umsg), _width(0), _height(0), 
                       _bpp(0), _flags(0), _offset(0), _pad(0), 
                       _bAlpha(false), _row(0), _col(0)
{
    char *pszError = 0;
    
    do
    {
        BITMAPFILEHEADER hdr;
        BITMAPV5HEADER info;
    
        memset(&hdr, 0, sizeof(hdr));
        memset(&info, 0, sizeof(info));
        _pFile = fopen(pszFile, "rb");
        if (_pFile == 0)
        {
            pszError = "Input file not found";
            break;
        }
    
        // Inspect header to verify that file type is .bmp
        if (fread(&hdr, sizeof(hdr), 1, _pFile) < 1)
        {
            pszError = "Input file is too short";
            break;
        }
        char *bfMagic = reinterpret_cast<char*>(&hdr.bfType);
        if (strncmp("BM", bfMagic, 2) != 0)
        {
            pszError = "Input file type is not supported";
            break;
        }
        _offset = hdr.bfOffBits;  // file offset to pixel data
    
        // Read in the size field of the info header
        if (fread(&info.biSize, sizeof(info.biSize), 1, _pFile) < 1)
        {
            pszError = "Input file is too short";
            break;
        }
    
        // Verify that the info header structure is supported
        if (info.biSize != sizeof(BITMAPINFOHEADER) &&
            info.biSize != sizeof(BITMAPV4HEADER) &&
            info.biSize != sizeof(BITMAPV5HEADER))
        {
            pszError = "File info header type is unsupported";
            break;
        }
    
        // Read in the rest of the info header structure
        int nbytes = info.biSize - sizeof(info.biSize);
        if (fread(&info.biWidth, nbytes, 1, _pFile) < 1)
        {
            pszError = "Input file is too short";
            break;
        }
        _width = info.biWidth;
        _height = info.biHeight;
        if (_width <= 0 || _height == 0)
        {
            pszError = "Bad value in file info header";
            break;
        }
    
        // For now, we don't support compressed pixel formats
        if (info.biCompression != BI_RGB &&
            info.biCompression != BI_BITFIELDS)
        {
            pszError = "Input file has unsupported pixel data format";
            break;
        }
    
        // If necessary, read in the R, G, and B color masks
        if (info.biSize == sizeof(BITMAPINFOHEADER) &&
            info.biCompression == BI_BITFIELDS &&
            fread(&info.biRedMask, 3*sizeof(DWORD), 1, _pFile) < 1)
        {
            pszError = "Input file is too short";
            break;
        }
    
        // Verify that we support the .bmp file's pixel format
        _bpp = info.biBitCount;
        if (_bpp != 24 && _bpp != 32)
        {
            pszError = "Input file has unsupported pixel data format";
            break;
        }
        if (info.biCompression == BI_BITFIELDS &&
            (info.biGreenMask != 0x0000ff00 ||
            (info.biRedMask | info.biBlueMask) != 0x00ff00ff))
        {
            pszError = "Input file has unsupported pixel data format";
            break;
        }
        if (info.biCompression == BI_RGB ||
            (info.biCompression == BI_BITFIELDS &&
             info.biBlueMask == 0x000000ff))
        {
            _flags |= FLAG_IMAGE_BGRA32;
        }
        _bAlpha = (info.biAlphaMask == 0xff000000);
        int stride = ((((_width * _bpp) + 31) & ~31) >> 3);
        _pad = stride - (_bpp >> 3)*_width;
        if (_height < 0)
            _height = -_height;
        else
            _flags |= FLAG_IMAGE_BOTTOMUP;
    
        // Sanity checks
        if (hdr.bfSize < _width*_height*(_bpp >> 3))
        {
            pszError = "Bad value in file info header";
            break;
        }
        if (_width > 5000 || _height > 5000)
        {
            pszError = "File image dimensions are excessively large";
            break;
        }
    
        // Move file position to start of pixel data
        RewindData();
    } while (0);
    if (pszError)
    {
        ErrorMessage(pszError);
        _width = _height = 0;
        if (_pFile)
        {
            fclose(_pFile);
            _pFile = 0;
        }
    }
}

BmpReader::~BmpReader()
{
    if (_pFile)
        fclose(_pFile);
}

void BmpReader::ErrorMessage(char *pszError)
{
    if (_umsg)
        _umsg->MessageOut(pszError, "Error", 0);
}

// Returns the image info flags, and writes the image width and
// height to the output pointer args. If the constructor has
// recognized the .bmp file as valid, the retrieved width and
// height are both nonzero; otherwise, the width and height
// values are both zero.
int BmpReader::GetImageInfo(int *width, int *height)
{
   *width  = _width;
   *height = _height;
   return _flags;
}

// Reads a block of pixel data from a .bmp file and writes it to a
// caller-supplied buffer. The 'count' parameter specifies the
// number of pixels to copy from the bitmap file to the specified
// 'buffer' array. The return value is the number of pixels the
// function has copied, which can be less than 'count' if the source
// supply of pixels is low, or zero if the source is empty. The
// function converts each pixel read from the file to a 32-bit BGRA
// format (that is, 0xaarrggbb) before writing it to the buffer.
int BmpReader::ReadPixels(COLOR *buffer, int count)
{
    int k;
    COLOR *pOut = 0;
    COLOR tinybuf = 0;

    if (_pFile == 0)
        return 0;

    if (_bpp == 24)
    {
        tinybuf = 0xff000000;  // tiny buffer holds 1 pixel

        // Each iteration of this for-loop reads one 24-bit pixel from
        // the .bmp file, converts it to 32 bits, and writes it to the
        // buffer
        pOut = &buffer[0];
        for (k = 0; k < count; ++k)
        {
            if (_col >= _width)  // at end of row?
            {
                ++_row;  // yes, start next row
                if (_row >= _height)  // at end of bitmap? 
                    return k;  // all done
    
                // Skip past padding at end of row. Padding is never
                // more than three bytes. Note that we avoid trying
                // to read any padding past the end of the last row.
                if (_pad > 0)
                {
                    if (fread(&tinybuf, sizeof(char), _pad, _pFile) != _pad)
                    {
                        ErrorMessage("Error reading pixel data from file");
                        return k;
                    }
                }
                _col = 0;  // start at column zero of new row
            }
            if (fread(&tinybuf, 3, 1, _pFile) < 1)
            {
                ErrorMessage("Read request extends past end of file");
                return k;
            }
            *pOut++ = tinybuf;
            ++_col;
        }
        return k;
    }
    else if (_bpp == 32)
    {
        assert(_pad == 0);
        if (_bAlpha)
        {    
            // Easy case: 32-bit source pixels have 8-bit alphas
            k = fread(&buffer[0], 4, count, _pFile);
            if (k < count)
                ErrorMessage("Read request extends past end of file");
    
            return k;
        }
        pOut = &buffer[0];
        for (k = 0; k < count; ++k)
        {
            if (fread(&tinybuf, 4, 1, _pFile) < 1)
            {
                ErrorMessage("Read request extends past end of file");
                return k;
            }
            *pOut++ = tinybuf | 0xff000000;  // set alpha field to 255
        }
        return count;
    }
    return 0;
}

// Rewinds the .bmp file to the start of the pixel data
int BmpReader::RewindData()
{
    _row = _col = 0;
    return fseek(_pFile, _offset, SEEK_SET);
}

