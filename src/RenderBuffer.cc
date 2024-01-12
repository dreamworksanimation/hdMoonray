// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#include "RenderBuffer.h"
#include "RenderDelegate.h"
#include "Camera.h"
#include "ValueConverter.h"
#include <scene_rdl2/scene/rdl2/SceneContext.h>
#include <scene_rdl2/scene/rdl2/SceneVariables.h>
#include <scene_rdl2/scene/rdl2/RenderOutput.h>
#include "pxr/base/work/loops.h"

#include <iostream>
#include <cmath>

#ifdef __GNUC__
#define UNUSED __attribute__ ((unused))
#else
#define UNUSED
#endif

//#define DEBUG_MSG

namespace {

UNUSED
std::ostream&
operator<<(std::ostream& o, pxr::HdFormat format)
{
    const char* t;
    switch (pxr::HdGetComponentFormat(format)) {
    case pxr::HdFormatUNorm8: t = "UNorm8"; break;
    case pxr::HdFormatSNorm8: t = "SNorm8"; break;
    case pxr::HdFormatFloat16: t = "Float16"; break;
    case pxr::HdFormatFloat32: t = "Float32"; break;
    case pxr::HdFormatInt32: t = "Int32"; break;
    default:
        return o << "HdFormatInvalid(" << int(format) << ')';
    }
    unsigned n = pxr::HdGetComponentCount(format);
    o << "HdFormat" << t;
    if (n != 1) o << "Vec" << n;
    return o;
}

// Lookup table from AOV name to RenderOutput settings
typedef scene_rdl2::rdl2::RenderOutput RO;
struct RODesc {
    pxr::HdFormat hdFormat;
    RO::Result result = RO::RESULT_BEAUTY;
    RO::StateVariable stateVariable = RO::STATE_VARIABLE_P;
    RODesc(pxr::HdFormat f): hdFormat(f) {}
    RODesc(pxr::HdFormat f, RO::Result r): hdFormat(f), result(r) {}
    RODesc(pxr::HdFormat f, RO::StateVariable v): hdFormat(f), result(RO::RESULT_STATE_VARIABLE), stateVariable(v) {}
};
const RODesc*
lookup(pxr::TfToken const& name) {
    static std::map<pxr::TfToken, RODesc> map = {
        {pxr::HdAovTokens->color, {pxr::HdFormatFloat32Vec4}},
        {pxr::HdAovTokens->primId, {pxr::HdFormatInt32}},
        {pxr::HdAovTokens->instanceId, {pxr::HdFormatInt32}},
        {pxr::HdAovTokens->elementId, {pxr::HdFormatInt32}},
        {pxr::HdAovTokens->edgeId, {pxr::HdFormatInt32}},
        {pxr::HdAovTokens->pointId, {pxr::HdFormatInt32}},
        // {pxr::HdAovTokens->Peye, pxr::HdFormatFloat32Vec3},
        // {pxr::HdAovTokens->Neye, pxr::HdFormatFloat32Vec3},
        // {pxr::HdAovTokens->patchCoord, pxr::HdFormatFloat32Vec3},
        {pxr::TfToken("beauty"), {pxr::HdFormatFloat32Vec3, RO::RESULT_BEAUTY}},
        {pxr::TfToken("alpha"), {pxr::HdFormatFloat32, RO::RESULT_ALPHA}},
        {pxr::HdAovTokens->depth, {pxr::HdFormatFloat32, RO::RESULT_DEPTH}}, // remapped to near=0, far=1
        {pxr::HdAovTokens->cameraDepth, {pxr::HdFormatFloat32, RO::RESULT_DEPTH}}, // pixar name for actual Z
        {pxr::TfToken("heat_map"), {pxr::HdFormatFloat32, RO::RESULT_HEAT_MAP}},
        {pxr::TfToken("wireframe"), {pxr::HdFormatFloat32Vec3, RO::RESULT_WIREFRAME}}, // crashes
        {pxr::TfToken("weight"), {pxr::HdFormatFloat32, RO::RESULT_WEIGHT}},
        {pxr::TfToken("beauty_aux"), {pxr::HdFormatFloat32Vec3, RO::RESULT_BEAUTY_AUX}},
        {pxr::TfToken("cryptomatte"), {pxr::HdFormatFloat32Vec3, RO::RESULT_CRYPTOMATTE}},
        {pxr::TfToken("alpha_aux"), {pxr::HdFormatFloat32, RO::RESULT_ALPHA_AUX}},
        {pxr::TfToken("P"), {pxr::HdFormatFloat32Vec3, RO::STATE_VARIABLE_P}},
        {pxr::TfToken("Ng"), {pxr::HdFormatFloat32Vec3, RO::STATE_VARIABLE_NG}},
        {pxr::TfToken("N"), {pxr::HdFormatFloat32Vec3, RO::STATE_VARIABLE_N}},
        {pxr::HdAovTokens->normal, {pxr::HdFormatFloat32Vec3, RO::STATE_VARIABLE_N}}, // pixar name
        {pxr::TfToken("St"), {pxr::HdFormatFloat32Vec2, RO::STATE_VARIABLE_ST}},
        {pxr::TfToken("primvars:st"), {pxr::HdFormatFloat32Vec2, RO::STATE_VARIABLE_ST}}, // pixar name
        {pxr::TfToken("dPds"), {pxr::HdFormatFloat32Vec3, RO::STATE_VARIABLE_DPDS}},
        {pxr::TfToken("dPdt"), {pxr::HdFormatFloat32Vec3, RO::STATE_VARIABLE_DPDT}},
        {pxr::TfToken("dSdx"), {pxr::HdFormatFloat32, RO::STATE_VARIABLE_DSDX}},
        {pxr::TfToken("dSdy"), {pxr::HdFormatFloat32, RO::STATE_VARIABLE_DSDY}},
        {pxr::TfToken("dTdx"), {pxr::HdFormatFloat32, RO::STATE_VARIABLE_DTDX}},
        {pxr::TfToken("dTdy"), {pxr::HdFormatFloat32, RO::STATE_VARIABLE_DTDY}},
        {pxr::TfToken("Wp"), {pxr::HdFormatFloat32Vec3, RO::STATE_VARIABLE_WP}},
        {pxr::TfToken("Z"), {pxr::HdFormatFloat32, RO::STATE_VARIABLE_DEPTH}},// is this the same as cameraDepth?
        {pxr::TfToken("motionvec"), {pxr::HdFormatFloat32Vec2, RO::STATE_VARIABLE_MOTION}},
    };
    auto i = map.find(name);
    if (i != map.end())
        return &i->second;
    else
        return nullptr;
}

// Guess at what type of data is in a named primitive
pxr::HdFormat
getPrimvarFormat(pxr::TfToken name) {
    static std::map<pxr::TfToken, pxr::HdFormat> map = {
        {pxr::TfToken("displayOpacity"), pxr::HdFormatFloat32},
        {pxr::HdAovTokens->primId, pxr::HdFormatInt32},
        {pxr::HdAovTokens->instanceId, pxr::HdFormatInt32},
        {pxr::TfToken("instanceIdA"), pxr::HdFormatInt32},
        {pxr::TfToken("instanceIdB"), pxr::HdFormatInt32},
        {pxr::TfToken("instanceIdC"), pxr::HdFormatInt32},
        {pxr::HdAovTokens->elementId, pxr::HdFormatInt32},
        {pxr::HdAovTokens->edgeId, pxr::HdFormatInt32},
        {pxr::HdAovTokens->pointId, pxr::HdFormatInt32},
    };
    auto i = map.find(name);
    return i != map.end() ? i->second : pxr::HdFormatFloat32Vec3;
}

// Split afer first ':' into two tokens. If no ':' return empty, token
void split(pxr::TfToken token, pxr::TfToken& prefix, pxr::TfToken& suffix)
{
    const std::string s(token.GetString());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == ':') {
            prefix = pxr::TfToken(s.substr(0,i+1));
            suffix = pxr::TfToken(s.substr(i+1));
            return;
        }
        if (not isalnum(s[i])) break; // prefix has to be a plain word
    }
    suffix = token;
}

}

namespace hdMoonray {

using scene_rdl2::logging::Logger;

// An HdAovDescriptor contains the format and multiSampled values passed to Allocate(), and the
// clearValue and aovSettings passed in aovBinding of Bind().
pxr::HdAovDescriptor
RenderBuffer::getAovDescriptor(pxr::TfToken const& name)
{
    auto desc = lookup(name);
    if (desc)
        return pxr::HdAovDescriptor(desc->hdFormat, false, pxr::VtValue());
    pxr::HdFormat format = pxr::HdFormatInvalid;
    pxr::TfToken prefix, suffix; split(name, prefix, suffix);
    if (prefix.IsEmpty() || prefix == pxr::HdAovTokens->primvars) {
        format = getPrimvarFormat(suffix);
    } else if (prefix == pxr::HdAovTokens->lpe) {
        format = pxr::HdFormatFloat32Vec3;
    } else if (prefix == pxr::HdAovTokens->shader) {
        format = pxr::HdFormatFloat32Vec3;
    }
    return pxr::HdAovDescriptor(format, false, pxr::VtValue());
}

void
RenderBuffer::Sync(pxr::HdSceneDelegate* sceneDelegate,
                   pxr::HdRenderParam* renderParam,
                   pxr::HdDirtyBits* dirtyBits)
{
#   ifdef DEBUG_MSG
    std::cerr << ">> RenderBuffer.cc RenderBuffer::Sync()\n";
#   endif

    // std::cout << "RenderBuffer::Sync " << GetId() << std::endl;
    mRenderDelegate = &RenderDelegate::get(renderParam);
    mRenderDelegate->setStartTime();
    pxr::HdRenderBuffer::Sync(sceneDelegate, renderParam, dirtyBits); // this calls Allocate()
}

// This is unfortunatly called before bind(), and thus mRenderOutput output may be unset.
// It would be nice to create mRenderOutput here, but this cannot be done because the
// aovName is inaccessible. Instead everything must be guessed from the dimensions & format.
bool
RenderBuffer::Allocate(const pxr::GfVec3i& dimensions, pxr::HdFormat format, bool multiSampled)
{
#   ifdef DEBUG_MSG
    std::cerr << ">> RenderBuffer.cc RenderBuffer::Allocate()\n";
#   endif

    // std::cout << "RenderBuffer::Allocate " << GetId() << ' ' << dimensions << ' ' << format << std::endl;
    if (dimensions[2] != 1) {
        Logger::error(GetId(), ": dimensions ", dimensions, " unsupported");
        return false;
    }

    mFormat = format;

    PixelSize request;
    switch (format) {
        case pxr::HdFormatInt32:
        case pxr::HdFormatFloat32:
            request.mChannels = 1; break;
        case pxr::HdFormatFloat32Vec2:
            request.mChannels = 2; break;
        case pxr::HdFormatFloat32Vec3:
            request.mChannels = 3; break;
        case pxr::HdFormatFloat32Vec4:
            request.mChannels = 4; break;
        default:
            Logger::error(GetId(), ": unknown format ", format);
            return false;
    }
    request.mWidth = dimensions[0];
    request.mHeight = dimensions[1];

    return mRenderDelegate->getRendererApplySettings().allocate(mRenderOutput, pd, request);
}

// Called by RenderPass. Only at this point do we have all the information needed to correctly
// set mRenderOutput and to set data needed by Resolve()
void
RenderBuffer::bind(const pxr::HdRenderPassAovBinding& aovBinding, const Camera* camera)
{
#   ifdef DEBUG_MSG
    std::cerr << ">> RenderBuffer.cc RenderBuffer::bind()\n";
#   endif

    mAovName = aovBinding.aovName;
    mNear = camera->getNear();
    mFar = camera->getFar();
    pxr::HdAovSettingsMap aovSettings = aovBinding.aovSettings;

    if (aovBinding.clearValue.IsHolding<pxr::GfVec4f>()) {
        pxr::GfVec4f v = aovBinding.clearValue.Get<pxr::GfVec4f>();
        if (v != clearValue) {
            clearValue = v;
            pd.filmActivity = ~0; // make it recomposite even if render has finished
        }
    }

    if (bound()) return; // Hydra does not reuse render buffers for different aovs, so assume it is unchanged
    mBound = true;

    if (mAovName == pxr::HdAovTokens->color) {
        // special case Color if it is 4 channels by using dummy RenderOutput
        mRenderOutput = nullptr;

    } else {
        // std::cout << "RenderBuffer::bind " << GetId() << std::endl;

        if (not mRenderOutput) {
            scene_rdl2::rdl2::SceneObject* object = mRenderDelegate->createSceneObject("RenderOutput", GetId());
            if (not object) return;
            mRenderOutput = object->asA<scene_rdl2::rdl2::RenderOutput>();
        }
        auto& ro(*mRenderOutput);
        {   UpdateGuard guard(ro);
            ro.setActive(true);

            mDepth = false;
            auto desc = lookup(mAovName);
            if (desc) {
                if (desc->hdFormat == pxr::HdFormatInt32) {
                    // this is for the instanceId and similar primvars
                    ro.setResult(RO::RESULT_PRIMITIVE_ATTRIBUTE);
                    ro.setPrimitiveAttribute(mAovName.GetString());
                    ro.setPrimitiveAttributeType(RO::PRIMITIVE_ATTRIBUTE_TYPE_FLOAT);
                    ro.setMathFilter(RO::MATH_FILTER_CLOSEST);

                } else {
                    ro.setResult(desc->result);
                    if (desc->result ==  RO::RESULT_STATE_VARIABLE) {
                        ro.setStateVariable(desc->stateVariable);
                    } else if (desc->result == RO::RESULT_DEPTH) {
                        ro.setMathFilter(mRenderOutput->MATH_FILTER_MIN);
                        if (mAovName == pxr::HdAovTokens->depth) {
                            mDepth = true; // process to OpenGL type clip space
                        }
                    }
                }

            } else {
                pxr::TfToken prefix, suffix; split(mAovName, prefix, suffix);
                if (prefix == pxr::HdAovTokens->primvars) {
                    ro.setResult(mRenderOutput->RESULT_PRIMITIVE_ATTRIBUTE);
                    ro.setPrimitiveAttribute(suffix.GetString());
                    switch (getPrimvarFormat(suffix)) {
                    case pxr::HdFormatInt32:
                        ro.setPrimitiveAttributeType(ro.PRIMITIVE_ATTRIBUTE_TYPE_FLOAT);
                        ro.setMathFilter(mRenderOutput->MATH_FILTER_CLOSEST);
                        break;
                    case pxr::HdFormatFloat32:
                        ro.setPrimitiveAttributeType(ro.PRIMITIVE_ATTRIBUTE_TYPE_FLOAT);
                        break;
                    case pxr::HdFormatFloat32Vec2:
                        ro.setPrimitiveAttributeType(ro.PRIMITIVE_ATTRIBUTE_TYPE_VEC2F);
                        break;
                    default:
                        ro.setPrimitiveAttributeType(ro.PRIMITIVE_ATTRIBUTE_TYPE_RGB);
                        break;
                    }
                } else if (prefix == pxr::HdAovTokens->lpe) {
                    ro.setResult(ro.RESULT_LIGHT_AOV);
                    ro.setLpe(suffix.GetString());
                } else if (prefix == pxr::HdAovTokens->shader) {
                    ro.setResult(ro.RESULT_MATERIAL_AOV);
                    ro.setMaterialAov(suffix.GetString());
                } else {
                    Logger::info("custom aov ", mAovName);
                    const SceneClass& sceneClass = mRenderOutput->getSceneClass();
                    bool valid = false;
                    for (auto it = sceneClass.beginAttributes(); it != sceneClass.endAttributes(); ++it) {
                        const std::string& attrName = (*it)->getName();
                        pxr::TfToken key = pxr::TfToken("parameters:moonray:" + attrName);
                        pxr::VtValue val = aovSettings[key];
                        if (not val.IsEmpty()) {
                            valid = true;
                            ValueConverter::setAttribute(mRenderOutput, *it, val);
                        }
                    }
                    if (!valid){
                        Logger::error(GetId(), ": invalid aov ", mAovName);
                        ro.setActive(false);
                        mFormat = pxr::HdFormatInvalid;
                    }
                }
            }
            ro.setFileName("/tmp/scene.exr"); // MOONRAY-3389
        }

        if (mAovName == pxr::HdAovTokens->instanceId) {
            // this output will be the sum of instanceId+instanceIdA+instanceIdB...
            for (size_t i = 0; i < INSTANCE_NESTING; ++i) {
                char letter = 'A'+i;
                scene_rdl2::rdl2::SceneObject* object =
                    mRenderDelegate->createSceneObject("RenderOutput", GetId().GetString() + letter);
                if (not object) return;
                mMoreOutputs[i] = object->asA<scene_rdl2::rdl2::RenderOutput>();
                auto& ro(*mMoreOutputs[i]);
                {   UpdateGuard guard(ro);
                    ro.setActive(true);
                    ro.setResult(RO::RESULT_PRIMITIVE_ATTRIBUTE);
                    ro.setPrimitiveAttribute(mAovName.GetString() + letter);
                    ro.setPrimitiveAttributeType(RO::PRIMITIVE_ATTRIBUTE_TYPE_FLOAT);
                    ro.setMathFilter(RO::MATH_FILTER_CLOSEST);
                    ro.setFileName("/tmp/scene.exr"); // MOONRAY-3389
                }
            }
        }
    }
}

bool
RenderBuffer::IsConverged() const
{
#   ifdef DEBUG_MSG
    std::cerr << ">> RenderBuffer.cc RenderBuffer::IsConverged()\n";
#   endif

    return true;
}

void
RenderBuffer::Resolve()
{
#   ifdef DEBUG_MSG
    std::cerr << ">> RenderBuffer.cc RenderBuffer::Resolve()\n";
#   endif

    // std::cout << "Resolve " << GetId() << std::endl;
    if (not bound()) {
        // Houdini does this, so don't print a message:
        //Logger::error(GetId(), ": buffer is not bound");
        return;
    }

    // See if the old image is ok
    // Return what we already have (which might be the initial buffer set by Allocate)
    Renderer& renderer = mRenderDelegate->renderer();
    if (not renderer.resolve(mRenderOutput, pd))
        return;

    // std::cout << GetId() << ' ' << pd.mChannels << " x " << pd.mWidth << " x " << pd.mHeight << " data=" << pd.mData << std::endl;

    switch (pd.mChannels) {
    case 1:
        if (isDepth()) {
            // Houdini is able to accept the linear depth buffer (see ../houdini/UsdRenderers.json)
            if (mRenderDelegate->isHoudini()) break;
            // Calculate the OpenGL style depth (near=0, far=1) from the linear depth
            const float n = mNear;
            const float f = mFar;
            const float A = ((f+n)/(f-n) + 1)/2;
            const size_t count = pd.mWidth * pd.mHeight;
            float* buffer = reinterpret_cast<float*>(pd.mData);
            pxr::WorkParallelForN(count, [buffer, n, A](size_t begin, size_t end) {
                    for (size_t i = begin; i < end; ++i) {
                        float z = buffer[i];
                        buffer[i] = z > n ? A * (1.0f - n/z) : 0.0f;
                    }
                });
        } else if (mFormat == pxr::HdFormatInt32) {
            // Convert float to int, for ids.
            const size_t count = pd.mWidth * pd.mHeight;
            const float* inbuffer = reinterpret_cast<const float*>(pd.mData);
            int32_t* outbuffer;
            if (mMoreOutputs[0]) {
                intBuffer.resize(count);
                outbuffer = intBuffer.data();
            } else { // write over float data with ints
                outbuffer = reinterpret_cast<int*>(pd.mData);
            }
            const float emptyValue = // usdview expects -1 in primId for empty areas
                mAovName == pxr::HdAovTokens->primId ? -1.0f : 0.0f;
            pxr::WorkParallelForN(count, [inbuffer, outbuffer, emptyValue](size_t begin, size_t end) {
                    for (size_t i = begin; i < end; ++i) {
                        if (std::isfinite(inbuffer[i]))
                            outbuffer[i] = inbuffer[i];
                        else
                            outbuffer[i] = emptyValue;
                    }
                });
            if (mMoreOutputs[0]) {
                // add other channels for higher-level instancers
                for (size_t i = 0; i < INSTANCE_NESTING; ++i) {
                    renderer.resolve(mMoreOutputs[i], pd);
                    inbuffer = reinterpret_cast<const float*>(pd.mData);
                    pxr::WorkParallelForN(count, [inbuffer, outbuffer](size_t begin, size_t end) {
                            for (size_t i = begin; i < end; ++i) {
                                if (std::isfinite(inbuffer[i]))
                                    outbuffer[i] += int(inbuffer[i]);
                            }
                        });
                }
            }
            pd.mData = outbuffer;
        } else {
            mFormat = pxr::HdFormatFloat32;
        }
        break;
    case 2:
        mFormat = pxr::HdFormatFloat32Vec2;
        break;
    case 3:
        mFormat = pxr::HdFormatFloat32Vec3;
        break;
    case 4:
        mFormat = pxr::HdFormatFloat32Vec4;
#if PXR_VERSION >= 2008
        // these versions require alpha compositing over background color to be done by delegate
        if (clearValue[3] > 0.0f) {
            const size_t count = pd.mWidth * pd.mHeight;
            typedef float v4sf __attribute__ ((vector_size (16)));
            v4sf* buffer = reinterpret_cast<v4sf*>(pd.mData);
            if (clearValue[0] || clearValue[1] || clearValue[2] || clearValue[3] < 1.0f) {
                const v4sf& cv = reinterpret_cast<const v4sf&>(clearValue[0]);
                pxr::WorkParallelForN(count, [buffer, cv](size_t begin, size_t end) {
                    for (size_t i = begin; i < end; ++i) {
                        v4sf& pixel = *(buffer + i);
                        if (pixel[3] < 1.0f) {
                            if (pixel[3] > 0.0f) {
                                const float m = 1.0f - pixel[3];
                                pixel += cv * m;
                            } else {
                                pixel += cv;
                            }
                        }
                    }
                });
            } else {
                // composite is simpler when clearValue is opaque black
                pxr::WorkParallelForN(count, [buffer](size_t begin, size_t end) {
                    for (size_t i = begin; i < end; ++i) {
                        v4sf& pixel = *(buffer + i);
                        pixel[3] = 1.0f;
                    }
                });
            }
        }
#endif
        break;
    default:
        Logger::error(GetId(), ": unknown channel count ", pd.mChannels);
        break;
    }
}

void
RenderBuffer::Finalize(pxr::HdRenderParam* renderParam)
{
#   ifdef DEBUG_MSG
    std::cerr << ">> RenderBuffer.cc RenderBuffer::Finalize()\n";
#   endif

    // std::cout << "RenderBuffer::Finalize " << GetId() << std::endl;
    if (mRenderOutput) {
        RenderDelegate& renderDelegate(RenderDelegate::get(renderParam));
        {   UpdateGuard guard(renderDelegate, mRenderOutput);
            mRenderOutput->setActive(false);
        }
        if (mMoreOutputs[0]) {
            for (size_t i = 0; i < INSTANCE_NESTING; ++i) {
                UpdateGuard guard(mMoreOutputs[i]);
                mMoreOutputs[i]->setActive(false);
            }
        }
    }
    HdRenderBuffer::Finalize(renderParam);
}

// Called *after* Finalize()
void
RenderBuffer::_Deallocate()
{
#   ifdef DEBUG_MSG
    std::cerr << ">> RenderBuffer.cc RenderBuffer::_Deallocate()\n";
#   endif

    // std::cout << "RenderBuffer::Deallocate " << GetId() << std::endl;
    if (pd.mData) {
        mRenderDelegate->renderer().deallocate(mRenderOutput, pd);
        pd.vec.clear();
        pd.vpb.cleanUp();
        intBuffer.clear();
        pd.mData = nullptr;
    }
}

}
