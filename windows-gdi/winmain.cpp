//---------------------------------------------------------------------
//
//  winmain.cpp:
//    This file contains all the platform-dependent functions for
//    running the ShapeGen demo on the Win32 API in Windows
//
//---------------------------------------------------------------------

#include <windows.h>
#include <assert.h>
#include "demo.h"

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

int WINAPI WinMain(HINSTANCE hInstance, 
                   HINSTANCE hPrevInstance, 
                   PSTR szCmdLine, 
                   int iCmdShow) 
{ 
    static TCHAR szAppName[] = TEXT("Polygonal Shape Generator"); 
    HWND         hwnd; 
    MSG          msg; 
    WNDCLASS     wndclass;

    wndclass.style         = CS_HREDRAW | CS_VREDRAW; 
    wndclass.lpfnWndProc   = WndProc; 
    wndclass.cbClsExtra    = 0; 
    wndclass.cbWndExtra    = 0; 
    wndclass.hInstance     = hInstance; 
    wndclass.hIcon         = LoadIcon(NULL, IDI_APPLICATION); 
    wndclass.hCursor       = LoadCursor(NULL, IDC_ARROW); 
    wndclass.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH); 
    wndclass.lpszMenuName  = NULL; 
    wndclass.lpszClassName = szAppName; 

    if (!RegisterClass(&wndclass))
    {
        MessageBox(NULL, 
                   TEXT("This program requires Windows NT!"), 
                   szAppName, 
                   MB_ICONERROR); 
        return 0; 
    }
    hwnd = CreateWindow(szAppName, 
                        TEXT("ShapeGen Graphics Demo"), 
                        WS_OVERLAPPEDWINDOW | WS_VSCROLL, 
                        CW_USEDEFAULT, CW_USEDEFAULT, 
                        DEMO_WIDTH+33, DEMO_HEIGHT+56, 
                        NULL, NULL, hInstance, NULL); 

    ShowWindow(hwnd, iCmdShow); 
    UpdateWindow(hwnd);
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg); 
        DispatchMessage(&msg); 
    } 
    return msg.wParam; 
}

//----------------------------------------------------------------------
//
// A basic renderer: Fills a shape with a solid color, but does _not_
// do antialiasing. This class is derived from the SimpleRenderer class
// in demo.h, which, in turn, is derived from the Renderer base class
// in shapegen.h. This BasicRenderer class implementation is platform-
// dependent -- it uses the FillRect function in Windows GDI to fill
// the horizontal spans that comprise the shape.
//
//----------------------------------------------------------------------

class BasicRenderer : public SimpleRenderer
{
    friend ShapeGen;

    HDC _hdc;
    HBRUSH _hBrush;

protected:
    void RenderShape(ShapeFeeder *feeder);

public:
    BasicRenderer(HDC hdc) : _hdc(hdc), _hBrush(0)
    {
    }
    ~BasicRenderer()
    {
        DeleteObject(_hBrush);
    }
    void SetColor(COLOR color);
};

// Fills a series of horizontal spans that comprise a shape
void BasicRenderer::RenderShape(ShapeFeeder *feeder)
{
    RECT rect;

    while (feeder->GetNextGDIRect(reinterpret_cast<SGRect*>(&rect)))
        FillRect(_hdc, &rect, _hBrush);
}

// Selects the color to be used for subsequent fills and strokes
void BasicRenderer::SetColor(COLOR color)
{
    DeleteObject(_hBrush);
    _hBrush = CreateSolidBrush(color & 0x00ffffff);
    SelectObject(_hdc, _hBrush);
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
// implementation is platform-dependent: it uses the Windows GDI
// CreateDIBSection function to create an offscreen buffer for one
// scanline of source pixels, and calls the AlphaBlend function to
// blend the source pixels with the destination image.
//
//----------------------------------------------------------------------

class AA4x8Renderer : public SimpleRenderer
{
    friend ShapeGen;

    HDC _hdc;          // device context handle for window
    HDC _hdcmem;       // device context handle for DIB
    HBITMAP _hBitmap;  // DIB handle
    BYTE _alpha;       // source alpha
    int *_dibits;      // DIB pixel data bits
    int _width;        // width (in pixels) of device clipping rect
    int *_aabuf;       // AA-buffer data bits (32 bits per pixel)
    int *_aarow[4];    // AA-buffer organized as 4 subpixel rows
    int _lut[33];      // lookup table for source blend values

    void FillSubpixelSpan(int xL, int xR, int ysub);
    void RenderAbuffer(int xmin, int xmax, int yscan);
    void Blend(int crFill);

protected:
    void RenderShape(ShapeFeeder *feeder);
    bool SetMaxWidth(int width);
    int QueryYResolution() { return 2; };

public:
    AA4x8Renderer(HDC hdc) : 
                _hdc(hdc), _hdcmem(0), _hBitmap(0), _dibits(0), 
                _width(0), _aabuf(0), _alpha(0)
    {
        memset(_aarow, 0, sizeof(_aarow));
        memset(_lut, 0xf, sizeof(_lut));
    }
    ~AA4x8Renderer()
    {
        delete[] _aabuf;
        DeleteDC(_hdcmem);
        DeleteObject(_hBitmap);
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
    memset(&_dibits[iL], 0, (iR-iL)*sizeof(_dibits[0]));

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

            _dibits[x] = _lut[index];
            ++x;
            count >>= 8;
        }
    }
    // Blit the painted source pixels from the DIB to the client rect
    int w = (xmax+7)/8 - xmin/8;
    BLENDFUNCTION bf = { AC_SRC_OVER, 0, _alpha, AC_SRC_ALPHA };

    AlphaBlend(_hdc, xmin/8, yscan, w, 1, _hdcmem, xmin/8, 0, w, 1, bf);
    memset(&_dibits[xmin/8], 0, w*sizeof(int));
}

// Adds a blended color component (R, G, B, or A) to the lookup table
inline void AA4x8Renderer::Blend(int component)
{
    int diff = component | (component << 8);
    int val = 15;

    for (int i = 0; i < ARRAY_LEN(_lut); ++i)
    {
        _lut[i] = (_lut[i] << 8) | (val >> 13);
        val += diff;
    }
}

// Initializes the lookup table array, _lut, that is used to blend
// the fill (source) color with the background (destination) color.
// This array contains 33 blend values corresponding to source
// alpha values 0/32, 1/32, ... , 32/32.
void AA4x8Renderer::SetColor(COLOR color)
{
    _alpha = color >> 24;  // set source constant alpha
    memset(&_lut[0], 0, sizeof(_lut));
    Blend(255);
    for (int shift = 0; shift <= 16; shift += 8)
        Blend((color >> shift) & 255);
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
    DeleteDC(_hdcmem);
    _hdcmem = 0;
    DeleteObject(_hBitmap);
    _hBitmap = 0;
    _dibits = 0;

    // Allocate AA-buffer
    _aabuf = new int[_width];  
    assert(_aabuf);
    memset(_aabuf, 0, _width*sizeof(_aabuf[0]));  // debug aid
    for (int i = 0; i < 4; ++i)
        _aarow[i] = &_aabuf[i*_width/4];

    // Allocate a DIB section big enough to store one scan line
    // of pixels. For biCompression = BI_RGB, each 32-bit pixel
    // in the DIB is an RGBQUAD struct with little-endian layout:
    // blue in the 8 LSBs, and alpha in the 8 MSBs.
    BITMAPINFO bmi;

    memset(&bmi.bmiHeader, 0, sizeof(bmi.bmiHeader));
    bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
    bmi.bmiHeader.biWidth = _width;
    bmi.bmiHeader.biHeight = 1;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    _hBitmap = CreateDIBSection(0, &bmi, DIB_RGB_COLORS,(void **)&_dibits, 0, 0);
    assert(_hBitmap);
    assert(_dibits);
    memset(_dibits, 0, _width*sizeof(_dibits[0]));
    _hdcmem = CreateCompatibleDC(_hdc);
    assert(_hdcmem);
    SelectObject(_hdcmem, _hBitmap);
    return true;
}

//----------------------------------------------------------------------
//
// Win32 window procedure: Processes the next message for this window
//
//----------------------------------------------------------------------

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) 
{
    RECT rect;
    PAINTSTRUCT ps;
    static int testnum = -1;
    static SGRect cliprect = { 0, 0, DEMO_WIDTH, DEMO_HEIGHT};
    HDC hdc;

    switch (message)
    {
    case WM_CREATE:
        SetScrollRange(hwnd, SB_HORZ, 0, cliprect.w, false);
        SetScrollPos(hwnd, SB_HORZ, 0, false);
        SetScrollRange(hwnd, SB_VERT, 0, cliprect.h, false);
        SetScrollPos(hwnd, SB_VERT, 0, true);
        return 0;

    case WM_KEYDOWN:
        ++testnum;
        cliprect.x = cliprect.y = 0;
        SetScrollPos(hwnd, SB_HORZ, cliprect.x, false);
        SetScrollPos(hwnd, SB_VERT, cliprect.y, true);
        InvalidateRect(hwnd, NULL, true);
        return 0;

    case WM_HSCROLL:
        if (LOWORD(wParam) == SB_THUMBTRACK)
        {
            cliprect.x = HIWORD(wParam);
            SetScrollPos(hwnd, SB_HORZ, cliprect.x, true);
            InvalidateRect(hwnd, NULL, true);
        }
        return 0;

    case WM_VSCROLL:
        if (LOWORD(wParam) == SB_THUMBTRACK)
        {
            cliprect.y = HIWORD(wParam);
            SetScrollPos(hwnd, SB_VERT, cliprect.y, true);
            InvalidateRect(hwnd, NULL, true);
        }
        return 0;

    case WM_PAINT: 
        hdc = BeginPaint(hwnd, &ps);
        {
            BasicRenderer rend(hdc);
            AA4x8Renderer aarend(hdc);

            if (!runtest(testnum, &rend, &aarend, cliprect))
            {
                testnum = 0;  // we ran the last test, so start over
                runtest(testnum, &rend, &aarend, cliprect);
            }
        }
        EndPaint(hwnd, &ps);
        return 0; 

    case WM_DESTROY: 
        PostQuitMessage(0); 
        return 0; 
    } 
    return DefWindowProc(hwnd, message, wParam, lParam); 
}


