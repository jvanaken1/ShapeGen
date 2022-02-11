/*
  Copyright (C) 2022 Jerry R. VanAken

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
// gradient.cpp:
//   Paint generator classes for linear and radial gradient fills
//
//---------------------------------------------------------------------

#include <math.h>
#include <string.h>
#include <assert.h>
#include "renderer.h"

// Color-stop array element
struct STOP_COLOR
{
    FIX16 offset;  // range 0..1 (fixed-point 0 to 0x0000ffff)
    COLOR ga;      // format = 0x00aa00gg
    COLOR rb;      // format = 0x00bb00rr
};

//---------------------------------------------------------------------
//
// ColorStops class -- The gradient color-stop array manager
//
//---------------------------------------------------------------------

class ColorStops
{
    STOP_COLOR _stop[STOPARRAY_MAXLEN+1];
    int _stopCount, _stopIndex;
    STOP_COLOR _tminStop, _tmaxStop;

    int SetColorStop(int index, FIX16 offset, COLOR color);

public:
    ColorStops()
    {
        ResetColorStops();
    }
    ~ColorStops() {}
    void ResetColorStops();
    bool AddColorStop(float offset, COLOR color);
    COLOR GetPadColor(int n, COLOR opacity);
    COLOR GetColorValue(FIX16 t, COLOR opacity);
};

// Loads a color-stop array element, identified by the index
// parameter, with the specified offset and 32-bit color value.
// The color value is nominally in RGBA format (that is,
// 0xaabbggrr), although some renderers might choose to quietly
// swap the red and blue fields. The function premultiplies the
// RGB fields of the color value by the alpha field. All
// subsequent pixel operations on the color table will assume
// that its color values are in premultiplied-alpha format.
int ColorStops::SetColorStop(int index, FIX16 offset, COLOR color)
{
    COLOR rb, ga, a = color >> 24;

    if (a != 0)
    {
        rb = color & 0x00ff00ff;
        ga = (color >> 8) & 0x00ff00ff;
        if (a != 255)
        {
            rb *= a;
            rb += 0x00800080;
            rb += (rb >> 8) & 0x00ff00ff;
            rb = (rb >> 8) & 0x00ff00ff;
            ga |= 0x00ff0000;
            ga *= a;
            ga += 0x00800080;
            ga += (ga >> 8) & 0x00ff00ff;
            ga = (ga >> 8) & 0x00ff00ff;
        }
    }
    else
        rb = ga = 0;

    _stop[index].ga = ga;
    _stop[index].rb = rb;
    _stop[index].offset = offset;

    // Force color-stop cache miss since color stops have changed
    _tminStop.offset = 0x0000ffff;
    _tmaxStop.offset = 0;
    return ++index;
}

// Deletes all existing color stops from color-stop array, but sets up
// a valid -- but temporary -- default array to use until a new set of
// color stops are defined by calls to the AddColorStop function.
void ColorStops::ResetColorStops()
{
    memset(&_stop[0], 0, sizeof(_stop));
    _stopIndex = 0;
    _stopCount = SetColorStop(1, 0x0000ffff, 0);
}

// Adds a color stop to the color-stop array. The problem is that
// this function can be called at any time to add a new color stop.
// But the color-stop array must always be in a state in which it's
// ready to handle a color look-up request; in particular, the first
// and last array elements must have respective offsets of 0 and 1.0
// (represented as fixed-point values 0 and 0x0000ffff). The solution
// is to have two distinct array indices: _stopIndex is used to track
// the caller's additions to the array, but _stopCount can point to
// an additional array element, if necessary, so that the offset in
// the final element is 1.0, as required for lookups.
bool ColorStops::AddColorStop(float offset, COLOR color)
{
    FIX16 offset2;

    // Prevent color-stop array from overflowing
    if (_stopIndex == ARRAY_LEN(_stop))
        return false;

    // Update _stopIndex to make room for user's new color stop
    offset2 = 0x0000ffff*offset;  // convert to fixed-point
    if (_stopIndex == 0)
    {
        _stopIndex = SetColorStop(0, 0, color);
        if (offset2 > 0)
        {
            if (offset2 > 0x0000ffff) offset = 0x0000ffff;
            _stopIndex = SetColorStop(1, offset2, color);
        }
    }
    else
    {
        if (offset2 > 0x0000ffff || (_stopIndex == ARRAY_LEN(_stop)-1))
            offset2 = 0x0000ffff;
        else if (offset2 < _stop[_stopIndex-1].offset)
            offset2 = _stop[_stopIndex-1].offset;

        _stopIndex = SetColorStop(_stopIndex, offset2, color);
    }

    // Make sure final color-stop element has offset of 1.0
    // (represented in fixed-point as 0x0000ffff).
    if (offset2 == 0x0000ffff)
        _stopCount = _stopIndex;
    else
        _stopCount = SetColorStop(_stopIndex, 0x0000ffff, color);

    return true;
}

// Returns the indicated pad color. This function is called when the
// SPREAD_PAD spread method is used. Parameter n indicates whether to
// retrieve the padding color for the inside of the starting circle
// (n < 0) or the outside of the ending circle (n > 0). The opacity
// parameter is an alpha value in the range 0 to 255. The color value
// is always in premultiplied-alpha format, so the function multiplies
// all four components (r,g,b,a) of this value by opacity.
COLOR ColorStops::GetPadColor(int n, COLOR opacity)
{
    STOP_COLOR& stop = (n < 0) ? _stop[0] : _stop[_stopCount-1];
    COLOR rb = stop.rb, ga = stop.ga;

    if (opacity == 255)
        return (ga << 8) | rb;

    rb *= opacity;
    rb += 0x00800080;
    rb += (rb >> 8) & 0x00ff00ff;
    rb = (rb >> 8) & 0x00ff00ff;
    ga *= opacity;
    ga += 0x00800080;
    ga += (ga >> 8) & 0x00ff00ff;
    ga &= 0xff00ff00;
    return ga | rb;
}

// Calculates the color of a pixel given (1) the color look-up
// parameter t at the pixel's center, and (2) the input opacity,
// which is an alpha value in the range 0 to 255. Parameter t
// is a fraction in the range 0 to 1.0 (or 0 to 0x0000ffff in
// fixed-point) that indicates where the pixel center falls
// within the repeating color-stop pattern. All four of the
// pixel's color components (r,g,b,a) by are then multiplied by
// 'opacity'. The two color stops that bracket the specified t
// value are cached in the hope that several subsequent pixels
// might re-use the cached color stops.
COLOR ColorStops::GetColorValue(FIX16 t, COLOR opacity)
{ 
    float width;
    FIX16 s1, s2;
    COLOR ga, rb, rb1, rb2, ga1, ga2;

    assert(0 <= t && t <= 0x0000ffff);
    if (t < _tminStop.offset || _tmaxStop.offset < t)
    {
        // A color-stop cache miss has occurred. Update the two
        // cached color stops to bracket the new t value.
        int k;
        for (k = 1; k < _stopCount; ++k)
        {
            if (t < _stop[k].offset)
                break;
        }
        _tminStop = _stop[k-1];
        _tmaxStop = _stop[k];
    }

    // Interpolate between the two cached color stops on either
    // side of t. The RGB components in the two stops have
    // previously been premultiplied by their alphas.
    ga1 = _tminStop.ga, ga2 = _tmaxStop.ga;
    if (!(ga1 | ga2))
        return 0;  // transparent pixel

    rb1 = _tminStop.rb, rb2 = _tmaxStop.rb;
    width = _tmaxStop.offset - _tminStop.offset;
    s2 = 0x0000ffff*((t - _tminStop.offset)/width);     
    s2 >>= 8;
    s1 = 255 ^ s2;
    rb = s1*rb1 + s2*rb2;
    rb += 0x00800080;
    rb += (rb >> 8) & 0x00ff00ff;
    rb = (rb >> 8) & 0x00ff00ff;
    ga = s1*ga1 + s2*ga2;
    ga += 0x00800080;
    ga += (ga >> 8) & 0x00ff00ff;
    ga &= 0xff00ff00;
    if (opacity == 255)
        return ga | rb;
    
    // Multiply all four components (r,g,b,a) by opacity
    rb *= opacity;
    rb += 0x00800080;
    rb += (rb >> 8) & 0x00ff00ff;
    rb = (rb >> 8) & 0x00ff00ff;
    ga = opacity*(ga >> 8);
    ga += 0x00800080;
    ga += (ga >> 8) & 0x00ff00ff;
    ga &= 0xff00ff00;
    return ga | rb;
}

//---------------------------------------------------------------------
//
// Linear class -- Paint generator for linear gradient fills
//
//---------------------------------------------------------------------

class Linear : public LinearGradient
{
    ColorStops *_cstops;    // color-stop manager object
    float _x0, _y0;         // starting point (t = 0)
    float _x1, _y1;         // ending point (t = 1)
    SPREAD_METHOD _spread;  // pad, repeat, or reflect
    bool _bExtStart;        // extend past starting point (t < 0)
    bool _bExtEnd;          // extend past ending point (t >= 1)
    COLOR _startColor;      // starting color if SPREAD_PAD
    COLOR _endColor;        // ending color if SPREAD_PAD
    int _xscroll, _yscroll; // scroll position coordinates
    bool _bSpecial;         // special case x0==x1 and y0==y1

    // These values are constant over the lifetime of the object
    float _dtdx;  // partial derivative dt/dx
    float _dtdy;  // partial derivative dt/dy

public:
    Linear() { assert(0); }
    Linear(float x0, float y0, float x1, float y1,
           SPREAD_METHOD spread, int flags, const float xform[]);
    ~Linear() {}
    void FillSpan(int xs, int ys, int length, COLOR outBuf[], const COLOR inAlpha[]);
    bool AddColorStop(float offset, COLOR color)
    {
        return _cstops->AddColorStop(offset, color);
    }
    bool SetScrollPosition(int x, int y)
    {
        _xscroll = x, _yscroll = y;
        return true;
    }
};

// Constructor: Defines a new linear-gradient fill pattern
Linear::Linear(float x0, float y0, float x1, float y1,
               SPREAD_METHOD spread, int flags, const float xform[])
               : _x0(x0), _y0(y0), _x1(x1), _y1(y1),
                 _xscroll(0), _yscroll(0)
{
    _cstops = new ColorStops();
    assert(_cstops);
    _bExtStart = (flags & FLAG_EXTEND_START) != 0;
    _bExtEnd = (flags & FLAG_EXTEND_END) != 0;
    switch (spread)
    {
    case SPREAD_PAD:
    case SPREAD_REFLECT:
        _spread = spread;
        break;
    default:
        _spread = SPREAD_REPEAT;
        break;
    }
    _bSpecial = (_x0 == _x1) && (_y0 == _y1);
    if (_bSpecial)
        return;

    if (xform != 0)
    {
        // Apply the user-specified affine transformation matrix to
        // start point (x0,y0) and end point (x1,y1)
        float tmp;

        tmp = xform[0]*_x0 + xform[2]*_y0 + xform[4];
        _y0 = xform[1]*_x0 + xform[3]*_y0 + xform[5];
        _x0 = tmp;
        tmp = xform[0]*_x1 + xform[2]*_y1 + xform[4];
        _y1 = xform[1]*_x1 + xform[3]*_y1 + xform[5];
        _x1 = tmp;
    }

    // Calculation of the gradient pattern will be simplified by
    // placing the origin at (x0,y0)
    _x1 -= _x0, _y1 -= _y0;
    _x0 -= 0.5, _y0 -= 0.5;

    // Calculate partial derivatives dt/dx and dt/dy
    float dist2 = _x1*_x1 + _y1*_y1;

    _dtdx = _x1/dist2;
    _dtdy = _y1/dist2;
}

// Public function: Fills the pixels in a single horizontal span with
// a linear gradient pattern. The span starts at pixel (xs,ys) and
// extends 'len' pixels to the right. The function writes the processed
// gradient-fill pixels to the outBuf array. To support shape anti-
// aliasing and source constant alpha, the inAlpha array contains
// 8-bit alpha values to apply to the gradient-fill pixels (in addition
// to the per-pixel alphas in the gradient).
void Linear::FillSpan(int xs, int ys, int len, COLOR outBuf[], const COLOR inAlpha[])
{
    // Special case: x0 == x1 and y0 == y1
    if (_bSpecial)
    {
        if (_bExtEnd)
        {
            for (int i = 0; i < len; ++i)
            {
                COLOR opacity = (inAlpha == 0) ? 255 : inAlpha[i];
                COLOR color = _cstops->GetPadColor(1, opacity);

                outBuf[i] = color;
            }
        }
        return;
    }

    float xp = xs - _x0 + _xscroll;
    float yp = ys - _y0 + _yscroll;
    float t = xp*_dtdx + yp*_dtdy;

    // Normal case: Each iteration of this for-loop paints one pixel
    for (int i = 0; i < len; ++i)
    {
        COLOR opacity = (inAlpha == 0) ? 255 : inAlpha[i];

        if (opacity != 0)
        {
            COLOR color = 0;
            bool bValid = ((_bExtStart || t >= 0) && (_bExtEnd || t < 1.0));

            if (bValid)
            {
                int n = t;

                if (t < 0) --n;
                if (_spread == SPREAD_PAD && n != 0)
                    color = _cstops->GetPadColor(n, opacity);
                else
                {
                    FIX16 tfix = 0x0000ffff*(t - n);

                    if (_spread == SPREAD_REFLECT && (n & 1))
                        tfix ^= 0x0000ffff;
    
                    color = _cstops->GetColorValue(tfix, opacity);
                }
            }
            outBuf[i] = color;
        }
        t += _dtdx;
    }
}

// Called by a renderer to create a new linear-gradient object
LinearGradient* CreateLinearGradient(float x0, float y0, float x1, float y1,
                                     SPREAD_METHOD spread, int flags,
                                     const float xform[])
{
    Linear *lin = new Linear(x0, y0, x1, y1, spread, flags, xform);
    assert(lin != 0);

    return lin;
}

//---------------------------------------------------------------------
//
// Radial class -- Paint generator for radial gradient fills
//
//---------------------------------------------------------------------

class Radial : public RadialGradient
{
    ColorStops *_cstops;    // color-stop manager object
    float _x0, _y0, _r0;    // starting circle
    float _x1, _y1, _r1;    // ending circle
    SPREAD_METHOD _spread;  // pad, repeat, or reflect
    bool _bExtStart;        // extend starting circle to t < 0
    bool _bExtEnd;          // extend ending circle to t >= 1
    bool _bSpecial;         // special case (x0,y0,r0) = (x1,y1,r1)
    float _vx, _vy;         // y-scaling and x-shear
    COLOR _startColor;      // starting color if SPREAD_PAD
    COLOR _endColor;        // ending color if SPREAD_PAD
    int _xscroll, _yscroll; // scroll position coordinates

    // These values are constant over the lifetime of the object
    float _dr;     // dr = r1 - r0
    float _a;      // a = x1^2 + y1^2 - dr^2
    float _inva;   // inva = 1/a
    float _A2;     // A2 = dr^2 - y1^2

    void TransformRadialGradient(const float xform[]);

public:
    Radial() { assert(0); }
    Radial(float x0, float y0, float r0, float x1, float y1, float r1,
           SPREAD_METHOD spread, int flags, const float xform[]);
    ~Radial() {}
    void FillSpan(int xs, int ys, int length, COLOR outBuf[], const COLOR inAlpha[]);
    bool AddColorStop(float offset, COLOR color)
    {
        return _cstops->AddColorStop(offset, color);
    }
    bool SetScrollPosition(int x, int y)
    {
        _xscroll = x, _yscroll = y;
        return true;
    }
};

// Constructor: Defines a new radial-gradient fill pattern
Radial::Radial(float x0, float y0, float r0, float x1, float y1, float r1,
               SPREAD_METHOD spread, int flags, const float xform[])
               : _x0(x0), _y0(y0), _r0(r0), _x1(x1), _y1(y1), _r1(r1),
                 _xscroll(0), _yscroll(0), _vx(0), _vy(0)
{
    assert((r0 >= 0) && (r1 >= 0) && ((r0 != 0) || (r1 != 0)));
    _cstops = new ColorStops();
    assert(_cstops);
    _bExtStart = (flags & FLAG_EXTEND_START) != 0;
    _bExtEnd = (flags & FLAG_EXTEND_END) != 0;
    switch (spread)
    {
    case SPREAD_PAD:
    case SPREAD_REFLECT:
        _spread = spread;
        break;
    default:
        _spread = SPREAD_REPEAT;
        break;
    }
    _bSpecial = (x0 == x1) && (y0 == y1) && (r0 == r1);
    if (_bSpecial)
        return;

    if (xform != 0)
        TransformRadialGradient(xform);
    else
        _vx = 0, _vy = 1.0;

    // Calculation of the gradient pattern will be simplified by
    // placing the origin at (x0,y0). Also, if the caller supplied
    // an affine transformation matrix, any required scaling or
    // shear has been incorporated into the vector (vx,vy).
    _x1 -= _x0, _y1 -= _y0;
    _x1 += _vx*_y1, _y1 *= _vy;
    _x0 -= 0.5, _y0 -= 0.5;

    // The values below are constant over this object's lifetime
    _dr = _r1 - _r0;
    _a = _x1*_x1 + _y1*_y1 - _dr*_dr;
    _inva = (_a == 0) ? 0 : 1.0/_a;
    _A2 = _dr*_dr - _y1*_y1;
}

// Private function: Transforms the radial gradient fill pattern.
// If the user provides an affine transformation matrix, this
// function is called by the constructor to apply the specified
// transformation to the radial gradient parameters.
void Radial::TransformRadialGradient(const float xform[])
{
    float px, py, qx, qy, tmp, rmax, rmin, ratio;

    if (_r1 > _r0)
        rmax = _r1, rmin = _r0; 
    else
        rmax = _r0, rmin = _r1;

    ratio = rmin/rmax;

    // Apply the user-specified affine transformation matrix to
    // circle centers (x0,y0) and (x1,y1). Also, convert circle
    // radius rmax into conjugate diameter end points (px,py)
    // and (qx,qy) of the corresponding transformed ellipse.
    tmp = xform[0]*_x0 + xform[2]*_y0 + xform[4];
    _y0 = xform[1]*_x0 + xform[3]*_y0 + xform[5];
    _x0 = tmp;
    tmp = xform[0]*_x1 + xform[2]*_y1 + xform[4];
    _y1 = xform[1]*_x1 + xform[3]*_y1 + xform[5];
    _x1 = tmp;
    px = xform[0]*rmax, py = xform[1]*rmax;
    qx = xform[2]*rmax, qy = xform[3]*rmax;

    // Initialize unit vectors u and v to point in the x and y
    // directions, respectively. These reference vectors will
    // track the cumulative effects of the transformations below.
    float ux = 1.0, uy = 0;
    _vx = 0, _vy = 1.0;

    // Derive coefficients A, B, and C of the implicit equation
    // for the ellipse: f(x,y) = Ax^2 + Bxy + Cy^2 + ... = 0
    float B = -2.0*(px*py + qx*qy);
    
    if (B != 0)
    {
        float A = py*py + qy*qy;
        float C = px*px + qx*qx;
        float beta = (C - A)/B;
        float slope = beta + sqrt(beta*beta + 1.0);
        float norm = sqrt(slope*slope + 1.0);

        // Rotate the ellipse so that its principal axes are
        // aligned with the x-y axes. Apply the same rotation
        // to unit vectors u and v.
        float cosa = 1.0/norm, sina = slope/norm;

        tmp = px*cosa + py*sina;
        py = -px*sina + py*cosa;
        px = tmp;
        tmp = qx*cosa + qy*sina;
        qy = -qx*sina + qy*cosa;
        qx = tmp;
        tmp = ux*cosa + uy*sina;
        uy = -ux*sina + uy*cosa;
        ux = tmp;
        tmp = _vx*cosa + _vy*sina;
        _vy = -_vx*sina + _vy*cosa;
        _vx = tmp;
    }

    // Squeeze or stretch the axis-aligned ellipse in the
    // x-dimension to transform it into a circle. Apply the
    // same scaling to vectors u and v; as a result, u and
    // v will no longer be unit vectors.
    float dx, dy, scale;

    dx = fabs(px) + fabs(qx);
    if (dx == 0)
    {
        _bSpecial = true;  // degenerate ellipse
        return;
    }
    dy = fabs(py) + fabs(qy);
    scale = dy/dx;
    px *= scale, qx *= scale;
    rmax = sqrt(px*px + py*py);
    ux *= scale, _vx *= scale;

    // Scale vector u back to unit-vector size, and scale
    // vector v and radii r1 and r0 by the same amount
    scale = 1.0/sqrt(ux*ux + uy*uy);
    ux *= scale, uy *= scale;
    _vx *= scale, _vy *= scale;
    rmax *= scale;
    rmin = ratio*rmax;
    if (_r1 > _r0)
        _r1 = rmax, _r0 = rmin; 
    else
        _r0 = rmax, _r1 = rmin;

    // Rotate unit vector u so that it's horizontal and points in the
    // +x direction. Rotate vector v by the same angle. Since we know
    // the new u is always (1,0), we need to save only the new v.
    float cosb = ux, sinb = uy;

    tmp = _vx*cosb + _vy*sinb;
    _vy = -_vx*sinb + _vy*cosb;
    _vx = tmp;
}

// Public function: Fills the pixels in a single horizontal span with
// a radial gradient pattern. The span starts at pixel (xs,ys) and
// extends 'len' pixels to the right. The function writes the processed
// gradient-fill pixels to the outBuf array. To support shape anti-
// aliasing and source constant alpha, the inAlpha array contains
// 8-bit alpha values to apply to the gradient-fill pixels (in addition
// to the per-pixel alphas in the gradient). The constructor previously
// set the values of constants _dr, _a, _inva, and _A2.
void Radial::FillSpan(int xs, int ys, int len, COLOR outBuf[], const COLOR inAlpha[])
{
    float xp, yp, b0, b, phi, A0, A1;

    // Special case: x0 == x1, y0 == y1, and r0 == r1
    if (_bSpecial)
    {
        if (_bExtEnd)
        {
            for (int i = 0; i < len; ++i)
            {
                COLOR opacity = (inAlpha == 0) ? 255 : inAlpha[i];
                COLOR color = _cstops->GetPadColor(1, opacity);

                outBuf[i] = color;
            }
        }
        return;
    }

    xp = xs - _x0 + _xscroll;
    yp = ys - _y0 + _yscroll;
    xp += _vx*yp, yp *= _vy;  // apply scaling + shearing transform
    b0 = yp*_y1 + _r0*_dr;
    b = xp*_x1 + b0;
    phi = yp*yp - _r0*_r0;
    A0 = b0*b0 - _a*phi;
    A1 = 2*b0*_x1;

    // Normal case: Each iteration of this for-loop paints one pixel
    for (int i = 0; i < len; ++i)
    {
        COLOR opacity = (inAlpha == 0) ? 255 : inAlpha[i];

        if (opacity != 0)
        {
            COLOR color = 0;
            float discr = (_A2*xp + A1)*xp + A0;

            if (discr < 0 && _a < 0)
                discr = 0;

            if (discr >= 0)
            {   
                float t0 = 0, t1 = 0;
                bool bValid0 = false, bValid1 = false;
    
                if (_a == 0)
                {
                    if (b != 0)
                    {
                        float c = xp*xp + phi;

                        t1 = (c/b)/2;
                        bValid1 = ((_bExtStart || t1 >= 0) &&
                                   (_bExtEnd || t1 < 1.0f) &&
                                   _r0 + t1*_dr > 0);
                    }
                }
                else
                {
                    float root = sqrt(discr);
    
                    if (_a > 0 || _dr < 0)
                    {
                        t0 = _inva*(b + root);
                        bValid0 = ((_bExtStart || t0 >= 0) &&
                                   (_bExtEnd || t0 < 1.0f) &&
                                   (_a < 0 || _r0 + t0*_dr >= 0));
                    }
                    if (_a > 0 || _dr > 0)
                    {
                        t1 = _inva*(b - root);
                        bValid1 = ((_bExtStart || t1 >= 0) &&
                                   (_bExtEnd || t1 < 1.0f) &&
                                   (_a < 0 || _r0 + t1*_dr >= 0));
                    }
                }
                if (bValid0 || bValid1)
                {
                    float t;
                    int n;
    
                    if (bValid0 && bValid1)
                        t = (t0 > t1) ? t0 : t1;
                    else
                        t = bValid0 ? t0 : t1;
    
                    n = t;
                    if (t < 0) --n;
                    if (_spread == SPREAD_PAD && n != 0)
                        color = _cstops->GetPadColor(n, opacity);
                    else
                    {
                        FIX16 tfix = 0x0000ffff*(t - n);

                        if (_spread == SPREAD_REFLECT && (n & 1))
                            tfix ^= 0x0000ffff;
    
                        color = _cstops->GetColorValue(tfix, opacity);
                    }
                }
            }
            outBuf[i] = color;
        }
        xp += 1.0f;
        b += _x1;
    }
}

// Called by a renderer to create a new radial-gradient object
RadialGradient* CreateRadialGradient(float x0, float y0, float r0,
                                     float x1, float y1, float r1,
                                     SPREAD_METHOD spread, int flags,
                                     const float xform[])
{
    Radial *rad = new Radial(x0, y0, r0, x1, y1, r1, spread, flags, xform);
    assert(rad != 0);

    return rad;
}
