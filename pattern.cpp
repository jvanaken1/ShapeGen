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
// pattern.cpp:
//   Paint generator class for tiled pattern fills
//
//---------------------------------------------------------------------

//#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "renderer.h"

struct XYPAIR { FIX16 x; FIX16 y; };
struct UVPAIR { FIX16 u; FIX16 v; };

//---------------------------------------------------------------------
//
// Local functions and data
//
//---------------------------------------------------------------------

namespace {

    // Multiplies a 32-bit pixel's RGB and alpha components by the
    // 'opacity' parameter, which is an alpha value in the range 0
    // to 255. Both the input pixel value and the return value are
    // in premultiplied-alpha format.
    COLOR MultiplyByOpacity(COLOR pixel, COLOR opacity)
    {
        COLOR rb, ga;
    
        if (opacity == 255)
            return pixel;
    
        if (opacity == 0 || pixel == 0)
            return 0;
    
        rb = pixel & 0x00ff00ff;
        rb *= opacity;
        rb += (rb >> 7) & ~0xfe00fe00;
        rb &= 0xff00ff00;
        ga = (pixel >> 8) & 0x00ff00ff;
        ga *= opacity;
        ga += (ga >> 7) & ~0xfe00fe00;
        ga &= 0xff00ff00;
        pixel = ga | (rb >> 8);
        return pixel;
    }
    
    // Premultiplies an array of 32-bit pixels by their alphas
    void PremultAlphaArray(COLOR *pixel, int len)
    {
        for (int i = 0; i < len; ++i, ++pixel)
        {
            COLOR rb, ga, color = *pixel, alfa = color >> 24;
            
            if (alfa == 255)
                continue;
        
            if (alfa == 0)
            {
                *pixel = 0;
                continue;
            }
            color |= 0xff000000;
            rb = color & 0x00ff00ff;
            rb *= alfa;
            rb += (rb >> 7) & 0x01ff01ff;
            rb &= 0xff00ff00;
            ga = (color >> 8) & 0x00ff00ff;
            ga *= alfa;
            ga += (ga >> 7) & 0x01ff01ff;
            ga &= 0xff00ff00;
            color = ga | (rb >> 8);
            *pixel = color;
        }
    }

    // Returns the value x modulo n
    int modulus(int x, int n)
    {
        unsigned int y = x;

        if (y < n)
            return x;

        x += (x < 0) ? n : -n;
        y = x;
        if (y < n)
            return x;

        x %= n;
        return (x < 0) ? x + n : x;
    }

    // A 4-pixel multisampling pattern for antialiasing. Each offset
    // value is in units of 1/8th of a pixel from the pixel center.
    const XYPAIR msaa4x4[4][4] = {
        { { -3, -2 }, { +2, -3 }, { +3, +2 }, { -2, +3 } },  // x even, y even
        { { -2, -3 }, { +3, -2 }, { +2, +3 }, { -3, +2 } },  // x odd,  y even
        { { -2, -3 }, { +3, -2 }, { +2, +3 }, { -3, +2 } },  // x even, y odd
        { { -3, -2 }, { +2, -3 }, { +3, +2 }, { -2, +3 } },  // x odd,  y odd
    };
}

//---------------------------------------------------------------------
//
// Pattern class -- Paint generator for tiled pattern fills
//
//---------------------------------------------------------------------

class Pattern : public TiledPattern
{
    COLOR **_pattern;     // stored pattern image
    int _w, _h;           // width and height of image
    float _xform[6];      // affine transformation matrix
    FIX16 _dudx;          // partial derivative du/dx
    FIX16 _dvdx;          // partial derivative dv/dx
    FIX16 _dudy;          // partial derivative du/dy
    FIX16 _dvdy;          // partial derivative dv/dy
    UVPAIR _offset[4][4]; // multisampling offsets for antialiasing
    int _xscroll, _yscroll; // scroll position coordinates

    // Initialization code common to both constructors
    void Init(float u0, float v0, int flags, const float xform[]);
    
public:
    Pattern() { assert(0); }
    Pattern(const COLOR *pattern, float u0, float v0, int w, int h, 
            int stride, int flags, const float xform[]);
    Pattern(ImageReader *imgrdr, float u0, float v0, int w, int h, 
            int flags, const float xform[]);
    ~Pattern();
    void FillSpan(int xs, int ys, int length, COLOR outBuf[], const COLOR inAlpha[]);
    bool SetScrollPosition(int x, int y);
};

// Contains initialization code common to both constructors
void Pattern::Init(float u0, float v0, int flags, const float xform[])
{
    // Need to convert from RGBA (0xaabbggrr) to BGRA (0xaarrggbb),
    // or vice versa?
    if (flags & FLAG_SWAP_REDBLUE)  // need to swap red/blue fields?
    {
        for (int j = 0; j < _h; ++j)
        {
            COLOR *p = _pattern[j];

            for (int i = 0; i < _w; ++i)
            {
                COLOR ga = *p & 0xff00ff00;
                COLOR rb = *p & 0x00ff00ff;
                rb = (rb << 16) | (rb >> 16);
                *p++ = ga | rb;
            }
        }
    }

    // If the pattern texels are not already in premultiplied-
    // alpha format, convert them now...
    if (~flags & FLAG_PREMULTALPHA)
    {
        for (int j = 0; j < _h; ++j)
            PremultAlphaArray(_pattern[j], _w);
    }

    // Set up matrix for affine transformation from viewport
    // x-y pixel coordinates to pattern u-v texel coordinates
    if (xform != 0)
        memcpy(&_xform[0], &xform[0], sizeof(_xform));
    else
    {
        memset(&_xform[0], 0, sizeof(_xform));
        _xform[0] = _xform[3] = 1.0f;
    }
    if (flags & FLAG_IMAGE_BOTTOMUP)
    {
        // Rows of bitmap image are ordered bottom-to-top
        _xform[1] = -_xform[1];
        _xform[3] = -_xform[3];
    }
    _xform[4] += (_xform[0]+_xform[2])/2 - u0;
    _xform[5] += (_xform[1]+_xform[3])/2 - v0; 
    _dudx = 65536*_xform[0];
    _dvdx = 65536*_xform[1];
    _dudy = 65536*_xform[2];
    _dvdy = 65536*_xform[3];

    // For each display pixel in the four-pixel multisampling pattern,
    // calculate the corresponding four u-v sampling offsets from the
    // center of the corresponding pattern texel.
    for (int i = 0; i < 4; ++i)
    {
        const XYPAIR *p = msaa4x4[i];
        UVPAIR *q = _offset[i];

        for (int j = 0; j < 4; ++j)
        {
            q[j].u = (_dudx*p[j].x + _dudy*p[j].y)/8;
            q[j].v = (_dvdx*p[j].x + _dvdy*p[j].y)/8;
        }
    }
}

// Copy pattern from caller-supplied 2-D image array. Input pixels
// are assumed to be in either 32-bit RGBA (0xaabbggrr) format or
// 32-bit BGRA (0xaarrggbb) format.
Pattern::Pattern(const COLOR *pattern, float u0, float v0, int w, int h, 
                 int stride, int flags, const float xform[])
                 : _w(w), _h(h), _xscroll(0), _yscroll(0)
{
    if (!pattern || w <= 0 || h <= 0 || stride < w)
    {
        _w = _h = 0;  // error -- null pattern
        return;
    }

    // Copy pattern image into internal 2-D array
    COLOR *pdata = new COLOR[w*h];  // for pattern image pixels
    assert(pdata != 0);
    _pattern = new COLOR*[h];  // for pointers to pattern rows
    assert(_pattern != 0);
    for (int i = 0; i < h; ++i)
    {
        memcpy(pdata, &pattern[0], w*sizeof(pattern[0]));
        _pattern[i] = pdata;
        pdata = &pdata[w];
        pattern = &pattern[stride];
    }
    Init(u0, v0, flags, xform);  // finish initializing
}

// Copy pattern from caller-specified bitmap file. Input pixels
// are assumed to be in either 32-bit RGBA (0xaabbggrr) format or
// 32-bit BGRA (0xaarrggbb) format.
Pattern::Pattern(ImageReader *imgrdr, float u0, float v0, int w, int h, 
                 int flags, const float xform[])
                 : _w(w), _h(h), _xscroll(0), _yscroll(0)
{   
    if (!imgrdr || w <= 0 || h <= 0)
    {
        _w = _h = 0;  // error -- null pattern
        return;
    }

    // Copy pattern image into internal 2-D array
    COLOR *p = new COLOR[w*h];  // to store pattern image pixels
    assert(p != 0);
    _pattern = new COLOR*[h];  // pointers to rows of pattern
    assert(_pattern != 0);
    for (int i = 0; i < h; ++i)
    {
        imgrdr->ReadPixels(p, w);
        _pattern[i] = p;
        p = &p[w];
    }
    Init(u0, v0, flags, xform);  // finish initializing
}

Pattern::~Pattern()
{
    delete[] _pattern[0];
    delete[] _pattern;
}

// Protected function
bool Pattern::SetScrollPosition(int x, int y)
{
    _xscroll = x, _yscroll = y;
    return true;
}

// Public function: Fills the pixels in a single horizontal span
// with a tiled pattern. The span starts at pixel (xs,ys) and
// extends to the right for len pixels.
//
void Pattern::FillSpan(int xs, int ys, int len, COLOR outBuf[], const COLOR inAlpha[])
{
    if (_w == 0)
        return;

    // Map starting point (xs,ys) to pattern u-v coordinates
    xs += _xscroll, ys += _yscroll;
    FIX16 u = 65536*(_xform[0]*xs + _xform[2]*ys + _xform[4]);
    FIX16 v = 65536*(_xform[1]*xs + _xform[3]*ys + _xform[5]);
    int incr = (ys & 1) ? 2 : 0;
    UVPAIR *off[2] = { _offset[incr], _offset[incr+1] };
    
    // Each iteration of the for-loop below paints one pixel
    for (int k = 0; k < len; ++k)
    {
        COLOR opacity = (inAlpha == 0) ? 255 : inAlpha[k];

        if (opacity != 0)
        {
            COLOR texel, color, ga = 0, rb = 0;
            UVPAIR *poff = off[xs & 1];
            int i0 = modulus(u >> 16, _w);
            int j0 = modulus(v >> 16, _h);

            // Map pixel center to u-v coordinate space
            u = (u & 0x0000ffff) | (i0 << 16);
            v = (v & 0x0000ffff) | (j0 << 16);

            // Do antialiasing with 4-point multisampling
            for (int n = 0; n < 4; ++n)
            {
                int i = (u + poff[n].u) >> 16;
                int j = (v + poff[n].v) >> 16;

                i = modulus(i, _w);
                j = modulus(j, _h);
                texel = _pattern[j][i];
                ga += (texel >> 8) & 0x00ff00ff;
                rb += texel & 0x00ff00ff;
            }
            ga &= 0x03fc03fc;
            rb &= 0x03fc03fc;
            color = (ga << 6) | (rb >> 2);
            color = MultiplyByOpacity(color, opacity);
            outBuf[k] = color;
        }
        u += _dudx;
        v += _dvdx;
    }
}

// Called by a renderer to create a new tiled-pattern object
//
TiledPattern* CreateTiledPattern(const COLOR *pattern, float u0, float v0, 
                                 int w, int h, int stride, int flags, 
                                 const float xform[])
{
    Pattern *pat = new Pattern(pattern, u0, v0, w, h, stride, flags, xform);
    assert(pat != 0);

    return pat;
}

TiledPattern* CreateTiledPattern(ImageReader *imgrdr, float u0, float v0, 
                                 int w, int h, int flags, const float xform[])
{
    Pattern *pat = new Pattern(imgrdr, u0, v0, w, h, flags, xform);
    assert(pat != 0);

    return pat;
}





