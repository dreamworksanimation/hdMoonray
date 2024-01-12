// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "RenderSettings.h"

#include <pxr/imaging/hd/renderDelegate.h>
#include <pxr/usdImaging/usdImaging/delegate.h>

namespace scene_rdl2 {namespace rdl2 {
class Camera;
class Geometry;
class GeometrySet;
class Layer;
class LayerAssignment;
class LightSet;
class Material;
class SceneObject;
class LayerAssignment;
class VolumeShader;
} }

namespace hdMoonray {

class Renderer;

/// This is the object created by RendererPlugin that implements all
/// the actual API. Hydra will create one or more of these as it needs renders.
class RenderDelegate final: public pxr::HdRenderDelegate
{
public:
    /// Render delegate constructor. Creates scene_rdl2::rndr::RenderContext
    /// and links moonray error handling to hydra error handling.
    RenderDelegate();
    RenderDelegate(pxr::HdRenderSettingsMap const& settings);

    /// Destroys the scene_rdl2::rndr::RenderContext
    ~RenderDelegate();

    /// Return this delegate's render param.
    pxr::HdRenderParam *GetRenderParam() const override { return const_cast<RenderParam*>(&renderParam); }
    /// Turn RenderParam back into RenderDelegate
    static RenderDelegate& get(pxr::HdRenderParam* p) { return *(((RenderParam*)p)->This); }

    const pxr::TfTokenVector &GetSupportedRprimTypes() const override;
    const pxr::TfTokenVector &GetSupportedSprimTypes() const override;
    const pxr::TfTokenVector &GetSupportedBprimTypes() const override;

    /// Returns the HdResourceRegistry instance used by this render delegate.
    pxr::HdResourceRegistrySharedPtr GetResourceRegistry() const override;

    /// Returns a list of user-configurable render settings.
    /// This is a reflection API for the render settings dictionary; it need
    /// not be exhaustive, but can be used for populating application settings
    /// UI.
    pxr::HdRenderSettingDescriptorList GetRenderSettingDescriptors() const override
    { return RenderSettings::getDescriptors(); }

    /// Return true to indicate that pausing and resuming are supported.
    bool IsPauseSupported() const override;

    /// Pause background rendering threads.
    bool Pause() override;

    /// Resume background rendering threads.
    bool Resume() override;

    /// Create a renderpass. Hydra renderpasses are responsible for drawing
    /// a subset of the scene (specified by the "collection" parameter) to the
    /// current framebuffer. This class creates objects of type
    /// RenderPass, which draw using moonray's raycasting API.
    ///   \param index The render index this renderpass will be bound to.
    ///   \param collection A specifier for which parts of the scene should
    ///                     be drawn.
    ///   \return An moonray renderpass object.
    pxr::HdRenderPassSharedPtr CreateRenderPass(
        pxr::HdRenderIndex *index,
        pxr::HdRprimCollection const& collection) override;

    /// Create an instancer. Hydra instancers store data needed for an
    /// instanced object to draw itself multiple times.
    /// \param id The unique identifier of this instancer.
    /// \return A pointer to the new instancer or nullptr on error.
    pxr::HdInstancer *CreateInstancer(pxr::HdSceneDelegate *delegate,
                                      pxr::SdfPath const& id
#if PXR_VERSION < 2102
                                      , pxr::SdfPath const& instancerId
#endif
    ) override;

    /// Destroy an instancer created with CreateInstancer.
    ///   \param instancer The instancer to be destroyed.
    void DestroyInstancer(pxr::HdInstancer *instancer) override;

    /// Create a hydra Rprim, representing scene geometry. This class creates
    /// moonray-specialized geometry containers like Mesh which map
    /// scene data to moonray scene graph objects.
    /// \param typeId the type identifier of the prim to allocate
    /// \param rprimId a unique identifier for the prim
    /// \return A pointer to the new prim or nullptr on error.
    pxr::HdRprim *CreateRprim(pxr::TfToken const& typeId,
                              pxr::SdfPath const& rprimId
#if PXR_VERSION < 2102
                              , pxr::SdfPath const& instancerId
#endif
    ) override;

    /// Destroy an Rprim created with CreateRprim.
    ///   \param rPrim The rprim to be destroyed.
    void DestroyRprim(pxr::HdRprim *rPrim) override;

    /// Create a hydra Sprim, representing scene or viewport state like cameras
    /// or lights.
    ///   \param typeId The sprim type to create. This must be one of the types
    ///                 from GetSupportedSprimTypes().
    ///   \param sprimId The scene graph ID of this sprim, used when pulling
    ///                  data from a scene delegate.
    ///   \return An moonray sprim object.
    pxr::HdSprim *CreateSprim(pxr::TfToken const& typeId,
                              pxr::SdfPath const& sprimId) override;

    /// Create a hydra Sprim using default values, and with no scene graph
    /// binding.
    ///   \param typeId The sprim type to create. This must be one of the types
    ///                 from GetSupportedSprimTypes().
    ///   \return An moonray fallback sprim object.
    pxr::HdSprim *CreateFallbackSprim(pxr::TfToken const& typeId) override;

    /// Destroy an Sprim created with CreateSprim or CreateFallbackSprim.
    ///   \param sPrim The sprim to be destroyed.
    void DestroySprim(pxr::HdSprim *sPrim) override;

    /// Create a hydra Bprim, representing data buffers such as textures.
    ///   \param typeId The bprim type to create. This must be one of the types
    ///                 from GetSupportedBprimTypes().
    ///   \param bprimId The scene graph ID of this bprim, used when pulling
    ///                  data from a scene delegate.
    ///   \return An moonray bprim object.
    pxr::HdBprim *CreateBprim(pxr::TfToken const& typeId,
                              pxr::SdfPath const& bprimId) override;

    /// Create a hydra Bprim using default values, and with no scene graph
    /// binding.
    ///   \param typeId The bprim type to create. This must be one of the types
    ///                 from GetSupportedBprimTypes().
    ///   \return An moonray fallback bprim object.
    pxr::HdBprim *CreateFallbackBprim(pxr::TfToken const& typeId) override;

    /// Destroy a Bprim created with CreateBprim or CreateFallbackBprim.
    ///   \param bPrim The bprim to be destroyed.
    void DestroyBprim(pxr::HdBprim *bPrim) override;

    /// This function is called after new scene data is pulled during prim
    /// Sync(), but before any tasks (such as draw tasks) are run, and gives the
    /// render delegate a chance to transfer any invalidated resources to the
    /// rendering kernel.
    ///   \param tracker The change tracker passed to prim Sync().
    void CommitResources(pxr::HdChangeTracker *tracker) override;

    /// This function tells the scene which material variant to reference.
    /// Moonray doesn't currently use materials but raytraced backends generally
    /// specify "full".
    pxr::TfToken GetMaterialBindingPurpose() const override { return pxr::HdTokens->full; }

    /// This functions tell the scene delegate the namespace selector we want to use to
    /// define our material networks.
    /// For example, this function returns "moonray", so the material surface output attribute we see
    /// will be the one called "outputs:moonray:surface.connect"
    pxr::TfToken GetMaterialNetworkSelector() const override
        { static pxr::TfToken tt("moonray"); return tt; }

    /// This function returns the default AOV descriptor for a given named AOV.
    /// This mechanism lets the renderer decide things like what format
    /// a given AOV will be written as.
    ///   \param name The name of the AOV whose descriptor we want.
    ///   \return A descriptor specifying things like what format the AOV
    ///           output buffer should be.
    pxr::HdAovDescriptor
        GetDefaultAovDescriptor(pxr::TfToken const& name) const override;

    /// This function allows the renderer to report back some useful statistics
    /// that the application can display to the user.
    pxr::VtDictionary GetRenderStats() const override;

/// Moonray-specific api /////////////////////////////////////////////////

    /// Remember the time of first call to delegate, for startup time report
    void setStartTime() const; // implementation in RenderPass.cc

    /// The Renderer hides the different backends, such as renderfarm vs local
    Renderer& renderer() const { return *mActiveRenderer; }

    /// Create or update the renderer to match current settings. This is fast
    /// if no settings have changed. You must call this before renderer().
    Renderer& getRendererApplySettings();

    // Fix for Pixar bug https://github.com/PixarAnimationStudios/USD/issues/801
    bool setRenderTags(pxr::HdRenderIndex* index, const pxr::TfTokenVector&);

    const scene_rdl2::rdl2::SceneContext& sceneContext() { return *mSceneContext; }

    void beginUpdate() { if (mActiveRenderer) mActiveRenderer->beginUpdate(); }

    /// Stop the render (if running) and return reference to writable scene context
    scene_rdl2::rdl2::SceneContext& acquireSceneContext() { beginUpdate(); return *mSceneContext; }

    scene_rdl2::rdl2::SceneObject* createSceneObject(const std::string& className, const pxr::SdfPath& id);
    scene_rdl2::rdl2::SceneObject* createSceneObject(const std::string& className, const std::string& id);

    scene_rdl2::rdl2::SceneObject* getSceneObject(const pxr::SdfPath& id);
    scene_rdl2::rdl2::SceneObject* getSceneObject(const std::string& id);

    // Default material showing displayColor + displayOpacity
    scene_rdl2::rdl2::Material* defaultMaterial();
    // Bright magenta for showing materials with errors
    scene_rdl2::rdl2::Material* errorMaterial();
    // Default volume shader
    scene_rdl2::rdl2::VolumeShader* defaultVolumeShader();

    // Add geometry to layer, assigns material/lights.
    int assign(scene_rdl2::rdl2::Geometry* geometry,
               const scene_rdl2::rdl2::LayerAssignment& assignment);
    int assign(scene_rdl2::rdl2::Geometry* geometry,
               const std::string& partName,
               const scene_rdl2::rdl2::LayerAssignment& assignment);

    // Adds geometry with no assignment (used for mesh lights)
    void addUnassigned(scene_rdl2::rdl2::Geometry* geometry);

    // a "category" is a USD collection used to assign geometry to
    // lights, light shadows or light filters. A light may have two
    // category collections (LightLink and ShadowLink); a LightFilter
    // has one.
    enum CategoryType { LightLink, ShadowLink, FilterLink, COUNT };

    // associate the given category id with the given object (which
    // must be a light or light filter). If a light(filter) has no
    // collection for a CategoryType, an empty id is passed.
    void setCategory(scene_rdl2::rdl2::SceneObject* obj,
                     CategoryType type,
                     const pxr::TfToken& id);

    // categories should be released before the object that they
    // are associated with it destroyed
    void releaseCategory(scene_rdl2::rdl2::SceneObject* obj,
                         CategoryType type,
                         const pxr::TfToken& id);

    // Update the layer assignment to contain the correct
    // light set, shadow set and light filter set for the categories object is in.
    // scene_rdl2::rdl2::Set objects are created as necessary.
    void updateAssignmentFromCategories(scene_rdl2::rdl2::LayerAssignment&,
                                        const pxr::VtArray<pxr::TfToken>& categories);

    // Reusable camera
    scene_rdl2::rdl2::Camera* primaryCamera() const { return mPrimaryCamera; }

    const std::string& rdlaOutput() const { return mRdlaOutput; }
    void setRdlaOutput(const std::string& v) { mRdlaOutput = v; }

    // keep track of how many lights there are and turn on default light if zero
    void addLight();
    void removeLight();

    bool isHoudini() const { return mRenderSettings.mIsHoudini; }
    bool getDisableLighting() const { return mDisableLighting; }
    void setDisableLighting(bool v);
    bool isDoubleSided() const { return mDoubleSided; }
    void setDoubleSided(bool v);
    bool getDecodeNormalsChanged() const { return mDecodeNormalsChanged; }
    void setDecodeNormals(bool v);
    bool getEnableMotionBlur() const { return mEnableMotionBlur; }
    void setEnableMotionBlur(bool v) { mEnableMotionBlur = v; }

    bool getPruneWillow() const {return mPruneWillow;}
    bool getPruneVolume() const {return mPruneVolume;}
    bool getPruneFurDeform() const {return mPruneFurDeform;}
    bool getPruneCurveDeform() const {return mPruneCurveDeform;}
    bool getPruneWrapDeform() const {return mPruneWrapDeform;}
    bool getForcePolygon() const {return mForcePolygon;}

    void setPruneWillow(bool v);
    void setPruneVolume(bool v);
    void setPruneCurveDeform(bool v);
    void setPruneWrapDeform(bool v);
    void setPruneFurDeform(bool v);
    void setForcePolygon(bool v);

    //bool pruneProceduralChanged() const {return mPruneProceduralChanged;}

    const RenderSettings& renderSettings() const { return mRenderSettings; }

    void markAllRprimsDirty(pxr::HdDirtyBits bits);

    void setSceneDelegate(pxr::HdSceneDelegate*);
    pxr::UsdImagingDelegate* usdImagingDelegate() const { return mUsdImagingDelegate; }

private:
    RenderDelegate(const RenderDelegate &)             = delete;
    RenderDelegate &operator =(const RenderDelegate &) = delete;

    class RenderParam final: public pxr::HdRenderParam {
    public:
        RenderDelegate* This; // possibly this should be const
    } renderParam;

    void _constructor();

    std::unique_ptr<Renderer> mRndrRenderer;
    std::unique_ptr<Renderer> mNonRndrRenderer;
    Renderer* mActiveRenderer = nullptr;
    RenderSettings mRenderSettings;
    bool mDisableLighting = false;
    bool mDoubleSided = true;
    unsigned mPreviousRenderSettings = 0;
    bool mDecodeNormals = false;
    bool mDecodeNormalsChanged = false;
    bool mEnableMotionBlur = false;
    bool mPruneWillow = false;
    bool mPruneFurDeform = false;
    bool mPruneVolume = false;
    bool mPruneWrapDeform = false;
    bool mPruneCurveDeform = false;
    bool mForcePolygon = false;

    void initializeSceneContext(); // part of constructor
    scene_rdl2::rdl2::SceneContext* mSceneContext;
    scene_rdl2::rdl2::Camera* mPrimaryCamera = nullptr;
    scene_rdl2::rdl2::GeometrySet* mAllGeometry = nullptr;
    scene_rdl2::rdl2::Layer* mDefaultLayer = nullptr;
    scene_rdl2::rdl2::Material* mDefaultMaterial = nullptr;
    scene_rdl2::rdl2::Material* mErrorMaterial = nullptr;
    scene_rdl2::rdl2::VolumeShader* mDefaultVolumeShader = nullptr;
    scene_rdl2::rdl2::Light* mDefaultLight = nullptr;
    unsigned mNumLights = 0;
    std::mutex layerMutex; // protects defaultLayer

    // stores the mapping from category type+ids to the associated set of lights
    std::map<pxr::TfToken, std::set<scene_rdl2::rdl2::SceneObject*>> mCategoryObjects[CategoryType::COUNT];

    // Keep track of all the category assignments made
    std::map<size_t, scene_rdl2::rdl2::LightSet*> mLightSets;
    std::map<size_t, scene_rdl2::rdl2::ShadowSet*> mShadowSets;
    std::map<size_t, scene_rdl2::rdl2::LightFilterSet*> mLightFilterSets;

    std::mutex mCategoriesMutex;
    std::mutex mCreateMutex;

    std::string mRdlaOutput;
    pxr::TfTokenVector mRenderTags;
    pxr::HdRenderIndex *mRenderIndex = nullptr; // stored by CreateRenderPass

    std::set<pxr::HdSprim*> mLights;
    std::set<pxr::HdRprim*> mProcedurals;
    std::set<pxr::HdRprim*> mVolumes;
    void setDefaultLight(bool);

    pxr::UsdImagingDelegate* mUsdImagingDelegate = nullptr;
};

// Same as scene_rdl2::rdl2:::SceneObject::UpdateGuard but also stops the renderer
class UpdateGuard {
public:
    // this constructor stops the renderer
    UpdateGuard(RenderDelegate& r, scene_rdl2::rdl2::SceneObject* obj):
        mSceneObject(obj) { r.beginUpdate(); obj->beginUpdate(); }
    // only use this constructor if you know rendering is stopped already
    UpdateGuard(scene_rdl2::rdl2::SceneObject* obj):
        mSceneObject(obj) { obj->beginUpdate(); }
    UpdateGuard(scene_rdl2::rdl2::SceneObject& obj):
        mSceneObject(&obj) { obj.beginUpdate(); }
    ~UpdateGuard() { mSceneObject->endUpdate(); }
    UpdateGuard(const UpdateGuard&) = delete;
    UpdateGuard& operator=(const UpdateGuard&) = delete;
private:
    scene_rdl2::rdl2::SceneObject* mSceneObject;
};

}
