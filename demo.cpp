/*
  Copyright (C) 2019 Jerry R. VanAken

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.

  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.

  3. This notice may not be removed or altered from any source distribution.
*/
//----------------------------------------------------------------------
//
// Demo code: Demonstrate how to use the 2-D Polygonal Shape Generator
//
//----------------------------------------------------------------------

#include <math.h>
#include <assert.h>
#include "demo.h"

//----------------------------------------------------------------------
//
// Layered stroke effects: A cheap trick for anti-aliasing stroked and
// filled shapes against a solid background color
//
//----------------------------------------------------------------------

// Blend two pixel values
COLOR BlendPixels(FIX16 alpha, COLOR pix1, COLOR pix2)
{
    //assert(0 <= alpha && alpha <= 0x00010000);
    FIX16 beta = 0x00010000 - alpha;
    int r1, g1, b1, r2, g2, b2, r, g, b;

    GetRgbValues(pix1, &r1, &g1, &b1);
    GetRgbValues(pix2, &r2, &g2, &b2);
    r = (alpha*r1 + beta*r2) >> 16;
    g = (alpha*g1 + beta*g2) >> 16;
    b = (alpha*b1 + beta*b2) >> 16;
    return RGBVAL(r,g,b);
}

// Draws an anti-aliased, stroked path against a solid background color
void StrokePathAA(ShapeGen *sg, SimpleRenderer *renderer, COLOR crBkgd, COLOR crFill)
{
    FIX16 t = 0x0000c000;
    float savewidth = sg->SetLineWidth(0);
    float width = max(1.5, savewidth + 0.5);

    for (int i = 0; i < 4; ++i)
    {
        COLOR cr = BlendPixels(t, crBkgd, crFill);
        t -= 0x00004000;
        renderer->SetColor(cr);
        sg->SetLineWidth(width);
        width -= 0.25;
        sg->StrokePath();
    }
    sg->SetLineWidth(savewidth);  // restore original line width
}

// Draws an anti-aliased, filled path against a solid background color
void FillPathAA(ShapeGen *sg, SimpleRenderer *renderer, COLOR crBkgd, COLOR crFill, FILLRULE rule)
{
    float savewidth = sg->SetLineWidth(2.0);
    float width = savewidth - 0.25;
    StrokePathAA(sg, renderer, crBkgd, crFill);
    sg->SetLineWidth(width);
    sg->FillPath(rule);
    sg->SetLineWidth(savewidth);
}

// Draws anti-aliased, rotated text against a solid background color
void DrawTextAA(ShapeGen *sg, SimpleRenderer *renderer, TextApp *txt, 
                const float xfrm[][3], const char *str, 
                COLOR crBkgd, COLOR crFill)
{
    FIX16 t = 0x0000c000;
    float savewidth = sg->SetLineWidth(0);
    float width = max(1.5, savewidth + 0.5);

    for (int i = 0; i < 4; ++i)
    {
        COLOR cr = BlendPixels(t, crBkgd, crFill);
        t -= 0x00004000;
        renderer->SetColor(cr);
        sg->SetLineWidth(width);
        width -= 0.25;
        txt->DisplayText(&(*sg), xfrm, str);
    }
    sg->SetLineWidth(savewidth);  // restore original line width
}

// Draws anti-aliased, horizontal text against a solid background color
void DrawTextAA(ShapeGen *sg, SimpleRenderer *renderer, TextApp *txt, 
                SGPoint& xystart, float scale, const char *str, 
                COLOR crBkgd, COLOR crFill)
{
    float xfrm[2][3];

    xfrm[0][0] = scale;
    xfrm[0][1] = 0;
    xfrm[0][2] = xystart.x;
    xfrm[1][0] = 0;
    xfrm[1][1] = scale;
    xfrm[1][2] = xystart.y;
    DrawTextAA(sg, renderer, txt, xfrm, str, crBkgd, crFill);
}

//---------------------------------------------------------------------
//
// These utility functions are handy for resizing shapes and moving
// them around in the window
//
//---------------------------------------------------------------------

// Converts the integer coordinates in a rectangle to 16.16 fixed point
inline SGRect* IntToFixed(SGRect *box)
{
    box->x <<= 16;
    box->y <<= 16;
    box->w <<= 16;
    box->h <<= 16;
    return box;
}

// Converts an array of integer x-y coordinates to 16.16 fixed point
inline SGPoint* IntToFixed(int npts, SGPoint xy[])
{
    for (int i = 0; i < npts; ++i)
    {
        xy[i].x = xy[i].x << 16;
        xy[i].y = xy[i].y << 16;
    }
    return xy;
}

// Calculates the x-y coordinates at the center of a bounding box
inline SGPoint* GetBboxCenter(SGPoint *xy, const SGRect *bbox)
{
    xy->x = bbox->x + (bbox->w + 1)/2;
    xy->y = bbox->y + (bbox->h + 1)/2;
    return xy;
}

// Calculates the minimum bounding box for an array of points
SGRect* GetBbox(SGRect *bbox, int npts, const SGPoint xy[])
{
    SGCoord xmin = (1<<31)-1, ymin = (1<<31)-1, xmax = -1<<31, ymax = -1<<31;

    for (int i = 0; i < npts; ++i)
    {
        xmin = min(xy[i].x, xmin);
        ymin = min(xy[i].y, ymin);
        xmax = max(xy[i].x, xmax);
        ymax = max(xy[i].y, ymax);
    }
    bbox->x = xmin;
    bbox->y = ymin;
    bbox->w = xmax - xmin;
    bbox->h = ymax - ymin;
    return bbox;
}

// Scales and translates points in the source bounding box, bbsrc,
// to exactly fit the destination bounding box, bbdst. The function
// calculates the transformation from bbsrc to bbdst, applies this
// transform to the points in the xysrc array, and writes the
// transformed points to the xydst array. Parameter npts is the
// length of the xysrc and xydst arrays. All source and destination
// coordinates are assumed to be in 16.16 fixed-point format.
void FitBbox(const SGRect *bbdst, int npts, SGPoint xydst[], 
             SGRect *bbsrc, const SGPoint xysrc[])
{
    SGPoint cin, cout;
    float xscale, yscale;

    // Calculate center coordinates of source/dest bounding boxes
    GetBboxCenter(&cin, bbsrc);
    GetBboxCenter(&cout, bbdst);

    // Scale the source points and translate them to the dest bbox
    xscale = float(bbdst->w)/bbsrc->w;
    yscale = float(bbdst->h)/bbsrc->h;
    for (int i = 0; i < npts; ++i)
    {
        xydst[i].x = xscale*(xysrc[i].x - cin.x) + cout.x;
        xydst[i].y = yscale*(xysrc[i].y - cin.y) + cout.y;
    }
}

//----------------------------------------------------------------------
//
// Zero-terminated arrays of dashed-line patterns
//
//-----------------------------------------------------------------------

char dashedLineDot[]           = { 3, 0 };
char dashedLineShortDash[]     = { 6, 0 };
char dashedLineDashDot[]       = { 9, 6, 3, 6, 0 };
char dashedLineDoubleDot[]     = { 3, 3, 3, 15, 0 };
char dashedLineLongDash[]      = { 18, 6, 0 };
char dashedLineDashDoubleDot[] = { 9, 3, 3, 3, 3, 3, 0 };

char *dasharray[] = {
    dashedLineDot,          
    dashedLineShortDash,    
    dashedLineDashDot,      
    dashedLineDoubleDot,    
    dashedLineLongDash,     
    dashedLineDashDoubleDot,
};

//----------------------------------------------------------------------
//
// Demo functions and code examples
//
//-----------------------------------------------------------------------

void demo01(SimpleRenderer *rend, const SGRect& clip)  // page 1
{
    SGPtr sg(rend, clip);
    TextApp txt;
    float xfrm[2][3] = {
        { 2.0,   0,  175.0},
        { -0.8, 2.5, 685.0},
    };
    COLOR crBkgd = RGBVAL(222,222,255);
    COLOR color[] = {
        RGBVAL(222, 100, 50), 
        RGBVAL(185, 100, 222), 
        RGBVAL(90, 206, 45),
        RGBVAL(220, 20, 60), 
        RGBVAL(0, 206, 209),
    };
    SGPoint arrow[] = {
        {  0+1170, 30+889 }, 
        { 30+1170, 30+889 }, 
        { 30+1170, 40+889 }, 
        { 63+1170, 20+889 }, 
        { 30+1170,  0+889 }, 
        { 30+1170, 10+889 }, 
        {  0+1170, 10+889 },
    };
    SGPoint xystart;
    SGPoint corner = { 40, 40 };
    SGPoint elips[] = { { 640, 440 }, { 640+550, 435 }, { 640, 435+110 }, };
    float len = 110;
    float angle = +PI/8.1;
    char *str = "???";
    float scale = 1.0;
    float width;
    int i;

    // Fill background and draw frame around window
    SGRect frame = { 10, 10, DEMO_WIDTH-20, DEMO_HEIGHT-20 };
    sg->BeginPath();
    sg->RoundedRectangle(frame, corner);
    rend->SetColor(crBkgd);  // RGBVAL(222,222,255);
    sg->FillPath(FILLRULE_EVENODD);
    sg->SetLineWidth(8.0);
    rend->SetColor(color[0]);  // RGBVAL(222, 100, 50)
    sg->StrokePath();

    // Draw background pattern: rotated ellipses
    rend->SetColor(RGBVAL(233, 150, 122));
    sg->BeginPath();
    for (i = 0; i < 26; ++i)
    {
        float cosa = cos(angle);
        float sina = sin(angle);
        elips[1] = elips[2] = elips[0];
        elips[1].x += 5.8*len*cosa;
        elips[1].y -= 5.8*len*sina;
        elips[2].x += len*sina;
        elips[2].y += len*cosa;
        sg->Ellipse(elips[0], elips[1], elips[2]);
        angle -= PI/30.0;
        len *= 0.95;
    }
    sg->StrokePath();

    // Draw horizontal text
    sg->SetLineWidth(6.0);
    str = "2-D Polygonal Shape Generator";
    scale = 0.73;
    width = txt.GetTextWidth(scale, str);
    xystart.x = (DEMO_WIDTH - width)/2;
    xystart.y = 130;
    DrawTextAA(&(*sg), rend, &txt, xystart, scale, str, 
               crBkgd, RGBVAL(40, 70, 110));
    sg->SetLineWidth(4.0);
    str = "At the core of a 2-D graphics system";
    scale = 0.5;
    width = txt.GetTextWidth(scale, str);
    xystart.x = (DEMO_WIDTH - width)/2;
    xystart.y = 780;
    DrawTextAA(&(*sg), rend, &txt, xystart, scale, str, 
               crBkgd, RGBVAL(40, 70, 110));
    str = "A portable, lightweight C++ implementation";
    width = txt.GetTextWidth(scale, str);
    xystart.x = (DEMO_WIDTH - width)/2;
    xystart.y += 64;
    DrawTextAA(&(*sg), rend, &txt, xystart, scale, str, 
               crBkgd, RGBVAL(40, 70, 110));
    sg->SetLineWidth(3.0);
    xystart.x = 748;
    xystart.y = 916;

    DrawTextAA(&(*sg), rend, &txt, xystart, 0.34, 
               "Hit space bar to continue", crBkgd, color[0]);
    sg->BeginPath();
    sg->Move(arrow[0].x, arrow[0].y);
    sg->PolyLine(ARRAY_LEN(arrow)-1, &arrow[1]);
    sg->FillPath(FILLRULE_EVENODD);

    // Draw "ShapeGen" text using slanted text and shadowing
    str = "ShapeGen";
    sg->SetFlatness(FLATNESS_DEFAULT);
    sg->SetLineWidth(32.0);
    rend->SetColor(RGBVAL(99,99,99));
    txt.SetTextSpacing(1.2);
    txt.DisplayText(&(*sg), xfrm, str);

    xfrm[0][2] -= 8.0;
    xfrm[1][2] -= 8.0;

    sg->SetLineWidth(32.0);
    rend->SetColor(color[3]);
    txt.DisplayText(&(*sg), xfrm, str);

    sg->SetLineWidth(24.0);
    rend->SetColor(color[0]);
    txt.DisplayText(&(*sg), xfrm, str);
}

void demo02(SimpleRenderer *rend, const SGRect& clip)
{
    SGPtr sg(rend, clip);
    TextApp txt;
    COLOR crBkgd  = RGBVAL(240,220,220);
    COLOR crFill  = RGBVAL(70,206,209);
    COLOR crFrame = RGBVAL(50,80,140);
    COLOR crText  = RGBVAL( 40,  70, 110);

    // Fill background and draw frame around window
    SGPoint corner = { 40, 40 };
    SGRect frame = { 10, 10, DEMO_WIDTH-20, DEMO_HEIGHT-20 };
    sg->BeginPath();
    sg->RoundedRectangle(frame, corner);
    rend->SetColor(crBkgd);
    sg->FillPath(FILLRULE_EVENODD);
    sg->SetLineWidth(8.0);
    rend->SetColor(crFrame); 
    sg->StrokePath();

    // Draw title text
    sg->SetLineWidth(6.0);
    char *str = "Path Drawing Modes";
    float scale = 0.73;
    float width = txt.GetTextWidth(scale, str);
    SGPoint xystart;
    xystart.x = (DEMO_WIDTH - width)/2;
    xystart.y = 136;
    DrawTextAA(&(*sg), rend, &txt, xystart, scale, str, 
               crBkgd, crText);

    // Draw captions for three figures
    sg->SetLineWidth(3.0);
    scale = 0.34;
    str = "Even-odd fill rule";
    width = txt.GetTextWidth(scale, str);
    xystart.x = 280 - width/2;
    xystart.y = 796;
    DrawTextAA(&(*sg), rend, &txt, xystart, scale, str, 
               crBkgd, crText);
    str = "Winding number fill rule";
    width = txt.GetTextWidth(scale, str);
    xystart.x = 1000 - width/2;
    DrawTextAA(&(*sg), rend, &txt, xystart, scale, str, 
               crBkgd, crText);
    str = "Brush stroke";
    width = txt.GetTextWidth(scale, str);
    xystart.x = 640 - width/2;
    xystart.y = 670;
    DrawTextAA(&(*sg), rend, &txt, xystart, scale, str, 
               crBkgd, crText);

    // Construct star shape
    SGPoint star[5];
    const int len = ARRAY_LEN(star);
    SGPoint xy[len];
    for (int i = 0; i < len; i++)
    {
        float t = (len-1)*i*PI/len;
        star[i].x =  65536*sin(t);
        star[i].y = -65536*cos(t);
    }
    SGRect bbsrc;
    SGRect bbdst[] = {
        { 100<<16, 380<<16, 360<<16, 360<<16 },  // bbox for even-odd fill
        { 820<<16, 380<<16, 360<<16, 360<<16 },  // bbox for winding fill
        { 460<<16, 238<<16, 360<<16, 360<<16 },  // bbox for stroked path
    };

    // Fill star using even-odd (aka parity) fill rule
    GetBbox(&bbsrc, len, star);
    FitBbox(&bbdst[0], len, xy, &bbsrc, star);
    sg->SetFixedBits(16);
    sg->BeginPath();
    sg->Move(xy[0].x, xy[0].y); 
    sg->PolyLine(ARRAY_LEN(xy)-1, &xy[1]);
    sg->CloseFigure();
    rend->SetColor(crFill);
    sg->FillPath(FILLRULE_EVENODD);
    rend->SetColor(0);
    sg->SetLineWidth(0);
    sg->StrokePath();

    // Fill star using winding number fill rule
    FitBbox(&bbdst[1], len, xy, &bbsrc, star);
    sg->BeginPath();
    sg->Move(xy[0].x, xy[0].y); 
    sg->PolyLine(ARRAY_LEN(xy)-1, &xy[1]);
    sg->CloseFigure();
    rend->SetColor(crFill);
    sg->FillPath(FILLRULE_WINDING);
    rend->SetColor(0);
    sg->SetLineWidth(0);
    sg->StrokePath();

    // Draw star using brush strokes
    FitBbox(&bbdst[2], len, xy, &bbsrc, star);
    rend->SetColor(crFill);
    sg->SetLineJoin(LINEJOIN_MITER);
    sg->SetMiterLimit(2.0);
    sg->SetLineWidth(28.0);
    sg->BeginPath();
    sg->Move(xy[0].x, xy[0].y); 
    sg->PolyLine(ARRAY_LEN(xy)-1, &xy[1]);
    sg->CloseFigure();
    rend->SetColor(crFill);
    sg->StrokePath();
    rend->SetColor(0);
    sg->SetLineWidth(0);
    sg->StrokePath();
}

void demo03(SimpleRenderer *rend, const SGRect& clip)
{
    SGPtr sg(rend, clip);
    TextApp txt;
    COLOR crBkgd  = RGBVAL(222, 222, 255);
    COLOR crFill  = RGBVAL(238, 130, 200);
    COLOR crFrame = RGBVAL( 32, 152, 224);
    COLOR crText  = RGBVAL( 40,  70, 110);

    // Fill background and draw frame around window
    SGPoint corner = { 40, 40 };
    SGRect frame = { 10, 10, DEMO_WIDTH-20, DEMO_HEIGHT-20 };
    sg->BeginPath();
    sg->RoundedRectangle(frame, corner);
    rend->SetColor(crBkgd);
    sg->FillPath(FILLRULE_EVENODD);
    sg->SetLineWidth(8.0);
    rend->SetColor(crFrame); 
    sg->StrokePath();

    // Draw title text
    sg->SetLineWidth(6.0);
    char *str = "Clip to Arbitrary Shapes";
    float scale = 0.73;
    float width = txt.GetTextWidth(scale, str);
    SGPoint xystart;
    xystart.x = (DEMO_WIDTH - width)/2;
    xystart.y = 130;
    DrawTextAA(&(*sg), rend, &txt, xystart, scale, str, 
               crBkgd, crText);

    // Draw text at lower left
    scale = 0.48;
    sg->SetLineWidth(4.0);
    str = "Clip to shape";
    width = txt.GetTextWidth(scale, str);
    xystart.x = 340 - width/2;
    xystart.y = 844;
    DrawTextAA(&(*sg), rend, &txt, xystart, scale, str, 
               crBkgd, crText);

    // Draw text at lower right
    str = "Mask off shape";
    width = txt.GetTextWidth(scale, str);
    xystart.x = 940 - width/2;
    xystart.y = 844;
    DrawTextAA(&(*sg), rend, &txt, xystart, scale, str, 
               crBkgd, crText);

    // Construct star shape
    SGPoint star[5];
    const int len = ARRAY_LEN(star);
    for (int i = 0; i < len; i++)
    {
        float t = (len-1)*i*PI/len;
        star[i].x =  65536*sin(t);
        star[i].y = -65536*cos(t);
    }
    
    // Highlight boxes on left and right
    SGRect rect[] = {
        {  80<<16, 240<<16, 520<<16, 520<<16 },
        { 680<<16, 240<<16, 520<<16, 520<<16 },
    };
    sg->SetLineJoin(LINEJOIN_MITER);
    sg->SetLineEnd(LINEEND_FLAT);
    sg->SetLineWidth(0);
    rend->SetColor(RGBVAL(211, 211, 222));
    sg->SetFixedBits(16);
    sg->BeginPath();
    sg->Rectangle(rect[0]);
    sg->Rectangle(rect[1]);
    sg->FillPath(FILLRULE_EVENODD);

    // Map star shape to clipping region on left
    SGRect bbsrc;
    SGRect bbdst[] = {
        { 100<<16, 280<<16, 480<<16, 440<<16 }, 
        { 700<<16, 280<<16, 480<<16, 440<<16 },
    };
    SGPoint xy[len];

    // Mask off star-shaped region on right side of window
    GetBbox(&bbsrc, len, star);
    FitBbox(&bbdst[1], len, xy, &bbsrc, star);
    sg->BeginPath();
    sg->Move(xy[0].x, xy[0].y);
    sg->PolyLine(len-1, &xy[1]);
    sg->CloseFigure();
    sg->SetMaskRegion(FILLRULE_WINDING);

    // Set to clip to star-shaped region on left
    FitBbox(&bbdst[0], len, xy, &bbsrc, star);
    sg->BeginPath();
    sg->Rectangle(rect[1]);
    sg->Move(xy[0].x, xy[0].y);
    sg->PolyLine(len-1, &xy[1]);
    sg->CloseFigure();
    sg->SetClipRegion(FILLRULE_WINDING);

    // Paint solid blue color inside new clipping region
    IntToFixed(&frame);
    rend->SetColor(RGBVAL(0,100, 200));
    sg->BeginPath();
    sg->Rectangle(frame);
    sg->FillPath(FILLRULE_EVENODD);

    // Paint abstract design inside clipping region
    COLOR color[] = {
        RGBVAL( 16, 108, 240), RGBVAL( 32, 152, 224), RGBVAL( 48, 196, 208),
        RGBVAL( 64, 240, 192), RGBVAL( 80,  28, 176), RGBVAL( 96,  72, 160),
        RGBVAL(112, 116, 144), RGBVAL(128, 160, 128), RGBVAL(144, 204, 112),
        RGBVAL(160, 248,  96), RGBVAL(176,  36,  80), RGBVAL(192,  80,  64),
        RGBVAL(208, 124,  48), RGBVAL(224, 168,  32), RGBVAL(240, 212,  16),
    };
    SGPoint point[3];
    int j = 0;
    point[0].x = frame.x;
    point[0].y = frame.y;
    point[1] = point[2] = point[0];
    point[1].y += frame.h;
    point[2].x += frame.w;
    sg->SetLineWidth(16.0);
    sg->SetLineEnd(LINEEND_FLAT);
    sg->SetLineJoin(LINEJOIN_BEVEL);
    for (int i = 0; i <= frame.w; i += 20<<16)
    {
        rend->SetColor(color[j]);
        if (++j == ARRAY_LEN(color))
            j = 0;

        sg->BeginPath();
        sg->Move(point[0].x, point[0].y);
        sg->PolyLine(ARRAY_LEN(point)-1, &point[1]);
        sg->StrokePath();
        point[1].x = i + frame.x;
    }
}

void demo04(SimpleRenderer *rend, const SGRect& clip)
{
    SGPtr sg(rend, clip);
    TextApp txt;
    LINEJOIN join[] = { LINEJOIN_BEVEL, LINEJOIN_ROUND, LINEJOIN_MITER };
    LINEEND cap[] = { LINEEND_FLAT, LINEEND_ROUND, LINEEND_SQUARE };
    COLOR crBkgd  = RGBVAL(255, 248, 240);
    COLOR crFrame = RGBVAL(230,140,70);
    COLOR crFill = RGBVAL(255, 175, 100);
    COLOR crText = RGBVAL(40, 70, 110);

    // Fill background and draw frame around window
    SGPoint corner = { 40, 40 };
    SGRect frame = { 10, 10, DEMO_WIDTH-20, DEMO_HEIGHT-20 };
    sg->BeginPath();
    sg->RoundedRectangle(frame, corner);
    rend->SetColor(crBkgd);
    sg->FillPath(FILLRULE_EVENODD);
    sg->SetLineWidth(8.0);
    rend->SetColor(crFrame); 
    sg->StrokePath();

    // Draw title text
    sg->SetLineWidth(6.0);
    char *str = "Stroked Line Caps and Joints";
    float scale = 0.73;
    float width = txt.GetTextWidth(scale, str);
    SGPoint xystart;
    xystart.x = (DEMO_WIDTH - width)/2;
    xystart.y = 130;
    DrawTextAA(&(*sg), rend, &txt, xystart, scale, str, 
               crBkgd, crText);

    // Label rows with text
    sg->SetLineWidth(3.0);
    scale = 0.38;
    str = "Flat cap";
    width = txt.GetTextWidth(scale, str);
    xystart.x = 310 - width;
    xystart.y = 428;
    DrawTextAA(&(*sg), rend, &txt, xystart, scale, str, 
               crBkgd, crText);
    str = " Round cap";
    width = txt.GetTextWidth(scale, str);
    xystart.x = 310 - width;
    xystart.y += 190;
    DrawTextAA(&(*sg), rend, &txt, xystart, scale, str, 
               crBkgd, crText);
    str = "Square cap";
    width = txt.GetTextWidth(scale, str);
    xystart.x = 310 - width;
    xystart.y += 190;
    DrawTextAA(&(*sg), rend, &txt, xystart, scale, str, 
               crBkgd, crText);

    // Label columns with text
    str = "Bevel joint";
    xystart.x = 392;
    xystart.y = 268;
    DrawTextAA(&(*sg), rend, &txt, xystart, scale, str, 
               crBkgd, crText);
    str = "Round joint";
    xystart.x += 280;
    DrawTextAA(&(*sg), rend, &txt, xystart, scale, str, 
               crBkgd, crText);
    str = "Miter joint";
    xystart.x += 280;
    DrawTextAA(&(*sg), rend, &txt, xystart, scale, str, 
               crBkgd, crText);

    // Draw stroked lines with different caps and joins
    SGPoint point[3] = { 
        { 40, 181 }, { 163, 44 }, { 228, 181 },
    };
    for (int i = 0; i < 3; ++i)
    {
        SGPoint p0, p1, p2;
        SGCoord dx = 320 + 280*i;

        p0.x = point[0].x + dx;
        p1.x = point[1].x + dx;
        p2.x = point[2].x + dx;
        sg->SetLineJoin(join[i]);
        for (int j = 0; j < 3; ++j)
        {
            SGCoord dy = 280 + 190*j;
            p0.y = point[0].y + dy;
            p1.y = point[1].y + dy;
            p2.y = point[2].y + dy;
            sg->SetLineEnd(cap[j]);
            rend->SetColor(crFill);
            sg->SetLineWidth(38.0);
            sg->BeginPath();
            sg->Move(p0.x, p0.y);
            sg->Line(p1.x, p1.y);
            sg->Line(p2.x, p2.y);
            sg->StrokePath();

            rend->SetColor(0);
            sg->SetLineWidth(0);
            sg->StrokePath();

            SGPoint diam[2];
            sg->BeginPath();
            diam[0] = diam[1] = p0;
            diam[0].x += 3;
            diam[1].y += 3;
            sg->Ellipse(p0, diam[0], diam[1]);
            diam[0] = diam[1] = p2;
            diam[0].x += 3;
            diam[1].y += 3;
            sg->Ellipse(p2, diam[0], diam[1]);
            sg->FillPath(FILLRULE_EVENODD);
        }
    }
}

void demo05(SimpleRenderer *rend, const SGRect& clip)
{
    SGPtr sg(rend, clip);
    TextApp txt;
    COLOR crBkgd  = RGBVAL(205,195,185);
    COLOR crFill  = RGBVAL(70,206,209);
    COLOR crFrame = RGBVAL(100,80,90);
    COLOR crText  = RGBVAL( 40,  70, 110);

    // Fill background and draw frame around window
    SGPoint corner = { 40, 40 };
    SGRect frame = { 10, 10, DEMO_WIDTH-20, DEMO_HEIGHT-20 };
    sg->BeginPath();
    sg->RoundedRectangle(frame, corner);
    rend->SetColor(crBkgd);
    sg->FillPath(FILLRULE_EVENODD);
    sg->SetLineWidth(8.0);
    rend->SetColor(crFrame); 
    sg->StrokePath();

    // Draw title text
    sg->SetLineWidth(6.0);
    char *str = "Layered Stroke Effects";
    float scale = 0.73;
    float width = txt.GetTextWidth(scale, str);
    SGPoint xystart;
    xystart.x = (DEMO_WIDTH - width)/2;
    xystart.y = 136;
    DrawTextAA(&(*sg), rend, &txt, xystart, scale, str, 
               crBkgd, crText);

    // Draw layered strokes in different colors
    SGPoint xy[] = {
        { 349, 640 }, { 545, 320 }, { 720, 620 }, { 625, 460 }, 
        { 450, 745 }, { 960, 745 }, { 755, 400 },
    };
    COLOR rgb[] = {
        0, RGBVAL(160,220,250), 0, RGBVAL(160,250,160), 
        0, RGBVAL(250,160,160), 0 
    };
    float dw = 35.8;
    float linewidth = 6.5*dw;

    sg->SetLineWidth(dw);
    sg->SetLineEnd(LINEEND_ROUND);
    sg->SetLineJoin(LINEJOIN_ROUND);
    for (int i = 0; i < 7; ++i)
    {
        sg->BeginPath();
        sg->Move(xy[0].x, xy[0].y);
        sg->PolyLine(2, &xy[1]);
        sg->Move(xy[3].x, xy[3].y);
        sg->PolyLine(3, &xy[4]);
        rend->SetColor(rgb[i]);
        sg->SetLineWidth(linewidth);
        StrokePathAA(&(*sg), rend, crBkgd, rgb[i]);
        crBkgd = rgb[i];
        linewidth -= dw;
    }
}

void demo06(SimpleRenderer *rend, const SGRect& clip)
{
    SGPtr sg(rend, clip);
    TextApp txt;
    COLOR crBkgd = RGBVAL(222,222,255);
    COLOR crFill = RGBVAL(238,130,200);
    COLOR crText = RGBVAL(40,70,110); 
    int i;

    // Fill background and draw frame around window
    SGPoint corner = { 40, 40 };
    SGRect frame = { 10, 10, DEMO_WIDTH-20, DEMO_HEIGHT-20 };
    sg->BeginPath();
    sg->RoundedRectangle(frame, corner);
    rend->SetColor(crBkgd);
    sg->FillPath(FILLRULE_EVENODD);
    sg->SetLineWidth(8.0);
    //sg->SetRenderer(&renderer);
    rend->SetColor(RGBVAL(80, 40, 140)); 
    sg->StrokePath();

    // Draw title text
    sg->SetLineWidth(6.0);
    char *str = "Miter Limit";
    float scale = 0.73;
    float width = txt.GetTextWidth(scale, str);
    SGPoint xystart;
    xystart.x = 75;
    xystart.y = 150;
    DrawTextAA(&(*sg), rend, &txt, xystart, scale, str, 
               crBkgd, crText);

    // Initialize parameters for circular miter-limit display
    SGPoint v[] = {
        { 4, 160 }, { 0, 240 }, { -4, 160 },
    };
    COLOR color[] = {
        RGBVAL(30, 144, 255), RGBVAL(221, 160, 221), RGBVAL(255, 69, 0), 
        RGBVAL(147, 112, 219), RGBVAL(50, 205, 50), RGBVAL(250, 235, 215), 
    };
    float mlim[] = { 1.0, 7.0, 13.0, 19.0 };
    SGPoint circle[3] = { { 720, 480 } };
    float linewidth = 20.0;

    // Draw thin black rings in background
    rend->SetColor(0);
    sg->SetLineWidth(0);
    sg->BeginPath();
    for (int i = 0; i < 3; ++i)
    {
        int radius = 240 + mlim[i]*linewidth/2;
        circle[1] = circle[2] = circle[0];
        circle[1].x += radius;
        circle[2].y += radius;
        sg->Ellipse(circle[0], circle[1], circle[2]);
    }
    sg->StrokePath();
    
    // Draw strokes with variety of miter limits
    sg->SetLineWidth(linewidth);
    sg->SetLineEnd(LINEEND_FLAT);
    sg->SetLineJoin(LINEJOIN_MITER);
    for (int i = 0; i < 4; ++i)
    {
        rend->SetColor(color[i]);
        sg->BeginPath();
        for (int j = 0; j < 8; ++j)
        {
            float cosa = cos(2*i*PI/32.0 + 2*j*PI/8.0);
            float sina = sin(2*i*PI/32.0 + 2*j*PI/8.0);
            int x[3], y[3];

            for (int k = 0; k < 3; ++k)
            {
                x[k] = circle[0].x + cosa*v[k].x - sina*v[k].y;
                y[k] = circle[0].y + sina*v[k].x + cosa*v[k].y;
            }
            sg->Move(x[0], y[0]);
            sg->Line(x[1], y[1]);
            sg->Line(x[2], y[2]);
        }
        sg->SetMiterLimit(mlim[i]);
        sg->StrokePath();
    }
    
    // Draw thin white lines over strokes
    sg->SetLineWidth(0);
    rend->SetColor(RGBVAL(255,255,255));
    sg->BeginPath();
    for (int i = 0; i < 4; ++i)
    {
        
        for (int j = 0; j < 8; ++j)
        {
            float cosa = cos(2*i*PI/32.0 + 2*j*PI/8.0);
            float sina = sin(2*i*PI/32.0 + 2*j*PI/8.0);
            int x[3], y[3];

            for (int k = 0; k < 3; ++k)
            {
                x[k] = circle[0].x + cosa*v[k].x - sina*v[k].y;
                y[k] = circle[0].y + sina*v[k].x + cosa*v[k].y;
            }
            sg->Move(x[0], y[0]);
            sg->Line(x[1], y[1]);
            sg->Line(x[2], y[2]);
        }
    }
    sg->StrokePath();

    // Draw text inside circle
    sg->SetLineWidth(4.0);
    char *msg[] = { "Get the", "point?" };
    scale = 0.5;
    xystart.y = circle[0].y - 16;
    for (int i = 0; i < ARRAY_LEN(msg); ++i)
    {
        width = txt.GetTextWidth(scale, msg[i]);
        xystart.x = circle[0].x - width/2.0;
        DrawTextAA(&(*sg), rend, &txt, xystart, scale, msg[i], 
                   crBkgd, crText);
        xystart.y += 50;
    }
}

void demo07(SimpleRenderer *rend, const SGRect& clip)
{
    SGPtr sg(rend, clip);
    TextApp txt;
    COLOR crBkgd = RGBVAL(222,222,255);
    COLOR crFill = RGBVAL(238, 130, 200);
    COLOR crText = RGBVAL(40, 70, 110); 
    int i;

    // Fill background and draw frame around window
    SGPoint corner = { 40, 40 };
    SGRect frame = { 10, 10, DEMO_WIDTH-20, DEMO_HEIGHT-20 };
    sg->BeginPath();
    sg->RoundedRectangle(frame, corner);
    rend->SetColor(crBkgd);
    sg->FillPath(FILLRULE_EVENODD);
    sg->SetLineWidth(8.0);
    rend->SetColor(RGBVAL(90, 160, 50)); 
    sg->StrokePath();

    // Draw title text
    sg->SetLineWidth(6.0);
    char *str = "Dashed Line Patterns";
    float scale = 0.73;
    float width = txt.GetTextWidth(scale, str);
    SGPoint xystart;
    xystart.x = (DEMO_WIDTH - width)/2;
    xystart.y = 130;
    DrawTextAA(&(*sg), rend, &txt, xystart, scale, str, 
               crBkgd, crText);

    // Draw windmill shape
    SGPoint blade[][7] = {
        {
            { 160-100, 120+95 },
            { 250-100,  30+95 },
            { 320-100, 100+95 },
            { 320-100, 240+95 },
            { 320-100, 380+95 },
            { 370-100, 450+95 },
            { 480-100, 360+95 },
        },                  
        {                   
            { 440-100,  80+95 },
            { 530-100, 170+95 },
            { 460-100, 240+95 },
            { 320-100, 240+95 },
            { 180-100, 240+95 },
            { 110-100, 310+95 },
            { 200-100, 400+95 },
        },
    };
    sg->SetLineEnd(LINEEND_SQUARE);
    sg->SetLineJoin(LINEJOIN_BEVEL);
    sg->BeginPath();
    sg->Move(blade[0][0].x, blade[0][0].y);
    sg->PolyBezier3(ARRAY_LEN(blade[0])-1, &blade[0][1]);
    sg->CloseFigure();
    sg->Move(blade[1][0].x, blade[1][0].y);
    sg->PolyBezier3(ARRAY_LEN(blade[1])-1, &blade[1][1]);
    rend->SetColor(RGBVAL(144, 238, 144));
    sg->CloseFigure();
    sg->FillPath(FILLRULE_EVENODD);
    rend->SetColor(RGBVAL(0, 100, 0));
    sg->SetLineWidth(8.0);
    sg->SetLineDash(dashedLineShortDash, 0, 4.0);
    sg->StrokePath();
    rend->SetColor(RGBVAL(100, 180, 50));
    sg->SetLineEnd(LINEEND_FLAT);
    sg->StrokePath();

    // Draw spikey shape
    SGPoint stretch[25] = { { 1040+200, 720+0 } };
    float angle = 0, cosa = 1.0, sina = 0;
    for (i = 1; i < ARRAY_LEN(stretch); i += 3)
    {
        stretch[i].x = 1040 + 40.0*cosa;
        stretch[i].y =  720 + 40.0*sina;
        angle += PI/4.0;
        cosa = cos(angle);
        sina = sin(angle);
        stretch[i+1].x = 1040 +  40.0*cosa;
        stretch[i+1].y =  720 +  40.0*sina;
        stretch[i+2].x = 1040 + 200.0*cosa; 
        stretch[i+2].y =  720 + 200.0*sina; 
    }
    sg->SetLineEnd(LINEEND_FLAT);
    sg->SetLineJoin(LINEJOIN_BEVEL);
    sg->BeginPath();
    sg->Move(stretch[0].x, stretch[0].y);
    sg->PolyBezier3(ARRAY_LEN(stretch)-1, &stretch[1]);
    sg->CloseFigure();
    rend->SetColor(RGBVAL(144, 238, 144));
    sg->FillPath(FILLRULE_EVENODD);
    rend->SetColor(RGBVAL(0, 100, 0));
    sg->SetLineWidth(4.0);
    sg->SetLineDash(dashedLineDot, 0, 3.0);
    sg->StrokePath();

    // Draw clubs suit symbol
    char dashedLineBeaded[] = { 1, 12, 0 };
    SGPoint clubs[] = {
        { 220+290, 400+145 },    // P0
        { 260+290, 400+145 },    // P1
        { 300+290, 320+145 },    // P2
        { 300+290, 260+145 },    // P3
        { 100+290, 460+145 },    // P4
        { 100+290,  20+145 },    // P5
        { 300+290, 220+145 },    // P6
        { 100+290,  20+145 },    // P7
        { 540+290,  20+145 },    // P8 
        { 340+290, 220+145 },    // P9 
        { 540+290,  20+145 },    // P10
        { 540+290, 460+145 },    // P11
        { 340+290, 260+145 },    // P12
        { 340+290, 320+145 },    // P13
        { 380+290, 400+145 },    // P14
        { 420+290, 400+145 },    // P15
    };
    sg->SetLineEnd(LINEEND_ROUND);
    sg->SetLineJoin(LINEJOIN_BEVEL);
    rend->SetColor(RGBVAL(120, 210, 100));
    sg->BeginPath();
    sg->Move(clubs[0].x, clubs[0].y);
    sg->PolyBezier3(15, &clubs[1]);
    sg->CloseFigure();
    sg->FillPath(FILLRULE_EVENODD);
    sg->SetLineWidth(8.0);
    sg->SetLineDash(dashedLineBeaded, 0, 1.0);
    rend->SetColor(RGBVAL(0, 100, 0));
    sg->StrokePath();

    // Draw circle
    char dashedLineBump[] = { 3, 2, 1, 2, 0 };
    SGPoint elips[3] = {
        { 1030, 340 }, { 1030+160, 340 }, { 1030, 340+120 }, 
    };
    COLOR crElips = RGBVAL(0,139,120);
    sg->SetLineEnd(LINEEND_FLAT);
    sg->SetLineJoin(LINEJOIN_BEVEL);
    sg->BeginPath();
    sg->Ellipse(elips[0], elips[1], elips[2]);
    rend->SetColor(crElips);
    sg->SetLineWidth(60.0);
    sg->SetLineDash(dashedLineBump, 3, 2.9 );
    sg->StrokePath();

    // Draw curly-cue shape
    int xc = 210, yc = 740;
    int x = 24, y = 0;
    SGPoint xy[100];
    SGPoint *pxy = &xy[0];     
    float alpha = 1.16;

    pxy[0].x = x + xc;
    pxy[0].y = y + yc;
    for (int i = 0; i < 2; ++i)
    {
        for (int j = 1; j <= 32; j += 4)
        {
            pxy[j].x = x + xc;
            pxy[j].y = (y = alpha*x) + yc;
            pxy[j+1].x = (x = 0) + xc;
            pxy[j+1].y = y + yc;
            pxy[j+2].x = (x = -alpha*y) + xc;
            pxy[j+2].y = y + yc;
            pxy[j+3].x = x + xc;
            pxy[j+3].y = (y = 0) + yc;
        }
        xc += 2*x;
        x = -x;
        alpha = -1.0/alpha;
        pxy = &xy[32];
    }
    sg->BeginPath();
    sg->Move(xy[0].x, xy[0].y);
    sg->PolyEllipticSpline(64, &xy[1]);
    rend->SetColor(RGBVAL(0, 100, 0));
    sg->SetLineWidth(16.0);
    sg->SetLineDash(dashedLineDashDoubleDot, 0, 4.0);
    sg->StrokePath();
    rend->SetColor(RGBVAL(80, 200, 70));
    sg->SetLineWidth(10.0);
    sg->StrokePath();
}

void demo08(SimpleRenderer *rend, const SGRect& clip)
{
    SGPtr sg(rend, clip);
    TextApp txt;
    COLOR crBkgd = RGBVAL(242,236,255);
    COLOR crFill = RGBVAL(238,130,200);
    COLOR crText = RGBVAL(40,70,110);

    // Fill background and draw frame around window
    SGPoint corner = { 40, 40 };
    SGRect frame = { 10, 10, DEMO_WIDTH-20, DEMO_HEIGHT-20 };
    sg->BeginPath();
    sg->RoundedRectangle(frame, corner);
    rend->SetColor(crBkgd);
    sg->FillPath(FILLRULE_EVENODD);
    sg->SetLineWidth(8.0);
    rend->SetColor(RGBVAL(132,24,10)); 
    sg->StrokePath();

    // Draw title text
    sg->SetLineWidth(6.0);
    char *str = "Thin Stroked Lines";
    float scale = 0.73;
    float width = txt.GetTextWidth(scale, str);
    SGPoint xystart;
    xystart.x = (DEMO_WIDTH - width)/2;
    xystart.y = 130;
    DrawTextAA(&(*sg), rend, &txt, xystart, scale, str, 
               crBkgd, crText);

    // Draw text at lower left
    scale = 0.48;
    sg->SetLineWidth(4.0);
    str = "Mimic";
    width = txt.GetTextWidth(scale, str);
    xystart.x = 340 - width/2;
    xystart.y = 812;
    DrawTextAA(&(*sg), rend, &txt, xystart, scale, str, 
               crBkgd, crText);
    str = "Bresenham lines";
    width = txt.GetTextWidth(scale, str);
    xystart.x = 340 - width/2;
    xystart.y = 868;
    DrawTextAA(&(*sg), rend, &txt, xystart, scale, str, 
               crBkgd, crText);

    // Draw text at lower right
    str = "Precisely track";
    width = txt.GetTextWidth(scale, str);
    xystart.x = 940 - width/2;
    xystart.y = 812;
    DrawTextAA(&(*sg), rend, &txt, xystart, scale, str, 
               crBkgd, crText);
    str = "contours";
    width = txt.GetTextWidth(scale, str);
    xystart.x = 940 - width/2;
    xystart.y = 868;
    DrawTextAA(&(*sg), rend, &txt, xystart, scale, str, 
               crBkgd, crText);

    // Draw many-pointed star on left side of window
    SGPoint point[33];
    int j = ARRAY_LEN(point);
    int k = j/2;
    for (int i = 0; i < ARRAY_LEN(point); i++)
    {
        float t = k*i*2*PI/j;
        point[i].x = 340 - 280*sin(t);
        point[i].y = 460 - 280*cos(t);
    }
    rend->SetColor(RGBVAL(0, 0, 80));
    sg->BeginPath();
    sg->Move(point[0].x, point[0].y);
    sg->PolyLine(ARRAY_LEN(point)-1, &point[1]);
    sg->SetLineWidth(0);
    sg->CloseFigure();
    sg->StrokePath();

    // Fill rectangle on right side of window
    SGRect rect = { 670, 190, 540, 540 };
    sg->BeginPath();
    sg->Rectangle(rect);
    rend->SetColor(RGBVAL(150,50,20));
    sg->FillPath(FILLRULE_EVENODD);
    sg->SetClipRegion(FILLRULE_EVENODD);

    // Draw concentric ellipses in rectangle on right
    SGPoint p1, p2, p0 = { 940<<16, 460<<16 };
    p1 = p2 = p0;
    sg->SetFixedBits(16);
    rend->SetColor(0);
    sg->SetLineWidth(0);
    sg->BeginPath();
    for (int i = 0; i < 28; ++i)
    {
        p1.x += 2458*(940 -  680);
        p1.y += 2458*(460 -  200);
        p2.x += 2458*(940 - 1070);
        p2.y += 2458*(460 -  350);
        sg->Ellipse(p0, p1, p2);
    }
    rend->SetColor(RGBVAL(30,100,180));
    sg->FillPath(FILLRULE_EVENODD);
    rend->SetColor(RGBVAL(255,255,255));
    sg->StrokePath();
}

void demo09(SimpleRenderer *rend, const SGRect& clip)  // Bezier 'S'
{
    SGPtr sg(rend, clip);
    TextApp txt;
    COLOR crBkgd = RGBVAL(222,222,255);
    COLOR crFill = RGBVAL(238, 130, 200);
    COLOR crText = RGBVAL(40, 70, 110); 
    int i;

    // Fill background and draw frame around window
    SGPoint corner = { 40, 40 };
    SGRect frame = { 10, 10, DEMO_WIDTH-20, DEMO_HEIGHT-20 };
    sg->BeginPath();
    sg->RoundedRectangle(frame, corner);
    rend->SetColor(crBkgd);
    sg->FillPath(FILLRULE_EVENODD);
    sg->SetLineWidth(8.0);
    rend->SetColor(RGBVAL(200, 100, 15)); 
    sg->StrokePath();

    // Draw title text
    sg->SetLineWidth(6.0);
    char *str = "Bezier Curves";
    float scale = 0.73;
    float len = txt.GetTextWidth(scale, str);
    SGPoint xystart;
    xystart.x = (DEMO_WIDTH - len)/2;
    xystart.y = 130;
    DrawTextAA(&(*sg), rend, &txt, xystart, scale, str, 
               crBkgd, crText);

    // Define Bezier control polygon for "S" glyph
    SGPoint glyph[] = {
        {   0, 165 },  //  0
        {  52, 100 },  //  1
        { 123,  67 },  //  2
        { 175,  67 },  //  3
        { 248,  67 },  //  4
        { 288, 115 },  //  5
        { 288, 152 },  //  6
        { 288, 290 },  //  7
        {   0, 305 },  //  8
        {   0, 490 },  //  9

        {   0, 598 },  // 10
        {  80, 667 },  // 11
        { 201, 667 },  // 12
        { 253, 667 },  // 13
        { 305, 656 },  // 14
        { 351, 620 },  // 15
        { 351, 584 },  // 16
        { 351, 548 },  // 17
        { 351, 512 },  // 18
        { 297, 567 },  // 19

        { 240, 582 },  // 20
        { 193, 582 },  // 21
        { 135, 582 },  // 22
        {  88, 551 },  // 23
        {  88, 501 },  // 24
        {  88, 372 },  // 25
        { 377, 375 },  // 26
        { 377, 161 },  // 27
        { 377,  56 },  // 28
        { 296, -20 },  // 29

        { 190, -20 },  // 30
        { 116, -20 },  // 31
        {  59,   0 },  // 32
        {   0,  42 },  // 33
        {   0,  83 },  // 34
        {   0, 124 },  // 35
        {   0, 165 },  // 36
    };
    SGRect bbsrc, bbdst = { 768<<16, 875<<16, 384<<16, -660<<16 };
    float width = 54.0;

    // Draw "S" glyph
    IntToFixed(ARRAY_LEN(glyph), glyph);
    GetBbox(&bbsrc, ARRAY_LEN(glyph), glyph);
    FitBbox(&bbdst, ARRAY_LEN(glyph), glyph, &bbsrc, glyph); 

    sg->SetFixedBits(16);
    rend->SetColor(RGBVAL(255, 120, 30));
    sg->SetLineJoin(LINEJOIN_MITER);
    sg->SetMiterLimit(1.10);
    sg->SetLineWidth(width);
    sg->BeginPath(); 
    sg->Move(glyph[0].x, glyph[0].y);
    sg->PolyBezier3(ARRAY_LEN(glyph)-1, &glyph[1]);
    sg->CloseFigure();
    sg->StrokePath();

    // Add details to largest "S" glyph
    sg->SetLineWidth(width - 6);
    rend->SetColor(RGBVAL(211, 233, 211));
    sg->StrokePath();

    sg->SetLineWidth(0);
    rend->SetColor(RGBVAL(200, 0, 0));
    sg->StrokePath();

    // Outline control polygon
    rend->SetColor(RGBVAL(85, 107, 47));
    sg->SetLineWidth(2.0);
    sg->BeginPath();
    sg->Move(glyph[0].x, glyph[0].y);
    for (int k = 1; k < ARRAY_LEN(glyph)-1; k += 3)
    { 
        sg->Line(glyph[k].x, glyph[k].y);
        sg->Move(glyph[k+1].x, glyph[k+1].y);
        sg->Line(glyph[k+2].x, glyph[k+2].y);
    }
    sg->StrokePath();

    // Highlight vertexes of control polygon
    sg->BeginPath();
    for (int k = 0; k < ARRAY_LEN(glyph)-1; k++)
    {
        SGPoint pt[2];
        pt[0] = pt[1] = glyph[k];
        pt[0].x += 4 << 16;
        pt[1].y += 4 << 16;
        sg->Ellipse(glyph[k], pt[0], pt[1]);
    }
    sg->StrokePath();

    // Move and resize points in clubs array
    SGPoint clubs[] = {       
        { 220, 400 }, { 260, 400 }, { 300, 320 }, { 300, 260 },
        { 100, 460 }, { 100,  20 }, { 300, 220 }, { 100,  20 },
        { 540,  20 }, { 340, 220 }, { 540,  20 }, { 540, 460 },
        { 340, 260 }, { 340, 320 }, { 380, 400 }, { 420, 400 },
    };
    SGPoint point[ARRAY_LEN(clubs)];
    COLOR crTrim = RGBVAL(0, 160, 0);
    SGRect bboxsrc, bboxdst = { 70<<16, 240<<16, 600<<16, 600<<16 };

    IntToFixed(ARRAY_LEN(clubs), &clubs[0]);
    GetBbox(&bboxsrc, ARRAY_LEN(clubs), &clubs[0]);
    FitBbox(&bboxdst, ARRAY_LEN(clubs), &point[0], 
            &bboxsrc, &clubs[0]);

    // Draw club suit symbol
    crFill = RGBVAL(100, 200, 47);
    sg->SetLineDash(0, 0, 0);
    sg->SetLineEnd(LINEEND_SQUARE);
    sg->SetLineJoin(LINEJOIN_BEVEL);
    sg->SetFixedBits(16);
    sg->BeginPath();
    sg->Move(point[0].x, point[0].y);
    sg->PolyBezier3(15, &point[1]);
    sg->CloseFigure();
    sg->SetLineWidth(6.0);
    rend->SetColor(crFill);
    sg->FillPath(FILLRULE_EVENODD);
    rend->SetColor(crTrim);
    sg->StrokePath();

    // Outline control polygon
    sg->BeginPath();
    sg->Move(point[0].x, point[0].y);
    sg->PolyLine(ARRAY_LEN(point)-1, &point[1]);
    rend->SetColor(RGBVAL(255, 0, 0));
    sg->SetLineWidth(0);
    sg->StrokePath();

    // Highlight vertexes of control polygon
    sg->BeginPath();
    for (int i = 0; i < ARRAY_LEN(point); ++i)
    {
        SGRect rect;
        rect.x = point[i].x - (2 << 16);
        rect.y = point[i].y - (2 << 16);
        rect.w = 4 << 16;
        rect.h = 4 << 16;
        sg->Rectangle(rect);
    }
    rend->SetColor(RGBVAL(200, 0, 0));
    sg->StrokePath();
}

void demo10(SimpleRenderer *rend, const SGRect& clip)  // glyph '@'
{
    SGPtr sg(rend, clip);
    TextApp txt;
    COLOR crBkgd = RGBVAL(220,220,220);
    COLOR crFill = RGBVAL(238, 130, 200);
    COLOR crText = RGBVAL(40, 70, 110);

    // Fill background and draw frame around window
    SGPoint corner = { 40, 40 };
    SGRect frame = { 10, 10, DEMO_WIDTH-20, DEMO_HEIGHT-20 };
    sg->BeginPath();
    sg->RoundedRectangle(frame, corner);
    rend->SetColor(crBkgd);
    sg->FillPath(FILLRULE_EVENODD);
    sg->SetLineWidth(8.0);
    rend->SetColor(RGBVAL(200, 100, 15)); 
    sg->StrokePath();

    // Draw title text
    sg->SetLineWidth(6.0);
    char *str = "Ellipses and Elliptic Splines";
    float scale = 0.73;
    float len = txt.GetTextWidth(scale, str);
    SGPoint xystart;
    xystart.x = (DEMO_WIDTH - len)/2;
    xystart.y = 130;
    DrawTextAA(&(*sg), rend, &txt, xystart, scale, str, 
               crBkgd, crText);

    // glyph '@'
    struct COORD
    {
        float x; float y;
    };
    COORD glyph[] = {
        {  1.5,  1.0 },  //  0 bounding box min
        { 39.2, 40.0 },  //  1 bounding box max
        { 19.4, 20.5 },  //  2
        { 22.3, 30.0 },  //  3
        { 26.5, 18.4 },  //  4
        { 27.6, 29.7 },  //  5
        { 25.0, 11.4 },  //  6
        { 32.0,  9.8 },  //  7
        { 39.2,  8.0 },  //  8
        { 39.2, 22.0 },  //  9
        { 39.2, 40.0 },  // 10
        { 20.8, 40.0 },  // 11
        {  1.5, 40.0 },  // 12
        {  1.5, 19.0 },  // 13
        {  1.5,  1.0 },  // 14
        { 18.0,  1.0 },  // 15
        { 23.5,  1.0 },  // 16
        { 26.6,  2.5 },  // 17
    };
    SGPoint xy[ARRAY_LEN(glyph)];
    int xoffset = 12;
    int width = 0;

    scale = 0.60;
    sg->SetFixedBits(16);
    rend->SetColor(RGBVAL(255, 120, 30));
    sg->SetLineEnd(LINEEND_ROUND);
    sg->SetLineJoin(LINEJOIN_ROUND);
    for (int i = 0; i < 5; ++i)
    {
        for (int j = 0; j < ARRAY_LEN(xy); ++j)
        {
            xy[j].x = 65536*(66 + xoffset + scale*glyph[j].x);
            xy[j].x += 0x00001000;
            xy[j].x &= 0xffff0000;
            xy[j].y = 65536*(828 - scale*glyph[j].y);
            xy[j].y += 0x00001000;
            xy[j].y &= 0xffff0000;
        }
        sg->SetLineWidth(float((width + 1) & ~1));
        sg->BeginPath();
        sg->Ellipse(xy[2], xy[3], xy[4]); 
        sg->Move(xy[5].x, xy[5].y);
        sg->PolyEllipticSpline(12, &xy[6]);
        sg->StrokePath();
        xoffset += xoffset + 24;
        scale += 1.2*scale;
        width += width + 3;
    }

    sg->SetLineWidth(width/2 - 6);
    rend->SetColor(RGBVAL(200, 210, 220));
    sg->StrokePath();

    sg->SetLineWidth(0);
    rend->SetColor(RGBVAL(200, 0, 0));
    sg->StrokePath();

    // Outline control polygon
    rend->SetColor(0);
    sg->SetLineWidth(0);
    sg->BeginPath();
    sg->Move(xy[3].x, xy[3].y);
    sg->Line(xy[2].x, xy[2].y);
    sg->Line(xy[4].x, xy[4].y);
    sg->Move(xy[5].x, xy[5].y);
    sg->PolyLine(12, &xy[6]);
    sg->StrokePath();

    SGPoint pt[] = { { 2<<16, 0}, { 0, 2<<16},};

    // Highlight vertexes of control polygon
    sg->BeginPath();
    for (int k = 2; k < ARRAY_LEN(xy); k++)
    {
        pt[0] = pt[1] = xy[k];
        pt[0].x += 3 << 16;
        pt[1].y += 3 << 16;
        sg->Ellipse(xy[k], pt[0], pt[1]);
    }
    sg->FillPath(FILLRULE_EVENODD);

    // Draw atomic symbol
    SGPoint p0 = { 250, 382 };
    SGPoint p1 = { 250, 382+12 };
    SGPoint p2 = { 250+12, 382 };
    char electron[] = { 1, 100, 0 };
    float astart = PI/4;
    sg->SetFixedBits(0);
    sg->BeginPath();
    sg->Ellipse(p0, p1, p2);
    rend->SetColor(RGBVAL(0, 80, 120));
    sg->FillPath(FILLRULE_EVENODD);
    sg->SetLineEnd(LINEEND_ROUND);
    for (int i = 0; i < 3; ++i)
    {
        float angle = i*PI/3;
        float sina = sin(i*PI/3);
        float cosa = cos(i*PI/3);
        sg->BeginPath();
        sg->SetLineWidth(4.0);
        sg->SetLineDash(0,0,0);
        p1 = p2 = p0;
        p1.x -= 160*sina;
        p1.y += 160*cosa;
        p2.x += 40*cosa;
        p2.y += 40*sina;
        sg->Ellipse(p0, p1, p2);
        sg->StrokePath();

        sg->BeginPath();
        sg->EllipticArc(p0, p1, p2, astart, PI/16);
        astart += PI;
        sg->SetLineWidth(12.0);
        sg->SetLineDash(electron, 0, 1.0);
        sg->StrokePath();
    }
}

void demo11(SimpleRenderer *rend, const SGRect& clip)
{
    SGPtr sg(rend, clip);
    TextApp txt;
    LINEJOIN join[] = { LINEJOIN_BEVEL, LINEJOIN_ROUND, LINEJOIN_MITER };
    LINEEND cap[] = { LINEEND_FLAT, LINEEND_ROUND, LINEEND_SQUARE };
    COLOR crBkgd = RGBVAL(222,222,255);
    COLOR crFill = RGBVAL(238, 130, 200);
    COLOR crText = RGBVAL(40, 70, 110);

    // Fill background and draw frame around window
    SGPoint corner = { 40, 40 };
    SGRect frame = { 10, 10, DEMO_WIDTH-20, DEMO_HEIGHT-20 };
    sg->BeginPath();
    sg->RoundedRectangle(frame, corner);
    rend->SetColor(crBkgd);
    sg->FillPath(FILLRULE_EVENODD);
    sg->SetLineWidth(8.0);
    rend->SetColor(RGBVAL(222, 100, 50)); 
    sg->StrokePath();

    // Draw title text
    sg->SetLineWidth(6.0);
    char *str = "Elliptic Arcs";
    float scale = 0.73;
    float width = txt.GetTextWidth(scale, str);
    SGPoint xystart;
    xystart.x = (DEMO_WIDTH - width)/2;
    xystart.y = 170;
    DrawTextAA(&(*sg), rend, &txt, xystart, scale, str, 
               crBkgd, crText);

    // Draw pie chart
    float percent[] = 
    {
        5.1, 12.5, 14.8, 
        5.2, 11.6, 8.7, 
        14.7, 19.2, 8.2,
    };
    COLOR crSlice[] = 
    {
        RGBVAL( 65, 105, 225),  RGBVAL(255, 99, 71),   RGBVAL(218, 165, 32),     
        RGBVAL( 60, 179, 113),  RGBVAL(199, 21, 133),  RGBVAL( 72, 209, 204),
        RGBVAL(255, 105, 180),  RGBVAL( 50, 205, 50),  RGBVAL(128,   0,   0),
    };
    COLOR crRim[ARRAY_LEN(crSlice)];
    COLOR crEdge = RGBVAL(51, 51, 51);
    const int high = 95;
    const int wide = 420;
    const int deep = 196;
    SGPoint v[] = {
        { 560,      500           }, 
        { 560,      500-deep      }, 
        { 560+wide, 500           },
        { 560,      500+high      }, 
        { 560,      500+high-deep }, 
        { 560+wide, 500+high      },
    };
    FIX16 alpha = 0.3*0x00010000;
    float startAngle = 0;

    for (int i = 0; i < ARRAY_LEN(crSlice); ++i)
    {
        crRim[i] = BlendPixels(alpha, 0, crSlice[i]);
    }
    sg->SetLineWidth(3.0);
    for (int i = 0; i < ARRAY_LEN(percent); ++i)
    {
        SGPoint xy;
        float sweepAngle = 2*PI*percent[i]/100.0;

        if (i == 2)
        {
            // Pull one slice out of the pie
            for (int j = 0; j < ARRAY_LEN(v); ++j)
            {
                v[j].x += 200;
            }
        }

        // Paint the top of the pie slice
        sg->BeginPath();
        sg->EllipticArc(v[0], v[1], v[2], startAngle, sweepAngle);
        sg->GetCurrentPoint(&xy);
        sg->Line(v[0].x, v[0].y);
        sg->CloseFigure();
        rend->SetColor(crSlice[i]);
        sg->FillPath(FILLRULE_EVENODD);
        rend->SetColor(crEdge);
        sg->StrokePath();

        if (i == 1 || i == 2)
        {
            // Paint the filling in the pulled-out pie slice
            COLOR crFilling = BlendPixels(alpha/2, 0, crSlice[i]);
            sg->BeginPath();
            sg->Move(xy.x, xy.y);
            sg->Line(xy.x, xy.y + high);
            sg->Line(v[3].x, v[3].y);
            sg->Line(v[0].x, v[0].y);
            sg->CloseFigure();
            rend->SetColor(crFilling);
            sg->FillPath(FILLRULE_EVENODD);
            rend->SetColor(crEdge);
            sg->StrokePath();
        }

        float delta, astart = startAngle, asweep = sweepAngle;

        // Determine whether the rim of the pie slice is visible
        if (astart < PI/2 && astart + asweep > PI/2)
        {
            // Rim of pie slice on right is partly visible
            asweep = astart + asweep - PI/2;
            astart = PI/2;
        }
        if (astart < 3*PI/2 && astart + asweep > 3*PI/2)
        {
            // Rim of pie slice on left is partly visible
            asweep = 3*PI/2 - astart;
        }
        if (astart >= PI/2 && astart < 3*PI/2)
        {
            // Rim of pie slice is visible, so paint it
            sg->BeginPath();
            sg->EllipticArc(v[0], v[1], v[2], astart+asweep, -asweep);
            sg->GetCurrentPoint(&xy);
            sg->Line(xy.x, xy.y + high);
            sg->EllipticArc(v[3], v[4], v[5], astart, asweep);
            rend->SetColor(crRim[i]);
            sg->CloseFigure();
            sg->FillPath(FILLRULE_EVENODD);
            rend->SetColor(crEdge);
            sg->StrokePath();
        }
        startAngle += sweepAngle;
        if (i == 2)
        { 
            for (int j = 0; j < ARRAY_LEN(v); ++j)
            {
                v[j].x -= 200;
            }
        }
    }
}

void demo12(SimpleRenderer *rend, const SGRect& clip)
{
    SGPtr sg(rend, clip);
    TextApp txt;
    COLOR crBkgd  = RGBVAL(230,230,255);
    COLOR crFrame = RGBVAL(65,105,225);
    COLOR crDash  = RGBVAL(140,190,255);
    COLOR crText  = RGBVAL(65,105,225);
    COLOR crWhite = RGBVAL(255,255,255);
    float linewidth = 10.0;
    char dash[] = { 1, 4, 0 };

    // Fill background and draw dash-pattern frame around window
    SGPoint corner = { 40, 40 };
    SGRect frame = { 150, 230, DEMO_WIDTH-300, DEMO_HEIGHT-380 };
    sg->SetLineEnd(LINEEND_ROUND);
    sg->BeginPath();
    sg->RoundedRectangle(frame, corner);
    rend->SetColor(crBkgd);
    sg->FillPath(FILLRULE_EVENODD);
    sg->SetLineWidth(linewidth);
    rend->SetColor(crFrame); 
    sg->StrokePath();
    sg->SetLineDash(dash, 0, linewidth/4.0);
    sg->SetLineWidth(linewidth - 4.0);
    rend->SetColor(crDash);
    sg->StrokePath();

    // Draw frame around title
    SGRect frame2 = { 320, 165, 640, 130 };
    sg->BeginPath();
    sg->Rectangle(frame2);
    rend->SetColor(crDash);
    sg->FillPath(FILLRULE_EVENODD);
    sg->SetLineJoin(LINEJOIN_ROUND);
    sg->SetLineWidth(14.0);
    rend->SetColor(crFrame);
    sg->SetLineDash(0,0,0);
    sg->StrokePath();
    sg->SetLineWidth(8.0);
    rend->SetColor(crWhite);
    sg->StrokePath();

    // Draw title text
    txt.SetTextSpacing(1.3);
    char *str = "Code Examples";
    float scale = 0.73;
    float width = txt.GetTextWidth(scale, str);
    SGPoint xystart;
    xystart.x = (DEMO_WIDTH - width)/2;
    xystart.y = 245;
    sg->SetLineDash(0,0,0);
    sg->SetLineWidth(12.0);
    DrawTextAA(&(*sg), rend, &txt, xystart, scale, str, 
               crDash, crText);
    sg->SetLineWidth(6.0);
    DrawTextAA(&(*sg), rend, &txt, xystart, scale, str, 
               crText, crWhite);

    // Write text message
    char *s[] = {
        "The graphics output that follows this screen",
        "is produced by the code examples in the",
        "ShapeGen User's Guide.*",
        "",
        "(*See the userdoc.pdf file in the main",
        "directory of this project.)"
    };
    txt.SetTextSpacing(1.0);
    scale = 0.40;
    sg->SetLineWidth(3.0);
    xystart.y = 400;
    crText = BlendPixels(0.4*(1<<16), 0, crText);
    width = txt.GetTextWidth(scale, s[0]);
    xystart.x = (DEMO_WIDTH - width)/2;
    for (int i = 0; i < ARRAY_LEN(s); ++i)
    {
        DrawTextAA(&(*sg), rend, &txt, xystart, scale, s[i], crBkgd, crText);
        xystart.y += 55;
    }
}

void MySub(ShapeGen *sg, SGRect& rect)
{
    sg->BeginPath();
    sg->Rectangle(rect);
    sg->FillPath(FILLRULE_EVENODD);
}

void MyTest(SimpleRenderer *rend, const SGRect& clip)
{
    SGPtr sg(rend, clip);
    SGRect rect = { 100, 80, 250, 160 };
     
    sg->BeginPath();
    sg->Rectangle(clip);
    rend->SetColor(RGBVAL(255,255,255));  // white
    sg->FillPath(FILLRULE_EVENODD);
     
    rend->SetColor(RGBVAL(0,120,255));  // blue
    MySub(&(*sg), rect);

    //-----  Label the output of this code example -----
    TextApp txt;
    COLOR crBkgd = RGBVAL(255,255,255);
    COLOR crText = RGBVAL(40,70,110);
    char *str = "Output of code example from \"Creating a "
                "ShapeGen object\" topic in userdoc.pdf";
    float scale = 0.3;
    SGPoint xystart = { 24, 360 };
    txt.SetTextSpacing(1.1);
    sg->SetLineWidth(3.0);
    DrawTextAA(&(*sg), rend, &txt, xystart, scale, str, 
               crBkgd, crText);
}

void EggRoll(SimpleRenderer *rend, const SGRect& clip)
{
    SGPtr sg(rend, clip);
    SGPoint xy[][4] = {
        { {  20,  80 }, {  20,  20 }, {  80,  20 } },
        { { 110, 100 }, { 110,  40 }, { 210,  40 } },
        { { 240,  80 }, { 280,  20 }, { 340,  60 } },
        { { 400, 160 }, { 360, 100 }, { 480,  20 } },
        { { 540, 100 }, { 580,  20 }, { 620, 100 } },
        { { 660, 120 }, { 640,  40 }, { 760, 100 } },
    };

    sg->SetLineJoin(LINEJOIN_MITER);
    sg->SetLineEnd(LINEEND_ROUND);
    for (int i = 0; i < ARRAY_LEN(xy); ++i)
    {
        SGPoint v0, v1, v2;

        // Use symmetry to calculate the fourth point of the
        // square, rectangle, or parallelogram
        xy[i][3].x = xy[i][0].x - xy[i][1].x + xy[i][2].x;
        xy[i][3].y = xy[i][0].y - xy[i][1].y + xy[i][2].y;
        sg->SetLineWidth(2.0);
        sg->BeginPath();
        sg->Move(xy[i][0].x, xy[i][0].y);
        sg->PolyLine(3, &xy[i][1]);
        sg->CloseFigure();
        rend->SetColor(RGBVAL(100,100,100));
        sg->StrokePath();

        // The center v0 of the ellipse is simply the center of
        // the enclosing square, rectangle, or parallelogram
        v0.x = (xy[i][0].x + xy[i][2].x)/2;
        v0.y = (xy[i][0].y + xy[i][2].y)/2;

        // The conjugate diameter end points are simply the
        // midpoints of two adjacent sides of the enclosing
        // square, rectangle, or parallelogram
        v1.x = (xy[i][0].x + xy[i][1].x)/2;
        v1.y = (xy[i][0].y + xy[i][1].y)/2;
        v2.x = (xy[i][1].x + xy[i][2].x)/2;
        v2.y = (xy[i][1].y + xy[i][2].y)/2;
        sg->BeginPath();
        sg->Ellipse(v0, v1, v2);
        rend->SetColor(RGBVAL(220,250,220));
        sg->FillPath(FILLRULE_EVENODD);
        sg->SetLineWidth(0);
        rend->SetColor(0);
        sg->StrokePath();

        // Draw a pair of red lines connecting the ellipse
        // center with the two conjugate diameter end points
        sg->SetLineWidth(3.0);
        sg->BeginPath();
        sg->Move(v1.x, v1.y);
        sg->Line(v0.x, v0.y);
        sg->Line(v2.x, v2.y);
        rend->SetColor(RGBVAL(255,0,0));
        sg->StrokePath();
    }

    //-----  Label the output of this code example -----
    TextApp txt;
    COLOR crBkgd = RGBVAL(255,255,255);
    COLOR crText = RGBVAL(40,70,110);
    char *str = "First screenshot from \"Ellipses and "
                "elliptic arcs\" topic in userdoc.pdf";
    float scale = 0.3;
    SGPoint xystart = { 24, 360 };
    txt.SetTextSpacing(1.1);
    sg->SetLineWidth(3.0);
    DrawTextAA(&(*sg), rend, &txt, xystart, scale, str, 
               crBkgd, crText);
}

void PieToss(SimpleRenderer *rend, const SGRect& clip)
{
    SGPtr sg(rend, clip);
    const float PI = 3.14159265;
    float percent[] = {
        5.1, 12.5, 14.8, 5.2, 11.6, 8.7, 15.3, 18.7
    };
    COLOR color[] = 
    {
        RGBVAL(240,160,160), RGBVAL(160,240,200), RGBVAL(200,160,240), RGBVAL(200,240,160),
        RGBVAL(240,160,200), RGBVAL(160,240,160), RGBVAL(240,200,160), RGBVAL(160,200,240), 
    };
    // Define three corner points of each square, rectangle,
    // or parallelogram. We'll calculate the fourth point.
    SGPoint xy[][4] = {
        { {  20,  80 }, {  20,  20 }, {  80,  20 } },
        { { 110, 100 }, { 110,  40 }, { 210,  40 } },
        { { 240,  80 }, { 280,  20 }, { 340,  60 } },
        { { 400, 160 }, { 360, 100 }, { 480,  20 } },
        { { 540, 100 }, { 580,  20 }, { 620, 100 } },
        { { 660, 120 }, { 640,  40 }, { 760, 100 } },
    };

    sg->SetLineJoin(LINEJOIN_MITER);
    for (int i = 0; i < ARRAY_LEN(xy); ++i)
    {
        SGPoint v0, v1, v2;
        float astart = 0;

        // Use symmetry to calculate the fourth point of the
        // square, rectangle, or parallelogram
        xy[i][3].x = xy[i][0].x - xy[i][1].x + xy[i][2].x;
        xy[i][3].y = xy[i][0].y - xy[i][1].y + xy[i][2].y;
        sg->BeginPath();
        sg->Move(xy[i][0].x, xy[i][0].y);
        sg->PolyLine(3, &xy[i][1]);
        sg->CloseFigure();
        rend->SetColor(RGBVAL(160,170,180));
        sg->SetLineWidth(2.0);
        sg->StrokePath();

        // The center point v0 of the ellipse is simply the center
        // of the enclosing square, rectangle, or parallelogram
        v0.x = (xy[i][0].x + xy[i][2].x)/2;
        v0.y = (xy[i][0].y + xy[i][2].y)/2;

        // The conjugate diameter end points are simply the
        // midpoints of two adjacent sides of the enclosing
        // square, rectangle, or parallelogram
        v1.x = (xy[i][0].x + xy[i][1].x)/2;
        v1.y = (xy[i][0].y + xy[i][1].y)/2;
        v2.x = (xy[i][1].x + xy[i][2].x)/2;
        v2.y = (xy[i][1].y + xy[i][2].y)/2;

        // Draw the pie chart inside the square, rectangle, or
        // parallelogram
        for (int j = 0; j < 8; ++j)
        {
            float asweep = 2.0*PI*percent[j]/100.0;

            sg->BeginPath();
            sg->EllipticArc(v0, v1, v2, astart, asweep);
            sg->Line(v0.x, v0.y); 
            sg->CloseFigure();
            rend->SetColor(color[j]);
            sg->FillPath(FILLRULE_EVENODD);
            rend->SetColor(0);
            sg->SetLineWidth(0);
            sg->StrokePath();
            astart += asweep;
        }
    }

    //-----  Label the output of this code example -----
    TextApp txt;
    COLOR crBkgd = RGBVAL(255,255,255);
    COLOR crText = RGBVAL(40,70,110);
    char *str = "Second screenshot from \"Ellipses and "
                "elliptic arcs\" topic in userdoc.pdf";
    float scale = 0.3;
    SGPoint xystart = { 24, 360 };
    txt.SetTextSpacing(1.1);
    sg->SetLineWidth(3.0);
    DrawTextAA(&(*sg), rend, &txt, xystart, scale, str, 
               crBkgd, crText);
}

// ShapeGen::Bezier2 example
void example01(SimpleRenderer *rend, const SGRect& clip)
{
    SGPtr sg(rend, clip);

    SGPoint v0 = { 100, 200 }, v1 = { 200, 75 }, v2 = { 230, 270 };

    // Draw quadratic Bezier spline in red
    rend->SetColor(RGBVAL(255,80,80));
    sg->SetLineWidth(2.0);
    sg->BeginPath();
    sg->Move(v0.x, v0.y);
    sg->Bezier2(v1, v2);
    sg->StrokePath();

    // Outline control polygon in black
    rend->SetColor(0);
    sg->SetLineWidth(0);
    sg->BeginPath();
    sg->Move(v0.x, v0.y);
    sg->Line(v1.x, v1.y);
    sg->Line(v2.x, v2.y);
    sg->StrokePath();

    //-----  Label the output of this code example -----
    TextApp txt;
    COLOR crBkgd = RGBVAL(255,255,255);
    COLOR crText = RGBVAL(40,70,110);
    char *str = "Output of code example from ShapeGen::"
                "Bezier2 reference topic in userdoc.pdf";
    SGPoint xystart = { 24, 360 };
    float scale = 0.3;
    txt.SetTextSpacing(1.1);
    sg->SetLineWidth(3.0);
    DrawTextAA(&(*sg), rend, &txt, xystart, scale, str, 
               crBkgd, crText);
}

// ShapeGen::Bezier3 example
void example02(SimpleRenderer *rend, const SGRect& clip)
{
    SGPtr sg(rend, clip);

    SGPoint v0 = { 100, 200 }, v1 = { 150, 30 }, 
            v2 = { 260, 200 }, v3 = { 330, 30 };

    // Draw cubic Bezier spline in red
    rend->SetColor(RGBVAL(255,80,80));
    sg->SetLineWidth(2.0);
    sg->BeginPath();
    sg->Move(v0.x, v0.y);
    sg->Bezier3(v1, v2, v3);
    sg->StrokePath();

    // Outline control polygon in black
    rend->SetColor(0);
    sg->SetLineWidth(0);
    sg->BeginPath();
    sg->Move(v0.x, v0.y);
    sg->Line(v1.x, v1.y);
    sg->Line(v2.x, v2.y);
    sg->Line(v3.x, v3.y);
    sg->StrokePath();

    //-----  Label the output of this code example -----
    TextApp txt;
    COLOR crBkgd = RGBVAL(255,255,255);
    COLOR crText = RGBVAL(40,70,110);
    char *str = "Output of code example from ShapeGen::"
                "Bezier3 reference topic in userdoc.pdf";
    SGPoint xystart = { 24, 360 };
    float scale = 0.3;
    txt.SetTextSpacing(1.1);
    sg->SetLineWidth(3.0);
    DrawTextAA(&(*sg), rend, &txt, xystart, scale, str, 
               crBkgd, crText);
}

// ShapeGen::Ellipse example
void example03(SimpleRenderer *rend, const SGRect& clip)
{
    SGPtr sg(rend, clip);
    SGPoint u[4] = { { 100, 275 }, { 100, 75 }, { 300, 75 }, };
    SGPoint du[3] = { { 244, 34 }, { 270, 74 }, { 308, -38 }, };
    SGPoint v0, v1, v2;

    sg->SetLineWidth(0);
    for (int i = 0; i < 2; ++i)
    {
        u[3].x = u[0].x - u[1].x + u[2].x;
        u[3].y = u[0].y - u[1].y + u[2].y;
        v0.x = (u[0].x + u[2].x)/2;
        v0.y = (u[0].y + u[2].y)/2;
        v1.x = (u[0].x + u[1].x)/2;
        v1.y = (u[0].y + u[1].y)/2;
        v2.x = (u[1].x + u[2].x)/2;
        v2.y = (u[1].y + u[2].y)/2;
        sg->BeginPath();
        sg->Move(u[0].x, u[0].y);
        sg->PolyLine(3, &u[1]);
        sg->CloseFigure();
        sg->Ellipse(v0, v1, v2);
        rend->SetColor(RGBVAL(200,200,240));
        sg->FillPath(FILLRULE_EVENODD);
        sg->Move(v1.x, v1.y);
        sg->Line(v0.x, v0.y);
        sg->Line(v2.x, v2.y);
        rend->SetColor(0);
        sg->StrokePath();
        for (int j = 0; j < 3; ++j)
        {
            u[j].x += du[j].x;
            u[j].y += du[j].y;
        }
    }

    //-----  Label the output of this code example -----
    TextApp txt;
    COLOR crBkgd = RGBVAL(255,255,255);
    COLOR crText = RGBVAL(40,70,110);
    char *str = "Output of code example from ShapeGen::"
                "Ellipse reference topic in userdoc.pdf";
    SGPoint xystart = { 24, 360 };
    float scale = 0.3;
    txt.SetTextSpacing(1.1);
    sg->SetLineWidth(3.0);
    DrawTextAA(&(*sg), rend, &txt, xystart, scale, str, 
               crBkgd, crText);
}

// ShapeGen::EllipticArc example
void example04(SimpleRenderer *rend, const SGRect& clip)
{
    SGPtr sg(rend, clip);

    const float PI = 3.14159265;
    float percent[] = {
        5.1, 12.5, 14.8, 5.2, 11.6, 8.7, 15.3, 18.7
    };
    COLOR color[] = 
    {
        RGBVAL(240,200,200), RGBVAL(200,240,220), RGBVAL(220,200,240), RGBVAL(220,240,200),
        RGBVAL(240,200,220), RGBVAL(200,240,200), RGBVAL(240,220,200), RGBVAL(200,220,240), 
    };
    SGPoint v0[] = { { 200, 175 }, { 476, 173 } };
    SGPoint v1[] = { { 200,  75 }, { 489,  93 } };
    SGPoint v2[] = { { 300, 175 }, { 595, 117 } };

    sg->SetLineWidth(0);
    for (int i = 0; i < 2; ++i)
    {
        float astart = 0;

        for (int j = 0; j < 8; ++j)
        {
            float asweep = 2.0*PI*percent[j]/100.0;

            sg->BeginPath();
            sg->EllipticArc(v0[i], v1[i], v2[i], astart, asweep);
            sg->Line(v0[i].x, v0[i].y);
            rend->SetColor(color[j]);
            sg->CloseFigure();
            sg->FillPath(FILLRULE_EVENODD);
            rend->SetColor(RGBVAL(80, 80, 80));
            sg->StrokePath();
            astart += asweep;
        }
    }

    //-----  Label the output of this code example -----
    TextApp txt;
    COLOR crBkgd = RGBVAL(255,255,255);
    COLOR crText = RGBVAL(40,70,110);
    char *str = "Output of code example from ShapeGen::"
                "EllipticArc reference topic in userdoc.pdf";
    SGPoint xystart = { 24, 360 };
    float scale = 0.3;
    txt.SetTextSpacing(1.1);
    sg->SetLineWidth(3.0);
    DrawTextAA(&(*sg), rend, &txt, xystart, scale, str, 
               crBkgd, crText);
}

// ShapeGen::EllipticSpline example
void example05(SimpleRenderer *rend, const SGRect& clip)
{
    SGPtr sg(rend, clip);

    SGPoint v0 = { 100, 200 }, v1 = { 200, 75 }, v2 = { 230, 270 };

    // Draw elliptic spline in green
    rend->SetColor(RGBVAL(40,220,80));
    sg->SetLineWidth(3.0);
    sg->BeginPath();
    sg->Move(v0.x, v0.y);
    sg->EllipticSpline(v1, v2);
    sg->StrokePath();

    // Outline control polygon in black
    rend->SetColor(0);
    sg->SetLineWidth(0);
    sg->BeginPath();
    sg->Move(v0.x, v0.y);
    sg->Line(v1.x, v1.y);
    sg->Line(v2.x, v2.y);
    sg->StrokePath();

    //-----  Label the output of this code example -----
    TextApp txt;
    COLOR crBkgd = RGBVAL(255,255,255);
    COLOR crText = RGBVAL(40,70,110);
    char *str = "Output of code example from ShapeGen::"
                "EllipticSpline reference topic in userdoc.pdf";
    SGPoint xystart = { 24, 360 };
    float scale = 0.3;
    txt.SetTextSpacing(1.1);
    sg->SetLineWidth(3.0);
    DrawTextAA(&(*sg), rend, &txt, xystart, scale, str, 
               crBkgd, crText);
}

// ShapeGen::GetBoundingBox example
void example06(SimpleRenderer *rend, const SGRect& clip)
{
    SGPtr sg(rend, clip);

    SGPoint xy[] = {
        { 104, 62 }, { 247, 196 }, { 261, 129 },
        { 80, 190 }, { 127, 234 }, { 165, 43 }
    };
    float linewidth = 20.0;
    SGRect bbox;

    // Fill shape in blue 
    sg->BeginPath();
    sg->Move(xy[0].x, xy[0].y);
    sg->PolyLine(5, &xy[1]);
    rend->SetColor(RGBVAL(200,220,240));
    sg->FillPath(FILLRULE_EVENODD);

    // Get bounding box and outline it in black
    sg->GetBoundingBox(&bbox);
    sg->SetLineWidth(1.0);
    sg->SetLineJoin(LINEJOIN_MITER);
    sg->BeginPath();
    sg->Rectangle(bbox);
    rend->SetColor(0);
    sg->StrokePath();

    // Move the shape to the right
    for (int i = 0; i < 6; ++i)
        xy[i].x += 270;

    // Stroke the figure in blue
    sg->SetLineJoin(LINEJOIN_ROUND);
    sg->SetLineWidth(linewidth);
    sg->BeginPath();
    sg->Move(xy[0].x, xy[0].y);
    sg->PolyLine(5, &xy[1]);
    sg->CloseFigure();
    rend->SetColor(RGBVAL(200,220,240));
    sg->StrokePath();

    // Get the bounding box and outline it in black
    sg->GetBoundingBox(&bbox);
    sg->SetLineWidth(1.0);
    sg->SetLineJoin(LINEJOIN_MITER);
    sg->BeginPath();
    sg->Rectangle(bbox);
    rend->SetColor(0);
    sg->StrokePath();

    // Expand each side of the bounding box by half the line width
    bbox.x -= linewidth/2;
    bbox.y -= linewidth/2;
    bbox.w += linewidth;
    bbox.h += linewidth;

    // Outline the modified bounding box in red
    sg->BeginPath();
    sg->Rectangle(bbox);
    rend->SetColor(RGBVAL(255,80,80));
    sg->StrokePath();

    //-----  Label the output of this code example -----
    TextApp txt;
    COLOR crBkgd = RGBVAL(255,255,255);
    COLOR crText = RGBVAL(40,70,110);
    char *str = "Output of code example from ShapeGen::"
                "GetBoundingBox reference topic";
    SGPoint xystart = { 24, 360 };
    float scale = 0.3;
    txt.SetTextSpacing(1.1);
    sg->SetLineWidth(3.0);
    DrawTextAA(&(*sg), rend, &txt, xystart, scale, str, 
               crBkgd, crText);
}

// ShapeGen::PolyBezier2 example
void example07(SimpleRenderer *rend, const SGRect& clip)
{
    SGPtr sg(rend, clip);

    SGPoint xy[] = {
        { 40, 140 }, { 70, 30 }, { 115, 120 }, { 160, 210 }, 
        { 195, 120 }, { 230, 30 }, { 274, 150 }
    };
    SGPoint v0, v1, v2;

    // Stroke three connected quadratic Bezier splines in green
    rend->SetColor(RGBVAL(160,200,160));
    sg->SetLineWidth(6.0);
    sg->BeginPath();
    sg->Move(xy[0].x, xy[0].y);
    sg->PolyBezier2(6, &xy[1]);
    sg->StrokePath();

    // Outline spline skeleton in black; mark knots & control points
    rend->SetColor(0);
    sg->SetLineWidth(0);
    sg->BeginPath();
    sg->Move(xy[0].x, xy[0].y);
    sg->PolyLine(6, &xy[1]);
    for (int i = 0; i < 7; ++i)
    {
        v0 = v1 = v2 = xy[i];
        v1.x -= 3;
        v2.y -= 3;
        sg->Ellipse(v0, v1, v2);
    }
    sg->StrokePath();

    //-----  Label the output of this code example -----
    TextApp txt;
    COLOR crBkgd = RGBVAL(255,255,255);
    COLOR crText = RGBVAL(40,70,110);
    char *str = "Output of code example from ShapeGen::"
                "PolyBezier2 reference topic in userdoc.pdf";
    SGPoint xystart = { 24, 360 };
    float scale = 0.3;
    txt.SetTextSpacing(1.1);
    sg->SetLineWidth(3.0);
    DrawTextAA(&(*sg), rend, &txt, xystart, scale, str, 
               crBkgd, crText);
}

// ShapeGen::PolyBezier3 example
void example08(SimpleRenderer *rend, const SGRect& clip)
{
    SGPtr sg(rend, clip);

    SGPoint xy[] = {
        { 30, 250 }, { 60, 120 }, { 130, 270 }, { 195, 145 }, { 260, 20 },
        { 140, 20 }, { 230, 135 }, { 320, 250 }, { 380, 190 }, { 380, 110 } 
    };
    SGPoint v0, v1, v2;
    int i;

    // Stroke three connected cubic Bezier splines in yellow
    rend->SetColor(RGBVAL(230,200,80));
    sg->SetLineWidth(6.0);
    sg->BeginPath();
    sg->Move(xy[0].x, xy[0].y);
    sg->PolyBezier3(9, &xy[1]);
    sg->StrokePath();

    // Draw spline handles in black
    rend->SetColor(0);
    sg->SetLineWidth(0);
    sg->BeginPath();
    for (i = 0; i < 9; i += 3)
    {
        sg->Move(xy[i].x, xy[i].y);
        sg->Line(xy[i+1].x, xy[i+1].y);
        sg->Move(xy[i+2].x, xy[i+2].y);
        sg->Line(xy[i+3].x, xy[i+3].y);
    }
    for (i = 0; i < 10; ++i)
    {
        v0 = v1 = v2 = xy[i];
        v1.x -= 3;
        v2.y -= 3;
        sg->Ellipse(v0, v1, v2);
    }
    sg->StrokePath();

    //-----  Label the output of this code example -----
    TextApp txt;
    COLOR crBkgd = RGBVAL(255,255,255);
    COLOR crText = RGBVAL(40,70,110);
    char *str = "Output of code example from ShapeGen::"
                "PolyBezier3 reference topic in userdoc.pdf";
    SGPoint xystart = { 24, 360 };
    float scale = 0.3;
    txt.SetTextSpacing(1.1);
    sg->SetLineWidth(3.0);
    DrawTextAA(&(*sg), rend, &txt, xystart, scale, str, 
               crBkgd, crText);
}

// ShapeGen::PolyEllipticSpline example
void example09(SimpleRenderer *rend, const SGRect& clip)
{
    SGPtr sg(rend, clip);

    SGPoint xy[] = {
        { 40, 140 }, { 70, 30 }, { 115, 120 }, { 160, 210 }, 
        { 195, 120 }, { 230, 30 }, { 274, 150 }
    };
    SGPoint v0, v1, v2;

    // Stroke three connected elliptic splines in blue
    rend->SetColor(RGBVAL(180,180,230));
    sg->SetLineWidth(6.0);
    sg->BeginPath();
    sg->Move(xy[0].x, xy[0].y);
    sg->PolyEllipticSpline(6, &xy[1]);
    sg->StrokePath();

    // Outline spline skeleton in black; mark knots & control points
    rend->SetColor(0);
    sg->SetLineWidth(0);
    sg->BeginPath();
    sg->Move(xy[0].x, xy[0].y);
    sg->PolyLine(6, &xy[1]);
    for (int i = 0; i < 7; ++i)
    {
        v0 = v1 = v2 = xy[i];
        v1.x -= 3;
        v2.y -= 3;
        sg->Ellipse(v0, v1, v2);
    }
    sg->StrokePath();

    //-----  Label the output of this code example -----
    TextApp txt;
    COLOR crBkgd = RGBVAL(255,255,255);
    COLOR crText = RGBVAL(40,70,110);
    char *str = "Output of code example from ShapeGen::"
                "PolyEllipticSpline reference topic";
    SGPoint xystart = { 24, 360 };
    float scale = 0.3;
    txt.SetTextSpacing(1.1);
    sg->SetLineWidth(3.0);
    DrawTextAA(&(*sg), rend, &txt, xystart, scale, str, 
               crBkgd, crText);
}

// ShapeGen::Rectangle example
void example10(SimpleRenderer *rend, const SGRect& clip)
{
    SGPtr sg(rend, clip);

    SGRect rect = { 100, 75, 300, 225 };

    sg->BeginPath();
    sg->Rectangle(rect);

    // Make second rectangle smaller than the first
    rect.x += 15;
    rect.y += 15;
    rect.w -= 95;
    rect.h -= 75;

    // Modify the parameters so that construction
    // of the second rectangle proceeds in the
    // CCW direction 
    rect.y += rect.h;
    rect.h = -rect.h;
    sg->Rectangle(rect);
    rend->SetColor(RGBVAL(200,200,240));
    sg->FillPath(FILLRULE_WINDING);  // <-- winding number fill rule
    sg->SetLineWidth(1.0);
    sg->SetLineJoin(LINEJOIN_MITER);
    rend->SetColor(RGBVAL(80,0,0));
    sg->StrokePath();

    //-----  Label the output of this code example -----
    TextApp txt;
    COLOR crBkgd = RGBVAL(255,255,255);
    COLOR crText = RGBVAL(40,70,110);
    char *str = "Output of code example from ShapeGen::"
                "Rectangle reference topic in userdoc.pdf";
    SGPoint xystart = { 24, 360 };
    float scale = 0.3;
    txt.SetTextSpacing(1.1);
    sg->SetLineWidth(3.0);
    DrawTextAA(&(*sg), rend, &txt, xystart, scale, str, 
               crBkgd, crText);
}

// ShapeGen::RoundedRectangle example
void example11(SimpleRenderer *rend, const SGRect& clip)
{
    SGPtr sg(rend, clip);

    SGRect rect = { 100, 75, 300, 225 };
    SGPoint round = { 32, 24 };
    sg->BeginPath();
    sg->RoundedRectangle(rect, round);

    // Make second rectangle smaller than the first
    rect.x += 15;
    rect.y += 15;
    rect.w -= 95;
    rect.h -= 75;

    // Modify the parameters so that construction
    // of the second rounded rectangle proceeds
    // in the CCW direction
    rect.y += rect.h;
    rect.h = -rect.h; 
    round.y = -round.y;
    sg->RoundedRectangle(rect, round);
    rend->SetColor(RGBVAL(200,200,240));
    sg->FillPath(FILLRULE_WINDING);  // <-- winding number fill rule
    sg->SetLineWidth(0);
    rend->SetColor(RGBVAL(80,0,0));
    sg->StrokePath();

    //-----  Label the output of this code example -----
    TextApp txt;
    COLOR crBkgd = RGBVAL(255,255,255);
    COLOR crText = RGBVAL(40,70,110);
    char *str = "Output of code example from ShapeGen::"
                "RoundedRectangle reference topic";
    SGPoint xystart = { 24, 360 };
    float scale = 0.3;
    txt.SetTextSpacing(1.1);
    sg->SetLineWidth(3.0);
    DrawTextAA(&(*sg), rend, &txt, xystart, scale, str, 
               crBkgd, crText);
}

// ShapeGen::SetClipRegion example
void example12(SimpleRenderer *rend, const SGRect& clip)
{
    SGPtr sg(rend, clip);

    const float PI = 3.14159265;
    const float t = 0.8*PI;
    const float sint = sin(t);
    const float cost = cos(t);
    const int xc = 160, yc = 150;
    int xr = -105, yr = 0;
    SGRect rect = { 50, 50, 200, 200 };

    // Draw a square filled with a light blue color
    sg->BeginPath();
    sg->Rectangle(rect);
    rend->SetColor(RGBVAL(220,240,255));
    sg->FillPath(FILLRULE_EVENODD);

    // Set the clipping region to a star-shaped area inside the square
    sg->BeginPath();
    sg->Move(xc + xr, yc + yr);
    for (int i = 0; i < 4; ++i)
    {
        int xtmp = xr*cost + yr*sint;
        yr = -xr*sint + yr*cost;
        xr = xtmp;
        sg->Line(xc + xr, yc + yr);
    }
    sg->SetClipRegion(FILLRULE_WINDING);

    // Draw a series of horizontal, red lines through the square
    rend->SetColor(RGBVAL(200,80,80));
    sg->SetLineWidth(1.0);
    sg->BeginPath();
    for (int y = 50; y < 254; y += 4)
    {
        sg->Move(50, y);
        sg->Line(250, y);
    }
    sg->StrokePath();

    //-----  Label the output of this code example -----
    TextApp txt;
    COLOR crBkgd = RGBVAL(255,255,255);
    COLOR crText = RGBVAL(40,70,110);
    char *str = "Output of code example from ShapeGen::"
                "SetClipRegion reference topic in userdoc.pdf";
    SGPoint xystart = { 24, 360 };
    float scale = 0.3;
    txt.SetTextSpacing(1.1);
    sg->SetLineWidth(3.0);
    sg->ResetClipRegion();
    DrawTextAA(&(*sg), rend, &txt, xystart, scale, str, 
               crBkgd, crText);
}

// ShapeGen::SetLineDash example
void example13(SimpleRenderer *rend, const SGRect& clip)
{
    SGPtr sg(rend, clip);

    const float PI = 3.14159265;
    SGPoint xy[] = {
        {  86, 192 }, {  86, 153 }, {  40, 192 }, { 140, 230 }, 
        { 140, 184 }, { 140, 153 }, {  71,  92 }, { 117,  32 }
    };
    float linewidth = 6.0;
    char dot[] = { 2, 0 };
    char dash[] = { 5, 2, 0 };
    char dashdot[] = { 5, 2, 2, 2, 0 };
    char dashdotdot[] = { 5, 2, 2, 2, 2, 2, 0 };
    char *pattern[] = { dot, dash, dashdot, dashdotdot };

    rend->SetColor(RGBVAL(205, 92, 92));
    sg->SetLineWidth(linewidth);
    for (int i = 0; i < 4; ++i)
    {
        sg->SetLineDash(pattern[i], 0, linewidth/2.0);
        sg->BeginPath();
        sg->EllipticArc(xy[0], xy[1], xy[2], 0, PI);
        sg->PolyEllipticSpline(4, &xy[3]);
        sg->Line(xy[7].x, xy[7].y);
        sg->StrokePath();
        for (int j = 0; j < 8; ++j)
            xy[j].x += 150;
    }

    //-----  Label the output of this code example -----
    TextApp txt;
    COLOR crBkgd = RGBVAL(255,255,255);
    COLOR crText = RGBVAL(40,70,110);
    char *str = "Output of code example from ShapeGen::"
                "SetLineDash reference topic in userdoc.pdf";
    SGPoint xystart = { 24, 360 };
    float scale = 0.3;
    txt.SetTextSpacing(1.1);
    sg->SetLineWidth(3.0);
    sg->SetLineDash(0,0,0);
    DrawTextAA(&(*sg), rend, &txt, xystart, scale, str, 
               crBkgd, crText);
}

// ShapeGen::SetLineEnd example
void example14(SimpleRenderer *rend, const SGRect& clip)
{
    SGPtr sg(rend, clip);

    LINEEND cap[] = { LINEEND_FLAT, LINEEND_ROUND, LINEEND_SQUARE };
    SGPoint vert[] = { { 70, 240 }, { 170, 95 }, { 220, 270 } };

    for (int i = 0; i < 3; ++i)
    {
        sg->BeginPath();
        sg->Move(vert[0].x, vert[0].y);
        sg->PolyLine(2, &vert[1]);
        sg->SetLineWidth(40.0);
        sg->SetLineEnd(cap[i]);
        rend->SetColor(RGBVAL(200,200,240));
        sg->StrokePath();
        sg->SetLineWidth(0);
        rend->SetColor(0);
        sg->StrokePath();
        for (int j = 0; j < 3; ++j)
            vert[j].x += 230;
    }

    //-----  Label the output of this code example -----
    TextApp txt;
    COLOR crBkgd = RGBVAL(255,255,255);
    COLOR crText = RGBVAL(40,70,110);
    char *str = "Output of code example from ShapeGen::"
                "SetLineEnd reference topic in userdoc.pdf";
    SGPoint xystart = { 24, 360 };
    float scale = 0.3;
    txt.SetTextSpacing(1.1);
    sg->SetLineWidth(3.0);
    DrawTextAA(&(*sg), rend, &txt, xystart, scale, str, 
               crBkgd, crText);
}

// ShapeGen::SetLineJoin example
void example15(SimpleRenderer *rend, const SGRect& clip)
{
    SGPtr sg(rend, clip);

    LINEJOIN join[] = { LINEJOIN_BEVEL, LINEJOIN_ROUND, LINEJOIN_MITER };
    SGPoint vert[] = { { 70, 240 }, { 170, 95 }, { 220, 270 } };

    for (int i = 0; i < 3; ++i)
    {
        sg->BeginPath();
        sg->Move(vert[0].x, vert[0].y);
        sg->PolyLine(2, &vert[1]);
        sg->SetLineWidth(40.0);
        sg->SetLineJoin(join[i]);
        sg->CloseFigure();
        rend->SetColor(RGBVAL(200,200,240));
        sg->StrokePath();
        sg->SetLineWidth(0);
        rend->SetColor(0);
        sg->StrokePath();
        for (int j = 0; j < 3; ++j)
            vert[j].x += 230;
    }

    //-----  Label the output of this code example -----
    TextApp txt;
    COLOR crBkgd = RGBVAL(255,255,255);
    COLOR crText = RGBVAL(40,70,110);
    char *str = "Output of code example from ShapeGen::"
                "SetLineJoin reference topic in userdoc.pdf";
    SGPoint xystart = { 24, 360 };
    float scale = 0.3;
    txt.SetTextSpacing(1.1);
    sg->SetLineWidth(3.0);
    DrawTextAA(&(*sg), rend, &txt, xystart, scale, str, 
               crBkgd, crText);
}

// ShapeGen::SetMaskRegion example
void example16(SimpleRenderer *rend, const SGRect& clip)
{
    SGPtr sg(rend, clip);

    const float PI = 3.14159265;
    const float t = 0.8*PI;
    const float sint = sin(t);
    const float cost = cos(t);
    const int xc = 160, yc = 150;
    int xr = -105, yr = 0;
    SGRect rect = { 50, 50, 200, 200 };

    // Draw a square filled with a light blue color
    sg->BeginPath();
    sg->Rectangle(rect);
    rend->SetColor(RGBVAL(220,240,255));
    sg->FillPath(FILLRULE_EVENODD);

    // Mask off a star-shaped area inside the square
    sg->BeginPath();
    sg->Move(xc + xr, yc + yr);
    for (int i = 0; i < 4; ++i)
    {
        int xtmp = xr*cost + yr*sint;
        yr = -xr*sint + yr*cost;
        xr = xtmp;
        sg->Line(xc + xr, yc + yr);
    }
    sg->SetMaskRegion(FILLRULE_WINDING);

    // Draw a series of horizontal, red lines through the square
    rend->SetColor(RGBVAL(200,80,80));
    sg->SetLineWidth(1.0);
    sg->BeginPath();
    for (int y = 50; y < 254; y += 4)
    {
        sg->Move(50, y);
        sg->Line(250, y);
    }
    sg->StrokePath();

    //-----  Label the output of this code example -----
    TextApp txt;
    COLOR crBkgd = RGBVAL(255,255,255);
    COLOR crText = RGBVAL(40,70,110);
    char *str = "Output of code example from ShapeGen::"
                "SetMaskRegion reference topic";
    SGPoint xystart = { 24, 360 };
    float scale = 0.3;
    txt.SetTextSpacing(1.1);
    sg->SetLineWidth(3.0);
    DrawTextAA(&(*sg), rend, &txt, xystart, scale, str, 
               crBkgd, crText);
}

// ShapeGen::SetMiterLimit example
void example17(SimpleRenderer *rend, const SGRect& clip)
{
    SGPtr sg(rend, clip);

    SGPoint vert[] = { { 100, 250 }, { 170, 95 }, { 210, 270 } };

    sg->SetLineEnd(LINEEND_SQUARE); 
    sg->SetLineJoin(LINEJOIN_MITER); 
    sg->SetMiterLimit(4.0);
    for (int i = 0; i < 2; ++i)
    {
        sg->BeginPath();
        sg->Move(vert[0].x, vert[0].y);
        sg->PolyLine(2, &vert[1]);
        sg->SetLineWidth(40.0);
        rend->SetColor(RGBVAL(200, 200, 240));
        sg->StrokePath();
        sg->SetLineWidth(0);
        rend->SetColor(0);
        sg->StrokePath();
        sg->SetMiterLimit(1.4);
        for (int j = 0; j < 3; ++j)
            vert[j].x += 210;
    }

    //-----  Label the output of this code example -----
    TextApp txt;
    COLOR crBkgd = RGBVAL(255,255,255);
    COLOR crText = RGBVAL(40,70,110);
    char *str = "Output of code example from ShapeGen::"
                "SetMiterLimit reference topic in userdoc.pdf";
    SGPoint xystart = { 24, 360 };
    float scale = 0.3;
    txt.SetTextSpacing(1.1);
    sg->SetLineWidth(3.0);
    DrawTextAA(&(*sg), rend, &txt, xystart, scale, str, 
               crBkgd, crText);
}

void (*testfunc[])(SimpleRenderer *rend, const SGRect& cliprect) =
{     
    demo01, demo02, demo03,  
    demo04, demo05, demo06,
    demo07, demo08, demo09, 
    demo10, demo11, demo12,
    MyTest, EggRoll, PieToss,
    example01, example02, example03,
    example04, example05, example06,
    example07, example08, example09,
    example10, example11, example12,
    example13, example14, example15,
    example16, example17,
};

//----------------------------------------------------------------------
//
// Main program calls the runtest function to run the tests
//
//----------------------------------------------------------------------

bool runtest(int testnum, SimpleRenderer *rend, const SGRect& cliprect)
{
    if (0 <= testnum && testnum < ARRAY_LEN(testfunc))
    {
        testfunc[testnum](rend, cliprect);
        return true;
    }
    return false;
}
