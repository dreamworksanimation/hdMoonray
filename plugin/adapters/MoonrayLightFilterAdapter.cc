// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#include "MoonrayLightFilterAdapter.h"
#include <pxr/usdImaging/usdImaging/lightAdapter.h>
#include <pxr/usdImaging/usdImaging/delegate.h>
#include <pxr/usdImaging/usdImaging/indexProxy.h>

#include "pxr/imaging/hd/tokens.h"



PXR_NAMESPACE_OPEN_SCOPE

namespace {
    TfToken cookie_projector_token("moonray:projector");
    TfToken combine_filters_token("moonray:light_filters");
}

TF_REGISTRY_FUNCTION(TfType)
{
    typedef MoonrayLightFilterAdapter Adapter;
    TfType t = TfType::Define<Adapter, TfType::Bases<Adapter::BaseAdapter> >();
    t.SetFactory< UsdImagingPrimAdapterFactory<Adapter> >();
}

MoonrayLightFilterAdapter::~MoonrayLightFilterAdapter()
{
}

VtValue
MoonrayLightFilterAdapter::Get(
    UsdPrim const& prim,
    SdfPath const& cachePath,
    TfToken const &key,
    UsdTimeCode time, 
    VtIntArray* outIndices) const
{
    // we need to provide access to "rel" properties on the prim, since
    // the standard "Get" doesn't see them
    if (key == cookie_projector_token) {
        UsdProperty prop = prim.GetProperty(key);
        if (UsdRelationship rel = prop.As<UsdRelationship>()) {
            SdfPathVector targets;
            if (rel.GetForwardedTargets(&targets) && !targets.empty()) {
                return VtValue(targets.front());
            }
        }
    } else if (key == combine_filters_token) {
        UsdProperty prop = prim.GetProperty(key);
        if (UsdRelationship rel = prop.As<UsdRelationship>()) {
            SdfPathVector targets;
            if (rel.GetForwardedTargets(&targets) && !targets.empty()) {
                return VtValue(targets);
            }
        }
    }
    return BaseAdapter::Get(prim, cachePath, key, time, outIndices);
}


PXR_NAMESPACE_CLOSE_SCOPE
