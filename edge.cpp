/*
  Copyright (C) 2019-2023 Jerry R. VanAken

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
// edge.cpp:
//   Polygonal edge manager: Converts paths into lists of polygonal
//   edges. The output edge list is in normalized form, which means
//   that each successive pair of edges describes the left and right
//   edges of the same trapezoid. The trapezoids in this list are
//   non-overlapping, clipped, and ready to be filled by a renderer.
//
//---------------------------------------------------------------------

#include "shapepri.h"
#include <stdlib.h>

#define sign(x)   ((x)<0?-1:1)       // sign (plus or minus) of value

namespace {
    //---------------------------------------------------------------------
    //
    // A qsort comparison function used to sort a list of edges in order of
    // the y coordinates at their top (minimum-y) end. The items in this
    // list are sorted in order of their ascending ytop values.
    //
    //----------------------------------------------------------------------

    int ycomp(const void *key1, const void *key2)
    {
        EDGE *p = *(EDGE**)key1;
        EDGE *q = *(EDGE**)key2;

        return (p->ytop - q->ytop);
    }

    //---------------------------------------------------------------------
    //
    // A qsort comparison function used to sort a list of edges in order of
    // the x coordinates at their top (minimum-y) end. The items in this
    // list are sorted in order of their ascending xtop values. If two edges
    // have equal xtop values, these edges are sorted in order of ascending
    // dxdy values. Two edges that have matching xtop and dxdy values are
    // sorted in order of their descending dy values.
    //
    //----------------------------------------------------------------------

    int xcomp(const void *key1, const void *key2)
    {
        EDGE *p = *(EDGE**)key1;
        EDGE *q = *(EDGE**)key2;

        if (p->xtop == q->xtop)
        {
            if (p->dxdy != q->dxdy)
                return (p->dxdy - q->dxdy);

            return (q->dy - p->dy);  // sort coincident edges
        }
        return (p->xtop - q->xtop);
    }

    //---------------------------------------------------------------------
    //
    // Uses the qsort function in stdlib.h to sort items in a singly linked
    // EDGE list. Parameter plist points to the head of the list. Parameter
    // length is the number of items in the list. Parameter comp is the
    // comparison function. The sortlist function returns a pointer to the
    // head of the new, sorted list.
    //
    //---------------------------------------------------------------------

    EDGE* sortlist(EDGE *plist, int length, int (*comp)(const void *, const void *))
    {
        if (length < 2)
            return plist;

        int i, count = 0;
        EDGE **ptr = new EDGE*[length];  // pointer array for qsort

        assert(ptr);  // out of memory?
        for (EDGE *p = plist; p; p = p->next)
            ptr[count++] = p;

        //assert(count == length);
        qsort(ptr, count, sizeof(EDGE*), comp);   // stdlib.h function
        for (i = 1; i < count; ++i)
            ptr[i-1]->next = ptr[i];  // update links in linked list

        ptr[i-1]->next = 0;
        EDGE *tmp = ptr[0];
        delete[] ptr;
        return tmp;
    }

    //---------------------------------------------------------------------
    //
    // Merges two pre-sorted, singly linked lists of edges. Each of the
    // two input lists has previously been sorted in ascending-y order.
    // The output list maintains this ordering.
    //
    //---------------------------------------------------------------------

    EDGE* mergelists(EDGE* list1, EDGE* list2)
    {
        EDGE *swap, *head = 0, **tail = &head;

        if (!list1)
            return list2;

        if (!list2)
            return list1;

        if (list1->ytop > list2->ytop)
            swap = list1, list1 = list2, list2 = swap;

        for (;;)
        {
            EDGE *p, *q;

            *tail = p = list1;
            do {
                q = p;
                p = p->next;
            } while (p && p->ytop <= list2->ytop);

            if (!p)
            {
                q->next = list2;
                break;
            }

            tail = &(q->next);
            list1 = list2;
            list2 = p;
        }
        return head;
    }
}  // end namespace

//---------------------------------------------------------------------
//
// EDGE allocation pool functions
//
//---------------------------------------------------------------------

POOL::POOL(int len) : blklen(len), watermark(0), index(0), count(0)
{
    block = new EDGE[blklen];
    assert(block);  // out of memory?
    memset(inventory, 0, ARRAY_LEN(inventory)*sizeof(EDGE*));
}

POOL::~POOL()
{
    delete[] block;
    for (int i = 0; i < index; ++i)
        delete[] inventory[i];
}

// Public function: Allocates an EDGE structure
EDGE* POOL::Allocate(EDGE *p)
{
    if (watermark == blklen)  // is this block exhausted?
        AcquireBlock();  // yes, acquire more pool memory

    EDGE *q = &block[watermark++];
    if (p)
    {
        *q = *p;  // copy contents of EDGE structure
        q->next = 0;
    }
    return q;
}

// Private function: Acquires more storage when pool is exhausted
void POOL::AcquireBlock()
{
    // The current block is 100 percent allocated. Acquire a new block.
    assert(index < ARRAY_LEN(inventory));  // array overflow?
    inventory[index++] = block;
    count += blklen;
    blklen += blklen;
    block = new EDGE[blklen];
    assert(block != 0);  // out of memory?
    watermark = 0;
}

// Public function: Resets the pool by freeing all currently
// allocated EDGE structures
void POOL::Reset()
{
    //
    if (index)
    {
        EDGE *swap = block;  block = inventory[0];  inventory[0] = swap;
        for (int i = 0; i < index; ++i)
        {
            delete[] inventory[i];
            inventory[i] = 0;
            blklen = blklen/2;
        }
        index = count = 0;
    }
    watermark = 0;
}

//---------------------------------------------------------------------
//
// Shape feeder: Breaks a shape (stored as a normalized edge list) into
// smaller pieces to feed to a renderer
//
//---------------------------------------------------------------------

class Feeder : ShapeFeeder
{
    friend EdgeMgr;

    EDGE *_list, *_edgeL, *_edgeR;
    FIX16 _xL, _xR, _dxL, _dxR;
    int _ytop, _height;

protected:
    Feeder() : _list(0), _edgeL(0), _edgeR(0), _ytop(0),
               _height(0), _xL(0), _xR(0), _dxL(0), _dxR(0)
    {
    }
    ~Feeder()
    {
    }
    void SetEdgeList(EDGE *list, int yshift)
    {
        if (yshift < 16)
            _list = list;  // antialiasing
        else
            _edgeL = list;  // no antialiasing
    }

public:
    bool GetNextGDIRect(SGRect *rect);
    bool GetNextSDLRect(SGRect *rect);
    bool GetNextSGSpan(SGSpan *span);
};

//---------------------------------------------------------------------
//
// Public function: Gets the next rectangle for the renderer to fill.
// The renderer iteratively calls this function to receive the shape
// stored in the normalized edge list as a series of rectangles. The
// function returns true to indicate that it has successfully
// retrieved a rectangle, or it returns false to indicate that
// no more rectangles are available (because the shape is complete).
// The x-y coordinates in each rectangle are integer values.
//
//---------------------------------------------------------------------

bool Feeder::GetNextSDLRect(SGRect *rect)
{
    // Do any more spans remain in current trapezoid?
    if (_height == 0)
    {
        // No more spans are left in the current trapezoid.
        // Do any more trapezoids remain in the edge list?
        if (_edgeL == 0)
            return false;  // no, the edge list is empty

        // Yes, get next trapezoid from edge list
        _edgeR = _edgeL->next;
        assert(_edgeR != 0);  // edges always come in L/R pairs
        _ytop = _edgeL->ytop;
        _height = _edgeL->dy;
        _xL = _edgeL->xtop;
        _dxL = _edgeL->dxdy;
        _xR = _edgeR->xtop;
        _dxR = _edgeR->dxdy;
        if (_dxL == 0 && _dxR == 0)
        {
            // Special case: an actual rectangle
            rect->x  = _xL >> 16;
            rect->w = (_xR >> 16) - rect->x;
            rect->y = _ytop;
            rect->h = _height;
            _height = 0;
            _edgeL = _edgeR->next;
            return true;
        }
    }

    // Send next span to renderer
    rect->x  = _xL >> 16;
    rect->w = (_xR >> 16) - rect->x;
    rect->y = _ytop;
    rect->h = 1;
    _xL += _dxL;
    _xR += _dxR;
    ++_ytop;
    if (--_height == 0)
        _edgeL = _edgeR->next;

    return true;
}

//---------------------------------------------------------------------
//
// This version fills in the members of the SGRect structure as though
// it was a Windows GDI RECT structure
//
//---------------------------------------------------------------------

bool Feeder::GetNextGDIRect(SGRect *rect)
{
    // Do any more spans remain in current trapezoid?
    if (_height == 0)
    {
        // No more spans are left in the current trapezoid.
        // Do any more trapezoids remain in the edge list?
        if (_edgeL == 0)
            return false;  // no, the edge list is empty

        // Yes, get next trapezoid from edge list
        _edgeR = _edgeL->next;
        assert(_edgeR != 0);  // edges always come in L/R pairs
        _ytop = _edgeL->ytop;
        _height = _edgeL->dy;
        _xL = _edgeL->xtop;
        _dxL = _edgeL->dxdy;
        _xR = _edgeR->xtop;
        _dxR = _edgeR->dxdy;
        if (_dxL == 0 && _dxR == 0)
        {
            // Special case: an actual rectangle
            rect->x = _xL >> 16;        // RECT.left
            rect->w = _xR >> 16;        // RECT.right
            rect->y = _ytop;            // RECT.top
            rect->h = _ytop + _height;  // RECT.bottom
            _height = 0;
            _edgeL = _edgeR->next;
            return true;
        }
    }

    // Send next span to renderer
    rect->x = _xL >> 16;  // RECT.left
    rect->w = _xR >> 16;  // RECT.right
    rect->y = _ytop;      // RECT.top
    rect->h = _ytop + 1;  // RECT.bottom
    _xL += _dxL;
    _xR += _dxR;
    ++_ytop;
    if (--_height == 0)
        _edgeL = _edgeR->next;

    return true;
}

//---------------------------------------------------------------------
//
// This version fills in the members of the SGSpan structure, which
// describes a subpixel span in an A-buffer. This function is called
// by a renderer that supports antialiasing.
//
//---------------------------------------------------------------------

bool Feeder::GetNextSGSpan(SGSpan *span)
{
    if (_list == 0 && _edgeL == 0)
        return false;

    // Are any more trapezoids left in the current scan line?
    if (_edgeL == 0)
    {
        // No, the next trapezoid starts on a new scan line
        int yscan = _list->ytop;
        EDGE *p = _list, *q = 0;

        do
        {
            q = p->next;
            p = q->next;
        } while (p != 0 && yscan == p->ytop);
        _edgeL = _list;
        _list = p;
        q->next = 0;
    }

    // Detach topmost span from the next trapezoid in this scan line
    _edgeR = _edgeL->next;
    span->xL = _edgeL->xtop;
    span->xR = _edgeR->xtop;
    span->y = _edgeL->ytop;

    // Are there more spans left in this trapezoid?
    if (_edgeL->dy > 1)
    {
        // Yes, update and save remainder of this trapezoid
        _edgeL->ytop = _edgeR->ytop += 1;
        _edgeL->dy -= 1;
        _edgeR->dy += 1;
        _edgeL->xtop += _edgeL->dxdy;
        _edgeR->xtop += _edgeR->dxdy;

        EDGE *tmp = _edgeL;
        _edgeL = _edgeR->next;
        _edgeR->next = _list;
        _list = tmp;
    }
    else
        _edgeL = _edgeR->next;  // discard empty trapezoid

    return true;
}

//---------------------------------------------------------------------
//
// Polygonal edge manager -- EdgeMgr constructor and destructor
//
//---------------------------------------------------------------------

EdgeMgr::EdgeMgr() : _renderer(0)
{
    _inpool = new POOL;
    _outpool = new POOL;
    _clippool = new POOL;
    _rendpool = new POOL;
    _savepool = new POOL;
    assert(_inpool != 0 && _outpool != 0 && _clippool != 0 &&
           _rendpool != 0 && _savepool != 0);  // out of memory?
}

EdgeMgr::~EdgeMgr()
{
    delete _inpool;
    delete _outpool;
    delete _clippool;
    delete _rendpool;
    delete _savepool;
}

//---------------------------------------------------------------------
//
// Protected function: Sets the Renderer object that the ShapeGen
// object will use to draw filled shapes on the display
//
//---------------------------------------------------------------------

bool EdgeMgr::SetRenderer(Renderer *renderer)
{
    if (renderer == 0)
    {
        assert(renderer);
        return false;
    }
    int yres = renderer->QueryYResolution();
    _yshift = 16 - yres;
    _ybias = FIX_BIAS >> yres;
    _yhalf = _ybias + 1;
    _renderer = renderer;
    return true;
}

//---------------------------------------------------------------------
//
// Protected function: Sets the clipping region to the normalized edge
// list in _outlist.head and discards the old clipping region. Returns true
// if the new clipping region is not empty; otherwise, returns false.
// (If the clipping region is empty, everything will get clipped, and
// nothing can be drawn.)
//
//---------------------------------------------------------------------

bool EdgeMgr::SetClipList()
{
    if (_outlist.head == 0)
    {
        _cliplist.head = 0;
        _clippool->Reset();
        return false;  // new clipping region is empty
    }
    _cliplist.head = _outlist.head;
    _outlist.head = 0;
    POOL *swap = _clippool; _clippool = _outpool; _outpool = swap;
    _outpool->Reset();
    return true;
}

//---------------------------------------------------------------------
//
// Protected function: Saves a copy of the current clipping region.
// This saved copy can later be restored by calling the SwapClipRegion
// function. If the current clipping region is not empty, the function
// returns true. Otherwise, it returns false.
//
//---------------------------------------------------------------------

bool EdgeMgr::SaveClipRegion()
{
    _savepool->Reset();
    if (_cliplist.head == 0)
    {
        _savelist.head = 0;
        return false;
    }

    // Make a copy of _cliplist
    _savelist.head = _savelist.tail = _savepool->Allocate(_cliplist.head);
    for (EDGE *p = _cliplist.head->next; p != 0; p = p->next)
        _savelist.tail = _savelist.tail->next = _savepool->Allocate(p);

    assert(_savelist.tail->next == 0);
    return true;
}

//---------------------------------------------------------------------
//
// Protected function: Swaps the current clipping region with the
// previously saved copy of a clipping region. Only one such copy
// exists at a time. This copy either was swapped out by an earlier
// SwapClipRegion call, or was previously saved by a SaveClipRegion
// call. The SwapClipRegion function returns true if the new
// clipping region is not empty. Otherwise, it returns false.
//
//---------------------------------------------------------------------

bool EdgeMgr::SwapClipRegion()
{
    EDGE *swap = _cliplist.head;  _cliplist.head = _savelist.head;  _savelist.head = swap;
    POOL *swap2 = _clippool;  _clippool = _savepool;  _savepool = swap2;
    return (_cliplist.head != 0);
}

//---------------------------------------------------------------------
//
// Protected function: Reverses the direction of each edge in the
// normalized edge list
//
//---------------------------------------------------------------------

void EdgeMgr::ReverseEdges()
{
    for (EDGE *p = _outlist.head; p; p = p->next)
        p->dy = -p->dy;
}

//---------------------------------------------------------------------
//
// Protected function: Translates the position of each edge in the
// input edge list by the specified x and y displacements
//
//---------------------------------------------------------------------

void EdgeMgr::TranslateEdges(int x, int y)
{
    x = x << 16;
    y = y << (16 - _yshift);

    for (EDGE *p = _inlist.head; p != 0; p = p->next)
    {
        p->xtop -= x;
        p->ytop -= y;
    }
}

//---------------------------------------------------------------------
//
// Protected function: Clips the newly created normalized edge list in
// _outlist to the current clipping region. On exit, _outlist contains
// the clipped edges.
//
//---------------------------------------------------------------------

void EdgeMgr::ClipEdges(FILLRULE fillrule)
{
    assert(_inlist.head == 0 && (_inpool->GetCount() == 0));
    assert(fillrule == FILLRULE_INTERSECT || fillrule == FILLRULE_EXCLUDE);

    // The output list may be empty if the path describes a shape
    // so tiny that it falls into a crack between pixels
    if (_outlist.head == 0)
        return;

    // An empty clip list means the clipping region has no interior,
    // so everything gets clipped and nothing gets drawn
    if (_cliplist.head == 0)
    {
        _outlist.head = 0;
        _outpool->Reset();
        return;
    }

    // Swap _inlist.head and _outlist.head (and their respective pools)
    _inlist.head = _outlist.head;
    _outlist.head = 0;
    POOL *swap = _inpool; _inpool = _outpool; _outpool = swap;

    // For a regular clipping operation (FILLRULE_INTERSECT), the part
    // of the clipping region that lies above the shape that's being
    // clipped has no effect and can be trivially rejected. When
    // masking off a shape from the clipping region (FILLRULE_EXCLUDE),
    // the clipping region above the shape can be copied directly to
    // the output list without first being processed by the
    // NormalizeEdges function.

    EDGELIST copylist;
    EDGE **p = &(_cliplist.head), **q = &(copylist.head);
    int yband = _inlist.head->ytop;  // y coord at top of shape

    while (*p && (*p)->ytop < yband)
    {
        int h = yband - (*p)->ytop;

        if (fillrule == FILLRULE_EXCLUDE)
        {
            // Copy clip region above shape to output list
            int dy = min(h, (*p)->dy);
            assert(dy > 0);
            SaveEdgePair(dy, *p, (*p)->next);
        }

        // Does this edge pair intrude into the shape's y space?
        // If so, discard the parts of the two edges that lie
        // above the shape, and copy just the intruding parts.
        if ((*p)->dy > h)
        {
            // Copy 1st member of edge pair and adjust its height
            *q = _inpool->Allocate(*p);
            (*q)->xtop += h*((*p)->dxdy);
            (*q)->ytop = yband;
            (*q)->dy -= h;  // for 1st edge, dy > 0
            q = &((*q)->next);
            p = &((*p)->next);  // pointer to 2nd member of edge pair

            // Copy 2nd member of edge pair and adjust its height
            *q = _inpool->Allocate(*p);
            (*q)->xtop += h*((*p)->dxdy);
            (*q)->ytop = yband;
            (*q)->dy += h;  // for 2nd edge, dy < 0
            q = &((*q)->next);
            p = &((*p)->next);  // pointer to 1st member of next pair
        }
        else
        {
            // Discard the entire edge pair
            p = &((*p)->next);  // pointer to 2nd member of edge pair
            p = &((*p)->next);  // pointer to 1st member of next pair
        }
    }

    // Copy the part of the clipping region that lies at or below
    // the y coordinate at the top of the shape that's being clipped
    while (*p != 0)
    {
        *q = _inpool->Allocate(*p);
        q = &((*q)->next);
        p = &((*p)->next);
    }
    assert(*q == 0);

    // Merge the two lists, which are pre-sorted in ascending-y order
    _inlist.head = mergelists(_inlist.head, copylist.head);

    // Get intersection of clip list with normalized edge list
    NormalizeEdges(fillrule);
    _inlist.head = 0;
    _inpool->Reset();
}

//---------------------------------------------------------------------
//
// Protected function: Invokes the renderer to fill all the trapezoids
// defined by the (clipped and normalized) edges in the _outlist.head
// list. Returns true if _outlist.head is not empty, in which case one
// or more trapezoids are filled. If _outlist.head is empty, the
// function does no fills and immediately returns false.
//
//---------------------------------------------------------------------

bool EdgeMgr::FillEdgeList()
{
    Feeder iter;
    int length;  // this is nonzero only if antialiasing is enabled

    if (_outlist.head == 0)
        return false;

    _rendlist.head = _outlist.head;
    POOL *swap = _outpool;  _outpool = _rendpool;  _rendpool = swap;
    _outlist.head = 0;
    _outpool->Reset();
    iter.SetEdgeList(_rendlist.head, _yshift);
    _renderer->RenderShape(&iter);
    return true;
}

//---------------------------------------------------------------------
//
// Protected function: Saves the next pair of mated edges to the
// normalized edge list pointed to by _outlist.head. Each pair of mated edges
// specifies a trapezoid. Parameter edgeL is a pointer to the edge that
// defines the left side of the trapezoid. Parameter edgeR is a pointer
// to the edge defining the right side.
//
//---------------------------------------------------------------------

void EdgeMgr::SaveEdgePair(int height, EDGE *edgeL, EDGE *edgeR)
{
    EDGE *p = _outpool->Allocate();
    EDGE *q = _outpool->Allocate();

    *p = *edgeL;
    p->dy = +height;  // positive so that winding number increments
    p->next = q;
    *q = *edgeR;
    q->dy = -height;  // negative so that winding number decrements
    q->next = 0;
    if (_outlist.head == 0)
        _outlist.head = p;
    else
        _outlist.tail->next = p;

    _outlist.tail = q;
}

//---------------------------------------------------------------------
//
// Protected function: Sets the clipping region to the dimensions of
// the device clipping rectangle. The input width and height values
// are both integers, and must be converted to the internal 16.16
// fixed-point format before the AttachEdge function calls.
//
//---------------------------------------------------------------------

void EdgeMgr::SetDeviceClipRectangle(int width, int height, bool bsave)
{
    assert(_inlist.head == 0 && _inpool->GetCount() == 0);
    if (!bsave)
    {
        // Discard any previously saved copy of the clipping region
        _savelist.head = 0;
        _savepool->Reset();
    }

    // Add left and right sides of rectangle to _inpool
    VERT16 v1 = { width<<16, height<<16 };
    VERT16 v2 = { width<<16, 0 };
    AttachEdge(&v1, &v2);
    v1.x = v2.x = 0;
    AttachEdge(&v2, &v1);  // <-- note reverse ordering

    // Swap _inpool with _clippool, and reset _inpool
    _cliplist.head = _inlist.head;
    _inlist.head = 0;
    POOL *swap = _clippool; _clippool = _inpool; _inpool = swap;
    _inpool->Reset();
}

//---------------------------------------------------------------------
//
// Protected function: Partition a complex polygonal shape (consisting
// of perhaps multiple closed figures) into a list of nonoverlapping
// trapezoids that are ready to be filled. The boundaries of each
// trapezoid are determined according to the specified polygon fill
// rule. The polygon may contain holes, disjoint regions, and boundary
// self-intersections. The path manager is responsible for ensuring
// that all figures are closed, which means that the number of edges
// intersected by any scan line is always even, and never odd.
//
//----------------------------------------------------------------------

void EdgeMgr::NormalizeEdges(FILLRULE fillrule)
{
    int y, h, length, yscan, wind;
    FIX16 xdist, ddx;
    EDGE *p, *q, *ylist, *xlist, head;

    if (_inlist.head == 0)
        return;  // nothing to do here

    if (_cliplist.head == 0)
    {
        _inlist.head = 0;
        _inpool->Reset();
        return;  // nothing to do here
    }

    // When a path is initially converted to a list of edges for a
    // filled or stroked shape, the edges have not yet been sorted
    if (fillrule == FILLRULE_EVENODD || fillrule == FILLRULE_WINDING)
    {
        assert(_outlist.head == 0 && _outpool->GetCount() == 0);
        length = _inpool->GetCount();
        _inlist.head = sortlist(_inlist.head, length, ycomp);
    }

    // Partition the polygon into a list of non-overlapping trapezoids.
    // The trapezoids will be produced in major order from top (minimum
    // y) to bottom, and in minor order from left (minimum x) to right.
    // Each iteration of the while-loop below produces a band of one or
    // more trapezoids that all have the same ytop value.

    ylist = _inlist.head;
    xlist = 0;
    while (ylist != 0)
    {
        yscan = ylist->ytop;  // y coordinate at current scan line

        // Starting at the head of the y-sorted list, remove each edge
        // for which ytop == yscan. Set height h to the minimum height
        // of these edges. Form a new, x-sorted list from these edges.

        h = BIGVAL16;
        p = ylist;
        length = 0;
        do
        {
            h = min(h, abs(p->dy));
            q = p->next;    // number of edges is always even
            h = min(h, abs(q->dy));
            length += 2;
        } while ((p = q->next) != 0 && p->ytop == yscan);
        q->next = 0;
        xlist = sortlist(ylist, length, xcomp);
        ylist = p;

        // The x-sorted list contains a band of trapezoids of height h.
        // The number of edges in a band is always even. If the top of
        // the first edge in the y-sorted list intrudes into the band,
        // reduce the band's height h just enough to exclude this edge.

        if (ylist != 0)
            h = min(h, ylist->ytop - yscan);

        // If any pair of adjacent edges in the x-sorted list intersect,
        // decrease height h to exclude the point of intersection. Don't
        // bother checking further if h reaches its minimum value of 1.

        p = xlist;
        while ((q = p->next) != 0 && h > 1)
        {
            ddx = p->dxdy - q->dxdy;
            xdist = q->xtop - p->xtop;
            if (ddx > 0 && xdist < (h - 1)*ddx)
                h = 1 + xdist/ddx;

            p = q;
        }

        // Use the specified fill rule to identify the non-overlapping
        // trapezoids within the current band. These trapezoids will be
        // saved to an output list and later used for rendering. The
        // INTERSECT and EXCLUDE fill rules are used for clipping.

        p = xlist;
        switch (fillrule)
        {
        case FILLRULE_EVENODD:
            do
            {
                q = p->next;
                SaveEdgePair(h, p, q);
            } while ((p = q->next) != 0);
            break;
        case FILLRULE_WINDING:
            do
            {
                wind = sign(p->dy);
                q = p->next;
                while ((wind += sign(q->dy)) != 0)
                {
                    q = q->next;
                    wind += sign(q->dy);
                    q = q->next;
                }
                SaveEdgePair(h, p, q);
            } while ((p = q->next) != 0);
            break;
        case FILLRULE_INTERSECT:
        case FILLRULE_EXCLUDE:
            wind = (fillrule == FILLRULE_INTERSECT) ? -1 : 0;
            for (;;)
            {
                while (p != 0 && (wind += sign(p->dy)) != 1)
                    p = p->next;
                if (p == 0)
                    break;
                q = p->next;
                while (q != 0 && (wind += sign(q->dy)) != 0)
                    q = q->next;
                if (q == 0)
                    break;
                SaveEdgePair(h, p, q);
                p = q->next;
            }
            break;
        default:
            assert(0);
            break;
        }

        // The trapezoids in the current band were just saved to the
        // output list. Now, for each edge in the x-sorted list, cut
        // off and discard the portion of the edge that lies within
        // the current band. Save any of the remaining edges in the
        // x-sorted list that are of nonzero height.

        p = xlist;
        q = &head;
        yscan += h;
        do
        {
            p->dy -= (p->dy < 0) ? -h : h;
            if (p->dy != 0)
            {
                p->xtop += h*(p->dxdy);
                p->ytop = yscan;
                q->next = p;
                q = p;
            }
        } while ((p = p->next) != 0);

        // All edges remaining in the x-sorted list have the same ytop
        // value. Insert this list at the head of the y-sorted list.

        if (q != &head)
        {
            q->next = ylist;
            ylist = head.next;
        }
    }
    _inlist.head = 0;
    _inpool->Reset();
}

//---------------------------------------------------------------------
//
// Protected function: Converts a directed line segment (taken from a
// path) to a polygonal edge and adds the edge to the input edge list.
// Input parameters v1 and v2 specify the 16.16 fixed-point x-y coordi-
// nates of the line's start and end points, respectively.
//
//----------------------------------------------------------------------

void EdgeMgr::AttachEdge(const VERT16 *v1, const VERT16 *v2)
{
    int j = (v1->y + _ybias) >> _yshift;
    int k = (v2->y + _ybias) >> _yshift;
    int dy = k - j;

    // If edge is horizontal (j == k), discard it and return
    if (dy)
    {
        VERT16 vtop, vbot;
        int ymin;

        // Identify top and bottom vertices on current edge
        if (dy > 0)
        {
            vtop = *v1;
            vbot = *v2;
            ymin = j;
        }
        else
        {
            vtop = *v2;
            vbot = *v1;
            ymin = k;
        }

        // Prepare to snip off any small tip of the edge that lies
        // above the topmost scanline that intersects the edge
        float dx = vbot.x - vtop.x;
        float dxdy = dx/(vbot.y - vtop.y);
        FIX16 xgap = dxdy*((ymin << _yshift) + _yhalf - vtop.y);

        // Create new EDGE structure and insert at head of _inlist.head
        EDGE *p = _inpool->Allocate();
        p->ytop = ymin;
        p->dy = dy;  // sign of dy indicates edge up/down direction
        p->xtop = vtop.x + xgap + FIX_BIAS;
        p->dxdy = dxdy*(1 << _yshift);
        p->next = _inlist.head;
        _inlist.head = p;
    }
}
