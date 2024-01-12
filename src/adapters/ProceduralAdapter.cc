// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#include "ProceduralAdapter.h"
#include <pxr/usd/usdGeom/procedural.h>
#include <pxr/usd/usdGeom/subset.h>
#include "pxr/usdImaging/usdImaging/indexProxy.h"

#include <pxr/usdImaging/usdImaging/tokens.h>
#include <pxr/imaging/hd/tokens.h>

#include <iostream>

// Portions of this implementation are from Pixar's USD, see copyright notices

PXR_NAMESPACE_OPEN_SCOPE

static TfToken proceduralToken("procedural");

TF_REGISTRY_FUNCTION(TfType)
{
    typedef ProceduralAdapter Adapter;
    TfType t = TfType::Define<Adapter, TfType::Bases<Adapter::BaseAdapter> >();
    t.SetFactory< UsdImagingPrimAdapterFactory<Adapter> >();
}

ProceduralAdapter::ProceduralAdapter(): UsdImagingGprimAdapter()
{
}

ProceduralAdapter::~ProceduralAdapter()
{
}

bool
ProceduralAdapter::IsSupported(UsdImagingIndexProxy const* index) const
{
    return index->IsRprimTypeSupported(proceduralToken);
}

// Same as UsdImagingPrimAdapter::GetMaterialUsdPath but looks in GeomSubsets
// onSubset is set true if the material is found on a GeomSubset
SdfPath
ProceduralAdapter::getMaterial(UsdPrim const& prim, bool* onSubset) const
{
    SdfPath material = GetMaterialUsdPath(prim);
    if (onSubset) *onSubset = false;
    if (material.IsEmpty()) {
        for (const UsdPrim& child : prim.GetChildren()) {
            if (UsdGeomSubset subset{child}) {
                material = GetMaterialUsdPath(child);
                if (not material.IsEmpty() && onSubset) *onSubset = true;
                break;
            }
        }
    }
    return material;
}

#if PXR_VERSION >= 2011
SdfPath
ProceduralAdapter::GetMaterialId(UsdPrim const& prim,
                                 SdfPath const& cachePath,
                                 UsdTimeCode time) const
{
    return getMaterial(prim);
}
#else
void
ProceduralAdapter::UpdateForTime(UsdPrim const& prim,
                                 SdfPath const& cachePath,
                                 UsdTimeCode time,
                                 HdDirtyBits requestedBits,
                                 UsdImagingInstancerContext const* instancerContext) const
{
    UsdImagingGprimAdapter::UpdateForTime(prim, cachePath, time, requestedBits, instancerContext);
    // code copied from UsdImagingGprimAdapter that dealt with material:
    if (requestedBits & (HdChangeTracker::DirtyPrimvar | HdChangeTracker::DirtyMaterialId)) {
        bool onSubset; SdfPath materialUsdPath = getMaterial(prim, &onSubset);
        // there was something here about instances, ignored as it does not work and can't for materials on subsets.
        // Also something about pruning primvars, but it does nothing if renderDelegate::IsPrimvarFilteringNeeded
        // is true, which is false.
        if (onSubset)
            _GetValueCache()->GetMaterialId(cachePath) = materialUsdPath;
    }
}
#endif

SdfPath
ProceduralAdapter::Populate(UsdPrim const& usdPrim,
                            UsdImagingIndexProxy* index,
                            UsdImagingInstancerContext const* instancerContext)
{
#if PXR_VERSION >= 2011
    return _AddRprim(proceduralToken, usdPrim, index, getMaterial(usdPrim), instancerContext);
#else
    // Same as _AddRprim except insert the actual UsdPrim into the cache so
    // GetLightParamValue works on an instance. This work-around no longer works in 20.11
    // but Get() was fixed in that version.
    SdfPath cachePath = _ResolveCachePath(usdPrim.GetPath(), instancerContext);
    SdfPath instancer = instancerContext ?
        instancerContext->instancerCachePath : SdfPath();

    index->InsertRprim(proceduralToken, cachePath, instancer, usdPrim,
        instancerContext ? instancerContext->instancerAdapter
            : UsdImagingPrimAdapterSharedPtr());
    //HD_PERF_COUNTER_INCR(UsdImagingTokens->usdPopulatedPrimCount);

    // Allow instancer context to override the material binding.
    SdfPath resolvedUsdMaterialPath = instancerContext ?
        instancerContext->instancerMaterialUsdPath : getMaterial(usdPrim);
    UsdPrim materialPrim =
        usdPrim.GetStage()->GetPrimAtPath(resolvedUsdMaterialPath);

    if (materialPrim) {
        if (materialPrim.IsA<UsdShadeMaterial>()) {
            UsdImagingPrimAdapterSharedPtr materialAdapter =
                index->GetMaterialAdapter(materialPrim);
            if (materialAdapter) {
                materialAdapter->Populate(materialPrim, index, nullptr);
                // We need to register a dependency on the material prim so
                // that geometry is updated when the material is
                // (specifically, DirtyMaterialId).
                // XXX: Eventually, it would be great to push this into hydra.
                index->AddDependency(cachePath, materialPrim);
            }
        } else {
            TF_WARN("Gprim <%s> has illegal material reference to "
                    "prim <%s> of type (%s)", usdPrim.GetPath().GetText(),
                    materialPrim.GetPath().GetText(),
                    materialPrim.GetTypeName().GetText());
        }
    }

    // Populate coordinate system sprims bound to rprims.
    if (_DoesDelegateSupportCoordSys()) {
        if (UsdImagingPrimAdapterSharedPtr coordSysAdapter =
            _GetAdapter(HdPrimTypeTokens->coordSys)) {
            coordSysAdapter->Populate(usdPrim, index, instancerContext);
        }
    }

    return cachePath;
#endif
}

PXR_NAMESPACE_CLOSE_SCOPE
