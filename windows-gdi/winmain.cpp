//---------------------------------------------------------------------
//
//  winmain.cpp:
//    This file contains all the platform-dependent code needed to
//    run the ShapeGen demo programs on the Win32 API in Windows
//
//---------------------------------------------------------------------

#include <windows.h>
#include <string.h>
#include "demo.h"

// Make command-line args globally accessible
int _argc_ = 0;
char **_argv_ = 0;

// Display error/warning/info text message for user
void UserMessage::ShowMessage(char *text, char *caption, int msgcode)
{
    int wincode = MB_ICONERROR;  // win32 message code

    if (msgcode == MESSAGECODE_INFORMATION)
        wincode = MB_ICONINFORMATION;
    else if (msgcode == MESSAGECODE_WARNING)
        wincode = MB_ICONWARNING;

    MessageBox(0, text, caption, wincode);
}

//----------------------------------------------------------------------
//
// WinMain entry point - Win32 window creation
//
//----------------------------------------------------------------------
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
                        DEMO_WIDTH+16, DEMO_HEIGHT+39,
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
// Win32 window procedure: Processes the next message for this window
//
//----------------------------------------------------------------------
LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    static BITMAPINFO bminfo;
    static void *bmpixels = 0;
    static HBITMAP hbmp = 0;
    static int testnum = 0;
    static SGRect cliprect = { 0, 0, DEMO_WIDTH, DEMO_HEIGHT };
    bool mod;
    PAINTSTRUCT ps;
    HDC hdc;
    HDC hdcMem;

    switch (message)
    {
    case WM_CREATE:
        memset(&bminfo, 0, sizeof(bminfo));
        bminfo.bmiHeader.biSize = sizeof(bminfo.bmiHeader);
        bminfo.bmiHeader.biWidth = cliprect.w;
        bminfo.bmiHeader.biHeight = -cliprect.h;
        bminfo.bmiHeader.biPlanes = 1;
        bminfo.bmiHeader.biBitCount = 32;
        bminfo.bmiHeader.biCompression = BI_RGB;
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
            if (mod)
            {
                PostQuitMessage(0);
                return 0;
            }
            else
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
        if (hbmp)
        {
            DeleteObject(hbmp);
            hbmp = 0;
        }
        if (cliprect.w > 0 && cliprect.h > 0)
        {
            bminfo.bmiHeader.biWidth = cliprect.w;
            bminfo.bmiHeader.biHeight = -cliprect.h;
            hbmp = CreateDIBSection(0, &bminfo, DIB_RGB_COLORS,
                                    &bmpixels, 0, 0);
            if (!hbmp || !bmpixels)
            {
                UserMessage usrmsg;
                usrmsg.ShowMessage("Failed to allocate back buffer",
                                   "ShapeGen demo - out of memory", MESSAGECODE_ERROR);
                PostQuitMessage(0);
                return 0;
            }
            memset(bmpixels, 0xff, 4*cliprect.w*cliprect.h);
            InvalidateRect(hwnd, NULL, true);
        }
        return 0;

    case WM_ERASEBKGND:  // prevent flicker
        return 0;

    case WM_PAINT:
        hdc = BeginPaint(hwnd, &ps);
        if (hbmp)
        {
            // Back-buffer descriptor will be passed to renderers
            BACK_BUFFER bkbuf;
            bkbuf.pixels = (COLOR*)bmpixels;
            bkbuf.width  = cliprect.w;
            bkbuf.height = cliprect.h;
            bkbuf.depth  = 32;
            bkbuf.pitch  = 4*cliprect.w;  // pitch specified in bytes

            // Render next frame into back buffer
            testnum = RunTest(testnum, bkbuf, cliprect);
            if (testnum < 0)
            {
                PostQuitMessage(0);  // negative retval means quit
                return 0;
            }

            // Copy back buffer to screen
            hdcMem = CreateCompatibleDC(hdc);
            SelectObject(hdcMem, hbmp);
            BitBlt(hdc, 0, 0, cliprect.w, cliprect.h, hdcMem, 0, 0, SRCCOPY);
            DeleteDC(hdcMem);

            // Clear back buffer (set background color = white)
            memset(bmpixels, 0xff, 4*cliprect.w*cliprect.h);
        }
        EndPaint(hwnd, &ps);
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, message, wParam, lParam);
}

