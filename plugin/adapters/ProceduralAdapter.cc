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
static TfToken partsToken("parts");

using PartMaterial = std::pair<SdfPath,SdfPath>;
using PerPartMaterials = std::vector<PartMaterial>;

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

VtValue
ProceduralAdapter::Get(UsdPrim const& prim,
                       SdfPath const& cachePath,
                       TfToken const& key,
                       UsdTimeCode time,
                       VtIntArray *outIndices) const
{
    if (key == partsToken) {
        
        PerPartMaterials ppm;
        if (UsdGeomImageable imageable = UsdGeomImageable(prim)) {
            for (const UsdGeomSubset &subset: UsdGeomSubset::GetAllGeomSubsets(imageable)) {
                const SdfPath& path = subset.GetPrim().GetPath();
                SdfPath materialUsdPath = GetMaterialUsdPath(subset.GetPrim());
                ppm.push_back(PartMaterial(path,materialUsdPath));
            }
        }
        return VtValue(ppm);
    }
    
    return BaseAdapter::Get(prim, cachePath, key, time, outIndices);
}

SdfPath
ProceduralAdapter::Populate(UsdPrim const& prim,
                            UsdImagingIndexProxy* index,
                            UsdImagingInstancerContext const* instancerContext)
{
    // This is essentially the same as UsdImagingMeshAdapter::Populate
    SdfPath cachePath = _AddRprim(proceduralToken, prim, index,
                                  GetMaterialUsdPath(prim), instancerContext);

    // Check for any UsdGeomSubset children of familyType 
    // UsdShadeTokens->materialBind and record dependencies for them.
    for (const UsdGeomSubset &subset:
         UsdShadeMaterialBindingAPI(prim).GetMaterialBindSubsets()) {
        index->AddDependency(cachePath, subset.GetPrim());

        // Ensure the bound material has been populated.
        UsdPrim materialPrim = prim.GetStage()->GetPrimAtPath(
                GetMaterialUsdPath(subset.GetPrim()));
        if (materialPrim) {
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
        }
    }

    return cachePath;
}

PXR_NAMESPACE_CLOSE_SCOPE
