/*
  Copyright (C) 2022-2024 Jerry R. VanAken

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
//  renderer.cpp:
//    This file contains the implementations of the BasicRenderer and
//    AA4x8Renderer classes declared in renderer.h. This rendering
//    code is platform-INdependent: the renderers write directly to
//    a window-backing buffer (or back buffer) that is described by a
//    PIXEL_BUFFER structure. The only platform-dependent code needed
//    is the BitBlt function call (contained in separate module) that
//    copies the back buffer to the window on the display. The back
//    buffer has a 32-bit BGRA pixel format (that is, 0xaarrggbb).
//
//---------------------------------------------------------------------

#include <string.h>
#include <assert.h>
#include "renderer.h"

//---------------------------------------------------------------------
//
// Utilities for manipulating pixel buffers
//
//---------------------------------------------------------------------

// Allocates a memory buffer that can hold enough 32-bit pixels to
// store a rectangular image of width 'w' and height 'h', where the
// rows occupy contiguous memory. Fills the allocated memory with
// pixel value 'fill', which is typically set to either 0 (for
// transparent black) or 0xffffffff (for opaque white). Parameter
// 'fill' is optional; it defaults to 0. The function returns a
// pointer to the buffer.
COLOR* AllocateRawPixels(int w, int h, COLOR fill)
{
    if (w < 1 || h < 1)
    {
        assert(w > 0 && h > 0);
        return 0;  // bad parameters
    }
    int len = w*h;
    COLOR *pixbuf = new COLOR[len];
    if (!pixbuf)
    {
        assert(pixbuf);
        return 0;  // out of memory
    }
    COLOR *p = pixbuf;
    for (int i = 0; i < len; ++i)
        *p++ = fill;

    return pixbuf;
}

// Deletes a buffer that was previously allocated by the
// AllocateRawPixels function. Always returns zero.
COLOR* DeleteRawPixels(COLOR *buf)
{
    if (buf) { delete[] buf; }
    return 0;
}

// Given a PIXEL_BUFFER structure 'bigbuf' that describes a pixel
// buffer, and a bounding box 'bbox' that specifies a rectangular
// subregion in the buffer, this function writes a description of the
// subregion to the PIXEL_BUFFER structure 'subbuf', overwriting its
// original contents. The 'pixels' member of the 'subbuf' structure
// is set to the memory address of the pixel at the top-left corner
// of the subregion (do not try to delete this memory!). The bounding
// box coordinates in 'bbox' must be integers; i.e., fixed-point
// coordinates are _not_ supported. The x-y coordinates at the top-left
// corner of bounding box 'bbox' are specified relative to the top-left
// corner of the rectangular pixel buffer specified by 'bigbuf'. E.g.,
// the first pixel in 'bigbuf' is located at x-y coordinates (0,0). The
// function returns true if the 'subbuf' pixel buffer fits within the
// bounds of the 'bigbuf' pixel buffer. Otherwise, it returns false. If
// the contents of 'bigbuf' or 'bbox' are not valid, the results are
// undefined.
bool DefineSubregion(PIXEL_BUFFER& subbuf, const PIXEL_BUFFER& bigbuf, const SGRect& bbox)
{
    int xmax = bbox.x + bbox.w - 1;
    int ymax = bbox.y + bbox.h - 1;
    bool valid = bigbuf.pixels &&
                 bbox.x >= 0 && bbox.y >= 0 &&
                 xmax < bigbuf.width && ymax < bigbuf.height;
    int stride = bigbuf.pitch/sizeof(COLOR);
    subbuf.width = bbox.w;
    subbuf.height = bbox.h;
    subbuf.pitch = bigbuf.pitch;
    subbuf.depth = bigbuf.depth;
    if (valid)
        subbuf.pixels = &bigbuf.pixels[bbox.x + stride*bbox.y];
    else
        subbuf.pixels = 0;
    return valid;
}

//---------------------------------------------------------------------
//
// BasicRenderer class: A platform-independent implementation of the
// 'SimpleRenderer' virtual base class defined in renderer.h. This
// renderer does solid color fills with no antialiasing or alpha
// blending, but can fill large areas faster than the enhanced
// renderer.
//
//---------------------------------------------------------------------

class BasicRenderer : public SimpleRenderer
{
    friend ShapeGen;

    PIXEL_BUFFER _backbuf;
    int _stride;
    COLOR _color;

protected:
    void RenderShape(ShapeFeeder *feeder);

public:
    bool GetStatus();  // for local use only

    // Simple renderer application interface
    BasicRenderer(const PIXEL_BUFFER *backbuf);
    ~BasicRenderer() {}
    void SetColor(COLOR color);
};

BasicRenderer::BasicRenderer(const PIXEL_BUFFER *backbuf)
{
    _backbuf = *backbuf;
    _stride = _backbuf.pitch/sizeof(COLOR);
    SetColor(RGBX(0,0,0));
}

// Returns true if constructor succeeded; otherwise, returns false.
bool BasicRenderer::GetStatus()
{
    return (_backbuf.pixels != 0);
}

// Fills a series of horizontal spans that comprise a shape
void BasicRenderer::RenderShape(ShapeFeeder *feeder)
{
    SGRect rect;

    while (feeder->GetNextSDLRect(&rect))
    {
        COLOR *prow = &_backbuf.pixels[rect.y*_stride + rect.x];
        for (int j = 0; j < rect.h; ++j)
        {
            COLOR *pixel = prow;
            for (int i = 0; i < rect.w; ++i)
                *pixel++ = _color;

            prow = &prow[_stride];
        }
    }
}

// Selects the color to be used for subsequent fills and strokes
void BasicRenderer::SetColor(COLOR color)
{
    int r = color & 0xff;
    int g = (color >> 8) & 0xff;
    int b = (color >> 16) & 0xff;

    _color = (0xff << 24) | (r << 16) | (g << 8) | b;
}

//---------------------------------------------------------------------
//
// Utility functions used by EnhancedRenderer to do alpha blending
//
//---------------------------------------------------------------------
namespace {
    // Premultiplies a 32-bit pixel's RGB components by its alpha
    // value. The pixel is in either BGRA format (that is,
    // 0xaarrggbb) or RGBA format (0xaabbggrr).
    COLOR PremultAlpha(COLOR color)
    {
        COLOR alpha = color >> 24;

        if (alpha == 255)
            return color;

        if (alpha == 0)
            return 0;

        color |= 0xff000000;
        COLOR rb = color & 0x00ff00ff;
        COLOR ga = (color ^ rb) >> 8;
        rb *= alpha;
        rb += 0x00800080;
        rb += (rb >> 8) & 0x00ff00ff;
        rb = (rb >> 8) & 0x00ff00ff;
        ga *= alpha;
        ga += 0x00800080;
        ga += (ga >> 8) & 0x00ff00ff;
        ga &= 0xff00ff00;
        color = ga | rb;
        return color;
    }

    // Alpha-blends a 1-D array of 32-bit source pixels into a 1-D
    // array of 32-bit destination pixels. Parameter len is the array
    // length. Both source and destination pixels are in either BGRA
    // format (that is, 0xaarrggbb) or RGBA format (0xaabbggrr), and
    // have already been premultiplied by their alpha values.
    void AlphaBlender(COLOR *dst, COLOR *src, int len)
    {
        while (len--)
        {
            COLOR srcpix, dstpix, anot;

            srcpix = *src++;
            anot = ~srcpix >> 24;
            if (anot == 0)
            {
                *dst++ = srcpix;  // source alpha is 255
            }
            else if (anot == 255)
            {
                ++dst;  // source alpha is 0
            }
            else if ((dstpix = *dst) == 0)
            {
                *dst++ = srcpix;  // dest alpha is 0
            }
            else
            {
                COLOR rb = dstpix & 0x00ff00ff;
                COLOR ga = (dstpix ^ rb) >> 8;
                rb *= anot;
                rb += 0x00800080;
                rb += (rb >> 8) & 0x00ff00ff;
                rb = (rb >> 8) & 0x00ff00ff;
                ga *= anot;
                ga += 0x00800080;
                ga += (ga >> 8) & 0x00ff00ff;
                ga &= 0xff00ff00;
                dstpix = ga | rb;
                *dst++ = dstpix + srcpix;
            }
        }
    }

    // Implements BLENDOP_ADD_WITH_SAT operation: Adds a row of source
    // pixels to a row of destination pixels. Uses add-with-saturate
    // blend operations so that any 8-bit component that overflows is
    // set to 255. This blend mode works well only if (1) the shapes
    // being composited together do not overlap at the subpixel level,
    // and (2) the background was previously initialized to transparent
    // black (all zeros). If small overlap errors occur, however, the
    // add-with-saturate operation mitigates any resulting overflows.
    // Source and destination pixels are in premultiplied-alpha format.
    void AddWithSaturation(COLOR *dst, COLOR *src, int len)
    {
        while (len--)
        {
            COLOR dstpix, srcpix = *src++;

            if (srcpix == 0)
            {
                ++dst;
            }
            else if ((dstpix = *dst) == 0)
            {
                *dst++ = srcpix;
            }
            else if (((srcpix >> 24) + (dstpix >> 24)) < 256)
            {
                *dst++ = dstpix + srcpix;
            }
            else  // need to saturate
            {
                COLOR red = (srcpix >> 16) & 255;
                COLOR grn = (srcpix >> 8) & 255;
                COLOR blu = (srcpix & 255) + (dstpix & 255);

                red += (dstpix >> 16) & 255;
                grn += (dstpix >> 8) & 255;
                red |= (255 - red) >> 24;
                grn |= (255 - grn) >> 24;
                blu |= (255 - blu) >> 24;
                *dst++ = (-1 << 24) | (red << 16) | (grn << 8) | blu;
            }
        }
    }

    // Implements BLENDOP_ALPHA_CLEAR operation: Multiplies a row of
    // destination pixels by the bitwise inverse of the corresponding
    // source alpha components. Source color components are not used.
    // A source alpha value of 255 clears (makes totally transparent)
    // the corresponding destination pixel, while a source alpha of
    // zero leaves the destination unchanged. Source and destination
    // pixels are in premultiplied-alpha format.
    void AlphaClear(COLOR *dst, COLOR *src, int len)
    {
        while (len--)
        {
            COLOR srcpix, dstpix, anot;

            srcpix = *src++;
            anot = ~srcpix >> 24;  // inverse of source alpha
            if (anot == 0)
            {
                *dst++ = 0;  // source alpha is 255
            }
            else if (anot == 255 || (dstpix = *dst) == 0)
            {
                ++dst;
            }
            else
            {
                COLOR rb = dstpix & 0x00ff00ff;
                COLOR ga = (dstpix ^ rb) >> 8;
                rb *= anot;
                rb += 0x00800080;
                rb += (rb >> 8) & 0x00ff00ff;
                rb = (rb >> 8) & 0x00ff00ff;
                ga *= anot;
                ga += 0x00800080;
                ga += (ga >> 8) & 0x00ff00ff;
                ga &= 0xff00ff00;
                dstpix = ga | rb;
                *dst++ = dstpix;
            }
        }
    }
}  // end namespace

//---------------------------------------------------------------------
//
// AA4x8Renderer class: A platform-independent implementation of the
// 'EnhancedRenderer' virtual base class defined in renderer.h. To
// support antialiasing and alpha blending, this class uses an
// 'AA-buffer' to keep track of pixel coverage. The AA-buffer
// dedicates a 32-bit bitmask (organized as 4 rows of 8 bits) to
// each pixel in the current scan line. Internally, this renderer
// uses an internal 32-bit BGRA pixel format (that is, 0xaarrggbb).
// Before being processed, input pixels in RGBA (0xaabbggrr) format
// are converted to BGRA format and premultiplied by their alphas.
//
//---------------------------------------------------------------------

class AA4x8Renderer : public EnhancedRenderer
{
    friend ShapeGen;

    PIXEL_BUFFER _pixbuf;  // pixel buffer descriptor
    int _stride;       // stride in pixels = pitch/sizeof(COLOR)
    bool _pixalloc;    // true if we allocated the pixel memory
    COLOR *_linebuf;   // pixel data bits in scanline buffer
    COLOR _alpha;      // source constant alpha
    COLOR _color;      // current color for solid color fills
    BLENDOP _blendop;  // how to blend source and destination pixels
    int _maxwidth;     // width (in pixels) of device clipping rect
    int *_aabuf;       // AA-buffer data bits (32 bits per pixel)
    int *_aarow[4];    // AA-buffer organized as 4 subpixel rows
    int _lut[33];      // look-up table for source alpha/RGB values
    PaintGen *_paintgen;  // paint generator (gradients, patterns)
    COLOR_STOP _cstop[STOPARRAY_MAXLEN+1];  // color-stop array
    int _stopCount;    // Number of elements in color-stop array
    float _xform[6];   // Transform matrix (gradients, patterns)
    float *_pxform;    // Pointer to transform matrix
    int _xscroll, _yscroll;  // Scroll position coordinates

    void FillSubpixelSpan(int xL, int xR, int ysub);
    void RenderAbuffer(int xmin, int xmax, int yscan);
    void BlendLUT(COLOR component);
    void BlendConstantAlphaLUT();

protected:
    void RenderShape(ShapeFeeder *feeder);
    bool SetMaxWidth(int maxwidth);
    int QueryYResolution() { return 2; }
    bool SetScrollPosition(int x, int y);

public:
    bool GetStatus();  // for local use only

    // Enhanced renderer application interface
    AA4x8Renderer(const PIXEL_BUFFER *pixbuf);
    ~AA4x8Renderer();
    bool GetPixelBuffer(PIXEL_BUFFER *pixbuf);
    void SetColor(COLOR color);
    bool SetPattern(const COLOR *pattern, float u0, float v0,
                    int w, int h, int stride, int flags);
    bool SetPattern(ImageReader *imgrdr, float u0, float v0,
                    int w, int h, int flags);
    bool SetLinearGradient(float x0, float y0, float x1, float y1,
                           SPREAD_METHOD spread, int flags);
    bool SetRadialGradient(float x0, float y0, float r0,
                           float x1, float y1, float r1,
                           SPREAD_METHOD spread, int flags);
    bool SetConicGradient(float x0, float y0,
                          float astart, float asweep,
                          SPREAD_METHOD spread, int flags);
    void AddColorStop(float offset, COLOR color);
    void ResetColorStops() { _stopCount = 0; }
    void SetTransform(const float xform[6]);
    void SetConstantAlpha(COLOR alpha);
    void SetBlendOperation(BLENDOP blendop);
};

AA4x8Renderer::AA4x8Renderer(const PIXEL_BUFFER *pixbuf) :
                    _maxwidth(0), _linebuf(0), _aabuf(0), _paintgen(0),
                    _stopCount(0), _pxform(0), _color(0), _alpha(255),
                    _xscroll(0), _yscroll(0), _pixalloc(false),
                    _blendop(BLENDOP_SRC_OVER_DST)
{
    if (pixbuf->width < 1 || pixbuf->height < 1 || pixbuf->depth != 32 ||
        (pixbuf->pitch/sizeof(COLOR) < pixbuf->width && pixbuf->pixels))
    {
        assert(pixbuf->width > 0 && pixbuf->height > 0);
        assert(pixbuf->depth == 32);
        assert(pixbuf->pitch/sizeof(COLOR) >= pixbuf->width && pixbuf->pixels);
        _pixbuf.pixels = 0;
        return;  // bad parameter
    }
    _pixbuf = *pixbuf;
    if (_pixbuf.pixels == 0)
    {
        // The caller wants us to allocate a pixel buffer
        _pixbuf.pixels = AllocateRawPixels(_pixbuf.width, _pixbuf.height, RGBA(0,0,0,0));
        if (_pixbuf.pixels == 0)
        {
            assert(_pixbuf.pixels != 0);
            return;  // fail - out of memory
        }
        _pixalloc = true;  // don't forget we allocated pixel memory
        _pixbuf.pitch = _pixbuf.width*sizeof(COLOR);
    }
    _stride = _pixbuf.pitch/sizeof(COLOR);
    memset(&_lut[0], 0, sizeof(_lut));
    memset(&_aarow[0], 0, sizeof(_aarow));
    memset(&_cstop[0], 0, sizeof(_cstop));
    memset(&_xform[0], 0, sizeof(_xform));
    SetColor(RGBX(0,0,0));
}

AA4x8Renderer::~AA4x8Renderer()
{
    delete[] _aabuf;
    delete[] _linebuf;
    if (_pixalloc)
        DeleteRawPixels(_pixbuf.pixels);
    if (_paintgen)
        _paintgen->~PaintGen();
}

// Returns true if constructor succeeded; otherwise, returns false.
bool AA4x8Renderer::GetStatus()
{
    return (_pixbuf.pixels != 0);
}

// Public function: The caller wants a copy of our pixel-buffer info
bool AA4x8Renderer::GetPixelBuffer(PIXEL_BUFFER *pixbuf)
{
    if (_pixbuf.pixels)
    {
        *pixbuf = _pixbuf;
        return true;
    }
    return false;
}

// Protected function: ShapeGen calls this function to notify the
// renderer when the width of the device clipping rectangle changes.
// This function rebuilds the AA-buffer and the scan-line buffer to
// accommodate the new width.
bool AA4x8Renderer::SetMaxWidth(int width)
{
    // Pad out specified width to be multiple of four
    width = (width + 3) & ~3;
    assert(width > 0);  // assumption: width is never zero
    if (_maxwidth != width)
    {
        _maxwidth = width;

        // Allocate buffer to store one scan line of BGRA pixels
        delete[] _linebuf;
        _linebuf = new COLOR[_maxwidth];
        assert(_linebuf);
        memset(_linebuf, 0, _maxwidth*sizeof(_linebuf[0]));

        // Allocate the new AA-buffer
        delete[] _aabuf;
        _aabuf = new int[_maxwidth];
        assert(_aabuf);
        memset(_aabuf, 0, _maxwidth*sizeof(_aabuf[0]));
        for (int i = 0; i < 4; ++i)
            _aarow[i] = &_aabuf[i*_maxwidth/4];
    }
    return true;
}

// Protected function: Called by ShapeGen to fill a series of
// horizontal spans that comprise a shape
void AA4x8Renderer::RenderShape(ShapeFeeder *feeder)
{
    if (_pixbuf.pixels == 0)
    {
        assert(_pixbuf.pixels);
        return;  // not a valid pixel buffer
    }

    const int FIX_BIAS = 0x00007fff;
    const int YSCAN_INVALID = 0x80000000;
    int yscan = YSCAN_INVALID;
    int xmin = 0, xmax = 0;
    SGSpan span;

    // To reduce memory requirements, the ShapeFeeder::GetNextSGSpan
    // function always supplies subpixel spans in y-ascending order.
    // Thus, the AA-buffer can construct each successive scanline in
    // its entirety before starting construction on the next scanline.
    while (feeder->GetNextSGSpan(&span))
    {
        // Preserve 3 subpixel bits in the fixed-point x coordinates.
        // Also, replace pixel offset bias with subpixel offset bias.
        int xL = (span.xL + FIX_BIAS/8 - FIX_BIAS) >> 13;
        int xR = (span.xR + FIX_BIAS/8 - FIX_BIAS) >> 13;
        int ysub = span.y;

        // Is this span so tiny that it falls into a gap between subpixels?
        if (xL == xR)
            continue;  // yes, nothing to do here

        // Are we still in the same scan line as before?
        if (yscan != ysub/4)
        {
            // No, use the AA-buffer to render the previous scan line
            if (yscan != YSCAN_INVALID)
                RenderAbuffer(xmin, xmax, yscan);

            // Initialize xmin/xmax values for the new scan line
            xmin = xL;
            xmax = xR;
            yscan = ysub/4;
        }
        FillSubpixelSpan(xL, xR, ysub);
        xmin = min(xmin, xL);
        xmax = max(xmax, xR);
    }

    // Flush the AA-buffer to render the final scan line
    if (yscan != YSCAN_INVALID)
        RenderAbuffer(xmin, xmax, yscan);
}

// Private function: Fills a subpixel span (horizontal string of bits)
// in the AA-buffer. The span starting and ending x coordinates, xL and
// xR, are fixed-point values with 3 fractional (subpixel) bits. The
// span's y coordinate, ysub, is fixed-point with 2 fractional bits.
void AA4x8Renderer::FillSubpixelSpan(int xL, int xR, int ysub)
{
    // To speed up AA-buffer accesses, we write 4 bytes at a time
    // (to update the bitmap data for 4 adjacent pixels in parallel).
    // Variables iL and iR are indices to the starting and ending
    // 4-byte blocks in the AA-buffer. Variables maskL and maskR are
    // the bitmasks for the starting and ending 4-byte blocks.

    int iL = xL >> 5;  // starting index into _aabuf array
    int iR = xR >> 5;  // ending index into _aabuf array
    int maskL = -(1 << (xL & 31));      // bitmask for _aabuf[iL]
    int maskR =  (1 << (xR & 31)) - 1;  // bitmask for _aabuf[iR]
    int *prow = _aarow[ysub & 3];

    if (iL != iR)
    {
        prow[iL] |= maskL;
        for (int i = iL + 1; i < iR; ++i)
            prow[i] = -1;

        if (maskR)
            prow[iR] |= maskR;
    }
    else
        prow[iL] |= maskL & maskR;
}

// Private function: Uses the data in the AA-buffer to paint the
// antialiased pixels in the scan line that was just completed
void AA4x8Renderer::RenderAbuffer(int xmin, int xmax, int yscan)
{
    int iL = xmin >> 5;         // index of first 4-byte block
    int iR = (xmax + 31) >> 5;  // index just past last 4-byte block
    int x = 4*iL;

    assert(iL < iR);

    // Count the coverage bits per pixel in the AA-buffer. To speed
    // things up, we'll tally the counts for four adjacent pixels at
    // a time. Then we'll use each pixel's count to look up the
    // color-blend value for that pixel, and write this color to the
    // source pixel buffer.
    for (int i = iL; i < iR; ++i)
    {
        int count = 0;

        for (int j = 0; j < 4; ++j)
        {
            unsigned int v0, v1 = _aarow[j][i];

            _aarow[j][i] = 0;  // <-- clears this AA-buffer element
            v0 = v1 & 0x55555555;
            v0 += (v0 ^ v1) >> 1;
            v1 = v0 & 0x33333333;
            v1 += (v1 ^ v0) >> 2;
            v0 = v1 & 0x0f0f0f0f;
            v0 += (v0 ^ v1) >> 4;
            count += v0;
        }

        // The four bytes in the 'count' variable contain the
        // individual population counts for four horizontally
        // adjacent pixels. Each byte in 'count' contains a
        // count in the range 0 to 32.
        for (int j = 0; j < 4; ++j)
        {
            int index = count & 63;

            _linebuf[x] = _lut[index];
            ++x;
            count >>= 8;
        }
    }

    // If this fill uses a paint generator, call its FillSpan function
    int xleft = xmin/8, xright = (xmax + 7)/8;
    int len = xright - xleft;
    COLOR *srcbuf = &_linebuf[xleft];

    if (_paintgen)
        _paintgen->FillSpan(xleft, yscan, len, srcbuf, srcbuf);

    // Blend the painted pixels into the back buffer
    COLOR *dest = &_pixbuf.pixels[yscan*_stride + xleft];

    if (_blendop == BLENDOP_SRC_OVER_DST)
        AlphaBlender(dest, srcbuf, len);
    else if (_blendop == BLENDOP_ADD_WITH_SAT)
        AddWithSaturation(dest, srcbuf, len);
    else
        AlphaClear(dest, srcbuf, len);
}

// Private function: Loads an RGB color component or alpha value into
// the look-up table in the _lut array. The array is loaded with 33
// elements corresponding to all possible per-pixel alpha values
// (0/32, 1/32, ... , 32/32) obtained from a pixel's 32-bit coverage
// bitmask in the AA-buffer. The motivation here is to substitute
// table lookups for multiplications during fill operations.
void AA4x8Renderer::BlendLUT(COLOR component)
{
    COLOR diff = component | (component << 8);
    COLOR val = 15;

    for (int i = 0; i < ARRAY_LEN(_lut); ++i)
    {
        _lut[i] = (_lut[i] << 8) | (val >> 13);
        val += diff;
    }
}

// Private function: Loads the _lut array with the product of the
// current source-constant alpha and all possible per-pixel alpha
// values (0/32, 1/32, ... , 32/32) obtained from a pixel's 32-bit
// coverage bitmask in the AA-buffer. Then we can substitute table
// lookups for multiplications during fill operations.
void AA4x8Renderer::BlendConstantAlphaLUT()
{
    memset(_lut, 0, sizeof(_lut));
    BlendLUT(_alpha);
}

// Public function: Sets up the renderer to do solid color fills. This
// function loads the _lut array with the premultiplied-alpha pixel
// values for all possible per-pixel alpha values (0/32, 1/32, ... ,
// 32/32) obtained from a pixel's 32-bit coverage bitmask in the
// AA-buffer. In the process, the pixel's color is converted from RGBA
// (that is, 0xaabbggrr) to BGRA (0xaarrggbb) format. Note that the
// source-constant alpha is first mixed with the per-pixel alpha.
void AA4x8Renderer::SetColor(COLOR color)
{
    COLOR opacity = _alpha*(color >> 24);

    _color = color;
    if (_paintgen)
    {
        _paintgen->~PaintGen();
        _paintgen = 0;
    }
    opacity += 128;
    opacity += opacity >> 8;
    opacity >>= 8;
    color = (opacity << 24) | (color & 0x00ffffff);
    BlendLUT(opacity);
    color = PremultAlpha(color);
    for (int shift = 0; shift <= 16; shift += 8)
        BlendLUT((color >> shift) & 255);
}

void AA4x8Renderer::SetConstantAlpha(COLOR alpha)
{
    _alpha = alpha & 255;
    if (_paintgen)
        BlendConstantAlphaLUT();
    else
        SetColor(_color);
}

// Protected function: ShapeGen calls this function so that the
// patterns and gradients in painted shapes can follow the shapes
// as they are scrolled horizontally and vertically.
bool AA4x8Renderer::SetScrollPosition(int x, int y)
{
    _xscroll = x, _yscroll = y;
    if (_paintgen)
        _paintgen->SetScrollPosition(_xscroll, _yscroll);

    return true;
}

// Public function: Prepares the renderer to do tiled-pattern fills
// from a pixel array containing a 2-D image
bool AA4x8Renderer::SetPattern(const COLOR *pattern, float u0, float v0,
                               int w, int h, int stride, int flags)
{
    if (_paintgen)
    {
        _paintgen->~PaintGen();
        _paintgen = 0;
    }
    if (~flags & FLAG_IMAGE_BGRA32)
    {
        // This renderer requires BGRA (0xaarrggbb) pixel format
        flags |= FLAG_SWAP_REDBLUE;
    }
    TiledPattern *pat;
    pat = CreateTiledPattern(pattern, u0, v0, w, h, stride, flags, _pxform);
    if (pat == 0)
    {
        assert(pat != 0);
        SetColor(RGBX(0,0,0));
        return false;  // out of memory
    }
    _paintgen = pat;
    _paintgen->SetScrollPosition(_xscroll, _yscroll);
    BlendConstantAlphaLUT();  // fill look-up table with 8-bit alphas
    return true;
}

// Public function: Sets up the renderer to use the 2-D image from a
// bitmap file or 2-D matrix to do tiled-pattern fills
bool AA4x8Renderer::SetPattern(ImageReader *imgrdr, float u0, float v0,
                               int w, int h, int flags)
{
    if (_paintgen)
    {
        _paintgen->~PaintGen();
        _paintgen = 0;
    }
    if (~flags & FLAG_IMAGE_BGRA32)
    {
        // This renderer requires BGRA (0xaarrggbb) pixel format
        flags |= FLAG_SWAP_REDBLUE;
    }
    TiledPattern *pat;
    pat = CreateTiledPattern(imgrdr, u0, v0, w, h, flags, _pxform);
    if (pat == 0)
    {
        assert(pat != 0);
        SetColor(RGBX(0,0,0));
        return false;  // out of memory
    }
    _paintgen = pat;
    _paintgen->SetScrollPosition(_xscroll, _yscroll);
    BlendConstantAlphaLUT();  // fill look-up table with 8-bit alphas
    return true;
}

// Public function: Prepares the renderer to do linear gradient fills
bool AA4x8Renderer::SetLinearGradient(float x0, float y0, float x1, float y1,
                                      SPREAD_METHOD spread, int flags)
{
    if (_paintgen)
    {
        _paintgen->~PaintGen();
        _paintgen = 0;
    }
    LinearGradient *grad;
    grad = CreateLinearGradient(x0, y0, x1, y1, spread, flags, _pxform);
    if (grad == 0)
    {
        assert(grad != 0);
        SetColor(RGBX(0,0,0));
        return false;  // out of memory
    }
    for (int i = 0; i < _stopCount; ++i)
        grad->AddColorStop(_cstop[i].offset, _cstop[i].color);

    _paintgen = grad;
    _paintgen->SetScrollPosition(_xscroll, _yscroll);
    BlendConstantAlphaLUT();  // fill look-up table with 8-bit alphas
    return true;
}

// Public function: Prepares the renderer to do radial gradient fills
bool AA4x8Renderer::SetRadialGradient(float x0, float y0, float r0,
                                      float x1, float y1, float r1,
                                      SPREAD_METHOD spread, int flags)
{
    if (_paintgen)
    {
        _paintgen->~PaintGen();
        _paintgen = 0;
    }
    RadialGradient *grad;
    grad = CreateRadialGradient(x0, y0, r0, x1, y1, r1, spread, flags, _pxform);
    if (grad == 0)
    {
        assert(grad != 0);
        SetColor(RGBX(0,0,0));
        return false;  // out of memory
    }
    for (int i = 0; i < _stopCount; ++i)
        grad->AddColorStop(_cstop[i].offset, _cstop[i].color);

    _paintgen = grad;
    _paintgen->SetScrollPosition(_xscroll, _yscroll);
    BlendConstantAlphaLUT();  // fill look-up table with 8-bit alphas
    return true;
}

// Public function: Prepares the renderer to do conic gradient fills
bool AA4x8Renderer::SetConicGradient(float x0, float y0,
                                     float astart, float asweep,
                                     SPREAD_METHOD spread, int flags)
{
    if (_paintgen)
    {
        _paintgen->~PaintGen();
        _paintgen = 0;
    }
    ConicGradient *grad;
    grad = CreateConicGradient(x0, y0, astart, asweep, spread, flags, _pxform);
    if (grad == 0)
    {
        assert(grad != 0);
        SetColor(RGBX(0,0,0));
        return false;  // out of memory
    }
    for (int i = 0; i < _stopCount; ++i)
        grad->AddColorStop(_cstop[i].offset, _cstop[i].color);

    _paintgen = grad;
    _paintgen->SetScrollPosition(_xscroll, _yscroll);
    BlendConstantAlphaLUT();  // fill look-up table with 8-bit alphas
    return true;
}

// Public function: Adds a gradient color stop. In the process, the
// 32-bit stop color is converted from RGBA (that is, 0xaabbggrr) to
// BGRA (0xaarrggbb) pixel format.
void AA4x8Renderer::AddColorStop(float offset, COLOR color)
{
    if (_stopCount < STOPARRAY_MAXLEN)
    {
        COLOR ga = color & 0xff00ff00;
        COLOR rb = color & 0x00ff00ff;

        rb = (rb >> 16) | (rb << 16);
        _cstop[_stopCount].color = ga | rb;
        _cstop[_stopCount].offset = offset;
        ++_stopCount;
    }
}

// Public function: Specifies the transformation matrix that will be
// applied to patterns and gradients. If the same transformation is
// applied to painted shapes, the patterns and gradients in the shapes
// will stay in sync with the transformed shapes. Setting the 'xform'
// parameter to 0 has the same effect as specifying the identify
// matrix, but avoids unnecessary matrix calculations.
void AA4x8Renderer::SetTransform(const float xform[6])
{
    if (xform)
    {
        memcpy(&_xform[0], &xform[0], sizeof(_xform));
        _pxform = &_xform[0];
    }
    else
        _pxform = 0;
}

// Public function: Sets the blending operation that will be used to
// blend source pixels with destination pixels
void AA4x8Renderer::SetBlendOperation(BLENDOP blendop)
{
    switch (blendop)
    {
    case BLENDOP_SRC_OVER_DST:
    case BLENDOP_ADD_WITH_SAT:
    case BLENDOP_ALPHA_CLEAR:
        _blendop = blendop;
        break;
    default:
        _blendop = BLENDOP_DEFAULT;
        break;
    }
}

//---------------------------------------------------------------------
//
// The following functions create a SimpleRenderer or EnhancedRender
// object and return a pointer to this object. The caller is
// responsible for deleting this object when it is no longer needed
// (hint: you can use a smart pointer; see the SmartPtr class template
// in shapegen.h). The 'pixbuf' parameter specifies the frame buffer,
// back buffer, or layer buffer that is to be the rendering target.
//
//---------------------------------------------------------------------

SimpleRenderer* CreateSimpleRenderer(const PIXEL_BUFFER *pixbuf)
{
    BasicRenderer *rend = new BasicRenderer(pixbuf);
    if (rend == 0 || rend->GetStatus() == false)
    {
        assert(rend != 0 && rend->GetStatus() == true);
        delete rend;
        return 0;  // constructor failed
    }
    return rend;  // success
}

EnhancedRenderer* CreateEnhancedRenderer(const PIXEL_BUFFER *pixbuf)
{
    AA4x8Renderer *aarend = new AA4x8Renderer(pixbuf);
    if (aarend == 0 || aarend->GetStatus() == false)
    {
        assert(aarend != 0 && aarend->GetStatus() == true);
        delete aarend;
        return 0;  // constructor failed
    }
    return aarend;
}

