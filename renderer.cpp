/*
  Copyright (C) 2022-2023 Jerry R. VanAken

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
#include "demo.h"

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
    if (w <= 0 || h <= 0)
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
    int xmax = bbox.x + bbox.w;
    int ymax = bbox.y + bbox.h;
    bool retval = bbox.x >= 0 && bbox.y >= 0 &&
                  xmax < bigbuf.width && ymax < bigbuf.height;
    int stride = bigbuf.pitch/sizeof(COLOR);
    subbuf.width = bbox.w;
    subbuf.height = bbox.h;
    subbuf.pitch = bigbuf.pitch;
    subbuf.depth = bigbuf.depth;
    if (bigbuf.pixels)
        subbuf.pixels = &bigbuf.pixels[bbox.x + stride*bbox.y];
    else
        subbuf.pixels = 0;
    return retval;
}

//---------------------------------------------------------------------
//
// BasicRenderer functions - This renderer does solid color fills with
// no antialiasing or alpha blending, but can fill large areas faster
// than the enhanced renderer
//
//---------------------------------------------------------------------

BasicRenderer::BasicRenderer(const PIXEL_BUFFER *backbuf)
{
    _backbuf = *backbuf;
    _stride = _backbuf.pitch/sizeof(COLOR);
    SetColor(RGBX(0,0,0));
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
        COLOR rb, ga, alpha = color >> 24;

        if (alpha == 255)
            return color;

        if (alpha == 0)
            return 0;

        color |= 0xff000000;
        rb = color & 0x00ff00ff;
        rb *= alpha;
        rb += 0x00800080;
        rb += (rb >> 8) & 0x00ff00ff;
        rb = (rb >> 8) & 0x00ff00ff;
        ga = (color >> 8) & 0x00ff00ff;
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
    void AlphaBlender(COLOR *src, COLOR *dst, int len)
    {
        while (len--)
        {
            COLOR srcpix, dstpix, rb, ga, anot;

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
                rb = dstpix & 0x00ff00ff;
                rb *= anot;
                rb += 0x00800080;
                rb += (rb >> 8) & 0x00ff00ff;
                rb = (rb >> 8) & 0x00ff00ff;
                ga = (dstpix >> 8) & 0x00ff00ff;
                ga *= anot;
                ga += 0x00800080;
                ga += (ga >> 8) & 0x00ff00ff;
                ga &= 0xff00ff00;
                dstpix = ga | rb;
                *dst++ = dstpix + srcpix;
            }
        }
    }
}  // end namespace

//---------------------------------------------------------------------
//
// EnhancedRenderer functions - To perform alpha blending, this
// renderer uses an internal 32-bit BGRA pixel format (that is,
// 0xaarrggbb). Input pixels in RGBA format are converted to BGRA
// before being processed.
//
//---------------------------------------------------------------------
AA4x8Renderer::AA4x8Renderer(const PIXEL_BUFFER *pixbuf)
                  : _maxwidth(0), _linebuf(0), _aabuf(0), _paintgen(0),
                    _stopCount(0), _pxform(0), _color(0), _alpha(255),
                    _xscroll(0), _yscroll(0), _pixalloc(false)
{
    if (pixbuf->width <= 0 || pixbuf->height <= 0 || pixbuf->depth != 32 ||
        (pixbuf->pitch/sizeof(COLOR) < pixbuf->width && pixbuf->pixels))
    {
        assert(0);
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
            assert(_pixbuf.pixels);
            return;  // out of memory
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

    assert(iL < iR);
    memset(&_linebuf[iL], 0, (iR-iL)*sizeof(_linebuf[0]));

    // Count the coverage bits per pixel in the AA-buffer. To speed
    // things up, we'll tally the counts for four adjacent pixels at
    // a time. Then we'll use each pixel's count to look up the
    // color-blend value for that pixel, and write this color to the
    // source pixel buffer.
    for (int i = iL; i < iR; ++i)
    {
        int count = 0;
        int x = 4*i;

        for (int j = 0; j < 4; ++j)
        {
            int val = _aarow[j][i];

            _aarow[j][i] = 0;  // <-- clears this AA-buffer element
            val = (val & 0x55555555) + ((val >> 1) & 0x55555555);
            val = (val & 0x33333333) + ((val >> 2) & 0x33333333);
            val = (val & 0x0f0f0f0f) + ((val >> 4) & 0x0f0f0f0f);
            count += val;
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
    int xleft = xmin/8, xright = (xmax+7)/8;
    int len = xright - xleft;
    COLOR *srcbuf = &_linebuf[xleft];

    if (_paintgen)
        _paintgen->FillSpan(xleft, yscan, len, srcbuf, srcbuf);

    // Alpha-blend the painted pixels into the back buffer
    COLOR *dest = &_pixbuf.pixels[yscan*_stride + xleft];
    AlphaBlender(srcbuf, dest, len);
    memset(&_linebuf[xleft], 0, len*sizeof(_linebuf[0]));
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
void AA4x8Renderer::SetPattern(const COLOR *pattern, float u0, float v0,
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
    assert(pat);
    _paintgen = pat;
    _paintgen->SetScrollPosition(_xscroll, _yscroll);
    BlendConstantAlphaLUT();  // fill look-up table with 8-bit alphas
}

// Public function: Sets up the renderer to use the 2-D image from a
// bitmap file or 2-D matrix to do tiled-pattern fills
void AA4x8Renderer::SetPattern(ImageReader *imgrdr, float u0, float v0,
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
    assert(pat);
    _paintgen = pat;
    _paintgen->SetScrollPosition(_xscroll, _yscroll);
    BlendConstantAlphaLUT();  // fill look-up table with 8-bit alphas
}

// Public function: Prepares the renderer to do linear gradient fills
void AA4x8Renderer::SetLinearGradient(float x0, float y0, float x1, float y1,
                                      SPREAD_METHOD spread, int flags)
{
    if (_paintgen)
    {
        _paintgen->~PaintGen();
        _paintgen = 0;
    }
    LinearGradient *lin;
    lin = CreateLinearGradient(x0, y0, x1, y1, spread, flags, _pxform);
    assert(lin);
    for (int i = 0; i < _stopCount; ++i)
        lin->AddColorStop(_cstop[i].offset, _cstop[i].color);

    _paintgen = lin;
    _paintgen->SetScrollPosition(_xscroll, _yscroll);
    BlendConstantAlphaLUT();  // fill look-up table with 8-bit alphas
}

// Public function: Prepares the renderer to do radial gradient fills
void AA4x8Renderer::SetRadialGradient(float x0, float y0, float r0,
                                      float x1, float y1, float r1,
                                      SPREAD_METHOD spread, int flags)
{
    if (_paintgen)
    {
        _paintgen->~PaintGen();
        _paintgen = 0;
    }
    RadialGradient *rad;
    rad = CreateRadialGradient(x0, y0, r0, x1, y1, r1, spread, flags, _pxform);
    assert(rad);
    for (int i = 0; i < _stopCount; ++i)
        rad->AddColorStop(_cstop[i].offset, _cstop[i].color);

    _paintgen = rad;
    _paintgen->SetScrollPosition(_xscroll, _yscroll);
    BlendConstantAlphaLUT();  // fill look-up table with 8-bit alphas
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
void AA4x8Renderer::SetTransform(const float xform[])
{
    if (xform)
    {
        memcpy(&_xform[0], &xform[0], sizeof(_xform));
        _pxform = &_xform[0];
    }
    else
        _pxform = 0;
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
    SimpleRenderer *rend = new BasicRenderer(pixbuf);
    assert(rend != 0);  // out of memory?

    return rend;
}

EnhancedRenderer* CreateEnhancedRenderer(const PIXEL_BUFFER *pixbuf)
{
    EnhancedRenderer *aarend = new AA4x8Renderer(pixbuf);
    assert(aarend != 0);  // out of memory?

    return aarend;
}

