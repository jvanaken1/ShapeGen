//---------------------------------------------------------------------
//
//  winmain.cpp:
//    This file contains all the platform-dependent functions for
//    running the ShapeGen demo on the Win32 API in Windows. Two
//    sample renderers are included.
//
//---------------------------------------------------------------------

#include <windows.h>
#include <assert.h>
#include "demo.h"

int _argc_ = 0;
char **_argv_ = 0;

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

int WINAPI WinMain(HINSTANCE hInstance,
                   HINSTANCE hPrevInstance,
                   PSTR szCmdLine,
                   int iCmdShow)
{
    static TCHAR szAppName[] = TEXT("ShapeGen Graphics Library");
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
    _argc_ = __argc;
    _argv_ = __argv;
    hwnd = CreateWindow(szAppName,
                        TEXT("ShapeGen Graphics Demo"),
                        WS_OVERLAPPEDWINDOW,
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

// Display error message for user
void UserMessage::ShowMessage(char *text, char *title, int msgcode)
{
    int wincode;  // Windows MessageBox code

    switch (msgcode)
    {
    case MESSAGECODE_INFORMATION:
        wincode = MB_ICONINFORMATION;
        break;
    case MESSAGECODE_WARNING:
        wincode = MB_ICONWARNING;
        break;
    case MESSAGECODE_ERROR:
    default:
        wincode = MB_ICONERROR;
        break;
    }
    MessageBox(0, text, title, wincode);
}

//---------------------------------------------------------------------
//
// A basic renderer: Fills a shape with a solid color, but does _NOT_
// do antialiasing. This class is derived from the SimpleRenderer class
// in demo.h, which, in turn, is derived from the Renderer base class
// in shapegen.h. This BasicRenderer class implementation is platform-
// dependent -- it uses the FillRect function in Windows GDI to fill
// the horizontal spans that comprise the shape.
//
//---------------------------------------------------------------------

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
        SetColor(RGBX(0,0,0));
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

//---------------------------------------------------------------------
//
// An enhanced renderer: Works exclusively with full-color displays.
// Does antialiasing, alpha blending, solid-color fills, pattern
// fills, linear gradient fills, and radial gradient fills. The
// RenderShape function uses an "AA-buffer" to keep track of pixel
// coverage (for antialiasing). The AA-buffer dedicates a 32-bit
// bitmask, organized as 4 rows of 8 bits, to each pixel in the
// current scan line. This AA4x8Renderer class implementation is
// platform-dependent: it calls the Windows GDI CreateDIBSection
// function to create an offscreen buffer for one scanline of source
// pixels, and calls the AlphaBlend function to blend the source
// pixels into the destination image in the window's client area.
//
//---------------------------------------------------------------------

class AA4x8Renderer : public EnhancedRenderer
{
    friend ShapeGen;

    HDC _hdc;          // device context handle for window
    HDC _hdcmem;       // device context handle for DIB
    HBITMAP _hBitmap;  // DIB handle
    COLOR *_pixbuf;    // DIB pixel data bits
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

protected:
    void RenderShape(ShapeFeeder *feeder);
    bool SetMaxWidth(int width);
    int QueryYResolution() { return 2; }
    bool SetScrollPosition(int x, int y);

public:
    AA4x8Renderer(HDC hdc);
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

AA4x8Renderer::AA4x8Renderer(HDC hdc)
        : _hdc(hdc), _hdcmem(0), _hBitmap(0),
          _width(0), _pixbuf(0), _aabuf(0), _paintgen(0),
          _stopCount(0), _pxform(0), _alpha(255),
          _xscroll(0), _yscroll(0)
{
    memset(&_lut[0], 0, sizeof(_lut));
    memset(&_aarow[0], 0, sizeof(_aarow));
    memset(&_cstop[0], 0, sizeof(_cstop));
    memset(&_xform[0], 0, sizeof(_xform));
    SetColor(RGBX(0,0,0));
}

AA4x8Renderer::~AA4x8Renderer()
{
    DeleteDC(_hdcmem);
    DeleteObject(_hBitmap);
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
    DeleteDC(_hdcmem);
    _hdcmem = 0;
    DeleteObject(_hBitmap);
    _hBitmap = 0;
    _pixbuf = 0;
    delete[] _aabuf;
    _aabuf = 0;

    // Allocate the new AA-buffer
    _aabuf = new int[_width];
    assert(_aabuf);
    memset(_aabuf, 0, _width*sizeof(_aabuf[0]));  // debug aid
    for (int i = 0; i < 4; ++i)
        _aarow[i] = &_aabuf[i*_width/4];

    // Allocate a DIB section big enough to store one scan line
    // of pixels. For biCompression = BI_RGB, each 32-bit pixel
    // in the DIB is an RGBQUAD struct with little-endian layout
    // (i.e., with blue in the 8 LSBs, and alpha in the 8 MSBs).
    BITMAPINFO bmi;

    memset(&bmi.bmiHeader, 0, sizeof(bmi.bmiHeader));
    bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
    bmi.bmiHeader.biWidth = _width;
    bmi.bmiHeader.biHeight = 1;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    _hBitmap = CreateDIBSection(0, &bmi, DIB_RGB_COLORS, (void **)&_pixbuf, 0, 0);
    assert(_hBitmap);
    assert(_pixbuf);
    memset(_pixbuf, 0, _width*sizeof(_pixbuf[0]));
    _hdcmem = CreateCompatibleDC(_hdc);
    assert(_hdcmem);
    SelectObject(_hdcmem, _hBitmap);
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

    // Blit the painted source pixels from the DIB to the client rect
    BLENDFUNCTION bfun = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };

    AlphaBlend(_hdc, xleft, yscan, len, 1, _hdcmem, xleft, 0, len, 1, bfun);
    memset(buffer, 0, len*sizeof(_pixbuf[0]));
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
    opacity += 128;
    opacity += opacity >> 8;
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

//----------------------------------------------------------------------
//
// Win32 window procedure: Processes the next message for this window
//
//----------------------------------------------------------------------
LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    bool mod;
    PAINTSTRUCT ps;
    static int testnum = 0;
    static SGRect cliprect = { 0, 0, DEMO_WIDTH, DEMO_HEIGHT };
    HDC hdc;

    switch (message)
    {
    case WM_CREATE:
        return 0;

    case WM_KEYDOWN:
        mod = (GetKeyState(VK_SHIFT) | GetKeyState(VK_CONTROL)) < 0;
        switch (wParam)
        {
        case VK_LEFT:
            if (mod)
                cliprect.x -= 5;
            else
            {
                --testnum;
                cliprect.x = cliprect.y = 0;
            }
            break;
        case VK_UP:
            if (mod)
                cliprect.y -= 5;
            else
                cliprect.x = cliprect.y = 0;
            break;
        case VK_RIGHT:
            if (mod)
                cliprect.x += 5;
            else
            {
                ++testnum;
                cliprect.x = cliprect.y = 0;
            }
            break;
        case VK_DOWN:
            if (mod)
                cliprect.y += 5;
            else
                cliprect.x = cliprect.y = 0;
            break;
        case VK_ESCAPE:
            testnum = 0;
            break;
        case VK_SHIFT:
        case VK_CONTROL:
            return 0;
        default:
            ++testnum;
            cliprect.x = cliprect.y = 0;
            break;
        }
        InvalidateRect(hwnd, NULL, true);
        return 0;

    case WM_SIZE:
        cliprect.w = LOWORD(lParam);
        cliprect.h = HIWORD(lParam);
        InvalidateRect(hwnd, NULL, true);
        return 0;

    case WM_PAINT:
        hdc = BeginPaint(hwnd, &ps);
        if (cliprect.w > 0 && cliprect.h > 0)
        {
            BasicRenderer rend(hdc);
            AA4x8Renderer aarend(hdc);

            testnum = runtest(testnum, &rend, &aarend, cliprect);
            if (testnum < 0)
                PostQuitMessage(0);
        }
        EndPaint(hwnd, &ps);
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, message, wParam, lParam);
}


