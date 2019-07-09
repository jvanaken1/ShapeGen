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
//----------------------------------------------------------------------
//
// thinline.cpp:
//   Functions for stroking paths that have line width = 0. This is a
//   special-case value that signifies line segments that mimic
//   Bresenham lines.
//
//----------------------------------------------------------------------

#include <math.h>
#include "shapepri.h"

namespace {

    //------------------------------------------------------------------
    //
    // Returns the quadrant number (in range 0 to 3) for vector
    // (dx,dy). Quadrant boundaries are defined by +/- 45-degree
    // diagonals. Any pair of adjacent line segments that are both in
    // the same quadrant are naturally joined. Note that the
    // ThinJoinLines function is called only to join two adjacent
    // segments that are in different quadrants. 
    //
    //------------------------------------------------------------------

    int getquadrant(FIX16 dx, FIX16 dy)
    {
        int quad;

        if (abs(dx) > abs(dy))
        {
            quad = (dx < 0) ? 2 : 0;
        }
        else
        {
            quad = (dy < 0) ? 3 : 1;
        }
        return quad;
    }

    //------------------------------------------------------------------
    //
    // Half-width offsets for thin stroked line segments. The first
    // four array elements are the offsets for quadrants 0 to 3. The
    // quadrant boundaries, in this case, are 45-degree diagonals. The
    // fifth array element, {0,0}, is used for end points of segments
    // that need to be capped.
    //
    //------------------------------------------------------------------

    const VERT16 offset[] = {
        { 0, -FIX_HALF }, { FIX_HALF, 0 }, { 0, FIX_HALF }, { -FIX_HALF, 0 }, { 0, 0 }
    };
}

//----------------------------------------------------------------------
//
// Private function: Joins two thin-stroked line segments. Constructs a
// beveled join to connect the end point of the previous line segment
// to the start point of the new line segment. This function is called
// only if the new line segment and the previous line segment are in
// different quadrants. The two directed line segments meet at the x-y
// coordinates given by parameter vert. Parameters inquad and outquad are
// the quadrant numbers for the incoming and outgoing line segments,
// respectively.
//
//----------------------------------------------------------------------

void PathMgr::ThinJoinLines(const VERT16 *vert, int inquad, int outquad)
{
    VERT16 cdir = offset[outquad];  // offset vector for incoming line
    VERT16 pdir = offset[inquad];   // offset vector for outgoing line 
    VERT16 v1, v2, xy[4];

    v1 = v2 = *vert;
    if (((inquad | outquad) & 4) != 0)
    {
        // Special index value "4" indicates that we're capping a
        // stroked-line end point, not joining two line segments
        if (inquad == 4)
        {
            v1.x += cdir.y;
            v1.y -= cdir.x;
        }
        else
        {
            v2.x -= pdir.y;
            v2.y += pdir.x;
        }
    }

    xy[0].x = v1.x + pdir.x;
    xy[0].y = v1.y + pdir.y;

    xy[1].x = v2.x + cdir.x;
    xy[1].y = v2.y + cdir.y;

    xy[2].x = v2.x - cdir.x;
    xy[2].y = v2.y - cdir.y;

    xy[3].x = v1.x - pdir.x;
    xy[3].y = v1.y - pdir.y;

    _edge->AttachEdge(&xy[0], &xy[1]);
    _edge->AttachEdge(&xy[2], &xy[3]);
}

//----------------------------------------------------------------------
//
// Private function: Strokes the current path for the special case of a
// line width of zero, which specifies that the line segments in the
// path are to be drawn as thinly connected, Bresenham-style lines.
// Stroke attributes such as line caps, join style, and dashed-line
// pattern are ignored for this case. The following function is an
// implementation of Vaughan Pratt's method for drawing thin, stroked
// lines, as described in section 4.5 of his SIGGRAPH '85 paper on
// conic splines.
//
//----------------------------------------------------------------------

bool PathMgr::ThinStrokePath()
{
    FIGURE *fig = _figure;  // empty figure terminates path
    VERT16 *fpt = _fpoint;  // first point (has undefined value)
    int off;

    // Each while-loop iteration below processes one figure (aka
    // subpath), starting with the last figure in the path
    assert(_fpoint == reinterpret_cast<VERT16*>(_figure + 1));
    while ((off = fig->offset) != 0)
    {
        VERT16 *vs = &fpt[-2];    // last point in new figure
        VERT16 *ve = &fpt[-off];  // first point in new figure
        int nlines = off - 2;
        FIX16 dx, dy;
        int prevquad;

        assert(vs != ve);  // figure has at least 2 points
        fig = &fig[-off];  // header for new figure
        fpt = ve;          // remember first point
        if (fig->isclosed)
        {
            ++nlines;
            dx = vs[0].x - vs[-1].x;
            dy = vs[0].y - vs[-1].y;
            assert((dx | dy) != 0);
            prevquad = getquadrant(dx, dy);
        }
        else
        {
            prevquad = 4;  // special value: do caps at line ends
            vs = ve++;
        }

        // Each for-loop iteration constructs one line segment
        for (int i = 0; i < nlines; ++i)
        {
            dx = ve->x - vs->x;
            dy = ve->y - vs->y;
            assert((dx | dy) != 0);
            int quad = getquadrant(dx, dy);
            VERT16 dir = offset[quad];
            VERT16 xy[4];

            // Offset stroked edges 1/2 pixel from line segment
            xy[0].x = vs->x + dir.x;
            xy[0].y = vs->y + dir.y;
            xy[1].x = xy[0].x + dx;
            xy[1].y = xy[0].y + dy;
            xy[2].x = ve->x - dir.x;
            xy[2].y = ve->y - dir.y;
            xy[3].x = xy[2].x - dx;
            xy[3].y = xy[2].y - dy;
            _edge->AttachEdge(&xy[0], &xy[1]);
            _edge->AttachEdge(&xy[2], &xy[3]);

            if (quad != prevquad)
            {
                // Join two line segments that are in different quadrants
                ThinJoinLines(vs, prevquad, quad);
                prevquad = quad;
            }
            vs = ve++;
        }
        if (fig->isclosed == false)
            ThinJoinLines(vs, prevquad, 4);  // "4" means "cap this line end"
    }

    // Fill the stroked path and initialize current path to empty
    _edge->TranslateEdges(-_scroll.x, -_scroll.y);
    _edge->NormalizeEdges(FILLRULE_WINDING);
    _edge->ClipEdges(FILLRULE_INTERSECT);
    return _edge->FillEdgeList();
}

