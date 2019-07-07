//-----------------------------------------------------------
// 
// Demonstrate use of 2-D Polygonal Shape Generator  
//
//-----------------------------------------------------------

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
// ColorFill class: Fills a shape with a solid color. Derived from 
// the SimpleRenderer class in tests.h, which is derived from the
// Renderer base class in shapegen.h. The ColorFill class
// implementation is device-dependent -- it uses the FillRect function
// in Windows GDI to fill the horizontal spans that comprise the shape.
// You can use the RenderShape function below as a model for
// implementing an enhanced renderer that does pattern fills, gradient
// fills, image fills, alpha blending, and so on.
//
//----------------------------------------------------------------------

class ColorFill : public SimpleRenderer
{
    HDC _hdc;
    HBRUSH _hBrush;
    COLORREF _color;

public:
    ColorFill(HDC hdc) : _hdc(hdc), _hBrush(0), _color(0)
    {
    }
    ~ColorFill()
    {
        DeleteObject(_hBrush);
    }
    void RenderShape(ShapeFeeder *feeder);
    void SetColor(COLOR color);
};

// Fills a series of horizontal spans that comprise a shape
void ColorFill::RenderShape(ShapeFeeder *feeder)
{
    RECT rect;
#if 1
    // This while-loop draws every single pixel in the demo
    while (feeder->GetNextGDIRect(reinterpret_cast<SGRect*>(&rect)))
    {
        FillRect(_hdc, &rect, _hBrush);
    }
#else
    SGTpzd tpzd;

    // Here's a do-it-yourself version that breaks up trapezoids
    // into spans that are fed to the FillRect function
    while (feeder->GetNextTrapezoid(&tpzd))
    {
        while (tpzd.height-- > 0)
        {
            rect.left   = tpzd.xL >> 16;  // xL is pre-biased
            rect.right  = tpzd.xR >> 16;  // xR is pre-biased
            rect.top    = tpzd.ytop++;
            rect.bottom = tpzd.ytop;
            tpzd.xL += tpzd.dxL;
            tpzd.xR += tpzd.dxR;
            FillRect(_hdc, &rect, _hBrush);
        }
    }
#endif
}

// Selects the color to be used for subsequent fills and strokes
void ColorFill::SetColor(COLOR color)
{
    _color = color;
    if (_hBrush)
        DeleteObject(_hBrush);

    _hBrush = CreateSolidBrush(_color);
    SelectObject(_hdc, _hBrush);
}

//----------------------------------------------------------------------
//
// Process the next message for this window
//
//----------------------------------------------------------------------

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) 
{
    RECT rect;
    PAINTSTRUCT ps;
    static HDC hdc = 0;
    static int testnum = -1;
    static SGRect cliprect = { 0, 0, DEMO_WIDTH, DEMO_HEIGHT };

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
            ColorFill fillobj(hdc);

            if (!runtest(testnum, &fillobj, cliprect))
            {
                testnum = 0;  // we ran the last test, so start over
                runtest(testnum, &fillobj, cliprect);
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


