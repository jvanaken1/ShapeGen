#include <math.h>
#include <assert.h>
#include "demo.h"

//---------------------------------------------------------------------
//
// An AlphaBlur object uses a Gaussian filter to blur images, and is
// typically used to draw drop shadows. The BlurImage function blurs
// only the alpha fields in the source image. The color components in
// the source image are ignored and replaced with a solid color. This
// color can be customized if the caller prefers not to use the
// default color, black. The user can create a custom filter kernel by
// specifying the standard deviation and filter width (always an odd
// integer), or simply accept the defaults for these parameters.
// Blurring expands the image. The dimensions of the resulting blurred
// image are obtained by extending all four sides of the source
// image's bounding box outward by r = floor(kernel_width/2) pixels.
// AlphaBlur implements the ImageReader interface, which is described
// in the EnhancedRenderer::SetPattern reference topic. Thus, blurred
// images can conveniently be used as fill patterns. For a code
// example, see the "Compositing and Filtering with Layers" topic.
//
//---------------------------------------------------------------------

AlphaBlur::~AlphaBlur()
{
    DeleteRawPixels(_pixels);
    if (_kcoeff)
        delete[] _kcoeff;
}

// Public function: Calculates the bounding box that would be needed
// to contain a blurred version of an image, given (1) the current
// kernel width and (2) the bounding box 'bbox' for the original,
// unblurred image. After being blurred, the image is extended by
// r = floor(kwidth/2) pixels on all four sides, where kwidth is the
// kernel width. The function writes the expanded bounding box
// coordinates to the 'blurbox' structure, overwriting its previous
// contents.
//
void AlphaBlur::GetBlurredBoundingBox(SGRect& blurbbox, const SGRect& bbox)
{
    int r = _kwidth/2;
    blurbbox.x = bbox.x - r;
    blurbbox.y = bbox.y - r;
    blurbbox.w = bbox.w + 2*r;
    blurbbox.h = bbox.h + 2*r;
}

// Public function: Generates the coefficients for the Gaussian filter
// kernel. Due to symmetry, only the coefficients on the right side
// (and center) of the kernel need to be explicitly calculated and
// stored in the kcoeff array. Note that this array is of length
// 1+kwidth/2, not kwidth! The coefficients are stored as 16-bit
// integer alpha values in the range 0 to 1.0, where 1.0 is
// represented as 0x0000ffff. If the caller does not explicitly
// supply values for the stddev or kwidth parameters, or if these
// parameters are zero, the function sets them to the default values
// stddev = 2.0 and kwidth = 11. Note that kwidth is always odd; if
// the caller supplies an even, nonzero value for kwidth, the
// function adds 1 to make it odd.
//
int AlphaBlur::CreateFilterKernel(float stddev, int kwidth)
{
    assert(stddev >= 0 && kwidth >= 0);
    if (stddev == 0)
        stddev = 2.0f;

    if (kwidth == 0)
        kwidth = 3.25f*stddev + 0.85f;

    kwidth |= 1;  // must be odd integer
    int r = kwidth/2;
    float *ktemp = new float[r+1];
    assert(ktemp);
    float denom = 2*stddev*stddev;
    float sum = ktemp[0] = 1.0f;

    for (int i = 1; i <= r; ++i)
    {
        ktemp[i] = exp(-(i*i)/denom);
        sum += 2*ktemp[i];
    }
    _kcoeff = new COLOR[r+1];
    for (int i = 0; i <= r; ++i)
        _kcoeff[i] = 0x0000ffff*ktemp[i]/sum;

    _stddev = stddev;
    _kwidth = kwidth;
    delete[] ktemp;
    return _kwidth;
}

// Private function: Convolves a 1-D source image 'src' with a
// Gaussian filter kernel of width 'kwidth', and writes the result to
// destination image 'dst'. The 'src' array is of length 'len', and
// the 'dst' array has length len+2*r, where r = floor(kwidth/2).
// Kernel width 'kwidth' is always odd. This function assumes that
// the caller previously set the 2*r pixels on either side of the
// 'len' pixels in the 'src' array to zero.
//
void AlphaBlur::ApplyGaussianFilter(COLOR dst[], const COLOR src[], int len)
{
    int r = _kwidth/2;
    const COLOR *psrc = &src[-r], *pR, *pL;
    COLOR *pdst = &dst[0], *pK, sum;

    len += _kwidth;
    for (int i = 0; i < len; ++i)
    {
        pR = pL = psrc++;
        pK = &_kcoeff[0];
        sum = (pK[0]*pR[0]) >> 16;

        for (int j = 0; j < r; ++j)
            sum += (*++pK * (*++pR + *--pL)) >> 16;

        *pdst++ = sum;
    }
}

// Public function: Uses a Gaussian filter to blur the 32-bpp image
// in the pixel buffer specified by the input parameter 'inimg'. The
// function allocates an output buffer and writes the blurred image to
// this buffer. Blurring will expand the image by 2*floor(kwidth/2)
// pixels on each of its four sides. Only the alpha channel in the
// source image is filtered; the source RGB values are ignored, and
// the RGB values in the '_color' member are used instead to color the
// resulting blurred image.
//
bool AlphaBlur::BlurImage(const PIXEL_BUFFER& inimg)
{
    if (inimg.pixels == 0 || inimg.width <= 0 || inimg.height <= 0 ||
        inimg.pitch < inimg.width || inimg.depth != 32)
    {
        assert(0);
        return false;  // input image is not valid
    }

    // Allocate pixel buffer to store blurred image
    int r = _kwidth/2;
    _index = 0;
    _width = inimg.width+2*r;
    _height = inimg.height+2*r;
    _numpixels = _width*_height;
    if (_pixels != 0)
        delete[] _pixels;

    _pixels = new COLOR[_numpixels];
    if (_pixels == 0)
    {
        assert(_pixels != 0);
        _numpixels = _width = _height = 0;
        return false;
    }

    COLOR *pincol = &inimg.pixels[0];
    COLOR *poutcol = &_pixels[r];
    int instride = inimg.pitch/sizeof(inimg.pixels[0]);
    int outstride = _width;

    // Allocate scratch buffer to filter columns from input image
    int scratchwidth = inimg.height + 4*r;
    COLOR *scratch = AllocateRawPixels(scratchwidth, 2, RGBA(0,0,0,0));

    // First, process the image in the vertical direction by
    // convolving each column with a 1-D filter. Blurring will
    // increase the height of each column by 2*r pixels. Each
    // for-loop iteration below vertically filters one column
    // of pixels.
    for (int i = 0; i < inimg.width; ++i)
    {
        // Copy the next column of pixels from the input image to
        // the first row of the scratch buffer, and leave margins
        // of 2*r pixels (set to zero) on either side of the copied
        // pixels. Only the alpha fields of the copied pixels are
        // preserved. To improve filtering precision, each 8-bit
        // alpha value is converted to a 16-bit alpha value.
        // TODO: Can we mask delays due to cache misses?
        COLOR *pin = pincol++;
        COLOR *pdst = &scratch[2*r];
        for (int j = 0; j < inimg.height; ++j)
        {
            COLOR alpha = *pin >> 24;
            *pdst++ = alpha | (alpha << 8);
            pin = &pin[instride];
        }

        // Convolve input pixels with 1-D Gaussian filter. The blurred
        // result is written to the second row of the scratch buffer.
        COLOR *psrc = &scratch[2*r];
        pdst = &scratch[r + scratchwidth];
        ApplyGaussianFilter(pdst, psrc, inimg.height);
        psrc = pdst;

        // Copy blurred pixels from last row of scratch buffer
        // to next column of intermediate image
        COLOR *pout = poutcol++;
        for (int j = 0; j < _height; ++j)
            *pout = *psrc++, pout = &pout[outstride];
    }
    delete[] scratch;

    // So far, vertical filtering has increased the height of the
    // image by 2*r. Next, this intermediate image will be filtered
    // horizontally, which will increase the image width by 2*r.
    COLOR *pinrow = &_pixels[r];
    COLOR *poutrow = &_pixels[0];

    // Allocate scratch buffer to filter rows from intermediate image
    scratchwidth = inimg.width + 4*r;
    scratch = AllocateRawPixels(scratchwidth, 2, RGBA(0,0,0,0));

    // To construct the final output image, each for-loop iteration
    // below horizontally filters one row from the intermediate image
    for (int j = 0; j < _height; ++j)
    {
        // Copy the next row of pixels from the intermediate image
        // to the first row of the scratch buffer, and leave
        // margins of 2*r pixels (set to zero) on either side.
        COLOR *pin = pinrow;
        COLOR *pdst = &scratch[2*r];
        pinrow = &pinrow[outstride];
        for (int i = 0; i < inimg.width; ++i)
            *pdst++ = *pin++;

        // Convolve input pixels with Gaussian filter. The blurred
        // result resides in the second row of the scratch buffer.
        COLOR *psrc = &scratch[2*r];
        pdst = &scratch[r + scratchwidth];
        ApplyGaussianFilter(pdst, psrc, inimg.width);
        psrc = pdst;

        // Copy blurred alpha values from scratch buffer to next row
        // of destination image. Convert alphas from 16 to 8 bits.
        COLOR *pout = poutrow;
        poutrow = &poutrow[outstride];
        for (int i = 0; i < _width; ++i)
        {
            COLOR alpha = *psrc++ >> 8;
            *pout++ = (alpha << 24) | _color;
        }
    }
    DeleteRawPixels(scratch);
    return true;
}

// Public function: Implements the ImageReader interface, which
// allows the blurred image to be supplied to the SetPattern
// function and used as a fill pattern.
int AlphaBlur::ReadPixels(COLOR *buffer, int count)
{
    if (_numpixels == 0 || _index >= _numpixels)
        return 0;

    if (count > (_numpixels - _index))
        count = _numpixels - _index;

    COLOR *ptr = &_pixels[_index];

    for (int i = 0; i < count; ++i)
        *buffer++ = *ptr++;

    _index += count;
    return count;
}
