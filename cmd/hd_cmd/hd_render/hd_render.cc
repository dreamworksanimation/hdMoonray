// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file hd_render.cc

// render usd scene using hydra

#include "FreeCamera.h"
#include "OutputFile.h"
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
#include <pxr/usdImaging/usdImaging/delegate.h>

#include <pxr/base/trace/collector.h>
#include <pxr/base/trace/reporter.h>
#include <fstream>

#include <scene_rdl2/common/rec_time/RecTime.h>

#include <iostream>
#include <memory>
#include <string>
#include <vector>

//#define DEBUG_MSG

scene_rdl2::rec_time::RecTime recTime;
std::vector<std::pair<std::string,float>> stepTimes;

// Begins a code section that can be traced
// using "-trace name"
void beginTrace(const hd_render::RenderOptions opts,
                const std::string& name)
{
    recTime.start();
    if (opts.hasTraceOpt(name)) {
        pxr::TraceCollector::GetInstance().SetEnabled(true);
    }
}

// Ends a code section that can be traced
void endTrace(const hd_render::RenderOptions opts,
              const std::string& name)
{
    stepTimes.emplace_back(name, recTime.end());

    if (opts.hasTraceOpt(name)) {
        pxr::TraceCollector::GetInstance().SetEnabled(false);
        std::cout << "Writing " << name << " trace to "
                  << opts.getTraceFile(name) << " ..." << std::endl;
        std::ofstream report(opts.getTraceFile(name));
        if (opts.useChromeTraceFormat()) {
            pxr::TraceReporter::GetGlobalReporter()->ReportChromeTracing(report);
        } else {
            pxr::TraceReporter::GetGlobalReporter()->Report(report);
        }
        std::cout << "...done" << std::endl;
        pxr::TraceCollector::GetInstance().Clear();
        pxr::TraceReporter::GetGlobalReporter()->ClearTree();
    }
}

void writeTiming(std::ostream &out)
{

}

int
main(int argc, char *argv[])
{
    // Parse arguments
    hd_render::RenderOptions options(argc, argv);
    if (options.helpRequested()) {
        std::cout << options.usage(argv[0]);
        return 0;
    } else if (!options.allFlagsValid()){
        std::cerr << "Invalid option\n";
        std::cerr << options.usage(argv[0]);
        return -1;
    }

    // Load the hydra renderer plugin and create the render delegate
    pxr::TfToken rendererId;
    pxr::HfPluginDescVector pluginDescriptors;
    pxr::HdRendererPluginRegistry &registry(pxr::HdRendererPluginRegistry::GetInstance());
    registry.GetPluginDescs(&pluginDescriptors);
    for (const pxr::HfPluginDesc &pluginDesc : pluginDescriptors) {
        // skip "GL" since we don't have OpenGL setup
        if (pluginDesc.displayName == "GL") continue;
        if (pluginDesc.displayName == options.getRenderer()) {
            rendererId = pluginDesc.id;
        }
    }
    if (rendererId.IsEmpty()) {
        const bool helpRequested = options.getRenderer().empty();
        std::ostream &ost = helpRequested ? std::cout : std::cerr;
        if (!helpRequested) {
            std::cerr << "Unable to load the requested renderer \"" << options.getRenderer() << "\"\n";
        }
        ost << "Available renderers include:\n";
        for (const pxr::HfPluginDesc &pluginDesc : pluginDescriptors) {
            // skip "GL" since we don't have OpenGL setup
            if (pluginDesc.displayName == "GL") continue;
            ost << "\t" << pluginDesc.displayName << '\n';
        }
        return options.getRenderer().empty() ? 0 : -1;
    }
    auto releasePlugin = [&](pxr::HdRendererPlugin *plugin) {
        registry.ReleasePlugin(plugin);
    };

    beginTrace(options,"load_plugin");
    Py_Initialize(); // HDM-133: plugin loader assumes this has been done
    std::unique_ptr<pxr::HdRendererPlugin, decltype(releasePlugin)>
        rendererPlugin(registry.GetRendererPlugin(rendererId), releasePlugin);
    MNRY_ASSERT_REQUIRE(rendererPlugin);
    std::unique_ptr<pxr::HdRenderDelegate>
        renderDelegate(rendererPlugin->CreateRenderDelegate());
    MNRY_ASSERT_REQUIRE(renderDelegate);
    endTrace(options,"load_plugin");

    // Create the hydra render index and tie the render delegate to it.
    // There is a 1-1 relationship between render delegate and render index.
    std::unique_ptr<pxr::HdRenderIndex>
#if PXR_VERSION >= 2005
        renderIndex(pxr::HdRenderIndex::New(renderDelegate.get(), {}));
#else
        renderIndex(pxr::HdRenderIndex::New(renderDelegate.get()));
#endif
    MNRY_ASSERT_REQUIRE(renderIndex);

    // Does the renderer know how to render the requested aov?
    const pxr::TfToken aov(options.getAov());
    const pxr::HdAovDescriptor aovDesc = renderDelegate->GetDefaultAovDescriptor(aov);
    if (aovDesc.format == pxr::HdFormatInvalid) {
        std::cerr << options.getRenderer() << " can't render aov " << aov << '\n';
        return -1;
    }

    // Get the available render settings
    hd_render::RenderSettings renderSettings(renderDelegate.get());
    if (options.getPrintRenderSettingsRequested()) {
        std::cout << "Avaiable " << options.getRenderer() << " render settings:\n";
        std::cout << renderSettings.printAvailableSettings();
        return -1;
    }
    for (const hd_render::RenderOptions::RenderSetting &setting : options.getRenderSettings()) {
        if (renderSettings.setRenderSetting(setting.first, setting.second)) {
            std::cerr << "Unable to set \"" << setting.first << "\" to \"" << setting.second << "\"\n";
            return -1;
        }
    }

    // At this point, we can check if all required options were passed
    if (!options.getMissingRequiredFlags().empty()) {
        for (const std::string &flag : options.getMissingRequiredFlags()) {
            std::cerr << flag << " is a required option\n";
        }
        std::cerr << options.usage(argv[0]);
        return -1;
    }

    // Fill out the hydra "purpose".  We always include the default purpose.  We support 3
    // other purposes: "render", "proxy", and "guide"
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

    // In hydra, there can be a many to one relationship between
    // scene delegates and the render index.  The only restriction is that
    // any primitive in the render index can be backed by only a single
    // scene delegate.  For the primitives that are in the input usd
    // file, we'll use a UsdImaging scene delegate.  We'll root
    // everything in the usd file at the path "/"
    const pxr::SdfPath usdSceneDelegateId = pxr::SdfPath::AbsoluteRootPath();
    pxr::UsdImagingDelegate usdSceneDelegate(renderIndex.get(), usdSceneDelegateId);

    pxr::UsdTimeCode timeCode = pxr::UsdTimeCode::Default();
    if (options.getTimeType() == hd_render::TimeType::Earliest) {
        timeCode = pxr::UsdTimeCode::EarliestTime();
    } else if (options.getTimeType() == hd_render::TimeType::DoubleValue) {
        timeCode = options.getTime();
    }
    usdSceneDelegate.SetTime(timeCode);

    // This is the value set by "Complexity/Medium" in usdview:
    usdSceneDelegate.SetRefineLevelFallback(options.getRefineLevel());

    usdSceneDelegate.SetWindowPolicy(pxr::CameraUtilMatchHorizontally);

    beginTrace(options,"open_stage");
    pxr::UsdStageRefPtr stage = pxr::UsdStage::Open(options.getInputSceneFile());
    endTrace(options,"open_stage");

   if (!stage) {
        std::cerr << "Unable to open " << options.getInputSceneFile() << '\n';
        return -1;
    }
    const pxr::UsdPrim &pseudoRoot = stage->GetPseudoRoot();

    beginTrace(options,"populate");
    usdSceneDelegate.Populate(pseudoRoot);
    endTrace(options,"populate");

    // Build an Rprim collection of the primitives we want to render
    // For now, we'll just grab all the rprims in the usd scene
    const pxr::TfToken collectionName(pxr::HdTokens->geometry); // collection name is "geometry"
    const pxr::HdReprSelector reprSelector(pxr::HdReprTokens->refined);
    pxr::HdRprimCollection collection(collectionName, reprSelector);
    collection.SetRootPath(pseudoRoot.GetPath());

    // Build a second scene delegate to manage the free camera,
    // render buffer and render task.  We'll root
    // everything that is app specific at "/app_scene"
    const pxr::SdfPath appSceneDelegateId =
        pxr::SdfPath::AbsoluteRootPath().AppendChild(pxr::TfToken("app_scene"));
    hd_render::SceneDelegate appSceneDelegate(renderIndex.get(), appSceneDelegateId);

    // image size is size / res
    const unsigned int resWidth = static_cast<unsigned int>(
        static_cast<float>(options.getWidth()) / options.getRes());
    const unsigned int resHeight = static_cast<unsigned int>(
        static_cast<float>(options.getHeight()) / options.getRes());

    if (options.getCamera().empty()) {
        // Define a viewing frustum for the free camera
        // and populate the app scene delegate.
        hd_render::FreeCamera camera(stage,
                                     float(options.getHeight()) / float(options.getWidth()),
                                     timeCode,
                                     purposes);
        appSceneDelegate.populate(camera,
                                  resWidth, resHeight,
                                  collection,
                                  purposes,
                                  aov, aovDesc);
    } else {
        // Verify that the camera exists in hydra, then populate the app scene delegate
        // using this camera.
        const pxr::SdfPath hdCameraId = usdSceneDelegateId.AppendPath(pxr::SdfPath(options.getCamera()));
        if (!renderIndex->GetSprim(pxr::HdPrimTypeTokens->camera, hdCameraId)) {
            std::cerr << "Could not find camera " << options.getCamera() << " in " << stage << '\n';
            return -1;
        }
        appSceneDelegate.populate(hdCameraId,
                                  resWidth, resHeight,
                                  collection,
                                  purposes,
                                  aov, aovDesc);

        // Set the sampling camera for motion blur
        usdSceneDelegate.SetCameraForSampling(hdCameraId);
    }

    // Render
    pxr::HdEngine engine;
    pxr::HdTaskSharedPtrVector tasks = { renderIndex->GetTask(appSceneDelegate.getTaskId()) };
    pxr::HdxRenderTask *renderTask = static_cast<pxr::HdxRenderTask *>(tasks[0].get());
    MNRY_ASSERT_REQUIRE(renderTask != nullptr);

    beginTrace(options,"render");

#ifdef DEBUG_MSG
    std::cerr << ">> hd_render.cc start loop {\n";
    for (unsigned i = 0; ; ++i) {
        if (!i) std::cerr << ">> hd_render.cc before Execute() {\n";
        engine.Execute(renderIndex.get(), &tasks);
        if (!i) std::cerr << ">> hd_render.cc } after Execute()\n";
        std::cerr << ">> hd_render.cc before isConverged() {\n";
        bool flag = renderTask->IsConverged();
        std::cerr << ">> hd_render.cc } after isConverged() flag:" << (flag ? "true" : "false") << '\n';

        if (flag) break;
    }
    std::cerr << ">> hd_render.cc } end loop\n";
#else // else DEBUG_MSG
    do {
        engine.Execute(renderIndex.get(), &tasks);
    } while (!renderTask->IsConverged());
#endif // end !DEBUG_MSG

    endTrace(options,"render");

#ifdef DEBUG_MSG
    std::cerr << ">> hd_render.cc before getBprim()\n";
#endif // end DEBUG_MSG
    // Get the buffer primitive from the render index
    pxr::HdRenderBuffer *renderBuffer = dynamic_cast<pxr::HdRenderBuffer *>
        (renderIndex->GetBprim(pxr::HdPrimTypeTokens->renderBuffer, appSceneDelegate.getRenderBufferId()));
#ifdef DEBUG_MSG
    std::cerr << ">> hd_render.cc after getBprim()\n";
#endif // end DEBUG_MSG
    hd_render::OutputFile outputFile(renderBuffer);
#ifdef DEBUG_MSG
    std::cerr << ">> hd_render.cc after outputFile() constructor\n";
#endif // end DEBUG_MSG

    if (outputFile.write(options.getOutputExrFile())) {
        std::cerr << "Failed to write " << options.getOutputExrFile() << '\n';
        return -1;
    }
    std::cout << "Wrote " << options.getOutputExrFile() << '\n';

    // Now handle deltas, if requested.  If the delta usd file is non-empty
    // or the delta set options are non-empty, then the output delta file
    // must be non-empty, and we should process the requested delta.
    const bool doDelta = !options.getDeltaOutputExrFile().empty() &&
        (!options.getDeltaInputSceneFile().empty() || !options.getDeltaRenderSettings().empty());
    if (options.getDeltaOutputExrFile().empty() &&
        (!options.getDeltaInputSceneFile().empty() || !options.getDeltaRenderSettings().empty())) {
        std::cerr << "-delta_out is required to render a delta image.\n";
        return -1;
    }
    if (!options.getDeltaOutputExrFile().empty() &&
        (options.getDeltaInputSceneFile().empty() && options.getDeltaRenderSettings().empty())) {
        std::cerr << "No delta information specified (use -delta_in and/or -delta_set)\n";
        return -1;
    }
    if (doDelta) {
        // First handle render settings
        for (hd_render::RenderOptions::RenderSetting setting : options.getDeltaRenderSettings()) {
            if (renderSettings.setRenderSetting(setting.first, setting.second)) {
                std::cerr << "Unable to set \"" << setting.first << "\" to \"" << setting.second << "\"\n";
                return -1;
            }
        }

        // Update the USD scene
        if (!options.getDeltaInputSceneFile().empty()) {
            // verify that we can open this file without error
            {
                pxr::UsdStageRefPtr deltaStage = pxr::UsdStage::Open(options.getDeltaInputSceneFile());
                if (!deltaStage) {
                    std::cerr << "Unable to open " << options.getDeltaInputSceneFile() << '\n';
                    return -1;
                }
            }
            beginTrace(options,"open_delta_stage");
            stage->GetSessionLayer()->InsertSubLayerPath(options.getDeltaInputSceneFile(), 0);
            usdSceneDelegate.ApplyPendingUpdates();
            endTrace(options,"open_delta_stage");
        }

        // Render
        beginTrace(options,"delta_render");
#       ifdef DEBUG_MSG
        std::cerr << ">> hd_render.cc delta start loop {\n";
        for (unsigned i = 0; ; ++i) {
            if (!i) std::cerr << ">> hd_render.cc before Execute() {\n";
            engine.Execute(renderIndex.get(), &tasks);            
            if (!i) std::cerr << ">> hd_render.cc } after Execute()\n";
            std::cerr << ">> hd_render.cc before IsConverged() {\n";
            bool flag = renderTask->IsConverged();
            std::cerr << ">> hd_render.cc } after IsConverged()\n";
            if (flag) break;
        }
        std::cerr << ">> hd_render.cc } delta end loop\n";
#       else // else DEBUG_MSG        
        do {
            engine.Execute(renderIndex.get(), &tasks);
        } while (!renderTask->IsConverged());
#       endif // end !DEBUG_MSG
        endTrace(options,"delta_render");

        // Write the delta output image
        if (outputFile.write(options.getDeltaOutputExrFile())) {
            std::cerr << "Failed to write " << options.getDeltaOutputExrFile() << '\n';
            return -1;
        }
        std::cout << "Wrote " << options.getDeltaOutputExrFile() << '\n';
    }

    if (options.isTimingEnabled()) {
        std::ostream* out;
        std::string timingFile = options.getTimingFile();
        if (timingFile.empty()) {
            out = &std::cout;
        } else {
            std::cout << "Writing timing to " << timingFile << std::endl;
            out = new std::ofstream(timingFile);
        }
        for (const auto& entry : stepTimes) {
            (*out) << entry.first << " " << entry.second << std::endl;
        }
        if (!timingFile.empty()) delete out;
    }
    return 0;
}

