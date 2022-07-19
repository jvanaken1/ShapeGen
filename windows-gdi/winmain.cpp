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
    UserMessage usrmsg;
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
                PostQuitMessage(0);
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
                usrmsg.ShowMessage("Failed to allocate frame buffer",
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
            // Pass frame-buffer descriptor to both renderers
            FRAME_BUFFER frmbuf;
            frmbuf.pixels = (COLOR*)bmpixels;
            frmbuf.width  = cliprect.w;
            frmbuf.height = cliprect.h;
            frmbuf.depth  = 32;
            frmbuf.pitch  = 4*cliprect.w;  // specified in bytes
            BasicRenderer rend(&frmbuf);
            AA4x8Renderer aarend(&frmbuf);

            // Draw next frame
            testnum = runtest(testnum, &rend, &aarend, cliprect);
            if (testnum < 0)
            {
                PostQuitMessage(0);
                return 0;
            }

            // Copy frame buffer to screen
            RECT rect = { 0, 0, cliprect.w, cliprect.h };
            hdcMem = CreateCompatibleDC(hdc);
            SelectObject(hdcMem, hbmp);
            BitBlt(hdc, 0, 0, cliprect.w, cliprect.h, hdcMem, 0, 0, SRCCOPY);
            DeleteDC(hdcMem);
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

