// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#include "RndrRenderer.h"
#include <hydramoonray/NullRenderer.h>
#include <hydramoonray/RenderDelegate.h>
#include <pxr/imaging/hd/rendererPlugin.h>
#include <pxr/imaging/hd/rendererPluginRegistry.h>

#include <iostream>

PXR_NAMESPACE_OPEN_SCOPE // this does not work unless inside the pxr namespace

class HdMoonrayRendererDebugPlugin final : public pxr::HdRendererPlugin {
public:
    HdMoonrayRendererDebugPlugin() {}

    pxr::HdRenderDelegate *CreateRenderDelegate() override {
        return new hdMoonray::RenderDelegate(new hdMoonray::RndrRenderer(0));
    }

    pxr::HdRenderDelegate *CreateRenderDelegate(pxr::HdRenderSettingsMap const& settings) override {
        // "disableRender" and "threads" settings can only be specified at creation time
        // via this constructor
        auto it = settings.find(pxr::TfToken("disableRender"));
        if (it != settings.end()) {
            if (it->second.Get<bool>()) {
                return new hdMoonray::RenderDelegate(new hdMoonray::NullRenderer(),settings);
            }
        }
        
        uint32_t threads = 0;
        it = settings.find(pxr::TfToken("threads"));
        if (it != settings.end()) {
            threads = it->second.Get<int>();
        }
        return new hdMoonray::RenderDelegate(new hdMoonray::RndrRenderer(threads),settings);
    }

    void DeleteRenderDelegate(pxr::HdRenderDelegate *renderDelegate) override {
        delete renderDelegate;
    }

#if PXR_VERSION >= 2302
    bool IsSupported(bool gpuEnabled = true) const override {
        return true;
    }
#else
bool IsSupported() const override {
        return true;
    }
#endif

private:
    // uncopyable
    HdMoonrayRendererDebugPlugin(const HdMoonrayRendererDebugPlugin&)             = delete;
    HdMoonrayRendererDebugPlugin &operator =(const HdMoonrayRendererDebugPlugin&) = delete;
};

TF_REGISTRY_FUNCTION(TfType)
{
    pxr::HdRendererPluginRegistry::Define<HdMoonrayRendererDebugPlugin>();
}

PXR_NAMESPACE_CLOSE_SCOPE

