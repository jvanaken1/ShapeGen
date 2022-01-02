//---------------------------------------------------------------------
//
//  sdlmain.cpp:
//    This file contains all the platform-dependent functions for
//    running the ShapeGen demo on SDL2 in Windows. Two sample
//    renderers are included.
//
//---------------------------------------------------------------------

#include <SDL.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "demo.h"

// Display error message for user
void UserMessage::MessageOut(char *text, char *title, int code)
{
    for (int i = 0; i < 8; ++i) printf("*********");
    printf("\n** %s\n", title);
    printf("**   %s\n", text);
    for (int i = 0; i < 8; ++i) printf("*********");
    printf("\n");
} 

//---------------------------------------------------------------------
//
// A basic renderer: Fills a shape with a solid color, but does _NOT_
// do antialiasing. This class is derived from the SimpleRenderer class
// in demo.h, which, in turn, is derived from the Renderer base class
// in shapegen.h. This BasicRenderer class implementation is platform-
// dependent -- it uses the SDL_FillRect function in SDL2 to fill the
// horizontal spans that comprise the shape.
//
//---------------------------------------------------------------------

class BasicRenderer : public SimpleRenderer
{
    friend ShapeGen;

    SDL_Surface* _surface;
    Uint32 _pixel;

protected:
    void RenderShape(ShapeFeeder *feeder);

public:
    BasicRenderer(SDL_Surface* screenSurface) : _surface(screenSurface), _pixel(0)
    {
        SetColor(RGBX(0,0,0));
    }
    ~BasicRenderer()
    {
    }
    void SetColor(COLOR color);
};

// Fills a series of horizontal spans that comprise a shape
void BasicRenderer::RenderShape(ShapeFeeder *feeder)
{
    SDL_Rect rect;

    while (feeder->GetNextSDLRect(reinterpret_cast<SGRect*>(&rect)))
        SDL_FillRect(_surface, &rect, _pixel);
}

// Selects the color to be used for subsequent fills and strokes
void BasicRenderer::SetColor(COLOR color)
{
    int r = color & 0xff;
    int g = (color >> 8) & 0xff;
    int b = (color >> 16) & 0xff;

    _pixel = SDL_MapRGB(_surface->format, r, g, b);
}

//---------------------------------------------------------------------
//
// An enhanced renderer: Works exclusively with full-color displays.
// Does antialiasing, alpha blending, solid-color fills, pattern
// fills, linear gradient fills, and radial gradient fills. The
// RenderShape function uses an "AA-buffer" to keep track of pixel
// coverage (for antialiasing). The AA-buffer dedicates a 32-bit
// bitmask, organized as 4 rows of 8 bits, to each pixel in the
// current scan line. This AA4x8Renderer class implementation is
// platform-dependent: it calls the SDL_CreateRGBSurface function in
// SDL2 to create offscreen buffers for a single scanline of source
// pixels, and calls the SDL_BlitSurface function to move pixels
// between an offscreen buffer and the window-backing surface.
//
//---------------------------------------------------------------------

class AA4x8Renderer : public EnhancedRenderer
{
    friend ShapeGen;

    SDL_Surface *_winsurf;   // backing surface for the window
    SDL_Surface *_blendsurf; // holds 1 scanline of source pixels
    SDL_Surface *_tempsurf;  // holds 1 scanline of background pixels
    bool _bFormatsMatch;     // true if pixel formats in _winsurf
                             // and _blendsurf match
    COLOR *_pixbuf;    // pixel data bits in _blendsurf
    COLOR _alpha;      // source constant alpha
    int _width;        // width (in pixels) of device clipping rect
    int *_aabuf;       // AA-buffer data bits (32 bits per pixel)
    int *_aarow[4];    // AA-buffer organized as 4 subpixel rows
    int _lut[33];      // look-up table for source alpha values
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
    COLOR PremultAlpha(COLOR color);
    void AlphaBlend(COLOR *src, COLOR *dst, int len);
    bool IsMatchingFormat(SDL_Surface* surf);

protected:
    void RenderShape(ShapeFeeder *feeder);
    bool SetMaxWidth(int width);
    int QueryYResolution() { return 2; }
    bool SetScrollPosition(int x, int y);

public:
    AA4x8Renderer(SDL_Surface* winsurf);
    ~AA4x8Renderer();
    void SetColor(COLOR color);
    void SetPattern(const COLOR *pattern, float u0, float v0, 
                    int w, int h, int stride, int flags);
    void SetPattern(ImageReader *imgrdr, float u0, float v0, 
                    int w, int h, int flags);
    void SetLinearGradient(float x0, float y0, float x1, float y1,
                           SPREAD_METHOD spread, int flags);
    void SetRadialGradient(float x0, float y0, float r0,
                           float x1, float y1, float r1,
                           SPREAD_METHOD spread, int flags);
    void AddColorStop(float offset, COLOR color);
    void ResetColorStops() { _stopCount = 0; }
    void SetTransform(const float xform[]);
    void SetConstantAlpha(COLOR alpha) { _alpha = alpha & 255; }
};

// Premultiplies a 32-bit pixel's RGB components by its alpha value
COLOR AA4x8Renderer::PremultAlpha(COLOR color)
{
    COLOR rb, ga, alfa = color >> 24;

    if (alfa == 255)
        return color;

    if (alfa == 0)
        return 0;

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
    return color;
}

// Alpha-blends an array of 32-bit source pixels into an array of
// 32-bit destination pixels. Parameter len is the array length.
// The source pixels are already in premultiplied-alpha format.
// Destination pixels are assumed to be 100 percent opaque.
void AA4x8Renderer::AlphaBlend(COLOR *src, COLOR *dst, int len)
{
    while (len--)
    {
        COLOR srcpix, dstpix, rb, ga, anot; 
        
        srcpix = *src++;
        anot = ~srcpix >> 24;
        dstpix = *dst | 0xff000000;
        rb = dstpix & 0x00ff00ff;
        rb *= anot;
        rb += (rb >> 7) & 0x01ff01ff;
        rb &= 0xff00ff00;
        ga = (dstpix >> 8) & 0x00ff00ff;
        ga *= anot;
        ga += (ga >> 7) & 0x01ff01ff;
        ga &= 0xff00ff00;
        dstpix = ga | (rb >> 8);
        *dst++ = dstpix + srcpix;
    }
}

// Returns true if the specified surface's pixel format is
// compatible with the renderer's internal pixel format
bool AA4x8Renderer::IsMatchingFormat(SDL_Surface* surf)
{
    return (surf->format->BitsPerPixel == 32) &&
           (surf->format->Rmask == 0x00ff0000) &&
           (surf->format->Gmask == 0x0000ff00) &&
           (surf->format->Bmask == 0x000000ff);
}

AA4x8Renderer::AA4x8Renderer(SDL_Surface* winsurf) 
                  : _winsurf(winsurf), _blendsurf(0), _tempsurf(0),
                    _width(0), _pixbuf(0), _aabuf(0), _paintgen(0),
                    _stopCount(0), _pxform(0), _alpha(255),
                    _xscroll(0), _yscroll(0)
{
    _bFormatsMatch = IsMatchingFormat(_winsurf);
    SDL_SetSurfaceBlendMode(_winsurf, SDL_BLENDMODE_NONE);
    memset(&_lut[0], 0, sizeof(_lut));   
    memset(&_aarow[0], 0, sizeof(_aarow));   
    memset(&_cstop[0], 0, sizeof(_cstop));   
    memset(&_xform[0], 0, sizeof(_xform));
    SetColor(RGBX(0,0,0));
}

AA4x8Renderer::~AA4x8Renderer()
{
    SDL_FreeSurface(_blendsurf);
    SDL_FreeSurface(_tempsurf);
    delete[] _aabuf;
    if (_paintgen)
        _paintgen->~PaintGen();
}

// ShapeGen calls this function to notify the renderer when the width
// of the device clipping rectangle changes. This function rebuilds
// the AA-buffer and the offscreen pixel buffer to accommodate the
// new width.
bool AA4x8Renderer::SetMaxWidth(int width)
{
    // Pad out specified width to be multiple of four
    _width = (width + 3) & ~3;

    // Free any previously allocated objects
    SDL_FreeSurface(_tempsurf);
    _tempsurf = 0;
    SDL_FreeSurface(_blendsurf);
    _blendsurf = 0;
    _pixbuf = 0;
    delete[] _aabuf;
    _aabuf = 0;

    // Allocate the new AA-buffer
    _aabuf = new int[_width];  
    assert(_aabuf);
    memset(_aabuf, 0, _width*sizeof(_aabuf[0]));  // debug aid
    for (int i = 0; i < 4; ++i)
        _aarow[i] = &_aabuf[i*_width/4];

    // Allocate offscreen buffer to store one scan line of BGRA pixels
    _blendsurf = SDL_CreateRGBSurface(0, _width, 1, 32, 
                                     0x00ff0000,   // Rmask
                                     0x0000ff00,   // Gmask
                                     0x000000ff,   // Bmask
                                     0xff000000);  // Amask
    if (_blendsurf == 0)
    {
        printf("ERROR-- %s\n", SDL_GetError());
        assert(_blendsurf);
        return false;
    }
    if (_bFormatsMatch == false)
    {
        _tempsurf = SDL_CreateRGBSurface(0, _width, 1, 32, 
                                         0x00ff0000,   // Rmask
                                         0x0000ff00,   // Gmask
                                         0x000000ff,   // Bmask
                                         0xff000000);  // Amask
        if (_tempsurf == 0)
        {
            printf("ERROR-- %s\n", SDL_GetError());
            assert(_tempsurf);
            SDL_FreeSurface(_blendsurf);
            _blendsurf = 0;
            return false;
        }
    }
    _pixbuf = reinterpret_cast<COLOR*>(_blendsurf->pixels);
    assert(_pixbuf);
    memset(_pixbuf, 0, _width*sizeof(_pixbuf[0]));
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

    // When antialiasing is enabled, the ShapeFeeder::GetNextSGSpan
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

        // Is this span so tiny that it falls into a gap between pixels?
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
    // Variables iL and iR are indexes to the starting and ending
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

// Use the data in the AA-buffer to paint the antialiased pixels in the
// most recent scan line
void AA4x8Renderer::RenderAbuffer(int xmin, int xmax, int yscan)
{
    int iL = xmin >> 5;         // index of first 4-byte block
    int iR = (xmax + 31) >> 5;  // index just past last 4-byte block

    assert(iL < iR);
    memset(&_pixbuf[iL], 0, (iR-iL)*sizeof(_pixbuf[0]));

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

            _pixbuf[x] = _lut[index];
            ++x;
            count >>= 8;
        }
    }

    // If this fill uses a paint generator, call its FillSpan function
    int xleft = xmin/8, xright = (xmax+7)/8;
    int len = xright - xleft;
    COLOR *buffer = &_pixbuf[xleft];

    if (_paintgen)
        _paintgen->FillSpan(xleft, yscan, len, buffer, buffer);

    // Alpha-blend the painted pixels into the window-backing surface.
    // SDL's inability to handle source pixels in premultiplied-alpha
    // format makes things here more complicated than they should be.
    if (_bFormatsMatch)
    {
        COLOR *dest = reinterpret_cast<COLOR*>(_winsurf->pixels) + xleft + 
                      yscan*_winsurf->pitch/sizeof(COLOR);
        AlphaBlend(buffer, dest, len);
    }
    else
    {
        SDL_Rect temprect = { xleft, 0, len, 1 };
        SDL_Rect winrect = { xleft, yscan, len, 1 };
        COLOR *dest = reinterpret_cast<COLOR*>(_tempsurf->pixels) + xleft;

        SDL_BlitSurface(_winsurf, &winrect, _tempsurf, &temprect);
        AlphaBlend(buffer, dest, len);
        SDL_BlitSurface(_tempsurf, &temprect, _winsurf, &winrect);
    }
    memset(&_pixbuf[xleft], 0, len*sizeof(_pixbuf[0]));
}

// Loads alpha values or RGB color components into the look-up
// table in the _lut array. The array is loaded with 33 elements
// corresponding to all possible per-pixel alpha values (0/32,
// 1/32, ... , 32/32) obtained from a pixel's coverage bitmask
// in the AA-buffer. The motivation here is to substitute table
// lookups for multiplications during fill operations.
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
// source constant alpha is first mixed with the per-pixel alpha.
void AA4x8Renderer::SetColor(COLOR color)
{
    COLOR opacity = _alpha*(color >> 24);

    if (_paintgen)
    {
        _paintgen->~PaintGen();
        _paintgen = 0;
    }
    opacity += opacity >> 7;
    opacity >>= 8;
    color = (opacity << 24) | (color & 0x00ffffff);
    BlendLUT(opacity);
    color = PremultAlpha(color);
    for (int shift = 0; shift <= 16; shift += 8)
        BlendLUT((color >> shift) & 255);
}

// Loads the _lut array with product of source constant alpha and
// all possible per-pixel alpha values (0/32, 1/32, ... , 32/32)
// obtained from a pixel's coverage bitmask in the AA-buffer.
void AA4x8Renderer::BlendConstantAlphaLUT()
{
    memset(_lut, 0, sizeof(_lut));
    BlendLUT(_alpha);
}

bool AA4x8Renderer::SetScrollPosition(int x, int y)
{
    _xscroll = x, _yscroll = y;
    if (_paintgen)
        _paintgen->SetScrollPosition(_xscroll, _yscroll);

    return true;
}

// Sets up the renderer to do tiled pattern fills from an array
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

// Sets up the renderer to do pattern fills from a bitmap file
// containing a 2-D image
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

// Sets up the renderer to do linear gradient fills
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

// Sets up the renderer to do radial gradient fills
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
// SDL2 main function
//
//---------------------------------------------------------------------

int main(int argc, char *argv[])
{
    SDL_Window *window = 0;
    SDL_Surface* winsurf = 0;
    SDL_Event evt;
    bool redraw = true;
    bool quit = false;
    int testnum = 0;
    SGRect cliprect = { 0, 0, DEMO_WIDTH, DEMO_HEIGHT};

    printf("Starting SDL2 app...\n");
    if (SDL_Init(SDL_INIT_VIDEO) < 0)
    {
        printf("ERROR-- %s\n", SDL_GetError());
        return -1;
    }
    window = SDL_CreateWindow("ShapeGen graphics demo",
                              SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                              DEMO_WIDTH, DEMO_HEIGHT, 0);
    if (window == 0)
    {
        printf("ERROR-- %s\n", SDL_GetError());
        return -1;
    }
    winsurf = SDL_GetWindowSurface(window);
    if (winsurf == 0)
    {
        SDL_DestroyWindow(window);
        printf("ERROR-- %s\n", SDL_GetError());
        return -1;
    }
    while (quit == false)
    {
        while (SDL_PollEvent(&evt) != 0)
        {
            if (evt.type == SDL_QUIT)
            {
                quit = true;
                break;
            }
            else if (evt.type == SDL_KEYDOWN)
            {
                int flags = KMOD_ALT | KMOD_SHIFT | KMOD_CTRL;
                bool mod = ((SDL_GetModState() & flags) != 0);

                redraw = true;
                switch (evt.key.keysym.sym)
                {
                case SDLK_LEFT:
                    if (mod)
                        cliprect.x -= 5;
                    else
                    {
                        --testnum;
                        cliprect.x = cliprect.y = 0;
                    }
                    break;
                case SDLK_UP:
                    if (mod)
                        cliprect.y -= 5;
                    else
                        cliprect.x = cliprect.y = 0;
                    break;
                case SDLK_RIGHT:
                    if (mod)
                        cliprect.x += 5;
                    else
                    {
                        ++testnum;
                        cliprect.x = cliprect.y = 0;
                    }
                    break;
                case SDLK_DOWN:
                    if (mod)
                        cliprect.y += 5;
                    else
                        cliprect.x = cliprect.y = 0;
                    break;
                case SDLK_ESCAPE:
                    if (mod)
                        quit = true;
                    else
                        testnum = 0;
                    break;
                case SDLK_RSHIFT:
                case SDLK_LSHIFT:
                case SDLK_RCTRL:
                case SDLK_LCTRL:
                case SDLK_RALT:
                case SDLK_LALT:
                    redraw = false;
                    break;
                default:
                    ++testnum;
                    cliprect.x = cliprect.y = 0;
                    break;
                }
            }
            if (redraw == true)
            {
                BasicRenderer rend(winsurf);
                AA4x8Renderer aarend(winsurf);
    
                SDL_FillRect(winsurf, 0, SDL_MapRGB(winsurf->format, 255, 255, 255));
                testnum = runtest(testnum, &rend, &aarend, cliprect);
                SDL_UpdateWindowSurface(window);
                redraw = false;
            }
        }
    }
    SDL_DestroyWindow(window);
    SDL_Quit();
    printf("Quitting SDL2 app...\n");
    return 0;
}
