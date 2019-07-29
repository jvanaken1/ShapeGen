//---------------------------------------------------------------------
//
//  sdlmain.cpp:
//    This file contains all the platform-dependent functions for
//    running the ShapeGen demo on SDL2 in Windows
//
//---------------------------------------------------------------------

#include <SDL.h>
#include <stdio.h>
#include "demo.h"

//---------------------------------------------------------------------
//
// ColorFill class: Fills a shape with a solid color. Derived from 
// the SimpleRenderer class in tests.h, which is derived from the
// Renderer base class in shapegen.h. The ColorFill class
// implementation is platform-dependent: it uses the SDL_FillRect
// function in SDL2 to fill the horizontal spans that comprise the
// shape. You can use the RenderShape function below as a model for
// implementing an enhanced renderer that does pattern fills, gradient
// fills, image fills, alpha blending, and so on.
//
//---------------------------------------------------------------------

class ColorFill : public SimpleRenderer
{
    SDL_Surface* _surface;
    unsigned int _pixel;

public:
    ColorFill(SDL_Surface* screenSurface) : _surface(screenSurface), _pixel(0)
    {
    }
    ~ColorFill()
    {
    }
    void SetColor(COLOR color);
    void RenderShape(ShapeFeeder *feeder);
};

// Fills a series of horizontal spans that comprise a shape
void ColorFill::RenderShape(ShapeFeeder *feeder)
{
    SDL_Rect rect;
#if 1
    // This while-loop draws every single pixel in the demo
    while (feeder->GetNextSDLRect(reinterpret_cast<SGRect*>(&rect)))
    {
        SDL_FillRect(_surface, &rect, _pixel);
    }
#else
    SGTpzd tpzd;

    // Here's a do-it-yourself version that breaks up trapezoids
    // into spans that are fed to the SDL_FillRect function
    while (feeder->GetNextTrapezoid(&tpzd))
    {
        while (tpzd.height-- > 0)
        {
            int iL = tpzd.xL >> 16;  // xL is pre-biased
            int iR = tpzd.xR >> 16;  // xR is pre-biased

            rect.x = iL;
            rect.y = tpzd.ytop++;
            rect.w = iR - iL;
            rect.h = 1;
            SDL_FillRect(_surface, &rect, _pixel);
            tpzd.xL += tpzd.dxL;
            tpzd.xR += tpzd.dxR;
        }
    }
#endif
}

// Sets the pixel value to be used for color-fill operations
void ColorFill::SetColor(COLOR color)
{
    int r, g, b;

    GetRgbValues(color, &r, &g, &b);
    _pixel = SDL_MapRGB(_surface->format, r, g, b);
}

//---------------------------------------------------------------------
//
// SDL2 main function
//
//---------------------------------------------------------------------

int main(int argc, char *argv[])
{
    SDL_Window *window = 0;
    SDL_Surface* screenSurface = 0;
    SDL_Event evt;
    bool redraw = true;
    bool quit = false;
    int testnum = -1;
    SGRect cliprect = { 0, 0, DEMO_WIDTH, DEMO_HEIGHT};

    printf("Starting SDL2 app...\n");
    SDL_Init(SDL_INIT_VIDEO);
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
    screenSurface = SDL_GetWindowSurface(window);
    if (screenSurface == 0)
    {
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
            ColorFill fillobj(screenSurface);

            SDL_FillRect(screenSurface, 0, SDL_MapRGB(screenSurface->format, 255, 255, 255));
            if (!runtest(testnum, &fillobj, cliprect))
            {
                testnum = 0;  // we just ran the final test, so start over
                runtest(testnum, &fillobj, cliprect);
            }
            SDL_UpdateWindowSurface(window);
            redraw = false;
        }
    }
    printf("Quitting SDL2 app...\n");
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
