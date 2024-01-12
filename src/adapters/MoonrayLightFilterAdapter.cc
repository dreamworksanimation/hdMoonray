// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#include "MoonrayLightFilterAdapter.h"
#include <pxr/usdImaging/usdImaging/lightAdapter.h>
#include <pxr/usdImaging/usdImaging/delegate.h>
#include <pxr/usdImaging/usdImaging/indexProxy.h>

#include "pxr/imaging/hd/tokens.h"

PXR_NAMESPACE_OPEN_SCOPE


namespace {
    TfToken textureToken("moonray:texture_map");
}


#if PXR_VERSION >= 2005
# define lightFilterToken (HdPrimTypeTokens->lightFilter)
#else
static TfToken lightFilterToken("lightFilter");
#endif


TF_REGISTRY_FUNCTION(TfType)
{
    typedef MoonrayLightFilterAdapter Adapter;
    TfType t = TfType::Define<Adapter, TfType::Bases<Adapter::BaseAdapter> >();
    t.SetFactory< UsdImagingPrimAdapterFactory<Adapter> >();
}

MoonrayLightFilterAdapter::~MoonrayLightFilterAdapter()
{
}
#if PXR_VERSION >= 2011
VtValue
MoonrayLightFilterAdapter::Get(
    UsdPrim const& prim,
    SdfPath const& cachePath,
    TfToken const &key,
    UsdTimeCode time
#if PXR_VERSION >= 2105
    , VtIntArray* outIndices
#endif
) const
{
    if (key == textureToken) {
        UsdProperty prop = prim.GetProperty(textureToken);
        if (UsdRelationship rel = prop.As<UsdRelationship>()) {
            // return the first target of "rel texture_map" as SdfPath
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

#if PXR_VERSION < 2105
bool
MoonrayLightFilterAdapter::IsSupported(
        UsdImagingIndexProxy const* index) const
{
    return index->IsSprimTypeSupported(lightFilterToken);
}

SdfPath
MoonrayLightFilterAdapter::Populate(UsdPrim const& prim,
                                    UsdImagingIndexProxy* index,
                                    UsdImagingInstancerContext const* instancerContext)
{
    index->InsertSprim(lightFilterToken, prim.GetPath(), prim);
    HD_PERF_COUNTER_INCR(lightFilterToken);
    return prim.GetPath();
}

void
MoonrayLightFilterAdapter::_RemovePrim(SdfPath const& cachePath,
                                       UsdImagingIndexProxy* index)
{
    index->RemoveSprim(lightFilterToken, cachePath);
}
#endif
PXR_NAMESPACE_CLOSE_SCOPE
