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

const float PI = 3.14159265;

// Dimensions of window for demo functions
const int DEMO_WIDTH  = 1280;
const int DEMO_HEIGHT =  960;

// Macros
#ifdef min
#undef min
#endif
#define min(x,y)  ((x)<(y)?(x):(y))  // take minimum of two values 
#ifdef max
#undef max
#endif
#define max(x,y)  ((x)>(y)?(x):(y))  // take maximum of two values
#ifdef sign
#undef sign
#endif 
#define sign(x)   ((x)<0?-1:1)	     // sign (plus or minus) of value

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

extern int runtest(int testnum, SimpleRenderer *rend, EnhancedRenderer *aarend, const SGRect& cliprect);

//---------------------------------------------------------------------
//
// Class UserMessage: Sends text message to user
//
//---------------------------------------------------------------------

class UserMessage
{
public:
    UserMessage() {}
    ~UserMessage() {}
    void MessageOut(char *text, char *title, int code = 0);
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
    UserMessage *_umsg;  // for sending error message to user
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
    BmpReader(const char *pszFile, UserMessage *umsg);
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

#endif // DEMO_H
