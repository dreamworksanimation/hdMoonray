// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file hd_usd2rdl.cc

// convert usd scene to rdl using hydra

#include "FreeCamera.h"
#include "RenderOptions.h"
#include "RenderSettings.h"
#include "SceneDelegate.h"

#include <scene_rdl2/common/platform/Platform.h>
#include <pxr/base/gf/frustum.h>
#include <pxr/base/tf/token.h>
#include <pxr/imaging/hd/engine.h>
#include <pxr/imaging/hd/renderBuffer.h>
#include <pxr/imaging/hd/rendererPlugin.h>
#include <pxr/imaging/hd/rendererPluginRegistry.h>
#include <pxr/imaging/hdx/renderTask.h>
#include <pxr/usd/usd/timeCode.h>
#include <pxr/usd/usdGeom/camera.h>
#include <pxr/usd/usdGeom/metrics.h>
#include <pxr/usdImaging/usdImaging/delegate.h>

#include <pxr/base/trace/collector.h>
#include <pxr/base/trace/reporter.h>
#include <fstream>

#include <scene_rdl2/common/rec_time/RecTime.h>

#include <iostream>
#include <memory>
#include <string>
#include <vector>

const char * const sRenderer = "Moonray";
const char * const sAov = "color";

pxr::GfFrustum
computeFrustumToFrameStage(const pxr::UsdStageRefPtr stage, float aspectRatio,
    pxr::UsdTimeCode timeCode, const pxr::TfTokenVector &includedPurposes);

int
main(int argc, char *argv[])
{
    // Parse arguments
    hd_usd2rdl::RenderOptions options(argc, argv);
    if (options.helpRequested()) {
        std::cout << options.usage(argv[0]);
        return 0;
    } else if (!options.allFlagsValid()){
        std::cerr << "Invalid option\n";
        std::cerr << options.usage(argv[0]);
        return -1;
    } else if (!options.getMissingRequiredFlags().empty()) {
        for (const std::string &flag : options.getMissingRequiredFlags()) {
            std::cerr << flag << " is a required option\n";
        }
        std::cerr << options.usage(argv[0]);
        return -1;
    }

    // Load the hydra renderer plugin and create the render delegate
    pxr::TfToken rendererId;
    pxr::HfPluginDescVector pluginDescriptors;
    pxr::HdRendererPluginRegistry &registry(pxr::HdRendererPluginRegistry::GetInstance());
    registry.GetPluginDescs(&pluginDescriptors);
    for (const pxr::HfPluginDesc &pluginDesc : pluginDescriptors) {
        if (pluginDesc.displayName == sRenderer) {
            rendererId = pluginDesc.id;
        }
    }
    
    if (rendererId.IsEmpty()) {
        std::cerr << "Unable to load " << sRenderer << " render plugin\n";
        return -1;
    }

    auto releasePlugin = [&](pxr::HdRendererPlugin *plugin) {
        registry.ReleasePlugin(plugin);
    };

    Py_Initialize(); // HDM-133: plugin loader assumes this has been done
    std::unique_ptr<pxr::HdRendererPlugin, decltype(releasePlugin)>
        rendererPlugin(registry.GetRendererPlugin(rendererId), releasePlugin);
    MNRY_ASSERT_REQUIRE(rendererPlugin);
    std::unique_ptr<pxr::HdRenderDelegate>
        renderDelegate(rendererPlugin->CreateRenderDelegate());
    MNRY_ASSERT_REQUIRE(renderDelegate);

    std::unique_ptr<pxr::HdRenderIndex>
#if PXR_VERSION >= 2005
        renderIndex(pxr::HdRenderIndex::New(renderDelegate.get(), {}));
#else
        renderIndex(pxr::HdRenderIndex::New(renderDelegate.get()));
#endif
    MNRY_ASSERT_REQUIRE(renderIndex);

    const pxr::TfToken aov(sAov);
    const pxr::HdAovDescriptor aovDesc = renderDelegate->GetDefaultAovDescriptor(aov);
    if (aovDesc.format == pxr::HdFormatInvalid) {
        std::cerr << "Can't render aov " << aov << '\n';
        return -1;
    }

    // Get the available render settings
    hd_usd2rdl::RenderSettings renderSettings(renderDelegate.get());
    if (options.getPrintRenderSettingsRequested()) {
        std::cout << "Available render settings:\n";
        std::cout << renderSettings.printAvailableSettings();
        return -1;
    }
    for (const hd_usd2rdl::RenderOptions::RenderSetting &setting : options.getRenderSettings()) {
        if (renderSettings.setRenderSetting(setting.first, setting.second)) {
            std::cerr << "Unable to set \"" << setting.first << "\" to \"" << setting.second << "\"\n";
            return -1;
        }
    }
    renderSettings.setRenderSetting("disableRender","true");
    renderSettings.setRenderSetting("rdlaOutput",options.getOutputRdlFile());

    pxr::TfTokenVector purposes = { pxr::UsdGeomTokens->default_ };
    for (const std::string p : options.getPurposes()) {
        const pxr::TfToken tp = pxr::TfToken(p);
        if (tp == pxr::UsdGeomTokens->render ||
            tp == pxr::UsdGeomTokens->proxy ||
            tp == pxr::UsdGeomTokens->guide) {
            purposes.push_back(tp);
        } else {
            std::cerr << "Unsupported purpose " << p << '\n';
            return -1;
        }
    }

    const pxr::SdfPath usdSceneDelegateId = pxr::SdfPath::AbsoluteRootPath();
    pxr::UsdImagingDelegate usdSceneDelegate(renderIndex.get(), usdSceneDelegateId);

    pxr::UsdTimeCode timeCode = pxr::UsdTimeCode::Default();
    if (options.getTimeType() == hd_usd2rdl::TimeType::Earliest) {
        timeCode = pxr::UsdTimeCode::EarliestTime();
    } else if (options.getTimeType() == hd_usd2rdl::TimeType::DoubleValue) {
        timeCode = options.getTime();
    }
    usdSceneDelegate.SetTime(timeCode);

    // This is the value set by "Complexity/Medium" in usdview:
    usdSceneDelegate.SetRefineLevelFallback(options.getRefineLevel());

    usdSceneDelegate.SetWindowPolicy(pxr::CameraUtilMatchHorizontally);

    pxr::UsdStageRefPtr stage = pxr::UsdStage::Open(options.getInputSceneFile());

   if (!stage) {
        std::cerr << "Unable to open " << options.getInputSceneFile() << '\n';
        return -1;
    }
    const pxr::UsdPrim &pseudoRoot = stage->GetPseudoRoot();

    usdSceneDelegate.Populate(pseudoRoot);

    const pxr::TfToken collectionName(pxr::HdTokens->geometry); // collection name is "geometry"
    const pxr::HdReprSelector reprSelector(pxr::HdReprTokens->refined);
    pxr::HdRprimCollection collection(collectionName, reprSelector);
    collection.SetRootPath(pseudoRoot.GetPath());

    const pxr::SdfPath appSceneDelegateId =
        pxr::SdfPath::AbsoluteRootPath().AppendChild(pxr::TfToken("app_scene"));
    hd_usd2rdl::SceneDelegate appSceneDelegate(renderIndex.get(), appSceneDelegateId);

    if (options.getCamera().empty()) {
        // Define a free camera
        // and populate the app scene delegate.
        hd_usd2rdl::FreeCamera camera(stage,
                                      float(options.getHeight()) / float(options.getWidth()),
                                      timeCode,
                                      purposes);
        appSceneDelegate.populate(camera,
                                  options.getWidth(), options.getHeight(),
                                  collection,
                                  purposes,
                                  aov, aovDesc);
    } else {
        const pxr::SdfPath hdCameraId = usdSceneDelegateId.AppendPath(pxr::SdfPath(options.getCamera()));

        // Set the sampling camera for motion blur
        usdSceneDelegate.SetCameraForSampling(hdCameraId);

        // Verify that the camera exists in hydra, then populate the app scene delegate
        // using this camera.
        if (!renderIndex->GetSprim(pxr::HdPrimTypeTokens->camera, hdCameraId)) {
            std::cerr << "Could not find camera " << options.getCamera() << " in " << stage << '\n';
            return -1;
        }
        appSceneDelegate.populate(hdCameraId,
                                  options.getWidth(), options.getHeight(),
                                  collection,
                                  purposes,
                                  aov, aovDesc);
    }

    // Render
    pxr::HdEngine engine;
    pxr::HdTaskSharedPtrVector tasks = { renderIndex->GetTask(appSceneDelegate.getTaskId()) };
    pxr::HdxRenderTask *renderTask = static_cast<pxr::HdxRenderTask *>(tasks[0].get());
    MNRY_ASSERT_REQUIRE(renderTask != nullptr);

    do {
        engine.Execute(renderIndex.get(), &tasks);
    } while (!renderTask->IsConverged());

    return 0;
}

