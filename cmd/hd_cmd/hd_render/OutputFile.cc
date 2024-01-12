// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file OutputFile.cc

#include "OutputFile.h"

#include "MapGuard.h"

#include <OpenImageIO/imageio.h>
#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>

#include <memory>
#include <vector>

//#define DEBUG_MSG

namespace hd_render {

OutputFile::OutputFile(pxr::HdRenderBuffer *renderBuffer):
    mRenderBuffer(renderBuffer)
{
}

int
OutputFile::write(const std::string &filename) const
{
#ifdef DEBUG_MSG
    std::cerr << ">> OutputFile.cc write() start\n";
#endif // end DEBUG_MSG

    mRenderBuffer->Resolve();
    MapGuard mapGuard(mRenderBuffer);
    void *pixelData = mapGuard.getPixels();
    const unsigned int width = mRenderBuffer->GetWidth();
    const unsigned int height = mRenderBuffer->GetHeight();
    const pxr::HdFormat format = mRenderBuffer->GetFormat();

    // TODO: support other formats
    int numChannels;
    OIIO::TypeDesc typeDesc;
    int sizeOfPixel; // in bytes

    switch (format) {
    case pxr::HdFormatFloat32:
        numChannels = 1;
        typeDesc = OIIO::TypeDesc::FLOAT;
        sizeOfPixel = numChannels * sizeof(float);
        break;
    case pxr::HdFormatFloat32Vec2:
        numChannels = 2;
        typeDesc = OIIO::TypeDesc::FLOAT;
        sizeOfPixel = numChannels * sizeof(float);
        break;
    case pxr::HdFormatFloat32Vec3:
        numChannels = 3;
        typeDesc = OIIO::TypeDesc::FLOAT;
        sizeOfPixel = numChannels * sizeof(float);
        break;
    case pxr::HdFormatFloat32Vec4:
        numChannels = 4;
        typeDesc = OIIO::TypeDesc::FLOAT;
        sizeOfPixel = numChannels * sizeof(float);
        break;
    case pxr::HdFormatInt32:
        // OpenEXR doesn't support signed 32 bit int output channels, and
        // openimageio will try to store the result using 16-bit half,
        // which is probably not what we want.  So we'll output as
        // unsigned 32 bit int and hope for the best.
        numChannels = 1;
        typeDesc = OIIO::TypeDesc::UINT32;
        sizeOfPixel = numChannels * sizeof(unsigned int);
        break;
    case pxr::HdFormatUNorm8Vec4:
        numChannels = 4;
        typeDesc = OIIO::TypeDesc::UINT8;
        sizeOfPixel = numChannels;
        break;
    default:
        std::cerr << "Don't know how to write format\t:" << format << '\n';
        return -1;
    };

    void *buf = const_cast<void *>(pixelData);

    std::unique_ptr<OIIO::ImageOutput> out(OIIO::ImageOutput::create(filename.c_str()));
    if (!out) {
        return -1;
    }

    // oiio is top down - we want bottom up.  so a flip is needed.
    OIIO::ImageSpec srcSpec(width, height, numChannels, typeDesc);
    OIIO::ImageBuf srcBuffer(filename, srcSpec, buf);
    OIIO::ImageBuf flippedBuffer(filename, srcSpec);
    OIIO::ImageBufAlgo::flip(flippedBuffer, srcBuffer);
    OIIO::ImageSpec destSpec(srcSpec);
    OIIO::ImageBuf destBuffer(filename, destSpec);
    std::vector<char> buf2(width * height * sizeOfPixel);
    flippedBuffer.get_pixels(OIIO::get_roi(srcSpec), typeDesc, &buf2[0]);
    destBuffer.set_pixels(OIIO::get_roi(destSpec), typeDesc, &buf2[0]);

    if (!out->open(filename.c_str(), destSpec)) {
        return -1;
    }
    destBuffer.write(out.get());
    out->close();

    return 0;
}

} // namespace hd_render

