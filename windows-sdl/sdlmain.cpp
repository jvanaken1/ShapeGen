//---------------------------------------------------------------------
//
//  sdlmain.cpp:
//    This file contains all the platform-dependent code needed to
//    run the ShapeGen demo programs on the SDL API in Windows
//
//---------------------------------------------------------------------

#include <SDL.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "demo.h"

// Make command-line args globally accessible
int _argc_ = 0;
char **_argv_ = 0;

// Display error/warning/info text message for user
void UserMessage::ShowMessage(char *text, char *caption, int msgcode)
{
    int sdlcode = SDL_MESSAGEBOX_ERROR;  // SDL MessageBox code

    if (msgcode == MESSAGECODE_INFORMATION)
        sdlcode = SDL_MESSAGEBOX_INFORMATION;
    else if (msgcode == MESSAGECODE_WARNING)
        sdlcode = SDL_MESSAGEBOX_WARNING;

    SDL_ShowSimpleMessageBox(sdlcode, caption, text, 0);
}

//---------------------------------------------------------------------
//
// SDL2 main function
//
//---------------------------------------------------------------------

int main(int argc, char *argv[])
{
    SDL_Window *window = 0;
    SDL_Surface *winsurf = 0;
    SDL_Surface *rgbsurf = 0;
    int testnum = 0;
    SGRect cliprect = { 0, 0, DEMO_WIDTH, DEMO_HEIGHT};
    bool quit = false;
    bool formatsMatch = false;

    printf("Starting SDL2 app...\n");
    _argc_ = argc;
    _argv_ = argv;
    if (SDL_Init(SDL_INIT_VIDEO) < 0)
    {
        printf("ERROR-- %s\n", SDL_GetError());
        return -1;
    }
    window = SDL_CreateWindow("ShapeGen graphics demo",
                              SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                              DEMO_WIDTH, DEMO_HEIGHT, SDL_WINDOW_RESIZABLE);
    if (window == 0)
    {
        printf("ERROR-- %s\n", SDL_GetError());
        SDL_Quit();
        return -1;
    }
    winsurf = SDL_GetWindowSurface(window);
    if (winsurf == 0)
    {
        printf("ERROR-- %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }
    // If the pixel format used by the window's backing surface
    // matches the renderer's internal format, the renderer can
    // do alpha compositing directly to this surface, and avoid
    // having to create a separate RGB surface for compositing.
    formatsMatch = winsurf->format->BitsPerPixel == 32 &&
                   winsurf->format->Rmask == 0x00ff0000 &&
                   winsurf->format->Gmask == 0x0000ff00 &&
                   winsurf->format->Bmask == 0x000000ff;
    // Begin main loop
    for (;;)
    {
        bool redraw = true;
        SDL_Event evt;

        SDL_WaitEvent(&evt);
        if (evt.type == SDL_QUIT)
        {
            quit = true;
        }
        else if (evt.type == SDL_KEYDOWN)
        {
            int flags = KMOD_ALT | KMOD_SHIFT | KMOD_CTRL;
            bool mod = flags & SDL_GetModState();

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
                    quit = true, redraw = false;
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
        else if (evt.type == SDL_WINDOWEVENT)
        {
            switch (evt.window.event)
            {
            case SDL_WINDOWEVENT_SHOWN:
            case SDL_WINDOWEVENT_RESIZED:
                SDL_GetWindowSize(window, &cliprect.w, &cliprect.h);
                winsurf = SDL_GetWindowSurface(window);
                if (winsurf == 0)
                {
                    printf("ERROR-- %s\n", SDL_GetError());
                    SDL_DestroyWindow(window);
                    SDL_Quit();
                    return -1;
                }
                if (!formatsMatch)
                {
                    if (rgbsurf)
                        SDL_FreeSurface(rgbsurf);

                    // Our two renderers require a back buffer
                    // with a 32-bit BGRA pixel format
                    rgbsurf = SDL_CreateRGBSurface(
                                    0,
                                    winsurf->w,
                                    winsurf->h,
                                    32,
                                    0x00ff0000,  // Rmask
                                    0x0000ff00,  // Gmask
                                    0x000000ff,  // Bmask
                                    0x00000000); // Amask
                    if (rgbsurf == 0)
                    {
                        printf("ERROR-- %s\n", SDL_GetError());
                        SDL_DestroyWindow(window);
                        SDL_Quit();
                        return -1;
                    }
                    SDL_FillRect(rgbsurf, 0, 0xffffffff);
                }
                else
                    SDL_FillRect(winsurf, 0, 0xffffffff);

                break;
            default:
                redraw = false;
                break;
            }
        }
        else
            redraw = false;

        if (redraw && cliprect.w > 0 && cliprect.h > 0)
        {
            // Back-buffer descriptor will be passed to renderers
            SDL_Surface *surf = formatsMatch ? winsurf : rgbsurf;
            BACK_BUFFER bkbuf;
            bkbuf.pixels = (COLOR*)surf->pixels;
            bkbuf.width  = surf->w;
            bkbuf.height = surf->h;
            bkbuf.depth  = 32;
            bkbuf.pitch  = surf->pitch;

            // Draw next frame in demo
            testnum = RunTest(testnum, bkbuf, cliprect);
            if (testnum >= 0)
            {
                // Copy back buffer to screen
                if (!formatsMatch)
                    SDL_BlitSurface(rgbsurf, 0, winsurf, 0);

                SDL_UpdateWindowSurface(window);

                // Clear back buffer (set background color = white)
                SDL_FillRect(surf, 0, 0xffffffff);
            }
            else
                quit = true;
        }
        if (quit)
        {
            printf("Quitting SDL2 app...\n");
            SDL_DestroyWindow(window);
            SDL_Quit();
            break;  // exit main loop
        }
    }
    return 0;
}
