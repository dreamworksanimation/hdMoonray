// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#include "RenderDelegate.h"
#include <pxr/imaging/hd/rendererPlugin.h>
#include <pxr/imaging/hd/rendererPluginRegistry.h>

#include <iostream>

PXR_NAMESPACE_OPEN_SCOPE // this does not work unless inside the pxr namespace

class HdMoonrayRendererPlugin final : public pxr::HdRendererPlugin {
public:
    HdMoonrayRendererPlugin() {}

    pxr::HdRenderDelegate *CreateRenderDelegate() override {
        return new hdMoonray::RenderDelegate;
    }

    pxr::HdRenderDelegate *CreateRenderDelegate(pxr::HdRenderSettingsMap const& settings) override {
        return new hdMoonray::RenderDelegate(settings);
    }

    void DeleteRenderDelegate(pxr::HdRenderDelegate *renderDelegate) override {
        delete renderDelegate;
    }

    bool IsSupported() const override {
        return true;
    }

private:
    // uncopyable
    HdMoonrayRendererPlugin(const HdMoonrayRendererPlugin&)             = delete;
    HdMoonrayRendererPlugin &operator =(const HdMoonrayRendererPlugin&) = delete;
};

TF_REGISTRY_FUNCTION(TfType)
{
    pxr::HdRendererPluginRegistry::Define<HdMoonrayRendererPlugin>();
}

PXR_NAMESPACE_CLOSE_SCOPE

