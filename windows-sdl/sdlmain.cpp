//---------------------------------------------------------------------
//
//  sdlmain.cpp:
//    This file contains all the platform-dependent functions for
//    running the ShapeGen demo on SDL2 in Windows
//
//---------------------------------------------------------------------

#include <SDL.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "demo.h"

inline void SetRect(SDL_Rect *rect, int x, int y, int w, int h)
{
    rect->x = x;
    rect->y = y;
    rect->w = w;
    rect->h = h;
}

//----------------------------------------------------------------------
//
// A basic renderer: Fills a shape with a solid color, but does _not_
// do antialiasing. This class is derived from the SimpleRenderer class
// in demo.h, which, in turn, is derived from the Renderer base class
// in shapegen.h. This BasicRenderer class implementation is platform-
// dependent -- it uses the SDL_FillRect function in SDL2 to fill the
// horizontal spans that comprise the shape.
//
//----------------------------------------------------------------------

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

// Sets the pixel value to be used for color-fill operations
void BasicRenderer::SetColor(COLOR color)
{
    int r, g, b;

    GetRgbValues(color, &r, &g, &b);
    _pixel = SDL_MapRGB(_surface->format, r, g, b);
}

//----------------------------------------------------------------------
//
// An antialiasing renderer: Fills a shape with a solid color, and uses
// antialiasing to smooth the shape's edges. This class is derived from
// the SimpleRenderer class in demo.h, which is, in turn, derived from
// the Renderer base class in shapegen.h. The RenderShape function uses
// an "AA-buffer" to keep track of pixel coverage. The AA-buffer
// dedicates a 32-bit bitmask, organized as 4 rows of 8 bits, to each
// pixel in the current scan line. This AA4x8Renderer class
// implementation is platform-dependent: it uses the
// SDL_CreateRGBSurface function to create an offscreen buffer for one
// scanline of source pixels, and calls the SDL_BlitSurface function to
// blend the source pixels with the destination image.
//
//----------------------------------------------------------------------

class AA4x8Renderer : public SimpleRenderer
{
    friend ShapeGen;

    SDL_Surface *_winsurf;   // backing surface for the window
    SDL_Surface *_argbsurf;  // stores one scanline of source pixels
    Uint32 *_pixbits;  // _argbsurf pixel data bits
    int _width;        // width (in pixels) of device clipping rect
    int *_aabuf;       // AA-buffer data bits (32 bits per pixel)
    int *_aarow[4];    // AA-buffer organized as 4 subpixel rows
    int _lut[33];      // lookup table for source blend values

    void FillSubpixelSpan(int xL, int xR, int ysub);
    void RenderAbuffer(int xmin, int xmax, int yscan);

protected:
    void RenderShape(ShapeFeeder *feeder);
    bool SetMaxWidth(int width);
    int QueryYResolution() { return 2; };

public:
    AA4x8Renderer(SDL_Surface* winsurf) : 
            _winsurf(winsurf), _argbsurf(0), _pixbits(0), _width(0), _aabuf(0)
    {
        memset(_aarow, 0, sizeof(_aarow));
        memset(_lut, 0xf, sizeof(_lut));
    }
    ~AA4x8Renderer()
    {
        delete[] _aabuf;
        SDL_FreeSurface(_argbsurf);
    }
    void SetColor(COLOR color);
};

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

            // Initialize max/min x values for new scan line
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
    // To speed up AA-buffer accesses, we write 4 bytes at a time (we
    // update the bitmap data for 4 adjacent pixels in parallel).
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
    memset(&_pixbits[iL], 0, (iR-iL)*sizeof(_pixbits[0]));

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

        // The 4 bytes in "count" contain the individual population
        // counts for 4 adjacent pixels. Paint these 4 pixels.
        for (int j = 0; j < 4; ++j)
        {
            int index = count & 63;

            _pixbits[x] = _lut[index];
            ++x;
            count >>= 8;
        }
    }
    // Blit the source pixels to the destination rect
    int w = (xmax+7)/8 - xmin/8;
    SDL_Rect srcrect, dstrect;

    SetRect(&srcrect, xmin/8, 0, w, 1);
    SetRect(&dstrect, xmin/8, yscan, 0, 0);
    SDL_BlitSurface(_argbsurf, &srcrect, _winsurf, &dstrect);
    memset(&_pixbits[xmin/8], 0, w*sizeof(int));
}

// Initializes the lookup table array, _lut, that is used to blend
// the fill (source) color with the background (destination) color.
// This array contains 33 blend values corresponding to source
// alpha values 0/32, 1/32, ... , 32/32.
void AA4x8Renderer::SetColor(COLOR color)
{
    Uint32 r = color & 255;
    Uint32 g = (color >> 8) & 255;
    Uint32 b = (color >> 16) & 255;
    Uint32 a = (color >> 24) & 255;
    int diff = a | (a << 8);
    int val = 15;

    memset(&_lut[0], 0, sizeof(_lut));
    for (int i = 0; i < ARRAY_LEN(_lut); ++i)
    {
        int alpha = val >> 13;
        val += diff;
        _lut[i] = (alpha << 24) | (b << 16) | (g << 8) | r;
    }
}

// Notifies the renderer when the width of the device clipping
// rectangle changes. This function rebuilds the AA-buffer and the
// offscreen pixel buffer to accommodate the new width.
bool AA4x8Renderer::SetMaxWidth(int width)
{
    // Pad out specified width to be multiple of four
    _width = (width + 3) & ~3;

    // Free any previously allocated objects
    delete[] _aabuf;
    _aabuf = 0;
    SDL_FreeSurface(_argbsurf);
    _argbsurf = 0;
    _pixbits = 0;

    // Allocate AA-buffer
    _aabuf = new int[_width];  
    assert(_aabuf);
    memset(_aabuf, 0, _width*sizeof(_aabuf[0]));  // debug aid
    for (int i = 0; i < 4; ++i)
        _aarow[i] = &_aabuf[i*_width/4];

    // Allocate offscreen pixel buffer to store one scan line of ARGB data
    _argbsurf = SDL_CreateRGBSurface(0, _width, 1, 32, 
                                     0x000000ff,   // Rmask
                                     0x0000ff00,   // Gmask
                                     0x00ff0000,   // Bmask
                                     0xff000000);  // Amask
    if (_argbsurf == 0)
    {
        printf("ERROR-- %s\n", SDL_GetError());
        assert(_argbsurf);
        return false;
    }
    _pixbits = reinterpret_cast<Uint32*>(_argbsurf->pixels);
    assert(_pixbits);
    memset(_pixbits, 0, _width*sizeof(_pixbits[0]));
    return true;
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
    int testnum = -1;
    SGRect cliprect = { 0, 0, DEMO_WIDTH, DEMO_HEIGHT};

    printf("Starting SDL2 app...\n");
    if (SDL_Init(SDL_INIT_VIDEO) < 0)
    {
        printf("ERROR-- %s\n", SDL_GetError());
        return -1;
    }
    window = SDL_CreateWindow("ShapeGen demo",
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
            }
            else if (evt.type == SDL_KEYDOWN)
            {
                switch (evt.key.keysym.sym)
                {
                case SDLK_LEFT:
                    cliprect.x -= 5;
                    break;
                case SDLK_UP:
                    cliprect.y -= 5;
                    break;
                case SDLK_RIGHT:
                    cliprect.x += 5;
                    break;
                case SDLK_DOWN:
                    cliprect.y += 5;
                    break;
                default:
                    ++testnum;
                    cliprect.x = cliprect.y = 0;
                    break;
                }
                redraw = true;
            }
        }
        if (redraw == true)
        {
            BasicRenderer rend(winsurf);
            AA4x8Renderer aarend(winsurf);

            SDL_FillRect(winsurf, 0, SDL_MapRGB(winsurf->format, 255, 255, 255));
            if (!runtest(testnum, &rend, &aarend, cliprect))
            {
                testnum = 0;  // we just ran the final test, so start over
                runtest(testnum, &rend, &aarend, cliprect);
            }
            SDL_UpdateWindowSurface(window);
            redraw = false;
        }
    }
    SDL_DestroyWindow(window);
    SDL_Quit();
    printf("Quitting SDL2 app...\n");
    return 0;
}
