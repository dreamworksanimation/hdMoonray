// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#include "MoonrayMeshLightAdapter.h"
#include <pxr/usdImaging/usdImaging/delegate.h>
#include <pxr/usdImaging/usdImaging/indexProxy.h>
#include <pxr/usdImaging/usdImaging/tokens.h>
#include "pxr/imaging/hd/tokens.h"

PXR_NAMESPACE_OPEN_SCOPE

namespace {
    TfToken geometryLightToken("geometryLight");
    TfToken geometryToken("inputs:geometry");
}

TF_REGISTRY_FUNCTION(TfType)
{
    typedef MoonrayMeshLightAdapter Adapter;
    TfType t = TfType::Define<Adapter, TfType::Bases<Adapter::BaseAdapter> >();
    t.SetFactory< UsdImagingPrimAdapterFactory<Adapter> >();
}

MoonrayMeshLightAdapter::~MoonrayMeshLightAdapter()
{
}

bool
MoonrayMeshLightAdapter::IsSupported(
        UsdImagingIndexProxy const* index) const
{
    return index->IsSprimTypeSupported(geometryLightToken);
}

SdfPath
MoonrayMeshLightAdapter::Populate(UsdPrim const& prim,
                               UsdImagingIndexProxy* index,
                               UsdImagingInstancerContext const* instancerContext)
{
    index->InsertSprim(geometryLightToken, prim.GetPath(), prim);
    HD_PERF_COUNTER_INCR(UsdImagingTokens->usdPopulatedPrimCount);

    return prim.GetPath();
}


VtValue
MoonrayMeshLightAdapter::Get(
    UsdPrim const& prim,
    SdfPath const& cachePath,
    TfToken const &key,
    UsdTimeCode time, 
    VtIntArray* outIndices
) const
{
    if (key == geometryToken) {
        UsdProperty prop = prim.GetProperty(geometryToken);
        if (UsdAttribute attr = prop.As<UsdAttribute>()) {
            // the GeometryLight schema should prevent an attr called
            // "geometry", but we might as well just return the value...
            VtValue v;
            attr.Get(&v, time);
            return v;
        } else if (UsdRelationship rel = prop.As<UsdRelationship>()) {
            // return the first target of "rel geometry" as SdfPath
            SdfPathVector targets;
            if (rel.GetForwardedTargets(&targets) && !targets.empty()) {
                return VtValue(targets.front());
            }
        }
    }
    return BaseAdapter::Get(prim, cachePath, key, time, outIndices);
}

void
MoonrayMeshLightAdapter::_RemovePrim(SdfPath const& cachePath,
                                  UsdImagingIndexProxy* index)
{
    index->RemoveSprim(geometryLightToken, cachePath);
}

PXR_NAMESPACE_CLOSE_SCOPE
