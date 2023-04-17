/*
  Copyright (C) 2019-2022 Jerry R. VanAken

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
// svgview.cpp:
//   This file contains the code for a simple SVG file viewer based on
//   the ShapeGen 2-D graphics library, and on Mikko Mononen's SVG
//   parser, which is implemented as a single header file, nanosvg.h.
//
//---------------------------------------------------------------------

#include <stdio.h>
#include <assert.h>
#define NANOSVG_IMPLEMENTATION
#define NANOSVG_ALL_COLOR_KEYWORDS
#include "nanosvg.h"
#include "demo.h"

namespace {
    //-------------------------------------------------------------------
    //
    // Prepares the paint to be used for a filled or stroked shape
    //
    //-------------------------------------------------------------------
    void PreparePaint(NSVGpaint *paint, float scale, EnhancedRenderer *aarend)
    {
        assert(paint->type != NSVG_PAINT_NONE);
        switch (paint->type)
        {
        case NSVG_PAINT_COLOR:
            aarend->SetColor(paint->color);
            break;
        case NSVG_PAINT_LINEAR_GRADIENT:
        case NSVG_PAINT_RADIAL_GRADIENT:
            {
                NSVGgradient* grad = paint->gradient;
                NSVGgradientStop* stop = grad->stops;
                SPREAD_METHOD spread;
                float xform[6];

                aarend->ResetColorStops();
                for (int i = 0; i < grad->nstops; ++i)
                {
                    aarend->AddColorStop(stop->offset, stop->color);
                    ++stop;
                }
                switch (grad->spread)
                {
                case NSVG_SPREAD_PAD:
                    spread = SPREAD_PAD;
                    break;
                case NSVG_SPREAD_REFLECT:
                    spread = SPREAD_REFLECT;
                    break;
                case NSVG_SPREAD_REPEAT:
                default:
                    spread = SPREAD_REPEAT;
                    break;
                }
                for (int i = 0; i < 6; ++i)
                    xform[i] = scale*grad->xform[i];

                aarend->SetTransform(xform);
                if (paint->type == NSVG_PAINT_LINEAR_GRADIENT)
                    aarend->SetLinearGradient(0,0, 0,1, spread,
                                              FLAG_EXTEND_START | FLAG_EXTEND_END);
                else
                    aarend->SetRadialGradient(grad->fx,grad->fy,grad->fr, 0,0,1, spread,
                                              FLAG_EXTEND_START | FLAG_EXTEND_END);
            }
            break;
        default:
            aarend->SetColor(RGBX(128,128,128));
            break;
        }
    }
}

//---------------------------------------------------------------------
//
// The main program calls this function to render an SVG file
//
//---------------------------------------------------------------------

int RunTest(int testnum, const PIXEL_BUFFER& bkbuf, const SGRect& clip)
{
    SGRect cliprect = clip;
    cliprect.w = (bkbuf.width < clip.w) ? bkbuf.width : clip.w;
    cliprect.h = (bkbuf.height < clip.h) ? bkbuf.height : clip.h;
    SmartPtr<SimpleRenderer> rend(CreateSimpleRenderer(&bkbuf));
    SmartPtr<EnhancedRenderer> aarend(CreateEnhancedRenderer(&bkbuf));
    SmartPtr<ShapeGen> sg(CreateShapeGen(&(*aarend), cliprect));
    NSVGimage* image;
    float scale, scale16;
    UserMessage umsg;

    if (_argc_ < 2)
    {
        umsg.ShowMessage("List one or more SVG filenames on command line, \n"
                         "separated by spaces. Press space key to step \n"
                         "through files in list.",
                         "SVG viewer - Usage info", MESSAGECODE_INFORMATION);
        return -1;
    }
    if (testnum < 0)
        testnum = _argc_ - 2;
    else if (testnum >= _argc_ - 1)
        testnum = 0;

    // Load SVG file, construct image data
    image = nsvgParseFromFile(_argv_[testnum + 1], "px", 96);
    if (image == 0 || image->width == 0 || image->height == 0)
    {
        // Send file error message to user
        char sbuf[256];
        sprintf(sbuf, "Unable to %s file \"%s\"\n",
                (image) ? "parse" : "open", _argv_[testnum + 1]);
        umsg.ShowMessage(sbuf, "SVG viewer - Error", MESSAGECODE_ERROR);
        sg->BeginPath();
        aarend->SetColor(RGBX(0,0,0));
        sg->Rectangle(cliprect);
        sg->FillPath(FILLRULE_EVENODD);
        return(_argc_ < 3) ? -1 : testnum;
    }

    // Calculate scaling for image
    if (image->hasViewport == 0 ||
        cliprect.w < image->width || cliprect.h < image->height)
    {
        // Either no viewport is defined in SVG file, or the viewport
        // has to be shrunk to display the full image in the window
        float xscale = (cliprect.w > 0) ? cliprect.w/image->width : 0;
        float yscale = (cliprect.h > 0) ? cliprect.h/image->height : 0;
        scale = (xscale < yscale) ? xscale : yscale;
    }
    else
        scale = 1;  // we'll honor the viewport defined in the SVG file

    scale16 = 65536*scale;  // to scale 16.16 fixed-point SGCoord values
    sg->SetFixedBits(16);

    // Render the image data
    for (NSVGshape *shape = image->shapes; shape != NULL; shape = shape->next)
    {
        // Construct the path -- push shape coordinates onto path stack
        sg->BeginPath();
        for (NSVGpath *path = shape->paths; path != NULL; path = path->next)
        {
            float* p = &path->pts[0];
            SGPoint v[4];

            // if primitive == cubic bezier, then...
            v[0].x = scale16*p[0], v[0].y = scale16*p[1];
            sg->Move(v[0].x, v[0].y);
            for (int i = 0; i < path->npts-1; i += 3)
            {
                p = &path->pts[i*2];
                v[1].x = scale16*p[2] + cliprect.x;
                v[1].y = scale16*p[3] + cliprect.y;
                v[2].x = scale16*p[4] + cliprect.x;
                v[2].y = scale16*p[5] + cliprect.y;
                v[3].x = scale16*p[6] + cliprect.x;
                v[3].y = scale16*p[7] + cliprect.y;
                sg->Bezier3(v[1], v[2], v[3]);
            }
            if (path->closed)
                sg->CloseFigure();
        }

        // If fill paint is specified, fill the path
        int alpha = shape->opacity*255.99;
        aarend->SetConstantAlpha(alpha);
        if (shape->fill.type != NSVG_PAINT_NONE)
        {
            bool bEvenOdd = (shape->fillRule == NSVG_FILLRULE_EVENODD);
            FILLRULE rule = (bEvenOdd) ? FILLRULE_EVENODD : FILLRULE_WINDING;

            PreparePaint(&shape->fill, scale, &(*aarend));
            sg->FillPath(rule);
        }

        // If stroke paint is specified, stroke the path
        if (shape->stroke.type != NSVG_PAINT_NONE)
        {
            LINEEND cap;
            LINEJOIN join;
            char dashArray[8+1];
            int dashCount = shape->strokeDashCount;

            sg->SetLineWidth(scale*shape->strokeWidth);
            switch (shape->strokeLineJoin)
            {
            case NSVG_JOIN_BEVEL:
                join = LINEJOIN_BEVEL;
                break;
            case NSVG_JOIN_ROUND:
                join = LINEJOIN_ROUND;
                break;
            case NSVG_JOIN_MITERCLIP:
                join = LINEJOIN_MITER;
                break;
            case NSVG_JOIN_MITER:
            default:
                join = LINEJOIN_SVG_MITER;
                sg->SetMiterLimit(shape->miterLimit);
                break;
            }
            sg->SetLineJoin(join);
            switch (shape->strokeLineCap)
            {
            case NSVG_CAP_ROUND:
                cap = LINEEND_ROUND;
                break;
            case NSVG_CAP_SQUARE:
                cap = LINEEND_SQUARE;
                break;
            case NSVG_CAP_BUTT:
            default:
                cap = LINEEND_FLAT;
                break;
            }
            sg->SetLineEnd(cap);
            if (dashCount != 0)
            {
                assert(dashCount <= 8);
                for (int i = 0; i < dashCount; ++i)
                    dashArray[i] = 10*shape->strokeDashArray[i];

                dashArray[dashCount] = 0;
                sg->SetLineDash(dashArray, shape->strokeDashOffset, scale/10);
            }
            else
                sg->SetLineDash(0,0,0);

            PreparePaint(&shape->stroke, scale, &(*aarend));
            sg->StrokePath();
        }
    }
    // Delete
    nsvgDelete(image);
    return testnum;
}
