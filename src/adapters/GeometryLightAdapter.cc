// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#include "GeometryLightAdapter.h"
#include <pxr/usdImaging/usdImaging/delegate.h>
#include <pxr/usdImaging/usdImaging/indexProxy.h>
#include <pxr/usdImaging/usdImaging/tokens.h>

#include "pxr/imaging/hd/tokens.h"

PXR_NAMESPACE_OPEN_SCOPE

namespace {
    TfToken geometryLightToken("geometryLight");
    TfToken geometryToken("geometry");
}

TF_REGISTRY_FUNCTION(TfType)
{
    typedef GeometryLightAdapter Adapter;
    TfType t = TfType::Define<Adapter, TfType::Bases<Adapter::BaseAdapter> >();
    t.SetFactory< UsdImagingPrimAdapterFactory<Adapter> >();
}

GeometryLightAdapter::~GeometryLightAdapter()
{
}

bool
GeometryLightAdapter::IsSupported(
        UsdImagingIndexProxy const* index) const
{
    return index->IsSprimTypeSupported(geometryLightToken);
}

SdfPath
GeometryLightAdapter::Populate(UsdPrim const& prim,
                               UsdImagingIndexProxy* index,
                               UsdImagingInstancerContext const* instancerContext)
{
    index->InsertSprim(geometryLightToken, prim.GetPath(), prim);
    HD_PERF_COUNTER_INCR(UsdImagingTokens->usdPopulatedPrimCount);

    return prim.GetPath();
}

// in 0.20.11, the GeometryLight prim can call this function
// to access the rel property "geometry", which is not
// otherwise visible to render delegates (HDM-12)
#if PXR_VERSION >= 2011
VtValue
GeometryLightAdapter::Get(
    UsdPrim const& prim,
    SdfPath const& cachePath,
    TfToken const &key,
    UsdTimeCode time
#if PXR_VERSION >= 2105
    , VtIntArray* outIndices
#endif
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
    return BaseAdapter::Get(prim, cachePath, key, time
#if PXR_VERSION >= 2105
                            , outIndices
#endif
);
}
#endif

void
GeometryLightAdapter::_RemovePrim(SdfPath const& cachePath,
                                  UsdImagingIndexProxy* index)
{
    index->RemoveSprim(geometryLightToken, cachePath);
}

PXR_NAMESPACE_CLOSE_SCOPE
