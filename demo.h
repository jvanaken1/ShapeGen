/*
  Copyright (C) 2019 Jerry R. VanAken

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
//   ShapeGen demo code, and by the code examples in demo.cpp. 
//
//---------------------------------------------------------------------

#include "shapegen.h"

const float PI = 3.14159265;

// Dimensions of canvas for demo functions
const int DEMO_WIDTH  = 1280;
const int DEMO_HEIGHT =  960;

typedef unsigned int COLOR;

// Macro definitions 
#define ARRAY_LEN(a)  (sizeof(a)/sizeof((a)[0]))
#define RGBX(r,g,b)    (COLOR)(((r)&255)|(((g)&255)<<8)|(((b)&255)<<16)|0xff000000)
#define RGBA(r,g,b,a)  (COLOR)(((r)&255)|(((g)&255)<<8)|(((b)&255)<<16)|((a)<<24))
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
// Pixels and RGB values
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
// A simple renderer derived from the Renderer base class. This
// renderer fills shapes with solid colors.
//
//---------------------------------------------------------------------

class SimpleRenderer : public Renderer
{
public:
    virtual void SetColor(COLOR color) = 0;
};

//---------------------------------------------------------------------
//
// Runs the graphics test function specified by index testnum.
// Parameter rend is a basic renderer that does no antialiasing.
// Parameter aarend is an antialiasing renderer. Parameter cliprect
// specifies the device clipping rectangle.
//
//---------------------------------------------------------------------

extern bool runtest(int testnum, SimpleRenderer *rend, SimpleRenderer *aarend, const SGRect& cliprect);

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
    GLYPH *_glyph;          // glyph descriptors
    float _width;           // current stroke width
    float _xspace;          // text spacing multiplier

    void DrawGlyph(ShapeGen *sg, char *displist, SGPoint xy[]);

public:
    TextApp();
    ~TextApp();
    void SetTextSpacing(float xspace);
    void DisplayText(ShapeGen *sg, const float xfrm[][3], const char *str);
    void DisplayText(ShapeGen *sg, SGPoint xystart, float scale, const char *str);
    void GetTextEndpoint(const float xfrm[][3], const char *str, XY *xyout);
    float GetTextWidth(float scale, const char *str);
};


