// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#include "RenderDelegate.h"
#include "BasisCurves.h"
#include "Camera.h"
#include "CoordSys.h"
#include "Instancer.h"
#include "Light.h"
#include "LightFilter.h"
#include "Material.h"
#include "Mesh.h"
#include "Points.h"
#include "Procedural.h"
#include "RenderBuffer.h"
#include "Renderer.h"
#include "RenderPass.h"
#include "Volume.h"

#include <pxr/imaging/hd/extComputation.h>
#include <pxr/base/gf/vec2f.h>
#include <pxr/usdImaging/usdImaging/delegate.h>

#include <scene_rdl2/common/except/exceptions.h>
#include <scene_rdl2/render/logging/logging.h>
#include <scene_rdl2/scene/rdl2/GeometrySet.h>
#include <scene_rdl2/scene/rdl2/Layer.h>
#include <scene_rdl2/scene/rdl2/Layer.h>
#include <scene_rdl2/scene/rdl2/Light.h>
#include <scene_rdl2/scene/rdl2/LightFilter.h>
#include <scene_rdl2/scene/rdl2/LightFilterSet.h>
#include <scene_rdl2/scene/rdl2/LightSet.h>
#include <scene_rdl2/scene/rdl2/Map.h>
#include <scene_rdl2/scene/rdl2/Material.h>
#include <scene_rdl2/scene/rdl2/ShadowSet.h>
#include <scene_rdl2/scene/rdl2/VolumeShader.h>

#include <scene_rdl2/render/logging/logging.h>

#include <iostream>
#include <cstdlib>

//#define DEBUG_MSG

namespace {

    // Testing is much easier if the names of code-generated objects are 
    // consistent from run to run. Given the nature of Hydra, 
    // we can't rely on objects to always be processed in the same order.
    // Therefore we have to name them using a content hash, rather than 
    // sequentially. The hash algorithm has to produces the same value
    // from run-to-run, with collisions very unlikely.
    constexpr size_t HASH_M = 2<<24 - 3; // prime, fits in 6 hex digits
    constexpr size_t HASH_P = 127; 

    size_t repeatableStringHash(const std::string& s)
    {
        // standard polynomial rolling algoritm
        size_t hash = 0;
        size_t p = 1;
        for (unsigned char c : s) {
            hash = (hash + c * p) % HASH_M;
            p = (p * HASH_P) % HASH_M;
        }
        return hash;
    }

    unsigned hashObjectSet(std::set<scene_rdl2::rdl2::SceneObject*> objSet)
    {
        unsigned hash = 0;
        for (auto& obj : objSet)
            hash += repeatableStringHash(obj->getName());
        return hash;
    }
}
namespace hdMoonray {

using scene_rdl2::logging::Logger;

RenderDelegate::RenderDelegate(Renderer* renderer)
    : HdRenderDelegate(),
      mRenderer(renderer),
      mRenderSettings(*this)
{
    _constructor();
}

RenderDelegate::RenderDelegate(Renderer* renderer, pxr::HdRenderSettingsMap const& settings)
    : HdRenderDelegate(settings),
      mRenderer(renderer),
      mRenderSettings(*this)
{
    _constructor();
}

void
RenderDelegate::_constructor()
{
    mRenderSettings.addDescriptors(mRenderSettingDescriptors);
    mRenderer->addDescriptors(mRenderSettingDescriptors);
    _PopulateDefaultSettings(mRenderSettingDescriptors);
    renderParam.This = this;
    initializeSceneContext();
}

RenderDelegate::~RenderDelegate()
{
    delete mRenderer;
}

void
RenderDelegate::CommitResources(pxr::HdChangeTracker *tracker) {}

#if PXR_VERSION >= 2108
pxr::HdCommandDescriptors RenderDelegate::GetCommandDescriptors() const
{
    pxr::HdCommandDescriptors descriptors;
    descriptors.emplace_back(pxr::TfToken("reload_textures"), "Reload textures");
    descriptors.emplace_back(pxr::TfToken("restart_arras"), "Restart Arras");
    descriptors.emplace_back(pxr::TfToken("output_rdl"), "Output Rdl");
    return descriptors;
}

bool RenderDelegate::InvokeCommand(const pxr::TfToken& command, const pxr::HdCommandArgs& args)
{
    if (command == pxr::TfToken("reload_textures")) {
        mRenderer->invalidateAllTextureResources();
        return true;
    } else if (command == pxr::TfToken("restart_arras")) {
        mRenderer->restartRenderer();
        return true;
    } else if (command == pxr::TfToken("output_rdl")) {
        mRenderer->outputRdl(rdlOutput());
        return true;
    }
    return false;
}
#endif

static pxr::TfToken proceduralToken("procedural");
static pxr::TfToken geometryLightToken("geometryLight");
#if PXR_VERSION >= 2005
# define lightFilterToken (pxr::HdPrimTypeTokens->lightFilter)
#else
static pxr::TfToken lightFilterToken("lightFilter");
#endif
// This token is repeated from usdVolImaging which we cannot access from here.
static pxr::TfToken openvdbAssetToken("openvdbAsset");

pxr::TfTokenVector const&
RenderDelegate::GetSupportedRprimTypes() const
{
    static const pxr::TfTokenVector SUPPORTED_RPRIM_TYPES = {
        pxr::HdPrimTypeTokens->mesh,
        pxr::HdPrimTypeTokens->basisCurves,
        pxr::HdPrimTypeTokens->points,
        pxr::HdPrimTypeTokens->volume,
        proceduralToken
    };
    return SUPPORTED_RPRIM_TYPES;
}


pxr::TfTokenVector const&
RenderDelegate::GetSupportedSprimTypes() const
{
    static const pxr::TfTokenVector SUPPORTED_SPRIM_TYPES = {
        pxr::HdPrimTypeTokens->camera,
        //pxr::HdPrimTypeTokens->drawTarget, // used by hdSt to return OpenGL textures
        pxr::HdPrimTypeTokens->material,
        pxr::HdPrimTypeTokens->coordSys,
        //pxr::HdPrimTypeTokens->simpleLight,
        pxr::HdPrimTypeTokens->cylinderLight,
        pxr::HdPrimTypeTokens->diskLight,
        pxr::HdPrimTypeTokens->distantLight,
        pxr::HdPrimTypeTokens->domeLight,
        pxr::HdPrimTypeTokens->rectLight,
        pxr::HdPrimTypeTokens->sphereLight,
        geometryLightToken,
        lightFilterToken,
        pxr::HdPrimTypeTokens->extComputation,
    };
    return SUPPORTED_SPRIM_TYPES;
}

pxr::TfTokenVector const&
RenderDelegate::GetSupportedBprimTypes() const
{
    static const pxr::TfTokenVector SUPPORTED_BPRIM_TYPES = {
        pxr::HdPrimTypeTokens->renderBuffer,
        openvdbAssetToken,
    };
    return SUPPORTED_BPRIM_TYPES;
}

pxr::HdResourceRegistrySharedPtr
RenderDelegate::GetResourceRegistry() const
{
    static pxr::HdResourceRegistrySharedPtr ptr;
    if (not ptr) ptr.reset(new pxr::HdResourceRegistry());
    return ptr;
}

// Result of this is passed to RenderBuffer::Allocate
pxr::HdAovDescriptor
RenderDelegate::GetDefaultAovDescriptor(pxr::TfToken const& name) const
{
    return RenderBuffer::getAovDescriptor(name);
}

pxr::VtDictionary
RenderDelegate::GetRenderStats() const
{
    pxr::VtDictionary stats;
    if (mRenderer && !mRenderer->isFrameComplete()) {
        auto progress = mRenderer->getProgress();
        // This is the only value usdview reads, you have to turn on "View/Heads-Up Display/GPU Stats" to see it
        stats[pxr::HdPerfTokens->numCompletedSamples] = int(progress * 100000);
        // Values used by Houdini:
        // See $HFS/toolkit/include/HUSD/XUSD_Tokens.h for full list
        static const pxr::TfToken percentDone("percentDone");
        stats[percentDone] = progress * 100;
        static const pxr::TfToken totalClockTime("totalClockTime");
        stats[totalClockTime] = mRenderer->getElapsedSeconds();
        static const pxr::TfToken rpAnn("renderProgressAnnotation");
        const std::string& status = mRenderer->getStatusString();
        if (!status.empty()) {
            stats[rpAnn] = status;
        }
    }
    return stats;
}

bool
RenderDelegate::IsPauseSupported() const
{
    return true;
}

bool
RenderDelegate::Pause()
{
    if (mRenderer) mRenderer->pause();
    return true;
}

bool
RenderDelegate::Resume()
{
    if (mRenderer) mRenderer->resume();
    return true;
}

pxr::HdRenderPassSharedPtr
RenderDelegate::CreateRenderPass(pxr::HdRenderIndex *renderIndex,
                                 pxr::HdRprimCollection const& collection)
{
    mRenderIndex = renderIndex;
    return pxr::HdRenderPassSharedPtr(new RenderPass(renderIndex, collection, this));
}

pxr::HdInstancer*
RenderDelegate::CreateInstancer(pxr::HdSceneDelegate *sceneDelegate,
                                pxr::SdfPath const& id)
{
    return new Instancer(sceneDelegate, id);
}

void
RenderDelegate::DestroyInstancer(pxr::HdInstancer *instancer)
{
    delete instancer;
}

pxr::HdRprim*
RenderDelegate::CreateRprim(pxr::TfToken const& typeId,
                            pxr::SdfPath const& rprimId)
{
    if (typeId == pxr::HdPrimTypeTokens->mesh) {
        return new Mesh(rprimId);
    } else if (typeId == pxr::HdPrimTypeTokens->basisCurves) {
        return new BasisCurves(rprimId);
    } else  if (typeId == pxr::HdPrimTypeTokens->points) {
        return new Points(rprimId);
    } else  if (typeId == pxr::HdPrimTypeTokens->volume) {
        auto p = new Volume(rprimId);
        if (not rprimId.IsEmpty())
            mVolumes.insert(p);
        return p;
    } else  if (typeId == proceduralToken) {
        auto p = new Procedural(rprimId);
        if (not rprimId.IsEmpty())
            mProcedurals.insert(p);
        return p;
    } else {
        Logger::warn(rprimId, ": unknown Rprim type ", typeId);
        return nullptr;
    }
}

void
RenderDelegate::DestroyRprim(pxr::HdRprim *rPrim)
{
    mProcedurals.erase(rPrim);
    mVolumes.erase(rPrim);

    delete rPrim;
}

pxr::HdSprim*
RenderDelegate::CreateSprim(pxr::TfToken const& typeId,
                            pxr::SdfPath const& sprimId)
{
    if (typeId == pxr::HdPrimTypeTokens->camera) {
        return new Camera(sprimId);
    } else  if (typeId == pxr::HdPrimTypeTokens->material) {
        return new Material(sprimId);
    } else  if (typeId == pxr::HdPrimTypeTokens->coordSys) {
        return new CoordSys(sprimId);
    } else if (typeId == pxr::HdPrimTypeTokens->extComputation) {
        return new pxr::HdExtComputation(sprimId); // no subclass needed
    } else if (typeId == lightFilterToken) {
        return new LightFilter(typeId,sprimId);
    } else if (Light::isSupportedType(typeId)) {
        auto p = new Light(typeId, sprimId);
        if (not sprimId.IsEmpty())
            mLights.insert(p);
        return p;
    }
    Logger::warn(sprimId, ": unknown Sprim type ", typeId);
    return nullptr;
}

pxr::HdSprim *
RenderDelegate::CreateFallbackSprim(pxr::TfToken const& typeId)
{
    // all other sprims set the default values in the constructor
    return CreateSprim(typeId, pxr::SdfPath::EmptyPath());
}

void
RenderDelegate::DestroySprim(pxr::HdSprim *sPrim)
{
    mLights.erase(sPrim);
    delete sPrim;
}

pxr::HdBprim *
RenderDelegate::CreateBprim(pxr::TfToken const& typeId,
                            pxr::SdfPath const& bprimId)
{
    if (typeId == pxr::HdPrimTypeTokens->renderBuffer) {
        return new RenderBuffer(bprimId);
    } else if (typeId == openvdbAssetToken) {
        return new OpenVdbAsset(bprimId);
    } else {
        Logger::warn(bprimId, ": unknown Bprim type ", typeId);
        return nullptr;
    }
}

pxr::HdBprim *
RenderDelegate::CreateFallbackBprim(pxr::TfToken const& typeId)
{
    // all bprims set the default values in the constructor
    return CreateBprim(typeId, pxr::SdfPath::EmptyPath());
}

void
RenderDelegate::DestroyBprim(pxr::HdBprim *bPrim)
{
    delete bPrim;
}

void
RenderDelegate::setSceneDelegate(pxr::HdSceneDelegate* sceneDelegate)
{
    if (not mUsdImagingDelegate) {
        mUsdImagingDelegate = dynamic_cast<pxr::UsdImagingDelegate*>(sceneDelegate);
    }
}

////////////////////////////////////////////////////////////////////////////////

Renderer&
RenderDelegate::getRendererApplySettings()
{
    unsigned v = GetRenderSettingsVersion();
    if (v != mPreviousRenderSettings) {
        mPreviousRenderSettings = v;
        mRenderSettings.apply(); // cannot be called before renderer exists
        mRenderer->applySettings(mRenderSettings);
        mRenderer->setIsHoudini(isHoudini());
    }
    return *mRenderer;
}

static bool
contains(const pxr::TfTokenVector& tags, const pxr::TfToken& tag)
{
    return std::find(tags.begin(), tags.end(), tag) != tags.end();
}

bool
RenderDelegate::setRenderTags(pxr::HdRenderIndex* index, const pxr::TfTokenVector& tags)
{
    if (tags == mRenderTags) return false;
    // fix Pixar bug https://github.com/PixarAnimationStudios/USD/issues/801
    // by turning off any rprims with a removed purpose
    pxr::TfTokenVector removed;
    for (auto& t : mRenderTags)
        if (not contains(tags, t)) removed.push_back(t);
    mRenderTags = tags;
    if (not removed.empty()) {
        for (auto& id : index->GetRprimIds()) {
            if (contains(removed, index->GetRenderTag(id)))
                (const_cast<pxr::HdRprim*>(index->GetRprim(id)))->Finalize(&renderParam);
        }
    }
    return true;
}

scene_rdl2::rdl2::SceneObject*
RenderDelegate::createSceneObject(const std::string& className, const pxr::SdfPath& id)
{
    return createSceneObject(className, id.GetString());
}

scene_rdl2::rdl2::SceneObject*
RenderDelegate::createSceneObject(const std::string& className, const std::string& id)
{
    // for now rely on sceneContext reusing existing object
    try {
        return acquireSceneContext().createSceneObject(className, id);
    } catch (const scene_rdl2::except::TypeError& e) {
        // assume this error is a className collision, try again with a different name
        Logger::info(e.what());
        return createSceneObject(className, id+className);
    } catch (const std::exception& e) {
        Logger::error(id, ": ", e.what());
        return nullptr;
    }
}

scene_rdl2::rdl2::SceneObject*
RenderDelegate::getSceneObject(const pxr::SdfPath& id)
{
    return getSceneObject(id.GetString());
}

scene_rdl2::rdl2::SceneObject*
RenderDelegate::getSceneObject(const std::string& id)
{
    try {
        return acquireSceneContext().getSceneObject(id);
    } catch (const std::exception& e) {
        // caller can print a more informative error message
        return nullptr;
    }
}

void
RenderDelegate::initializeSceneContext()
{
    scene_rdl2::rdl2::SceneContext& wsc = acquireSceneContext();

    mAllGeometry = wsc.createSceneObject("GeometrySet", "allGeometry")->asA<scene_rdl2::rdl2::GeometrySet>();
    mDefaultLayer = wsc.createSceneObject("Layer", "defaultLayer")->asA<scene_rdl2::rdl2::Layer>();
    scene_rdl2::rdl2::SceneVariables& wsv = wsc.getSceneVariables();
    {   UpdateGuard guard(wsv);
        wsv.set(wsv.sLayer, mDefaultLayer);
    }

    // create cameras[0] that can be moved around to match the active camera as it changes
    mPrimaryCamera = wsc.createSceneObject("PerspectiveCamera", "primaryCamera")->asA<scene_rdl2::rdl2::Camera>();
}

scene_rdl2::rdl2::Material*
RenderDelegate::defaultMaterial()
{
    if (not mDefaultMaterial) {
        std::lock_guard<std::mutex> lock(mCreateMutex);
        if (not mDefaultMaterial) {
            scene_rdl2::rdl2::SceneContext& wsc = acquireSceneContext();
            scene_rdl2::rdl2::Map* displayColor = wsc.createSceneObject("AttributeMap", "displayColor")->asA<scene_rdl2::rdl2::Map>();
            {   UpdateGuard guard(displayColor);
                displayColor->set("primitive_attribute_name", std::string("displayColor"));
                // displayColor->set("primitive_attribute_type", 3); // color
                displayColor->set("default_value", scene_rdl2::rdl2::Rgb(0.5f,0.5f,0.5f)); // default from hdSt/shaders/mesh.glslfx
            }
            scene_rdl2::rdl2::Map* displayOpacity = wsc.createSceneObject("AttributeMap", "displayOpacity")->asA<scene_rdl2::rdl2::Map>();
            {   UpdateGuard guard(displayOpacity);
                displayOpacity->set("primitive_attribute_name", std::string("displayOpacity"));
                displayOpacity->set("primitive_attribute_type", 0); // FLOAT
            }
            mDefaultMaterial = wsc.createSceneObject("UsdPreviewSurface", "defaultMaterial")->asA<scene_rdl2::rdl2::Material>();
            UpdateGuard guard(mDefaultMaterial);
            mDefaultMaterial->set("diffuseColor", scene_rdl2::rdl2::Rgb(1.0f,1.0f,1.0f));
            mDefaultMaterial->setBinding("diffuseColor", displayColor);
            mDefaultMaterial->setBinding("opacity", displayOpacity);
            mDefaultMaterial->set("roughness", 0.3f);
        }
    }
    return mDefaultMaterial;
}

scene_rdl2::rdl2::Material*
RenderDelegate::errorMaterial()
{
    if (not mErrorMaterial) {
        std::lock_guard<std::mutex> lock(mCreateMutex);
        if (not mErrorMaterial) {
            scene_rdl2::rdl2::SceneContext& wsc = acquireSceneContext();
            mErrorMaterial = wsc.createSceneObject("UsdPreviewSurface", "errorMaterial")->asA<scene_rdl2::rdl2::Material>();
            UpdateGuard guard(mErrorMaterial);
            mErrorMaterial->set("diffuseColor", scene_rdl2::rdl2::Rgb(1.0f, 0.0f, 1.0f));
        }
    }
    return mErrorMaterial;
}

scene_rdl2::rdl2::VolumeShader*
RenderDelegate::defaultVolumeShader()
{
    if (not mDefaultVolumeShader) {
        std::lock_guard<std::mutex> lock(mCreateMutex);
        if (not mDefaultVolumeShader) {
            scene_rdl2::rdl2::SceneContext& wsc = acquireSceneContext();
            mDefaultVolumeShader = wsc.createSceneObject("BaseVolume", "defaultVolumeShader")->asA<scene_rdl2::rdl2::VolumeShader>();
        }
    }
    return mDefaultVolumeShader;
}

// synchronous layer modifications
int
RenderDelegate::assign(scene_rdl2::rdl2::Geometry* geometry,
                       const scene_rdl2::rdl2::LayerAssignment& assignment)
{
    static const std::string nopart;
    return assign(geometry, nopart, assignment);
}

int
RenderDelegate::assign(scene_rdl2::rdl2::Geometry* geometry, const std::string& partName,
                       const scene_rdl2::rdl2::LayerAssignment& assignment)
{
    std::lock_guard<std::mutex> guard(layerMutex);
    if (partName.empty()) {
        UpdateGuard guard2(mAllGeometry);
        mAllGeometry->add(geometry);
    }
    UpdateGuard guard2(mDefaultLayer);
    return mDefaultLayer->assign(geometry, partName, assignment);
}

void
RenderDelegate::addUnassigned(scene_rdl2::rdl2::Geometry* geometry)
{
    std::lock_guard<std::mutex> guard(layerMutex);
    UpdateGuard guard2(mAllGeometry);
    mAllGeometry->add(geometry);
}

void
RenderDelegate::setCategory(scene_rdl2::rdl2::SceneObject* obj,
                            CategoryType type,
                            const pxr::TfToken& id)
{
    std::lock_guard<std::mutex> lock(mCategoriesMutex);
    mCategoryObjects[type][id].emplace(obj);
}

void
RenderDelegate::releaseCategory(scene_rdl2::rdl2::SceneObject* obj,
                                CategoryType type,
                                const pxr::TfToken& id)
{
    std::lock_guard<std::mutex> lock(mCategoriesMutex);
    mCategoryObjects[type][id].erase(obj);
}

void
RenderDelegate::updateAssignmentFromCategories(
    scene_rdl2::rdl2::LayerAssignment& assignment,
    const pxr::VtArray<pxr::TfToken>& categories)
{

    // Lock is not needed here as it appears Hydra has already called Sync() on all lights and filters
    const auto& mCategoryObjects = this->mCategoryObjects; // prevent non-const set operations

    // create the default light if there are no lights
    if (not mNumLights && not mDefaultLight) {        
        std::lock_guard<std::mutex> lock(mCreateMutex);
        if (not mDefaultLight) {
            scene_rdl2::rdl2::SceneContext& wsc = acquireSceneContext();
            scene_rdl2::rdl2::Light* defaultLight = wsc.createSceneObject("EnvLight", "defaultLight")->asA<scene_rdl2::rdl2::Light>();
            setCategory(defaultLight, CategoryType::LightLink, pxr::TfToken());
            setCategory(defaultLight, CategoryType::ShadowLink, pxr::TfToken());
            UpdateGuard guard(*this, defaultLight);
            defaultLight->set("max_shadow_distance", 100.0f);
            //defaultLight->set("sample_upper_hemisphere_only", true);
            mDefaultLight = defaultLight;
        }
    } 

    std::set<scene_rdl2::rdl2::SceneObject*> sets[CategoryType::COUNT];

    for (int type = 0; type < CategoryType::COUNT; ++type) {
        const auto& typeCategoryObjects = mCategoryObjects[type];
        auto i = typeCategoryObjects.find(pxr::TfToken()); // all uncategorized objects
        if (i != typeCategoryObjects.end()) sets[type] = i->second;
        for (auto& id : categories) {
            i = typeCategoryObjects.find(id);
            if (i != typeCategoryObjects.end()) {
                for (scene_rdl2::rdl2::SceneObject* item : i->second)
                    sets[type].insert(item);
            }
        }
    }

    // the set for shadows is inverted
    {std::set<scene_rdl2::rdl2::SceneObject*> noShadowSet;
    for (auto& i : sets[CategoryType::LightLink]) {
        if (not sets[CategoryType::ShadowLink].count(i))
            noShadowSet.insert(i);
    }
    std::swap(sets[CategoryType::ShadowLink], noShadowSet);}

    // compute a hash for each set
    unsigned hash[CategoryType::COUNT];
    for (size_t i = 0; i < CategoryType::COUNT; ++i) {
        hash[i] = hashObjectSet(sets[i]);
    }

    // Lock is needed so each set is only created once
    std::lock_guard<std::mutex> lock(mCategoriesMutex);

    if (sets[CategoryType::LightLink].empty()) {
        // must be set to a valid LightSet object, even if empty (MOONRAY-4854)
        if (not mEmptyLightSet) {
            scene_rdl2::rdl2::SceneContext& wsc = acquireSceneContext();
            mEmptyLightSet = wsc.createSceneObject("LightSet", "emptyLightSet")->asA<scene_rdl2::rdl2::LightSet>();
        }
        assignment.mLightSet = mEmptyLightSet;
    } else {
        unsigned h = hash[CategoryType::LightLink];
        scene_rdl2::rdl2::LightSet*& set = mLightSets[h];
        if (not set) {
            char name[20]; snprintf(name, 20, "LightSet%06X", h);
            set = createSceneObject("LightSet", name)->asA<scene_rdl2::rdl2::LightSet>();
            UpdateGuard guard(set);
            for (auto& i : sets[CategoryType::LightLink])
                set->add(i->asA<scene_rdl2::rdl2::Light>());
        }
        assignment.mLightSet = set;
    }
    if (sets[CategoryType::ShadowLink].empty()) {
        assignment.mShadowSet = nullptr;
    } else {
        unsigned h = hash[CategoryType::ShadowLink];
        scene_rdl2::rdl2::ShadowSet*& set = mShadowSets[h];
        if (not set) {
            char name[20]; snprintf(name, 20, "ShadowSet%06X", h);
            set = createSceneObject("ShadowSet", name)->asA<scene_rdl2::rdl2::ShadowSet>();
            UpdateGuard guard(set);
            for (auto& i : sets[CategoryType::ShadowLink])
                set->add(i->asA<scene_rdl2::rdl2::Light>());
        }
        assignment.mShadowSet = set;
    }
    if (sets[CategoryType::FilterLink].empty()) {
        assignment.mLightFilterSet = nullptr;
    } else {
        unsigned h = hash[CategoryType::FilterLink];
        scene_rdl2::rdl2::LightFilterSet*& set = mLightFilterSets[h];
        if (not set) {
            char name[20]; snprintf(name, 20, "LightFilterSet%06X", h);
            set = createSceneObject("LightFilterSet", name)->asA<scene_rdl2::rdl2::LightFilterSet>();
            UpdateGuard guard(set);
            for (auto& i : sets[CategoryType::FilterLink])
                set->add(i->asA<scene_rdl2::rdl2::LightFilter>());
        }
        assignment.mLightFilterSet = set;
    }
}

void RenderDelegate::addLight()
{
    std::lock_guard<std::mutex> lock(mCategoriesMutex); // reuse this lock to protect this
    if (++mNumLights == 1) {
        setDefaultLight(false);
    }
}

void RenderDelegate::removeLight()
{
    std::lock_guard<std::mutex> lock(mCategoriesMutex); // reuse this lock to protect this
    if (mNumLights == 0) {
        Logger::error("removeLight() called more often than addLight()");
    } else if (--mNumLights == 0) {
        setDefaultLight(true);
    }
}

void RenderDelegate::setDefaultLight(bool v)
{
    if (mDefaultLight) {
        UpdateGuard guard(*this, mDefaultLight);
        mDefaultLight->set(scene_rdl2::rdl2::Light::sOnKey, v);
    } else if (v) {
        // this will cause updateAssignmentFromCategories to create mDefaultLight:
        markAllRprimsDirty(pxr::HdChangeTracker::DirtyCategories);
    }
}

void RenderDelegate::setDisableLighting(bool v)
{
    if (v != mDisableLighting) {
        mDisableLighting = v;
        // dirty all the lights, which will cause them to call add/removeLight and
        // that will turn the default light on/off
        for (auto& p : mLights)
            mRenderIndex->GetChangeTracker().MarkSprimDirty(p->GetId(), pxr::HdLight::DirtyBits::AllDirty);
    }
}

void RenderDelegate::setPruneProcedural(const std::string& name, bool prune)
{
    if (prune != getPruneProcedural(name)) {
        if (prune) mPrunedProcedurals.insert(name);
        else mPrunedProcedurals.erase(name);
        for (auto& p : mProcedurals){
            if (mRenderIndex)
                mRenderIndex->GetChangeTracker().MarkRprimDirty(
                            p->GetId(),
                            pxr::HdChangeTracker::DirtyVisibility);
        }
    }
}

void RenderDelegate::setPruneVolume(bool v)
{
    if (v != mPruneVolume) {
        mPruneVolume = v;
        for (auto& p : mVolumes){
            if (mRenderIndex)
                mRenderIndex->GetChangeTracker().MarkRprimDirty(
                            p->GetId(),
                            pxr::HdChangeTracker::DirtyVisibility);
        }
    }
}

void RenderDelegate::setDecodeNormals(bool v)
{
    mDecodeNormalsChanged = false;
    if (v != mDecodeNormals) {
        mDecodeNormals = v;
        markAllRprimsDirty(pxr::HdChangeTracker::DirtyMaterialId);
        mDecodeNormalsChanged = true;
    }
}

void RenderDelegate::setDoubleSided(bool v)
{
    if (v != mDoubleSided) {
        mDoubleSided = v;
        markAllRprimsDirty(pxr::HdChangeTracker::DirtyDoubleSided);
    }
}

void RenderDelegate::setForcePolygon(bool v)
{
    if (v != mForcePolygon) {
        mForcePolygon = v;
        markAllRprimsDirty(pxr::HdChangeTracker::DirtySubdivTags);
    }
}

void RenderDelegate::markAllRprimsDirty(pxr::HdDirtyBits bits)
{
    if (mRenderIndex) mRenderIndex->GetChangeTracker().MarkAllRprimsDirty(bits);
}

}
