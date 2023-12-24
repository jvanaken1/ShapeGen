/*
  Copyright (C) 2019-2022 Jerry R. VanAken

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
// demo.h:
//   Header file for demo code. This header is included by the
//   ShapeGen demo code and code examples in demo.cpp.
//
//---------------------------------------------------------------------

#ifndef DEMO_H
  #define DEMO_H

#include <stdio.h>
#include "renderer.h"

// Dimensions of window for demo functions
const int DEMO_WIDTH  = 1280;
const int DEMO_HEIGHT =  960;

// Make command-line args globally accessible
extern int _argc_;
extern char **_argv_;

//---------------------------------------------------------------------
//
// Pixels and RGBA components
//
//---------------------------------------------------------------------

// Extract the r, g, and b components from a pixel value
inline void GetRgbValues(const COLOR color, int *r, int *g, int *b)
{
    *r = color & 0xff;
    *g = (color >> 8) & 0xff;
    *b = (color >> 16) & 0xff;
}

//---------------------------------------------------------------------
//
// Runs the graphics test function specified by 'testnum' (an array
// index). Parameter rend is a basic renderer that does solid color
// fills with no antialiasing. Parameter aarend is an enhanced
// renderer that does antialiasing, gradient fills, and pattern
// fills. Parameter cliprect specifies the device clipping rectangle.
//
//---------------------------------------------------------------------

extern int RunTest(int testnum, const PIXEL_BUFFER& bkbuf, const SGRect& cliprect);

//---------------------------------------------------------------------
//
// Class UserMessage: Shows text message to user
//
//---------------------------------------------------------------------

const int MESSAGECODE_ERROR = 0;
const int MESSAGECODE_WARNING = 1;
const int MESSAGECODE_INFORMATION = 2;

class UserMessage
{
public:
    UserMessage() {}
    ~UserMessage() {}
    void ShowMessage(char *text, char *caption, int msgcode = 0);
};

//---------------------------------------------------------------------
//
// Class BmpReader:
//   Reads pixel data serially from a .bmp file. Supplies image data
//   to TiledPattern objects. Inherits from ImageReader class defined
//   in render.h.
//
//---------------------------------------------------------------------

class BmpReader : public ImageReader
{
    FILE *_pFile;  // .bmp file pointer
    UserMessage _umsg;  // shows error message to user
    int _flags;    // image info flags
    int _offset;   // file offset to start of pixel data
    int _width;    // width of bitmap, in pixels
    int _height;   // height of bitmap, in pixels
    int _bpp;      // bits per pixel
    bool _bAlpha;  // true if BMP file data has 8-bit alpha channel
    int _row;      // current row in bitmap
    int _col;      // current column in bitmap
    int _pad;      // bytes of padding at end of each row

public:
    BmpReader(const char *pszFile);
    ~BmpReader();
    int GetImageInfo(int *width, int *height);
    int ReadPixels(COLOR *buffer, int count);
    int RewindData();
    void ErrorMessage(char *pszError);
};

//---------------------------------------------------------------------
//
// A simple graphical text application implemented in textapp.cpp
//
//---------------------------------------------------------------------

struct XY
{
    float x;
    float y;
};

struct GLYPH;

class TextApp
{
    GLYPH *_glyphtbl[128];  // glyph look-up table
    GLYPH *_glyph;          // glyph display lists
    float _width;           // current stroke width
    float _xspace;          // text spacing multiplier

    void DrawGlyph(ShapeGen *sg, char *displist, SGPoint xy[]);

public:
    TextApp();
    ~TextApp();
    void SetTextSpacing(float xspace);
    void DisplayText(ShapeGen *sg, const float xform[], const char *str);
    void DisplayText(ShapeGen *sg, SGPoint xystart, float scale, const char *str);
    void GetTextEndpoint(const float xform[], const char *str, XY *xyout);
    float GetTextWidth(float scale, const char *str);
};

//---------------------------------------------------------------------
//
// An AlphaBlur object uses a Gaussian filter to blur images, and is
// typically used to draw drop shadows. The AlphaBlur class is
// implemented in alfablur.cpp.
//
//---------------------------------------------------------------------

class AlphaBlur : public ImageReader
{
    float _stddev;   // standard deviation
    COLOR *_kcoeff;  // kernel coefficients
    int _kwidth;     // kernel width
    COLOR _rgba, _rgb, _alpha;  // fill color components
    COLOR *_pixels;  // pointer to blurred image pixels
    int _width;      // width of blurred image
    int _height;     // height of blurred image
    int _numpixels;  // number of pixels in blurred image
    int _index;      // current index into blurred image

    void ApplyGaussianFilter(COLOR dst[], const COLOR src[], int len);

public:
    AlphaBlur(float stddev, int kwidth = 0);

    ~AlphaBlur();

    void GetBlurredBoundingBox(SGRect& blurbbox, const SGRect& bbox);

    int CreateFilterKernel(float stddev = 0, int kwidth = 0);

    float GetStandardDeviation() { return _stddev; }

    int GetKernelWidth() { return _kwidth; }

    void SetColor(COLOR color = 0);

    bool BlurImage(const PIXEL_BUFFER& srcimage);

    int GetImageInfo(int *width, int *height)
    {
        *width  = _width;
        *height = _height;
        return 0;
    }

    int ReadPixels(COLOR *buffer, int count);

    int RewindData()
    {
        _index = 0;
        return 0;
    }
};

#endif // DEMO_H
