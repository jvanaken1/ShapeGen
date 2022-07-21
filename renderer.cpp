//---------------------------------------------------------------------
//
//  renderer.cpp:
//    This file contains the implementations of the BasicRenderer and
//    AA4x8Renderer classes declared in renderer.h. Both renderers are
//    platform independent, and write directly to a frame buffer that
//    is described by a FRAME_BUFFER structure. The frame buffer has
//    a 32-bit BGRA pixel format (that is, 0xaarrggbb), but the 8-bit
//    alpha field is never checked and always assumed to be 0xff.
//
//---------------------------------------------------------------------

#include <string.h>
#include <assert.h>
#include "demo.h"

//---------------------------------------------------------------------
//
// BasicRenderer functions - This renderer does solid color fills with
// no antialiasing or alpha blending, but can fill large areas faster
// than the enhanced renderer
//
//---------------------------------------------------------------------

BasicRenderer::BasicRenderer(FRAME_BUFFER *framebuf)
{
    _frmbuf = *framebuf;
    _frmbuf.pitch /= 4;  // convert from bytes to 32-bit pixels
    SetColor(RGBX(0,0,0));
}

// Fills a series of horizontal spans that comprise a shape
void BasicRenderer::RenderShape(ShapeFeeder *feeder)
{
    SGRect rect;

    while (feeder->GetNextSDLRect(&rect))
    {
        COLOR *p = &_frmbuf.pixels[rect.y*_frmbuf.pitch + rect.x];
        for (int j = 0; j < rect.h; ++j)
        {
            COLOR *q = p;
            for (int i = 0; i < rect.w; ++i)
                *q++ = _color;

            p = &p[_frmbuf.pitch];
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
    // value. The pixel is in BGRA format (that is, 0xaarrggbb).
    COLOR PremultAlpha(COLOR color)
    {
        COLOR rb, ga, alfa = color >> 24;

        if (alfa == 255)
            return color;

        if (alfa == 0)
            return 0;

        color |= 0xff000000;
        rb = color & 0x00ff00ff;
        rb *= alfa;
        rb += 0x00800080;
        rb += (rb >> 8) & 0x00ff00ff;
        rb = (rb >> 8) & 0x00ff00ff;
        ga = (color >> 8) & 0x00ff00ff;
        ga *= alfa;
        ga += 0x00800080;
        ga += (ga >> 8) & 0x00ff00ff;
        ga &= 0xff00ff00;
        color = ga | rb;
        return color;
    }

    // Alpha-blends an array of 32-bit source pixels into an array of
    // 32-bit destination pixels. Parameter len is the array length.
    // The source pixels are in BGRA format (that is, 0xaarrggbb),
    // and have already been premultiplied by their alpha values.
    // Destination pixels are assumed to be 100 percent opaque.
    void AlphaBlender(COLOR *src, COLOR *dst, int len)
    {
        while (len--)
        {
            COLOR srcpix, dstpix, rb, ga, anot;

            srcpix = *src++;
            anot = ~srcpix >> 24;
            dstpix = *dst | 0xff000000;
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

//---------------------------------------------------------------------
//
// EnhancedRenderer functions - To perform alpha blending, this
// renderer uses an internal 32-bit BGRA pixel format (that is,
// 0xaarrggbb). Input pixels in RGBA format are converted to BGRA
// before being processed.
//
//---------------------------------------------------------------------
AA4x8Renderer::AA4x8Renderer(FRAME_BUFFER *framebuf)
                  : _width(0), _linebuf(0), _aabuf(0), _paintgen(0),
                    _stopCount(0), _pxform(0), _alpha(255),
                    _xscroll(0), _yscroll(0)
{
    assert(framebuf->depth == 32);
    _frmbuf = *framebuf;
    _frmbuf.pitch /= sizeof(COLOR);  // convert from bytes to pixels
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
    if (_paintgen)
        _paintgen->~PaintGen();
}

// ShapeGen calls this function to notify the renderer when the width
// of the device clipping rectangle changes. This function rebuilds
// the AA-buffer and the scan-line buffer to accommodate the new width.
bool AA4x8Renderer::SetMaxWidth(int width)
{
    // Pad out specified width to be multiple of four
    width = (width + 3) & ~3;
    assert(width > 0);  // assumption: width is never zero

    if (_width != width)
    {
        _width = width;

        // Allocate buffer to store one scan line of BGRA pixels
        delete[] _linebuf;
        _linebuf = new COLOR[_width];
        assert(_linebuf);
        memset(_linebuf, 0, _width*sizeof(_linebuf[0]));

        // Allocate the new AA-buffer
        delete[] _aabuf;
        _aabuf = new int[_width];
        assert(_aabuf);
        memset(_aabuf, 0, _width*sizeof(_aabuf[0]));
        for (int i = 0; i < 4; ++i)
            _aarow[i] = &_aabuf[i*_width/4];
    }
    return true;
}

// Fills a series of horizontal spans that comprise a shape
void AA4x8Renderer::RenderShape(ShapeFeeder *feeder)
{
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

// Fills a subpixel span (horizontal string of bits) in the AA-buffer.
// The span starting and ending x coordinates, xL and xR, are
// fixed-point values with 3 fractional (subpixel) bits. The span
// y coordinate, ysub, is a fixed-point value with 2 fractional bits.
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

// Use the data in the AA-buffer to paint the antialiased pixels in
// the scan line that was just completed
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

    // Alpha-blend the painted pixels into the frame buffer
    COLOR *dest = &_frmbuf.pixels[yscan*_frmbuf.pitch + xleft];
    AlphaBlender(srcbuf, dest, len);
    memset(&_linebuf[xleft], 0, len*sizeof(_linebuf[0]));
}

// Loads an RGB color component or alpha value into the look-up
// table in the _lut array. The array is loaded with 33 elements
// corresponding to all possible per-pixel alpha values (0/32,
// 1/32, ... , 32/32) obtained from a pixel's 32-bit coverage
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

// Sets up the renderer to do solid color fills. This function
// loads the _lut array with the premultiplied-alpha pixel values
// for all possible per-pixel alpha values (0/32, 1/32, ... ,
// 32/32) obtained from a pixel's coverage bitmask in the AA-buffer.
// In the process, the pixel's color is converted from RGBA (that
// is, 0xaabbggrr) to BGRA (0xaarrggbb) format. Note that the
// source-constant alpha is first mixed with the per-pixel alpha.
void AA4x8Renderer::SetColor(COLOR color)
{
    COLOR opacity = _alpha*(color >> 24);

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

// Loads the _lut array with the product of the current source-
// constant alpha and all possible per-pixel alpha values (0/32,
// 1/32, ... , 32/32) obtained from a pixel's 32-bit coverage
// bitmask in the AA-buffer. The motivation here is to substitute
// table lookups for multiplications during fill operations.
void AA4x8Renderer::BlendConstantAlphaLUT()
{
    memset(_lut, 0, sizeof(_lut));
    BlendLUT(_alpha);
}

// The patterns and gradients in painted shapes must follow the
// shapes as they are scrolled horizontally and vertically.
bool AA4x8Renderer::SetScrollPosition(int x, int y)
{
    _xscroll = x, _yscroll = y;
    if (_paintgen)
        _paintgen->SetScrollPosition(_xscroll, _yscroll);

    return true;
}

// Prepares the renderer to do tiled-pattern fills from a pixel array
// containing a 2-D image
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

// Prepares the renderer to use the 2-D image from a bitmap file
// to do tiled-pattern fills
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

// Prepares the renderer to do linear gradient fills
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

// Prepares the renderer to do radial gradient fills
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

// Adds a gradient color stop. In the process, the 32-bit stop
// color is converted from RGBA (that is, 0xaabbggrr) to BGRA
// (0xaarrggbb) pixel format.
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

// Sets the transformation matrix to use for patterns and gradients
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


