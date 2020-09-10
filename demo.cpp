/*
  Copyright (C) 2019 Jerry R. VanAken

  This software is provided 'as-is', without any express or implied
  warranty. In no event will the authors be held liable for any damages
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
//---------------------------------------------------------------------
//
// demo.cpp:
//   This file contains the code for the ShapeGen demo and for the
//   code examples presented in the ShapeGen User's Guide.
//
//---------------------------------------------------------------------

#include <math.h>
#include <assert.h>
#include "demo.h"

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
    return RGBX(r,g,b);
}

//---------------------------------------------------------------------
//
// Zero-terminated arrays of dashed-line patterns
//
//----------------------------------------------------------------------

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

//---------------------------------------------------------------------
//
// Demo functions and code examples
//
//----------------------------------------------------------------------

// Demo screen 1: ShapeGen logo
void demo01(SimpleRenderer *rend, SimpleRenderer *aarend, const SGRect& clip)  // page 1
{
    SGPtr sg(rend, clip);
    TextApp txt;
    float xfrm[2][3] = {
        { 2.0,  0, 175.0 }, { -0.8, 2.5, 685.0 }
    };
    COLOR crBkgd = RGBX(222,222,255);
    COLOR crText = RGBX(40, 70, 110);
    COLOR color[] = {
        RGBX(222,100, 50), RGBX(185,100,222), 
        RGBX( 90,206, 45), RGBX(220, 20, 60), 
        RGBX(  0,206,209)
    };
    SGPoint arrow[] = {
        {  0+1170, 30+889 }, { 30+1170, 30+889 }, 
        { 30+1170, 40+889 }, { 63+1170, 20+889 }, 
        { 30+1170,  0+889 }, { 30+1170, 10+889 }, 
        {  0+1170, 10+889 } 
    };
    SGPoint xystart;
    SGPoint corner = { 40, 40 };
    SGPoint elips[] = { { 640, 440 }, { 640+550, 435 }, { 640, 435+110 } };
    float len = 110;
    float angle = +PI/8.1;
    char *str = 0;
    float scale = 1.0;
    float width;
    int i;

    // Use the basic renderer to do a background color fill
    SGRect frame = { 10, 10, DEMO_WIDTH-20, DEMO_HEIGHT-20 };
    sg->BeginPath();
    sg->RoundedRectangle(frame, corner);
    rend->SetColor(crBkgd);
    sg->FillPath(FILLRULE_EVENODD);

    // Now switch to the antialiasing renderer
    sg->SetRenderer(aarend);

    // Draw a frame around the window
    sg->SetLineWidth(8.0);
    aarend->SetColor(color[0]);
    sg->StrokePath();

    // Draw background pattern: rotated ellipses
    aarend->SetColor(RGBX(233, 150, 122));
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
    aarend->SetColor(crText);
    txt.DisplayText(&(*sg), xystart, scale, str);
    sg->SetLineWidth(4.0);
    str = "At the core of a 2-D graphics system";
    scale = 0.5;
    width = txt.GetTextWidth(scale, str);
    xystart.x = (DEMO_WIDTH - width)/2;
    xystart.y = 780;
    txt.DisplayText(&(*sg), xystart, scale, str);
    str = "A portable, lightweight C++ implementation";
    width = txt.GetTextWidth(scale, str);
    xystart.x = (DEMO_WIDTH - width)/2;
    xystart.y += 64;
    txt.DisplayText(&(*sg), xystart, scale, str);
    sg->SetLineWidth(3.0);
    xystart.x = 748;
    xystart.y = 916;

    txt.DisplayText(&(*sg), xystart, 0.34, "Hit space bar to continue");
    sg->BeginPath();
    sg->Move(arrow[0].x, arrow[0].y);
    sg->PolyLine(ARRAY_LEN(arrow)-1, &arrow[1]);
    sg->FillPath(FILLRULE_EVENODD);

    // Draw letters "ShapeGen" with slanted text and shadowing
    str = "ShapeGen";
    sg->SetFlatness(FLATNESS_DEFAULT);
    sg->SetLineWidth(32.0);
    aarend->SetColor(RGBA(99,99,99,170));
    txt.SetTextSpacing(1.2);
    txt.DisplayText(&(*sg), xfrm, str);
    xfrm[0][2] -= 8.0;
    xfrm[1][2] -= 8.0;
    sg->SetLineWidth(32.0);
    aarend->SetColor(color[3]);
    txt.DisplayText(&(*sg), xfrm, str);
    sg->SetLineWidth(24.0);
    aarend->SetColor(color[0]);
    txt.DisplayText(&(*sg), xfrm, str);
}

// Demo screen 2: Path drawing modes
void demo02(SimpleRenderer *rend, SimpleRenderer *aarend, const SGRect& clip)
{
    SGPtr sg(rend, clip);
    TextApp txt;
    COLOR crBkgd  = RGBX(240,220,220);
    COLOR crFrame = RGBX(50,80,140);
    COLOR crText  = RGBX(40,70,110);
    COLOR crFill  = RGBX(70,206,209);
    COLOR crBlack = RGBX(0,0,0);

    // Use the basic renderer to do a background color fill
    SGPoint corner = { 40, 40 };
    SGRect frame = { 10, 10, DEMO_WIDTH-20, DEMO_HEIGHT-20 };
    sg->BeginPath();
    sg->RoundedRectangle(frame, corner);
    rend->SetColor(crBkgd);
    sg->FillPath(FILLRULE_EVENODD);

    // Now switch to the antialiasing renderer
    sg->SetRenderer(aarend);

    // Draw a frame around the window
    sg->SetLineWidth(8.0);
    aarend->SetColor(crFrame); 
    sg->StrokePath();

    // Draw title text
    sg->SetLineWidth(6.0);
    char *str = "Path Drawing Modes";
    float scale = 0.73;
    float width = txt.GetTextWidth(scale, str);
    SGPoint xystart;
    xystart.x = (DEMO_WIDTH - width)/2;
    xystart.y = 136;
    aarend->SetColor(crText);
    txt.DisplayText(&(*sg), xystart, scale, str);

    // Draw captions for three figures
    sg->SetLineWidth(3.0);
    scale = 0.34;
    str = "Even-odd fill rule";
    width = txt.GetTextWidth(scale, str);
    xystart.x = 280 - width/2;
    xystart.y = 796;
    txt.DisplayText(&(*sg), xystart, scale, str);
    str = "Winding number fill rule";
    width = txt.GetTextWidth(scale, str);
    xystart.x = 1000 - width/2;
    txt.DisplayText(&(*sg), xystart, scale, str);
    str = "Brush stroke";
    width = txt.GetTextWidth(scale, str);
    xystart.x = 640 - width/2;
    xystart.y = 670;
    txt.DisplayText(&(*sg), xystart, scale, str);

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
    aarend->SetColor(crFill);
    sg->FillPath(FILLRULE_EVENODD);
    aarend->SetColor(crBlack);
    sg->SetLineWidth(1.4);
    sg->StrokePath();

    // Fill star using winding number fill rule
    FitBbox(&bbdst[1], len, xy, &bbsrc, star);
    sg->BeginPath();
    sg->Move(xy[0].x, xy[0].y); 
    sg->PolyLine(ARRAY_LEN(xy)-1, &xy[1]);
    sg->CloseFigure();
    aarend->SetColor(crFill);
    sg->FillPath(FILLRULE_WINDING);
    aarend->SetColor(crBlack);
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
    aarend->SetColor(crFill);
    sg->StrokePath();
    sg->SetLineWidth(1.4);
    aarend->SetColor(crBlack);
    sg->StrokePath();
}

// Demo screen 3: Clip to arbitrary shapes
void demo03(SimpleRenderer *rend, SimpleRenderer *aarend, const SGRect& clip)
{
    SGPtr sg(rend, clip);
    TextApp txt;
    COLOR crBkgd  = RGBX(222, 222, 255);
    COLOR crFrame = RGBX( 32, 152, 224);
    COLOR crText  = RGBX(40,70,110);

    // Use the basic renderer to do background color fill
    SGPoint corner = { 40, 40 };
    SGRect frame = { 10, 10, DEMO_WIDTH-20, DEMO_HEIGHT-20 };
    sg->BeginPath();
    sg->RoundedRectangle(frame, corner);
    rend->SetColor(crBkgd);
    sg->FillPath(FILLRULE_EVENODD);

    // Now switch to the antialiasing renderer
    sg->SetRenderer(aarend);

    // Draw a frame around the window
    sg->SetLineWidth(8.0);
    aarend->SetColor(crFrame); 
    sg->StrokePath();

    // Draw the title text
    sg->SetLineWidth(6.0);
    char *str = "Clip to Arbitrary Shapes";
    float scale = 0.73;
    float width = txt.GetTextWidth(scale, str);
    SGPoint xystart;
    xystart.x = (DEMO_WIDTH - width)/2;
    xystart.y = 130;
    aarend->SetColor(crText);
    txt.DisplayText(&(*sg), xystart, scale, str);

    // Draw text at lower left corner of window
    scale = 0.48;
    sg->SetLineWidth(4.0);
    str = "Clip to shape";
    width = txt.GetTextWidth(scale, str);
    xystart.x = 340 - width/2;
    xystart.y = 844;
    txt.DisplayText(&(*sg), xystart, scale, str);

    // Draw text at lower right corner of window
    str = "Mask off shape";
    width = txt.GetTextWidth(scale, str);
    xystart.x = 940 - width/2;
    xystart.y = 844;
    txt.DisplayText(&(*sg), xystart, scale, str);

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
    sg->SetLineWidth(1.7);
    aarend->SetColor(RGBX(211,211,222));
    sg->SetFixedBits(16);
    sg->BeginPath();
    sg->Rectangle(rect[0]);
    sg->Rectangle(rect[1]);
    sg->FillPath(FILLRULE_EVENODD);

    // Define bounding boxes on left and right
    SGRect bbsrc;
    SGRect bbdst[] = {
        { 100<<16, 280<<16, 480<<16, 440<<16 }, 
        { 700<<16, 280<<16, 480<<16, 440<<16 },
    };
    SGPoint xy[len];

    // Mask off star-shaped region on right side
    GetBbox(&bbsrc, len, star);
    FitBbox(&bbdst[1], len, xy, &bbsrc, star);
    sg->BeginPath();
    sg->Move(xy[0].x, xy[0].y);
    sg->PolyLine(len-1, &xy[1]);
    sg->CloseFigure();
    sg->SetMaskRegion(FILLRULE_WINDING);

    // Set up star-shaped clipping region on left
    FitBbox(&bbdst[0], len, xy, &bbsrc, star);
    sg->BeginPath();
    sg->Rectangle(rect[1]);
    sg->Move(xy[0].x, xy[0].y);
    sg->PolyLine(len-1, &xy[1]);
    sg->CloseFigure();
    sg->SetClipRegion(FILLRULE_WINDING);

    // Paint solid blue color inside new clipping region
    IntToFixed(&frame);
    aarend->SetColor(RGBA(0,100,200,200));
    sg->BeginPath();
    sg->Rectangle(frame);
    sg->FillPath(FILLRULE_EVENODD);

    // Paint abstract design inside clipping region
    COLOR color[] = {
        RGBX( 16, 108, 240), RGBX( 32, 152, 224), RGBX( 48, 196, 208),
        RGBX( 64, 240, 192), RGBX( 80,  28, 176), RGBX( 96,  72, 160),
        RGBX(112, 116, 144), RGBX(128, 160, 128), RGBX(144, 204, 112),
        RGBX(160, 248,  96), RGBX(176,  36,  80), RGBX(192,  80,  64),
        RGBX(208, 124,  48), RGBX(224, 168,  32), RGBX(240, 212,  16),
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
        aarend->SetColor(color[j]);
        if (++j == ARRAY_LEN(color))
            j = 0;

        sg->BeginPath();
        sg->Move(point[0].x, point[0].y);
        sg->PolyLine(ARRAY_LEN(point)-1, &point[1]);
        sg->StrokePath();
        point[1].x = i + frame.x;
    }
}

// Demo screen 4: Stroked line caps and joins
void demo04(SimpleRenderer *rend, SimpleRenderer *aarend, const SGRect& clip)
{
    SGPtr sg(rend, clip);
    TextApp txt;
    LINEJOIN join[] = { LINEJOIN_BEVEL, LINEJOIN_ROUND, LINEJOIN_MITER };
    LINEEND cap[] = { LINEEND_FLAT, LINEEND_ROUND, LINEEND_SQUARE };
    COLOR crBkgd  = RGBX(255,248,240);
    COLOR crFrame = RGBX(230,140,70);
    COLOR crText  = RGBX(40,70,110);
    COLOR crFill  = RGBX(255,175,100);
    COLOR crBlack = RGBX(0,0,0);

    // Use basic renderer to do background color fill
    SGPoint corner = { 40, 40 };
    SGRect frame = { 10, 10, DEMO_WIDTH-20, DEMO_HEIGHT-20 };
    sg->BeginPath();
    sg->RoundedRectangle(frame, corner);
    rend->SetColor(crBkgd);
    sg->FillPath(FILLRULE_EVENODD);

    // Now switch to antialiasing renderer
    sg->SetRenderer(aarend);

    // Draw a frame around the window
    sg->SetLineWidth(8.0);
    aarend->SetColor(crFrame); 
    sg->StrokePath();

    // Draw title text
    sg->SetLineWidth(6.0);
    char *str = "Stroked Line Caps and Joins";
    float scale = 0.73;
    float width = txt.GetTextWidth(scale, str);
    SGPoint xystart;
    xystart.x = (DEMO_WIDTH - width)/2;
    xystart.y = 130;
    aarend->SetColor(crText);
    txt.DisplayText(&(*sg), xystart, scale, str);

    // Label rows with text
    sg->SetLineWidth(3.0);
    scale = 0.38;
    str = "Flat cap";
    width = txt.GetTextWidth(scale, str);
    xystart.x = 310 - width;
    xystart.y = 428;
    txt.DisplayText(&(*sg), xystart, scale, str);
    str = " Round cap";
    width = txt.GetTextWidth(scale, str);
    xystart.x = 310 - width;
    xystart.y += 190;
    txt.DisplayText(&(*sg), xystart, scale, str);
    str = "Square cap";
    width = txt.GetTextWidth(scale, str);
    xystart.x = 310 - width;
    xystart.y += 190;
    txt.DisplayText(&(*sg), xystart, scale, str);

    // Label columns with text
    str = "Bevel join";
    xystart.x = 392;
    xystart.y = 268;
    txt.DisplayText(&(*sg), xystart, scale, str);
    str = "Round join";
    xystart.x += 280;
    txt.DisplayText(&(*sg), xystart, scale, str);
    str = "Miter join";
    xystart.x += 280;
    txt.DisplayText(&(*sg), xystart, scale, str);

    // Draw stroked lines with different cap and join styles
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
            aarend->SetColor(crFill);
            sg->SetLineWidth(44.0);
            sg->BeginPath();
            sg->Move(p0.x, p0.y);
            sg->Line(p1.x, p1.y);
            sg->Line(p2.x, p2.y);
            sg->StrokePath();
            aarend->SetColor(crBlack);
            sg->SetLineWidth(1.7);
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

// Demo screen 5: Miter limit
void demo05(SimpleRenderer *rend, SimpleRenderer *aarend, const SGRect& clip)
{
    SGPtr sg(rend, clip);
    TextApp txt;
    COLOR crBkgd = RGBX(222,222,255);
    COLOR crFrame = RGBX(80,40,140);
    COLOR crText = RGBX(40,70,110); 
    
    // Use basic renderer to do background color fill
    SGPoint corner = { 40, 40 };
    SGRect frame = { 10, 10, DEMO_WIDTH-20, DEMO_HEIGHT-20 };
    sg->BeginPath();
    sg->RoundedRectangle(frame, corner);
    rend->SetColor(crBkgd);
    sg->FillPath(FILLRULE_EVENODD);

    // Now switch to antialiasing renderer
    sg->SetRenderer(aarend);

    // Draw a frame around the window
    sg->SetLineWidth(8.0);
    aarend->SetColor(crFrame); 
    sg->StrokePath();

    // Draw the title text
    SGPoint xystart;
    xystart.x = 75;
    xystart.y = 150;
    char *str = "Miter Limit";
    float scale = 0.73;
    float width = txt.GetTextWidth(scale, str);
    aarend->SetColor(crText);
    sg->SetLineWidth(6.0);
    txt.DisplayText(&(*sg), xystart, scale, str);

    // Initialize parameters for circular miter-limit display
    SGPoint v[] = {
        { 4, 160 }, { 0, 240 }, { -4, 160 },
    };
    COLOR color[] = {
        RGBX( 30,144,255), RGBX(221,160,221), RGBX(255, 69,  0), 
        RGBX(147,112,219), RGBX( 50,205, 50), RGBX(250,235,215), 
    };
    float mlim[] = { 1.0, 7.0, 13.0, 19.0 };
    SGPoint center = { 720, 480 };
    float linewidth = 20.0;

    // Draw dark, thin rings in background
    aarend->SetColor(RGBX(80,80,80));
    sg->SetFixedBits(16);
    sg->SetLineWidth(1.7);
    sg->BeginPath();
    for (int i = 0; i < 3; ++i)
    {
        int radius = 65536*(240 + mlim[i]*linewidth/2);
        SGPoint ring[3] = { { 720<<16, 480<<16 } };

        ring[1] = ring[2] = ring[0];
        ring[1].x += radius;
        ring[2].y += radius;
        sg->Ellipse(ring[0], ring[1], ring[2]);
    }
    sg->StrokePath();
    
    for (int i = 0; i < 4; ++i)
    {
        for (int j = 0; j < 8; ++j)
        {
            float cosa = cos(2*i*PI/32.0 + 2*j*PI/8.0);
            float sina = sin(2*i*PI/32.0 + 2*j*PI/8.0);
            int x[3], y[3];

            // Stroke fat lines with a variety of miter limits
            for (int k = 0; k < 3; ++k)
            {
                x[k] = 65536*(center.x + cosa*v[k].x - sina*v[k].y);
                y[k] = 65536*(center.y + sina*v[k].x + cosa*v[k].y);
            }
            aarend->SetColor(color[i]);
            sg->SetLineWidth(linewidth);
            sg->SetLineEnd(LINEEND_FLAT);
            sg->SetLineJoin(LINEJOIN_MITER);
            sg->BeginPath();
            sg->Move(x[0], y[0]);
            sg->Line(x[1], y[1]);
            sg->Line(x[2], y[2]);
            sg->SetMiterLimit(mlim[i]);
            sg->StrokePath();

            // Draw thin white lines over fat lines
            aarend->SetColor(RGBX(255,255,255));
            sg->SetLineJoin(LINEJOIN_BEVEL);
            sg->SetLineWidth(1.7);
            sg->BeginPath();
            sg->Move(x[0], y[0]);
            sg->Line(x[1], y[1]);
            sg->Line(x[2], y[2]);
            sg->StrokePath();
        }
    }

    // Draw text inside circle
    char *msg[] = { "Get the", "point?" }; 
    sg->SetFixedBits(0);
    sg->SetLineWidth(4.0);
    scale = 0.5;
    xystart.y = center.y - 16;
    aarend->SetColor(crText);
    for (int i = 0; i < ARRAY_LEN(msg); ++i)
    {
        width = txt.GetTextWidth(scale, msg[i]);
        xystart.x = center.x - width/2.0;
        txt.DisplayText(&(*sg), xystart, scale, msg[i]);
        xystart.y += 50;
    }
}

// Demo screen 6: Dashed line patterns
void demo06(SimpleRenderer *rend, SimpleRenderer *aarend, const SGRect& clip)
{
    SGPtr sg(rend, clip);
    TextApp txt;
    COLOR crBkgd = RGBX(222,222,255);
    COLOR crFrame = RGBX(90,160,50);
    COLOR crText = RGBX(40,70,110); 
    int i;

    // Use basic renderer to do a background color fill
    SGPoint corner = { 40, 40 };
    SGRect frame = { 10, 10, DEMO_WIDTH-20, DEMO_HEIGHT-20 };
    sg->BeginPath();
    sg->RoundedRectangle(frame, corner);
    rend->SetColor(crBkgd);
    sg->FillPath(FILLRULE_EVENODD);

    // Now switch to the antialiasing renderer
    sg->SetRenderer(aarend);

    // Draw a frame around the window
    sg->SetLineWidth(8.0);
    aarend->SetColor(crFrame); 
    sg->StrokePath();

    // Draw the title text
    sg->SetLineWidth(6.0);
    char *str = "Dashed Line Patterns";
    float scale = 0.73;
    float width = txt.GetTextWidth(scale, str);
    SGPoint xystart;
    xystart.x = (DEMO_WIDTH - width)/2;
    xystart.y = 130;
    aarend->SetColor(crText);
    txt.DisplayText(&(*sg), xystart, scale, str);

    // Draw windmill shape
    SGPoint blade[][7] = {
        {
            { 160-100, 120+95 }, { 250-100,  30+95 },
            { 320-100, 100+95 }, { 320-100, 240+95 },
            { 320-100, 380+95 }, { 370-100, 450+95 },
            { 480-100, 360+95 },
        },                  
        {                   
            { 440-100,  80+95 }, { 530-100, 170+95 },
            { 460-100, 240+95 }, { 320-100, 240+95 },
            { 180-100, 240+95 }, { 110-100, 310+95 },
            { 200-100, 400+95 },
        },
    };
    sg->SetLineEnd(LINEEND_SQUARE);
    sg->SetLineJoin(LINEJOIN_MITER);
    sg->BeginPath();
    sg->Move(blade[0][0].x, blade[0][0].y);
    sg->PolyBezier3(ARRAY_LEN(blade[0])-1, &blade[0][1]);
    sg->CloseFigure();
    sg->Move(blade[1][0].x, blade[1][0].y);
    sg->PolyBezier3(ARRAY_LEN(blade[1])-1, &blade[1][1]);
    aarend->SetColor(RGBX(144,238,144));
    sg->CloseFigure();
    sg->FillPath(FILLRULE_EVENODD);
    aarend->SetColor(RGBX(0,100,0));
    sg->SetLineDash(dashedLineShortDash, 0, 4.0);
    sg->SetLineWidth(10.0);
    sg->StrokePath();
    aarend->SetColor(RGBX(100,180,50));
    sg->SetLineEnd(LINEEND_FLAT);
    sg->SetLineWidth(4.0);
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
    aarend->SetColor(RGBX(144,238,144));
    sg->FillPath(FILLRULE_EVENODD);
    aarend->SetColor(RGBX(0,100,0));
    sg->SetLineWidth(4.0);
    sg->SetLineDash(dashedLineDot, 0, 3.0);
    sg->StrokePath();

    // Draw clubs suit symbol
    char dashedLineBeaded[] = { 1, 12, 0 };
    SGPoint clubs[] = {
        { 220, 400 },  { 260, 400 }, { 300, 320 },  { 300, 260 },
        { 100, 460 },  { 100,  20 }, { 300, 220 },  { 100,  20 },
        { 540,  20 },  { 340, 220 }, { 540,  20 },  { 540, 460 },
        { 340, 260 },  { 340, 320 }, { 380, 400 },  { 420, 400 },
    };
    SGRect srcbbox, dstbbox = { 600-250, 370-250, 500, 500 };
    IntToFixed(ARRAY_LEN(clubs), &clubs[0]);
    IntToFixed(&dstbbox);
    GetBbox(&srcbbox, ARRAY_LEN(clubs), &clubs[0]);
    FitBbox(&dstbbox, ARRAY_LEN(clubs), &clubs[0], &srcbbox, &clubs[0]); 
    sg->SetFixedBits(16);
    sg->SetLineEnd(LINEEND_ROUND);
    sg->SetLineJoin(LINEJOIN_BEVEL);
    aarend->SetColor(RGBX(120,210,100));
    sg->BeginPath();
    sg->Move(clubs[0].x, clubs[0].y);
    sg->PolyBezier3(15, &clubs[1]);
    sg->CloseFigure();
    sg->FillPath(FILLRULE_EVENODD);
    sg->SetLineWidth(8.0);
    sg->SetLineDash(dashedLineBeaded, 0, 1.0);
    aarend->SetColor(RGBX(0,100,0));
    sg->StrokePath();

    // Draw square-weave pattern
    SGPoint shape[] = {
         {   0,   0 }, { 100,   0 }, { 100, 300 }, {   0, 300 },
         {   0, 200 }, { 300, 200 }, { 300, 300 }, { 200, 300 },
         { 200,   0 }, { 300,   0 }, { 300, 100 }, {   0, 100 },   {0,0}
    };
    char dashedline[] = { 6,2, 5,2, 4,2, 3,2, 2,2, 1,2, 2,2, 3,2, 4,2, 5,2, 0 };

    for (int i = 0; i < ARRAY_LEN(shape); ++i)
    {
        shape[i].x += 890;
        shape[i].y += 205;
    }
    aarend->SetColor(RGBX(0,139,120));
    sg->SetLineEnd(LINEEND_FLAT);
    sg->SetLineJoin(LINEJOIN_ROUND);
    sg->SetLineWidth(60.0);
    sg->SetLineDash(dashedline, 3, 0.909);
    sg->SetFixedBits(0);
    sg->BeginPath();
    sg->Move(shape[0].x, shape[0].y);
    sg->PolyLine(ARRAY_LEN(shape)-1, &shape[1]);
    sg->CloseFigure();
    sg->StrokePath();

    // Draw curly-cue shape
    int xc = 210, yc = 735;
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
    sg->SetLineEnd(LINEEND_ROUND);
    sg->SetLineJoin(LINEJOIN_BEVEL);
    sg->BeginPath();
    sg->Move(xy[0].x, xy[0].y);
    sg->PolyEllipticSpline(64, &xy[1]);
    aarend->SetColor(RGBX(0,100,0));
    sg->SetLineWidth(16.0);
    sg->SetLineDash(dashedLineDashDoubleDot, 0, 4.6);
    sg->StrokePath();
    aarend->SetColor(RGBX(80,200,70));
    sg->SetLineWidth(10.0);
    sg->StrokePath();
    aarend->SetColor(RGBX(144,238,144));
    sg->SetLineWidth(6.0);
    sg->StrokePath();
}

// Demo screen 7: Thin stroked lines
void demo07(SimpleRenderer *rend, SimpleRenderer *aarend, const SGRect& clip)
{
    SGPtr sg(rend, clip);
    TextApp txt;
    COLOR crBkgd = RGBX(242,236,255);
    COLOR crFrame = RGBX(132,24,10);
    COLOR crText = RGBX(40,70,110);
    COLOR crBox = RGBX(150,50,20);

    // Use the basic renderer to do a background fill
    SGPoint corner = { 40, 40 };
    SGRect frame = { 10, 10, DEMO_WIDTH-20, DEMO_HEIGHT-20 };
    sg->BeginPath();
    sg->RoundedRectangle(frame, corner);
    rend->SetColor(crBkgd);
    sg->FillPath(FILLRULE_EVENODD);

    // Now switch to the antialiasing renderer
    sg->SetRenderer(aarend);

    // Draw a frame around the window
    sg->SetLineWidth(8.0);
    aarend->SetColor(crFrame); 
    sg->StrokePath();

    // Draw title text
    sg->SetLineWidth(6.0);
    char *str = "Thin Stroked Lines";
    float scale = 0.73;
    float width = txt.GetTextWidth(scale, str);
    SGPoint xystart;
    xystart.x = (DEMO_WIDTH - width)/2;
    xystart.y = 130;
    aarend->SetColor(crText);
    txt.DisplayText(&(*sg), xystart, scale, str);

    // Draw text at lower left corner of window
    scale = 0.48;
    sg->SetLineWidth(4.0);
    str = "Mimic";
    width = txt.GetTextWidth(scale, str);
    xystart.x = 340 - width/2;
    xystart.y = 812;
    txt.DisplayText(&(*sg), xystart, scale, str);
    str = "Bresenham lines";
    width = txt.GetTextWidth(scale, str);
    xystart.x = 340 - width/2;
    xystart.y = 868;
    txt.DisplayText(&(*sg), xystart, scale, str);

    // Draw text at lower right
    str = "Precisely track";
    width = txt.GetTextWidth(scale, str);
    xystart.x = 940 - width/2;
    xystart.y = 812;
    txt.DisplayText(&(*sg), xystart, scale, str);
    str = "contours";
    width = txt.GetTextWidth(scale, str);
    xystart.x = 940 - width/2;
    xystart.y = 868;
    txt.DisplayText(&(*sg), xystart, scale, str);

    // Switch to basic renderer
    sg->SetRenderer(rend);

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
    rend->SetColor(RGBX(0,0,80));
    sg->BeginPath();
    sg->Move(point[0].x, point[0].y);
    sg->PolyLine(ARRAY_LEN(point)-1, &point[1]);
    sg->SetLineWidth(0);
    sg->CloseFigure();
    sg->StrokePath();

    // Fill rectangle (square box) on right side of window
    SGRect rect = { 670, 190, 540, 540 };
    sg->BeginPath();
    sg->Rectangle(rect);
    rend->SetColor(crBox);
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
    rend->SetColor(RGBX(30,100,180));
    sg->FillPath(FILLRULE_EVENODD);
    rend->SetColor(RGBX(255,255,255));
    sg->StrokePath();
}

// Demo screen 8: Bezier curves
void demo08(SimpleRenderer *rend, SimpleRenderer *aarend, const SGRect& clip)  // Bezier 'S'
{
    SGPtr sg(rend, clip);
    TextApp txt;
    COLOR crBkgd = RGBX(222,222,255);
    COLOR crText = RGBX(40,70,110); 
    int i;

    // Use basic renderer to do background fill
    SGPoint corner = { 40, 40 };
    SGRect frame = { 10, 10, DEMO_WIDTH-20, DEMO_HEIGHT-20 };
    sg->BeginPath();
    sg->RoundedRectangle(frame, corner);
    rend->SetColor(crBkgd);
    sg->FillPath(FILLRULE_EVENODD);

    // Now switch to the antialiasing renderer
    sg->SetRenderer(aarend);

    // Draw a frame around the window
    sg->SetLineWidth(8.0);
    aarend->SetColor(RGBX(200,100,15)); 
    sg->StrokePath();

    // Draw the title text
    sg->SetLineWidth(6.0);
    char *str = "Bezier Curves";
    float scale = 0.73;
    float len = txt.GetTextWidth(scale, str);
    SGPoint xystart;
    xystart.x = (DEMO_WIDTH - len)/2;
    xystart.y = 130;
    aarend->SetColor(crText);
    txt.DisplayText(&(*sg), xystart, scale, str);

    // Define Bezier control polygon for 'S' glyph
    SGPoint glyph[] = {
        {   0, 165 }, {  52, 100 }, { 123,  67 }, { 175,  67 }, 
        { 248,  67 }, { 288, 115 }, { 288, 152 }, { 288, 290 }, 
        {   0, 305 }, {   0, 490 }, {   0, 598 }, {  80, 667 }, 
        { 201, 667 }, { 253, 667 }, { 305, 656 }, { 351, 620 }, 
        { 351, 584 }, { 351, 548 }, { 351, 512 }, { 297, 567 },
        { 240, 582 }, { 193, 582 }, { 135, 582 }, {  88, 551 }, 
        {  88, 501 }, {  88, 372 }, { 377, 375 }, { 377, 161 }, 
        { 377,  56 }, { 296, -20 }, { 190, -20 }, { 116, -20 }, 
        {  59,   0 }, {   0,  42 }, {   0,  83 }, {   0, 124 }, 
        {   0, 165 }, 
    };
    SGRect bbsrc, bbdst = { 768<<16, 875<<16, 384<<16, -660<<16 };
    float width = 54.0;

    // Scale and translate control points for 'S' glyph
    IntToFixed(ARRAY_LEN(glyph), glyph);
    GetBbox(&bbsrc, ARRAY_LEN(glyph), glyph);
    FitBbox(&bbdst, ARRAY_LEN(glyph), glyph, &bbsrc, glyph); 

    // Draw the 'S' glyph
    sg->SetFixedBits(16);
    aarend->SetColor(RGBX(255,120,30));
    sg->SetLineJoin(LINEJOIN_MITER);
    sg->SetMiterLimit(1.10);
    sg->SetLineWidth(width);
    sg->BeginPath(); 
    sg->Move(glyph[0].x, glyph[0].y);
    sg->PolyBezier3(ARRAY_LEN(glyph)-1, &glyph[1]);
    sg->CloseFigure();
    sg->StrokePath();
    sg->SetLineWidth(width - 6);
    aarend->SetColor(RGBX(211,233,211));
    sg->StrokePath();

    // Outline control polygon for 'S' glyph
    sg->SetLineWidth(1.0);
    aarend->SetColor(RGBX(200,0,0));
    sg->StrokePath();

    // Draw Bezier handles on 'S' glyph's control polygon
    aarend->SetColor(RGBX(85,107,47));
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

    // Highlight vertexes of 'S' glyph's control polygon
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

    // Define control points for club suit symbol
    SGPoint clubs[] = {       
        { 220, 400 }, { 260, 400 }, { 300, 320 }, { 300, 260 },
        { 100, 460 }, { 100,  20 }, { 300, 220 }, { 100,  20 },
        { 540,  20 }, { 340, 220 }, { 540,  20 }, { 540, 460 },
        { 340, 260 }, { 340, 320 }, { 380, 400 }, { 420, 400 },
    };
    SGPoint point[ARRAY_LEN(clubs)];
    SGRect bboxsrc, bboxdst = { 70<<16, 240<<16, 600<<16, 600<<16 };

    // Move and resize control points for club suit symbol
    IntToFixed(ARRAY_LEN(clubs), &clubs[0]);
    GetBbox(&bboxsrc, ARRAY_LEN(clubs), &clubs[0]);
    FitBbox(&bboxdst, ARRAY_LEN(clubs), &point[0], 
            &bboxsrc, &clubs[0]);

    // Switch to basic renderer for large area fill
    sg->SetRenderer(rend);

    // Fill club suit symbol
    COLOR crFill = RGBX(100,200,47);
    rend->SetColor(crFill);
    sg->SetLineDash(0,0,0);
    sg->SetLineEnd(LINEEND_SQUARE);
    sg->SetLineJoin(LINEJOIN_BEVEL);
    sg->SetFixedBits(16);
    sg->BeginPath();
    sg->Move(point[0].x, point[0].y);
    sg->PolyBezier3(15, &point[1]);
    sg->CloseFigure();
    sg->FillPath(FILLRULE_EVENODD);

    // Switch to antialiasing renderer to stroke edges
    sg->SetRenderer(aarend);

    // Stroke edges of club suit symbol
    COLOR crTrim = RGBX(0,160,0);
    aarend->SetColor(crTrim);
    sg->SetLineWidth(6.0);
    sg->StrokePath();

    // Outline control polygon for club suit symbol
    sg->BeginPath();
    sg->Move(point[0].x, point[0].y);
    sg->PolyLine(ARRAY_LEN(point)-1, &point[1]);
    aarend->SetColor(RGBX(200,0,0));
    sg->SetLineWidth(1.0);
    sg->StrokePath();

    // Highlight vertexes of club suit control polygon
    aarend->SetColor(RGBX(200,0,0));
    sg->BeginPath();
    for (int i = 0; i < ARRAY_LEN(point); ++i)
    {
        SGPoint p0, p1, p2;

        p0 = p1 = p2 = point[i];
        p1.x += 3 << 16;
        p2.y += 3 << 16;
        sg->Ellipse(p0, p1, p2);
    }
    sg->FillPath(FILLRULE_WINDING);
}

// Demo screen 9: Ellipses and elliptic splines
void demo09(SimpleRenderer *rend, SimpleRenderer *aarend, const SGRect& clip)
{
    SGPtr sg(rend, clip);
    TextApp txt;
    COLOR crBkgd = RGBX(220,220,220);
    COLOR crText = RGBX(40,70,110);

    // Use basic renderer to do a background fill
    SGPoint corner = { 40, 40 };
    SGRect frame = { 10, 10, DEMO_WIDTH-20, DEMO_HEIGHT-20 };
    sg->BeginPath();
    sg->RoundedRectangle(frame, corner);
    rend->SetColor(crBkgd);
    sg->FillPath(FILLRULE_EVENODD);

    // Now switch to the antialiasing renderer
    sg->SetRenderer(aarend);

    // Draw a frame around the window
    sg->SetLineWidth(8.0);
    aarend->SetColor(RGBX(200,100,15)); 
    sg->StrokePath();

    // Draw the title text
    sg->SetLineWidth(6.0);
    char *str = "Ellipses and Elliptic Splines";
    float scale = 0.73;
    float len = txt.GetTextWidth(scale, str);
    SGPoint xystart;
    xystart.x = (DEMO_WIDTH - len)/2;
    xystart.y = 130;
    aarend->SetColor(crText);
    txt.DisplayText(&(*sg), xystart, scale, str);

    // Define the points for the "at" (i.e., '@') glyph 
    XY glyph[] = {
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
    float width = 2.0;

    // Draw the '@' glyph at several different scales
    scale = 0.60;
    sg->SetFixedBits(16);
    aarend->SetColor(RGBX(255,120,30));
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
        sg->SetLineWidth(width);
        sg->BeginPath();
        sg->Ellipse(xy[2], xy[3], xy[4]); 
        sg->Move(xy[5].x, xy[5].y);
        sg->PolyEllipticSpline(12, &xy[6]);
        sg->StrokePath();
        xoffset += xoffset + 24;
        scale += 1.2*scale;
        width += width + 1;
    }
    sg->SetLineWidth(width/2 - 6);
    aarend->SetColor(RGBX(200,210,220));
    sg->StrokePath();
    sg->SetLineWidth(1.7);
    aarend->SetColor(RGBX(200,0,0));
    sg->StrokePath();

    // Outline control polygon
    aarend->SetColor(RGBX(0,0,0));
    sg->SetLineWidth(1.4);
    sg->BeginPath();
    sg->Move(xy[3].x, xy[3].y);
    sg->Line(xy[2].x, xy[2].y);
    sg->Line(xy[4].x, xy[4].y);
    sg->Move(xy[5].x, xy[5].y);
    sg->PolyLine(12, &xy[6]);
    sg->StrokePath();

    // Highlight vertexes of control polygon
    SGPoint pt[] = { { 2<<16, 0 }, { 0, 2<<16 } };
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
    SGPoint p0 = {    250,    392 };
    SGPoint p1 = {    250, 15+392 };
    SGPoint p2 = { 15+250,    392 };
    char electron[] = { 1, 100, 0 };
    float astart = PI/4;
    sg->SetFixedBits(0);
    sg->BeginPath();
    sg->Ellipse(p0, p1, p2);  // nucleus
    aarend->SetColor(RGBX(0,80,120));
    sg->FillPath(FILLRULE_EVENODD);
    sg->SetLineEnd(LINEEND_ROUND);
    for (int i = 0; i < 3; ++i)
    {
        float angle = i*PI/3;
        float sina = sin(i*PI/3);
        float cosa = cos(i*PI/3);
        sg->BeginPath();
        sg->SetLineWidth(5.0);
        sg->SetLineDash(0,0,0);
        p1 = p2 = p0;
        p1.x -= 200*sina;
        p1.y += 200*cosa;
        p2.x +=  50*cosa;
        p2.y +=  50*sina;
        sg->Ellipse(p0, p1, p2);  // orbit
        sg->StrokePath();
        sg->BeginPath();
        sg->EllipticArc(p0, p1, p2, astart, PI/16);
        astart += PI;
        sg->SetLineWidth(15.0);
        sg->SetLineDash(electron, 0, 1.0);
        sg->StrokePath();
    }
}

// Demo screen 10: Elliptic arcs
void demo10(SimpleRenderer *rend, SimpleRenderer *aarend, const SGRect& clip)
{
    SGPtr sg(rend, clip);
    TextApp txt;
    COLOR crBkgd = RGBX(222,222,255);
    COLOR crText = RGBX(40,70,110);

    // Use the basic renderer to do a background fill
    SGPoint corner = { 40, 40 };
    SGRect frame = { 10, 10, DEMO_WIDTH-20, DEMO_HEIGHT-20 };
    sg->BeginPath();
    sg->RoundedRectangle(frame, corner);
    rend->SetColor(crBkgd);
    sg->FillPath(FILLRULE_EVENODD);

    // Now switch to the antialiasing renderer
    sg->SetRenderer(aarend);

    // Draw the outer frame
    sg->SetLineWidth(8.0);
    aarend->SetColor(RGBX(222,100,50)); 
    sg->StrokePath();

    // Draw the title text
    sg->SetLineWidth(6.0);
    char *str = "Elliptic Arcs";
    float scale = 0.73;
    float width = txt.GetTextWidth(scale, str);
    SGPoint xystart;
    xystart.x = (DEMO_WIDTH - width)/2;
    xystart.y = 170;
    aarend->SetColor(crText);
    txt.DisplayText(&(*sg), xystart, scale, str);

    // Define the data values for the pie chart
    float percent[] = {
        5.1, 12.5, 14.8, 5.2, 11.6, 8.7, 14.7, 19.2, 8.2
    };
    COLOR crSlice[] = 
    {
        RGBX( 65,105,225), RGBX(255, 99, 71), RGBX(218,165, 32),     
        RGBX( 60,179,113), RGBX(199, 21,133), RGBX( 72,209,204),
        RGBX(255,105,180), RGBX( 50,205, 50), RGBX(128,  0,  0),
    };
    COLOR crRim[ARRAY_LEN(crSlice)];
    COLOR crEdge = RGBX(51,51,51);
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

    // Create darker shading for rims of pie slices
    for (int i = 0; i < ARRAY_LEN(crSlice); ++i)
        crRim[i] = BlendPixels(alpha, 0, crSlice[i]);

    // Draw all the pie slices
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
        // Switch to the basic renderer to do large area fills
        sg->SetRenderer(rend);

        // Paint the top of the pie slice
        sg->BeginPath();
        sg->EllipticArc(v[0], v[1], v[2], startAngle, sweepAngle);
        sg->GetCurrentPoint(&xy);
        sg->Line(v[0].x, v[0].y);
        sg->CloseFigure();
        rend->SetColor(crSlice[i]);
        sg->FillPath(FILLRULE_EVENODD);

        // Switch to antialiasing renderer to outline edges
        sg->SetRenderer(aarend);
        aarend->SetColor(crEdge);
        sg->StrokePath();

        if (i == 1 || i == 2)
        {
            // Switch to basic renderer to do large area fills
            sg->SetRenderer(rend);

            // Paint the filling in the pulled-out pie slice
            COLOR crFill = BlendPixels(alpha/2, 0, crSlice[i]);
            sg->BeginPath();
            sg->Move(xy.x, xy.y);
            sg->Line(xy.x, xy.y + high);
            sg->Line(v[3].x, v[3].y);
            sg->Line(v[0].x, v[0].y);
            sg->CloseFigure();
            rend->SetColor(crFill);
            sg->FillPath(FILLRULE_EVENODD);

            // Switch to antialiasing renderer to outline edges
            sg->SetRenderer(aarend);
            aarend->SetColor(crEdge);
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
            // Switch to basic renderer to do large area fills
            sg->SetRenderer(rend);

            // Rim of pie slice is visible, so paint it
            sg->BeginPath();
            sg->EllipticArc(v[0], v[1], v[2], astart+asweep, -asweep);
            sg->GetCurrentPoint(&xy);
            sg->Line(xy.x, xy.y + high);
            sg->EllipticArc(v[3], v[4], v[5], astart, asweep);
            rend->SetColor(crRim[i]);
            sg->CloseFigure();
            sg->FillPath(FILLRULE_EVENODD);

            // Switch to antialiasing renderer to outline edges
            sg->SetRenderer(aarend);
            aarend->SetColor(crEdge);
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

// Demo screen 11: Layered stroke effects
void demo11(SimpleRenderer *rend, SimpleRenderer *aarend, const SGRect& clip)
{
    SGPtr sg(rend, clip);
    TextApp txt;
    COLOR crBkgd = RGBX(205,195,185);
    COLOR crFrame = RGBX(100,80,90);
    COLOR crText = RGBX(40,70,110);
    COLOR crBlack = RGBX(0,0,0);

    // Use basic renderer to do background color fill
    SGPoint corner = { 40, 40 };
    SGRect frame = { 10, 10, DEMO_WIDTH-20, DEMO_HEIGHT-20 };
    sg->BeginPath();
    sg->RoundedRectangle(frame, corner);
    rend->SetColor(crBkgd);
    sg->FillPath(FILLRULE_EVENODD);

    // Now switch to antialiasing renderer
    sg->SetRenderer(aarend);

    // Draw a frame around the window
    sg->SetLineWidth(8.0);
    aarend->SetColor(crFrame); 
    sg->StrokePath();

    // Draw the title text
    sg->SetLineWidth(6.0);
    char *str = "Layered Stroke Effects";
    float scale = 0.73;
    float width = txt.GetTextWidth(scale, str);
    SGPoint xystart;
    xystart.x = (DEMO_WIDTH - width)/2;
    xystart.y = 136;
    aarend->SetColor(crText);
    txt.DisplayText(&(*sg), xystart, scale, str);

    // Draw layered strokes in different colors
    SGPoint xy[] = {
        { 349, 640 }, { 545, 320 }, { 720, 620 }, { 625, 460 }, 
        { 450, 745 }, { 960, 745 }, { 755, 400 },
    };
    COLOR rgb[] = {
        crBlack, RGBX(160,220,250), crBlack, RGBX(160,250,160), 
        crBlack, RGBX(250,160,160), crBlack 
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
        aarend->SetColor(rgb[i]);
        sg->SetLineWidth(linewidth);
        sg->StrokePath();
        linewidth -= dw;
    }
}

// Demo screen 12: Composite paths
void demo12(SimpleRenderer *rend, SimpleRenderer *aarend, const SGRect& clip)
{
    SGPtr sg(rend, clip);
    TextApp txt;
    const float PI = 3.14159265;
    const float sin30 = sin(PI/6);
    const float cos30 = cos(PI/6);
    SGCoord x1 = 0, y1 = -190<<16;
    const SGPoint v0 = { 640<<16, 540<<16 };
    COLOR crBkgd = RGBX(170,170,170);
    COLOR crFrame = RGBX(140,150,180);
    COLOR crText = RGBX(40,70,110);

    // Use basic renderer to do background color fill
    SGPoint corner = { 40, 40 };
    SGRect frame = { 10, 10, DEMO_WIDTH-20, DEMO_HEIGHT-20 };
    sg->BeginPath();
    sg->RoundedRectangle(frame, corner);
    rend->SetColor(crBkgd);
    sg->FillPath(FILLRULE_EVENODD);

    // Now switch to the antialiasing renderer
    sg->SetRenderer(aarend);

    // Draw a frame around the window
    sg->SetLineWidth(8.0);
    aarend->SetColor(crFrame); 
    sg->StrokePath();

    // Draw the title text
    sg->SetLineWidth(6.0);
    char *str = "Composite Paths";
    float scale = 0.73;
    float width = txt.GetTextWidth(scale, str);
    SGPoint xystart;
    xystart.x = (DEMO_WIDTH - width)/2;
    xystart.y = 130;
    aarend->SetColor(crText);
    txt.DisplayText(&(*sg), xystart, scale, str);

    // Switch to the basic renderer to fill large areas
    sg->SetRenderer(rend);

    // Combine multiple ellipses into a single path
    sg->SetFixedBits(16);
    sg->SetLineWidth(8.0);
    sg->BeginPath();
    for (int i = 0; i < 12; ++i)
    {
        SGCoord x2 = 0.4*y1, y2 = -0.4*x1;
        SGPoint v1 = v0, v2 = v0;

        v1.x += x1;
        v1.y += y1;
        v2.x += x1 + x2;
        v2.y += y1 + y2;
        sg->Ellipse(v1, v0, v2);
        x2 =  x1*cos30 + y1*sin30;
        y1 = -x1*sin30 + y1*cos30;
        x1 = x2;
    }

    // Fill the ellipses in yellow
    rend->SetColor(RGBX(255,255,0));
    sg->FillPath(FILLRULE_EVENODD);

    // Switch to the antialiasing renderer
    sg->SetRenderer(aarend);

    // Stroke the edges of the ellipses in red
    aarend->SetColor(RGBX(255,0,0));
    sg->StrokePath();
}

// Demo screen 13: Alpha blending
void demo13(SimpleRenderer *rend, SimpleRenderer *aarend, const SGRect& clip)
{
    SGPtr sg(rend, clip);
    TextApp txt;
    COLOR crBkgd = RGBX(240,240,240);
    COLOR crFrame = RGBX(170,170,170);
    COLOR crText = RGBX(40,70,110);
    COLOR crBlack = RGBX(0,0,0);
    SGPoint xy[][6] = {
        { { 240,  80 }, { 560,  80 }, { 880,  80 }, { 1200,  80 }, { 1200, 400 }, { 1200, 720 } },
        { {  80, 240 }, {  80, 560 }, {  80, 880 }, {  400, 880 }, {  720, 880 }, { 1040, 880 } },
        { {  80, 720 }, {  80, 400 }, {  80,  80 }, {  400,  80 }, {  720,  80 }, { 1040,  80 } },
        { { 240, 880 }, { 560, 880 }, { 880, 880 }, { 1200, 880 }, { 1200, 560 }, { 1200, 240 } },
    };

    // Use basic renderer to do background color fill
    SGPoint corner = { 40, 40 };
    SGRect frame = { 10, 10, DEMO_WIDTH-20, DEMO_HEIGHT-20 };
    sg->BeginPath();
    sg->RoundedRectangle(frame, corner);
    rend->SetColor(crBkgd);
    sg->FillPath(FILLRULE_EVENODD);

    // Now switch to antialiasing renderer
    sg->SetRenderer(aarend);

    // Draw a frame around the window
    sg->SetLineWidth(8.0);
    aarend->SetColor(crFrame); 
    sg->StrokePath();

    // Draw the title text
    char *str[] = { "ALPHA", "BLENDING" };
    float width, scale = 2.2; 
    SGPoint xystart;
    sg->SetLineWidth(40.0);
    txt.SetTextSpacing(1.3);
    aarend->SetColor(RGBX(220,20,60));
    for (int i = 0; i < 2; ++i)
    {
        width = txt.GetTextWidth(scale, str[0]);
        xystart.x = (DEMO_WIDTH - width)/2;
        xystart.y = DEMO_HEIGHT/2 - 60;
        txt.DisplayText(&(*sg), xystart, scale, str[0]);
        width = txt.GetTextWidth(scale, str[1]);
        xystart.x = (DEMO_WIDTH - width)/2;
        xystart.y += 250;
        txt.DisplayText(&(*sg), xystart, scale, str[1]);
        sg->SetLineWidth(28.0);
        aarend->SetColor(RGBX(222,120,80));
    }

    // Stroke a set of crossed diagonal lines
    sg->SetLineWidth(100.0);
    sg->SetLineEnd(LINEEND_ROUND);
    for (int i = 0; i < 6; ++i)
    {   
        sg->BeginPath();
        sg->Move(xy[0][i].x, xy[0][i].y);
        sg->Line(xy[1][i].x, xy[1][i].y);
        aarend->SetColor(RGBA(0,100,200,100));
        sg->StrokePath();

        sg->BeginPath();
        sg->Move(xy[2][i].x, xy[2][i].y);
        sg->Line(xy[3][i].x, xy[3][i].y);
        aarend->SetColor(RGBA(100,200,0,100));
        sg->StrokePath();
    }
}

// Demo screen 14: Code examples
void demo14(SimpleRenderer *rend, SimpleRenderer *aarend, const SGRect& clip)
{
    SGPtr sg(rend, clip);
    TextApp txt;
    COLOR crBkgd  = RGBX(230,230,255);
    COLOR crFrame = RGBX( 65,105,225);
    COLOR crDash  = RGBX(140,190,255);
    COLOR crText  = RGBX( 65,105,225);
    COLOR crWhite = RGBX(255,255,255);
    float linewidth = 10.0;
    char dash[] = { 1, 4, 0 };

    // Use the basic renderer to do a background fill
    SGPoint corner = { 40, 40 };
    SGRect frame = { 10, 10, DEMO_WIDTH-20, DEMO_HEIGHT-20 };
    sg->SetLineEnd(LINEEND_ROUND);
    sg->BeginPath();
    sg->RoundedRectangle(frame, corner);
    rend->SetColor(crBkgd);
    sg->FillPath(FILLRULE_EVENODD);

    // Now switch to the antialiasing renderer
    sg->SetRenderer(aarend);

    // Draw the outer frame
    sg->SetLineWidth(linewidth);
    aarend->SetColor(crFrame); 
    sg->StrokePath();
    sg->SetLineDash(dash, 0, linewidth/4.0);
    sg->SetLineWidth(linewidth - 4.0);
    aarend->SetColor(crDash);
    sg->StrokePath();

    // Draw a frame around the title text
    SGRect frame2 = { 320, 165, 640, 130 };
    sg->BeginPath();
    sg->Rectangle(frame2);
    aarend->SetColor(crDash);
    sg->FillPath(FILLRULE_EVENODD);
    sg->SetLineJoin(LINEJOIN_ROUND);
    sg->SetLineWidth(14.0);
    aarend->SetColor(crFrame);
    sg->SetLineDash(0,0,0);
    sg->StrokePath();
    sg->SetLineWidth(8.0);
    aarend->SetColor(crWhite);
    sg->StrokePath();

    // Draw the title text
    txt.SetTextSpacing(1.3);
    char *str = "Code Examples";
    float scale = 0.73;
    float width = txt.GetTextWidth(scale, str);
    SGPoint xystart;
    xystart.x = (DEMO_WIDTH - width)/2;
    xystart.y = 245;
    sg->SetLineDash(0,0,0);
    sg->SetLineWidth(12.0);
    aarend->SetColor(crText);
    txt.DisplayText(&(*sg), xystart, scale, str);
    sg->SetLineWidth(6.0);
    aarend->SetColor(crWhite);
    txt.DisplayText(&(*sg), xystart, scale, str);

    // Write the text message
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
    aarend->SetColor(crText);
    sg->SetLineWidth(3.0);
    xystart.y = 400;
    width = txt.GetTextWidth(scale, s[0]);
    xystart.x = (DEMO_WIDTH - width)/2;
    for (int i = 0; i < ARRAY_LEN(s); ++i)
    {
        txt.DisplayText(&(*sg), xystart, scale, s[i]);
        xystart.y += 55;
    }
}

// Code example from UG topic "Creating a ShapeGen object"
void MySub(ShapeGen *sg, SGRect& rect)
{
    sg->BeginPath();
    sg->Rectangle(rect);
    sg->FillPath(FILLRULE_EVENODD);
}

void MyTest(SimpleRenderer *rend, SimpleRenderer *aarend, const SGRect& clip)
{
    SGPtr sg(rend, clip);
    SGRect rect = { 100, 80, 250, 160 };
     
    sg->BeginPath();
    sg->Rectangle(clip);
    rend->SetColor(RGBX(255,255,255));  // white
    sg->FillPath(FILLRULE_EVENODD);
     
    rend->SetColor(RGBX(0,120,255));  // blue
    MySub(&(*sg), rect);

    //-----  Label the output of this code example -----
    TextApp txt;
    COLOR crText = RGBX(40,70,110);
    char *str = "Output of code example from \"Creating a "
                "ShapeGen object\" topic in userdoc.pdf";
    float scale = 0.3;
    SGPoint xystart = { 24, 420 };
    txt.SetTextSpacing(1.1);
    sg->SetRenderer(aarend);
    aarend->SetColor(crText);
    sg->SetLineWidth(3.0);
    txt.DisplayText(&(*sg), xystart, scale, str);
}

// 1st code example from UG topic "Ellipses and elliptic arcs"
void EggRoll(SimpleRenderer *rend, SimpleRenderer *aarend, const SGRect& clip)
{
    SGPtr sg(aarend, clip);
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
        aarend->SetColor(RGBX(100,100,100));
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
        aarend->SetColor(RGBX(220,250,220));
        sg->FillPath(FILLRULE_EVENODD);
        sg->SetLineWidth(1.7);
        aarend->SetColor(RGBX(0,0,0));
        sg->StrokePath();

        // Draw a pair of red lines connecting the ellipse
        // center with the two conjugate diameter end points
        sg->SetLineWidth(3.0);
        sg->BeginPath();
        sg->Move(v1.x, v1.y);
        sg->Line(v0.x, v0.y);
        sg->Line(v2.x, v2.y);
        aarend->SetColor(RGBX(255,0,0));
        sg->StrokePath();
    }

    //-----  Label the output of this code example -----
    TextApp txt;
    COLOR crText = RGBX(40,70,110);
    char *str = "First screenshot from \"Ellipses and "
                "elliptic arcs\" topic in userdoc.pdf";
    float scale = 0.3;
    SGPoint xystart = { 24, 420 };
    txt.SetTextSpacing(1.1);
    sg->SetLineWidth(3.0);
    aarend->SetColor(crText);
    txt.DisplayText(&(*sg), xystart, scale, str);
}

// 2nd code example from UG topic "Ellipses and elliptic arcs"
void PieToss(SimpleRenderer *rend, SimpleRenderer *aarend, const SGRect& clip)
{
    SGPtr sg(aarend, clip);
    const float PI = 3.14159265;
    float percent[] = {
        5.1, 12.5, 14.8, 5.2, 11.6, 8.7, 15.3, 18.7
    };
    COLOR color[] = 
    {
        RGBX(240,160,160), RGBX(160,240,200), RGBX(200,160,240), RGBX(200,240,160),
        RGBX(240,160,200), RGBX(160,240,160), RGBX(240,200,160), RGBX(160,200,240), 
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
        // square, rectangle, or parallelogram. Draw it.
        xy[i][3].x = xy[i][0].x - xy[i][1].x + xy[i][2].x;
        xy[i][3].y = xy[i][0].y - xy[i][1].y + xy[i][2].y;
        sg->BeginPath();
        sg->Move(xy[i][0].x, xy[i][0].y);
        sg->PolyLine(3, &xy[i][1]);
        sg->CloseFigure();
        aarend->SetColor(RGBX(150,160,170));
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
            aarend->SetColor(color[j]);
            sg->FillPath(FILLRULE_EVENODD);
            aarend->SetColor(RGBX(80,80,80));
            sg->SetLineWidth(1.0);
            sg->StrokePath();
            astart += asweep;
        }
    }

    //-----  Label the output of this code example -----
    TextApp txt;
    COLOR crText = RGBX(40,70,110);
    char *str = "Second screenshot from \"Ellipses and "
                "elliptic arcs\" topic in userdoc.pdf";
    float scale = 0.3;
    SGPoint xystart = { 24, 420 };
    txt.SetTextSpacing(1.1);
    sg->SetLineWidth(3.0);
    aarend->SetColor(crText);
    txt.DisplayText(&(*sg), xystart, scale, str);
}

// Code example from ShapeGen::Bezier2 reference topic
void example01(SimpleRenderer *rend, SimpleRenderer *aarend, const SGRect& clip)
{
    SGPtr sg(aarend, clip);
    SGPoint v0 = { 100, 200 }, v1 = { 200, 75 }, v2 = { 230, 270 };

    // Draw quadratic Bezier spline in red
    aarend->SetColor(RGBX(255,120,100));
    sg->SetLineWidth(12.0);
    sg->BeginPath();
    sg->Move(v0.x, v0.y);
    sg->Bezier2(v1, v2);
    sg->StrokePath();

    // Outline control polygon in black
    aarend->SetColor(RGBX(0,0,0));
    sg->SetLineWidth(2.0);
    sg->BeginPath();
    sg->Move(v0.x, v0.y);
    sg->Line(v1.x, v1.y);
    sg->Line(v2.x, v2.y);
    sg->StrokePath();

    //-----  Label the output of this code example -----
    TextApp txt;
    COLOR crText = RGBX(40,70,110);
    char *str = "Output of code example from ShapeGen::"
                "Bezier2 reference topic in userdoc.pdf";
    SGPoint xystart = { 24, 420 };
    float scale = 0.3;
    txt.SetTextSpacing(1.1);
    sg->SetLineWidth(3.0);
    aarend->SetColor(crText);
    txt.DisplayText(&(*sg), xystart, scale, str);
}

// Code example from ShapeGen::Bezier3 reference topic
void example02(SimpleRenderer *rend, SimpleRenderer *aarend, const SGRect& clip)
{
    SGPtr sg(aarend, clip);
    SGPoint v0 = { 100, 200 }, v1 = { 150, 30 }, 
            v2 = { 260, 200 }, v3 = { 330, 30 };

    // Draw cubic Bezier spline in red
    aarend->SetColor(RGBX(255,120,100));
    sg->SetLineWidth(12.0);
    sg->BeginPath();
    sg->Move(v0.x, v0.y);
    sg->Bezier3(v1, v2, v3);
    sg->StrokePath();

    // Outline control polygon in black
    aarend->SetColor(RGBX(0,0,0));
    sg->SetLineWidth(2.0);
    sg->BeginPath();
    sg->Move(v0.x, v0.y);
    sg->Line(v1.x, v1.y);
    sg->Line(v2.x, v2.y);
    sg->Line(v3.x, v3.y);
    sg->StrokePath();

    //-----  Label the output of this code example -----
    TextApp txt;
    COLOR crText = RGBX(40,70,110);
    char *str = "Output of code example from ShapeGen::"
                "Bezier3 reference topic in userdoc.pdf";
    SGPoint xystart = { 24, 420 };
    float scale = 0.3;
    txt.SetTextSpacing(1.1);
    sg->SetLineWidth(3.0);
    aarend->SetColor(crText);
    txt.DisplayText(&(*sg), xystart, scale, str);
}

// Code example from ShapeGen::Ellipse reference topic
void example03(SimpleRenderer *rend, SimpleRenderer *aarend, const SGRect& clip)
{
    SGPtr sg(aarend, clip);
    SGPoint u[4] = { { 100, 275 }, { 100, 75 }, { 300, 75 }, };
    SGPoint du[3] = { { 244, 34 }, { 270, 74 }, { 308, -38 }, };
    SGPoint v0, v1, v2;

    sg->SetLineWidth(2.0);
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
        aarend->SetColor(RGBX(200,200,240));
        sg->FillPath(FILLRULE_EVENODD);
        sg->Move(v1.x, v1.y);
        sg->Line(v0.x, v0.y);
        sg->Line(v2.x, v2.y);
        aarend->SetColor(RGBX(0,0,0));
        sg->StrokePath();
        for (int j = 0; j < 3; ++j)
        {
            u[j].x += du[j].x;
            u[j].y += du[j].y;
        }
    }

    //-----  Label the output of this code example -----
    TextApp txt;
    COLOR crText = RGBX(40,70,110);
    char *str = "Output of code example from ShapeGen::"
                "Ellipse reference topic in userdoc.pdf";
    SGPoint xystart = { 24, 420 };
    float scale = 0.3;
    txt.SetTextSpacing(1.1);
    sg->SetLineWidth(3.0);
    aarend->SetColor(crText);
    txt.DisplayText(&(*sg), xystart, scale, str);
}

// Code example from ShapeGen::EllipticArc reference topic
void example04(SimpleRenderer *rend, SimpleRenderer *aarend, const SGRect& clip)
{
    SGPtr sg(aarend, clip);
    const float PI = 3.14159265;
    float percent[] = {
        5.1, 12.5, 14.8, 5.2, 11.6, 8.7, 15.3, 18.7
    };
    COLOR color[] = 
    {
        RGBX(240,200,200), RGBX(200,240,220), RGBX(220,200,240), RGBX(220,240,200),
        RGBX(240,200,220), RGBX(200,240,200), RGBX(240,220,200), RGBX(200,220,240), 
    };
    SGPoint v0[] = { { 200, 175 }, { 476, 173 } };
    SGPoint v1[] = { { 200,  75 }, { 489,  93 } };
    SGPoint v2[] = { { 300, 175 }, { 595, 117 } };

    sg->SetLineWidth(1.7);
    for (int i = 0; i < 2; ++i)
    {
        float astart = 0;

        for (int j = 0; j < 8; ++j)
        {
            float asweep = 2.0*PI*percent[j]/100.0;

            sg->BeginPath();
            sg->EllipticArc(v0[i], v1[i], v2[i], astart, asweep);
            sg->Line(v0[i].x, v0[i].y);
            aarend->SetColor(color[j]);
            sg->CloseFigure();
            sg->FillPath(FILLRULE_EVENODD);
            aarend->SetColor(RGBX(80, 80, 80));
            sg->StrokePath();
            astart += asweep;
        }
    }

    //-----  Label the output of this code example -----
    TextApp txt;
    COLOR crText = RGBX(40,70,110);
    char *str = "Output of code example from ShapeGen::"
                "EllipticArc reference topic in userdoc.pdf";
    SGPoint xystart = { 24, 420 };
    float scale = 0.3;
    txt.SetTextSpacing(1.1);
    sg->SetLineWidth(3.0);
    aarend->SetColor(crText);
    txt.DisplayText(&(*sg), xystart, scale, str);
}

// Code example from ShapeGen::EllipticSpline reference topic
void example05(SimpleRenderer *rend, SimpleRenderer *aarend, const SGRect& clip)
{
    SGPtr sg(aarend, clip);
    SGPoint v0 = { 100, 200 }, v1 = { 200, 75 }, v2 = { 230, 270 };

    // Draw elliptic spline in green
    aarend->SetColor(RGBX(40,220,80));
    sg->SetLineWidth(12.0);
    sg->BeginPath();
    sg->Move(v0.x, v0.y);
    sg->EllipticSpline(v1, v2);
    sg->StrokePath();

    // Outline control polygon in black
    aarend->SetColor(RGBX(0,0,0));
    sg->SetLineWidth(2.0);
    sg->BeginPath();
    sg->Move(v0.x, v0.y);
    sg->Line(v1.x, v1.y);
    sg->Line(v2.x, v2.y);
    sg->StrokePath();

    //-----  Label the output of this code example -----
    TextApp txt;
    COLOR crText = RGBX(40,70,110);
    char *str = "Output of code example from ShapeGen::"
                "EllipticSpline reference topic in userdoc.pdf";
    SGPoint xystart = { 24, 420 };
    float scale = 0.3;
    txt.SetTextSpacing(1.1);
    sg->SetLineWidth(3.0);
    aarend->SetColor(crText);
    txt.DisplayText(&(*sg), xystart, scale, str);
}

// Code example from ShapeGen::GetBoundingBox reference topic
void example06(SimpleRenderer *rend, SimpleRenderer *aarend, const SGRect& clip)
{
    SGPtr sg(aarend, clip);
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
    aarend->SetColor(RGBX(200,220,240));
    sg->FillPath(FILLRULE_EVENODD);

    // Get bounding box and outline it in black
    sg->GetBoundingBox(&bbox);
    sg->SetLineWidth(1.0);
    sg->SetLineJoin(LINEJOIN_MITER);
    sg->BeginPath();
    sg->Rectangle(bbox);
    aarend->SetColor(RGBX(0,0,0));
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
    aarend->SetColor(RGBX(200,220,240));
    sg->StrokePath();

    // Get the bounding box and outline it in black
    sg->GetBoundingBox(&bbox);
    sg->SetLineWidth(1.0);
    sg->SetLineJoin(LINEJOIN_MITER);
    sg->BeginPath();
    sg->Rectangle(bbox);
    aarend->SetColor(RGBX(0,0,0));
    sg->StrokePath();

    // Expand each side of the bounding box by half the line width
    bbox.x -= linewidth/2;
    bbox.y -= linewidth/2;
    bbox.w += linewidth;
    bbox.h += linewidth;

    // Outline the modified bounding box in red
    sg->BeginPath();
    sg->Rectangle(bbox);
    aarend->SetColor(RGBX(255,80,80));
    sg->StrokePath();

    //-----  Label the output of this code example -----
    TextApp txt;
    COLOR crText = RGBX(40,70,110);
    char *str = "Output of code example from ShapeGen::"
                "GetBoundingBox reference topic";
    SGPoint xystart = { 24, 420 };
    float scale = 0.3;
    txt.SetTextSpacing(1.1);
    sg->SetLineWidth(3.0);
    aarend->SetColor(crText);
    txt.DisplayText(&(*sg), xystart, scale, str);
}

// Code example from ShapeGen::PolyBezier2 reference topic
void example07(SimpleRenderer *rend, SimpleRenderer *aarend, const SGRect& clip)
{
    SGPtr sg(aarend, clip);
    SGPoint xy[] = {
        { 40, 140 }, { 70, 30 }, { 115, 120 }, { 160, 210 }, 
        { 195, 120 }, { 230, 30 }, { 274, 150 }
    };
    SGPoint v0, v1, v2;

    // Stroke three connected quadratic Bezier splines in green
    aarend->SetColor(RGBX(160,200,160));
    sg->SetLineWidth(14.0);
    sg->BeginPath();
    sg->Move(xy[0].x, xy[0].y);
    sg->PolyBezier2(6, &xy[1]);
    sg->StrokePath();

    // Outline spline skeleton in black; mark knots & control points
    aarend->SetColor(RGBX(60,60,60));
    sg->SetLineWidth(1.25);
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
    COLOR crText = RGBX(40,70,110);
    char *str = "Output of code example from ShapeGen::"
                "PolyBezier2 reference topic in userdoc.pdf";
    SGPoint xystart = { 24, 420 };
    float scale = 0.3;
    txt.SetTextSpacing(1.1);
    sg->SetLineWidth(3.0);
    aarend->SetColor(crText);
    txt.DisplayText(&(*sg), xystart, scale, str);
}

// Code example from ShapeGen::PolyBezier3 reference topic
void example08(SimpleRenderer *rend, SimpleRenderer *aarend, const SGRect& clip)
{
    SGPtr sg(aarend, clip);
    SGPoint xy[] = {
        { 30, 250 }, { 60, 120 }, { 130, 270 }, { 195, 145 }, { 260, 20 },
        { 140, 20 }, { 230, 135 }, { 320, 250 }, { 380, 190 }, { 380, 110 } 
    };
    SGPoint v0, v1, v2;
    int i;

    // Stroke three connected cubic Bezier splines in yellow
    aarend->SetColor(RGBX(230,200,80));
    sg->SetLineWidth(12.0);
    sg->BeginPath();
    sg->Move(xy[0].x, xy[0].y);
    sg->PolyBezier3(9, &xy[1]);
    sg->StrokePath();

    // Draw spline handles in black
    aarend->SetColor(RGBX(0,0,0));
    sg->SetLineWidth(1.25);
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
    COLOR crText = RGBX(40,70,110);
    char *str = "Output of code example from ShapeGen::"
                "PolyBezier3 reference topic in userdoc.pdf";
    SGPoint xystart = { 24, 420 };
    float scale = 0.3;
    txt.SetTextSpacing(1.1);
    sg->SetLineWidth(3.0);
    aarend->SetColor(crText);
    txt.DisplayText(&(*sg), xystart, scale, str);
}

// Code example from ShapeGen::PolyEllipticSpline reference topic
void example09(SimpleRenderer *rend, SimpleRenderer *aarend, const SGRect& clip)
{
    SGPtr sg(aarend, clip);
    SGPoint xy[] = {
        { 40, 140 }, { 70, 30 }, { 115, 120 }, { 160, 210 }, 
        { 195, 120 }, { 230, 30 }, { 274, 150 }
    };
    SGPoint v0, v1, v2;

    // Stroke three connected elliptic splines in blue
    aarend->SetColor(RGBX(180,180,230));
    sg->SetLineWidth(16.0);
    sg->BeginPath();
    sg->Move(xy[0].x, xy[0].y);
    sg->PolyEllipticSpline(6, &xy[1]);
    sg->StrokePath();

    // Outline spline skeleton in black; mark knots & control points
    aarend->SetColor(RGBX(0,0,0));
    sg->SetLineWidth(1.25);
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
    COLOR crText = RGBX(40,70,110);
    char *str = "Output of code example from ShapeGen::"
                "PolyEllipticSpline reference topic";
    SGPoint xystart = { 24, 420 };
    float scale = 0.3;
    txt.SetTextSpacing(1.1);
    sg->SetLineWidth(3.0);
    aarend->SetColor(crText);
    txt.DisplayText(&(*sg), xystart, scale, str);
}

// Code example from ShapeGen::Rectangle reference topic
void example10(SimpleRenderer *rend, SimpleRenderer *aarend, const SGRect& clip)
{
    SGPtr sg(rend, clip);
    SGRect rect = { 100, 75, 300, 225 };

    sg->BeginPath();
    sg->Rectangle(rect);

    // Make the second rectangle smaller than the first
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
    rend->SetColor(RGBX(200,200,240));
    sg->FillPath(FILLRULE_WINDING);  // <-- winding number fill rule
    sg->SetLineWidth(2.0);
    sg->SetLineJoin(LINEJOIN_MITER);
    rend->SetColor(RGBX(0,0,0));
    sg->StrokePath();

    //-----  Label the output of this code example -----
    TextApp txt;
    COLOR crText = RGBX(40,70,110);
    char *str = "Output of code example from ShapeGen::"
                "Rectangle reference topic in userdoc.pdf";
    SGPoint xystart = { 24, 420 };
    float scale = 0.3;
    txt.SetTextSpacing(1.1);
    sg->SetRenderer(aarend);
    aarend->SetColor(crText);
    sg->SetLineWidth(3.0);
    txt.DisplayText(&(*sg), xystart, scale, str);
}

// Code example from ShapeGen::RoundedRectangle reference topic
void example11(SimpleRenderer *rend, SimpleRenderer *aarend, const SGRect& clip)
{
    SGPtr sg(rend, clip);
    SGRect rect = { 100, 75, 300, 225 };
    SGPoint round = { 30, 30 };

    // Construct outer rounded rectangle 
    sg->BeginPath();
    sg->RoundedRectangle(rect, round);

    // Make the second rectangle smaller than the first
    rect.x += 15;
    rect.y += 15;
    rect.w -= 95;
    rect.h -= 75;
    round.x = round.y -= 10;

    // Modify the parameters so that construction
    // of the inner rounded rectangle proceeds
    // in the CCW direction
    rect.y += rect.h;
    rect.h = -rect.h; 
    round.y = -round.y;
    sg->RoundedRectangle(rect, round);
    rend->SetColor(RGBX(200,200,240));
    sg->FillPath(FILLRULE_WINDING);  // <-- winding number fill rule

    // Switch to antialiasing renderer and stroke boundaries
    sg->SetRenderer(aarend);
    aarend->SetColor(RGBX(80,80,80));
    sg->SetLineWidth(2.0);
    sg->StrokePath();

    //-----  Label the output of this code example -----
    TextApp txt;
    COLOR crText = RGBX(40,70,110);
    char *str = "Output of code example from ShapeGen::"
                "RoundedRectangle reference topic";
    SGPoint xystart = { 24, 420 };
    float scale = 0.3;
    txt.SetTextSpacing(1.1);
    sg->SetLineWidth(3.0);
    aarend->SetColor(crText);
    txt.DisplayText(&(*sg), xystart, scale, str);
}

// Code example from ShapeGen::SetClipRegion reference topic
void example12(SimpleRenderer *rend, SimpleRenderer *aarend, const SGRect& clip)
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
    rend->SetColor(RGBX(220,240,255));
    sg->FillPath(FILLRULE_EVENODD);

    // Switch to the antialiasing renderer
    sg->SetRenderer(aarend);

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
    aarend->SetColor(RGBX(238,50,50));
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
    COLOR crText = RGBX(40,70,110);
    char *str = "Output of code example from ShapeGen::"
                "SetClipRegion reference topic in userdoc.pdf";
    SGPoint xystart = { 24, 420 };
    float scale = 0.3;
    txt.SetTextSpacing(1.1);
    sg->SetLineWidth(3.0);
    sg->ResetClipRegion();
    aarend->SetColor(crText);
    txt.DisplayText(&(*sg), xystart, scale, str);
}

// Code example from ShapeGen::SetLineDash reference topic
void example13(SimpleRenderer *rend, SimpleRenderer *aarend, const SGRect& clip)
{
    SGPtr sg(aarend, clip);
    const float PI = 3.14159265;
    SGPoint xy[] = {
        {  86, 193 }, {  86, 153 }, {  40, 193 }, { 152, 233 }, 
        { 152, 153 }, {  71,  92 }, { 117,  32 }
    };
    float linewidth = 7.0;
    char dot[] = { 2, 0 };
    char dash[] = { 5, 2, 0 };
    char dashdot[] = { 5, 2, 2, 2, 0 };
    char dashdotdot[] = { 5, 2, 2, 2, 2, 2, 0 };
    char *pattern[] = { dot, dash, dashdot, dashdotdot, 0 };

    aarend->SetColor(RGBX(205, 92, 92));
    sg->SetLineWidth(linewidth);
    for (int i = 0; i < 5; ++i)
    {
        sg->SetLineDash(pattern[i], 0, linewidth/2.0);
        sg->BeginPath();
        sg->EllipticArc(xy[0], xy[1], xy[2], 0, PI);
        sg->PolyBezier3(3, &xy[3]);
        sg->Line(xy[6].x, xy[6].y);
        sg->StrokePath();
        for (int j = 0; j < 7; ++j)
            xy[j].x += 150;
    }

    //-----  Label the output of this code example -----
    TextApp txt;
    COLOR crText = RGBX(40,70,110);
    char *str = "Output of code example from ShapeGen::"
                "SetLineDash reference topic in userdoc.pdf";
    SGPoint xystart = { 24, 420 };
    float scale = 0.3;
    txt.SetTextSpacing(1.1);
    sg->SetLineWidth(3.0);
    sg->SetLineDash(0,0,0);
    aarend->SetColor(crText);
    txt.DisplayText(&(*sg), xystart, scale, str);
}

// Code example from ShapeGen::SetLineEnd reference topic
void example14(SimpleRenderer *rend, SimpleRenderer *aarend, const SGRect& clip)
{
    SGPtr sg(aarend, clip);
    LINEEND cap[] = { LINEEND_FLAT, LINEEND_ROUND, LINEEND_SQUARE };
    SGPoint vert[] = { { 70, 240 }, { 170, 95 }, { 220, 270 } };

    for (int i = 0; i < 3; ++i)
    {
        sg->BeginPath();
        sg->Move(vert[0].x, vert[0].y);
        sg->PolyLine(2, &vert[1]);
        sg->SetLineWidth(40.0);
        sg->SetLineEnd(cap[i]);
        aarend->SetColor(RGBX(200,200,240));
        sg->StrokePath();
        sg->SetLineWidth(1.7);
        aarend->SetColor(RGBX(0,0,0));
        sg->StrokePath();
        for (int j = 0; j < 3; ++j)
            vert[j].x += 230;
    }

    //-----  Label the output of this code example -----
    TextApp txt;
    COLOR crText = RGBX(40,70,110);
    char *str = "Output of code example from ShapeGen::"
                "SetLineEnd reference topic in userdoc.pdf";
    SGPoint xystart = { 24, 420 };
    float scale = 0.3;
    txt.SetTextSpacing(1.1);
    sg->SetLineWidth(3.0);
    aarend->SetColor(crText);
    txt.DisplayText(&(*sg), xystart, scale, str);
}

// Code example from ShapeGen::SetLineJoin reference topic
void example15(SimpleRenderer *rend, SimpleRenderer *aarend, const SGRect& clip)
{
    SGPtr sg(aarend, clip);
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
        aarend->SetColor(RGBX(200,200,240));
        sg->StrokePath();
        sg->SetLineWidth(1.7);
        aarend->SetColor(RGBX(0,0,0));
        sg->StrokePath();
        for (int j = 0; j < 3; ++j)
            vert[j].x += 230;
    }

    //-----  Label the output of this code example -----
    TextApp txt;
    COLOR crText = RGBX(40,70,110);
    char *str = "Output of code example from ShapeGen::"
                "SetLineJoin reference topic in userdoc.pdf";
    SGPoint xystart = { 24, 420 };
    float scale = 0.3;
    txt.SetTextSpacing(1.1);
    sg->SetLineWidth(3.0);
    aarend->SetColor(crText);
    txt.DisplayText(&(*sg), xystart, scale, str);
}

// Code example from ShapeGen::SetMaskRegion reference topic
void example16(SimpleRenderer *rend, SimpleRenderer *aarend, const SGRect& clip)
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
    rend->SetColor(RGBX(220,240,255));
    sg->FillPath(FILLRULE_EVENODD);

    // Switch to antialiasing renderer
    sg->SetRenderer(aarend);

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
    aarend->SetColor(RGBX(234,50,50));
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
    COLOR crText = RGBX(40,70,110);
    char *str = "Output of code example from ShapeGen::"
                "SetMaskRegion reference topic";
    SGPoint xystart = { 24, 420 };
    float scale = 0.3;
    txt.SetTextSpacing(1.1);
    sg->SetLineWidth(3.0);
    aarend->SetColor(crText);
    txt.DisplayText(&(*sg), xystart, scale, str);
}

// Code example from ShapeGen::SetMiterLimit reference topic
void example17(SimpleRenderer *rend, SimpleRenderer *aarend, const SGRect& clip)
{
    SGPtr sg(aarend, clip);
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
        aarend->SetColor(RGBX(200, 200, 240));
        sg->StrokePath();
        sg->SetLineWidth(1.7);
        aarend->SetColor(RGBX(0,0,0));
        sg->StrokePath();
        sg->SetMiterLimit(1.4);
        for (int j = 0; j < 3; ++j)
            vert[j].x += 210;
    }

    //-----  Label the output of this code example -----
    TextApp txt;
    COLOR crText = RGBX(40,70,110);
    char *str = "Output of code example from ShapeGen::"
                "SetMiterLimit reference topic in userdoc.pdf";
    SGPoint xystart = { 24, 420 };
    float scale = 0.3;
    txt.SetTextSpacing(1.1);
    sg->SetLineWidth(3.0);
    aarend->SetColor(crText);
    txt.DisplayText(&(*sg), xystart, scale, str);
}

// Code example from ShapeGen::SetRenderer reference topic
void example18(SimpleRenderer *rend, SimpleRenderer *aarend, const SGRect& clip)
{
    SGPtr sg(rend, clip);
    SGRect rect = { 100, 110, 400, 240 };
    SGPoint v0 = { 360, 170 }, v1 = { 360+160, 170 }, v2 = { 360, 170+110 };

    // Use the basic renderer to fill a green rectangle
    sg->BeginPath();
    sg->Rectangle(rect);
    rend->SetColor(RGBX(0,200,0));  // green (opaque)
    sg->FillPath(FILLRULE_EVENODD);

    // Switch to the antialiased renderer
    sg->SetRenderer(aarend);

    // Alpha-blend a magenta, stroked ellipse over the rectangle
    sg->BeginPath();
    sg->Ellipse(v0, v1, v2);
    aarend->SetColor(RGBA(200,0,200,100));  // magenta + alpha
    sg->SetLineWidth(60.0);
    sg->StrokePath();

    //-----  Label the output of this code example -----
    TextApp txt;
    COLOR crText = RGBX(40,70,110);
    char *str = "Output of code example from ShapeGen::"
                "SetRenderer reference topic in userdoc.pdf";
    SGPoint xystart = { 24, 420 };
    float scale = 0.3;
    txt.SetTextSpacing(1.1);
    sg->SetLineWidth(3.0);
    aarend->SetColor(crText);
    txt.DisplayText(&(*sg), xystart, scale, str);
}

void (*testfunc[])(SimpleRenderer *rend, SimpleRenderer *aarend, const SGRect& cliprect) =
{
    // Demo screens
    demo01, demo02, demo03, demo04, 
    demo05, demo06, demo07, demo08, 
    demo09, demo10, demo11, demo12, 
    demo13, demo14,

    // Code examples from userdoc.pdf
    MyTest, EggRoll, PieToss,
    example01, example02, example03,
    example04, example05, example06,
    example07, example08, example09,
    example10, example11, example12,
    example13, example14, example15,
    example16, example17, example18, 
};

//---------------------------------------------------------------------
//
// Main program calls the runtest function to run the tests
//
//---------------------------------------------------------------------

bool runtest(int testnum, SimpleRenderer *rend, SimpleRenderer *aarend, const SGRect& cliprect)
{
    if (0 <= testnum && testnum < ARRAY_LEN(testfunc))
    {
        testfunc[testnum](rend, aarend, cliprect);
        return true;
    }
    return false;
}
