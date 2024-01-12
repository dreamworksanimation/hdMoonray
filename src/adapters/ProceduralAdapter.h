// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <tbb/tbb_machine.h> // This must be included before any pxr headers

#include <pxr/pxr.h>
#include "pxr/usdImaging/usdImaging/api.h"
#include "pxr/usdImaging/usdImaging/primAdapter.h"
#include "pxr/usdImaging/usdImaging/gprimAdapter.h"

PXR_NAMESPACE_OPEN_SCOPE

class ProceduralAdapter : public UsdImagingGprimAdapter {
public:
    typedef UsdImagingGprimAdapter BaseAdapter;

    ProceduralAdapter();
    ~ProceduralAdapter() override;

    SdfPath Populate(UsdPrim const&,
                     UsdImagingIndexProxy*,
                     UsdImagingInstancerContext const* = nullptr) override;

    bool IsSupported(UsdImagingIndexProxy const*) const override;
#if PXR_VERSION >= 2011
    SdfPath GetMaterialId(UsdPrim const&, SdfPath const&, UsdTimeCode time) const override;
#else
    void UpdateForTime(UsdPrim const& prim,
                       SdfPath const& cachePath,
                       UsdTimeCode time,
                       HdDirtyBits requestedBits,
                       UsdImagingInstancerContext const*
                       instancerContext = NULL) const override;
#endif
protected:
    SdfPath getMaterial(UsdPrim const&, bool* onSubset = nullptr) const;
};

PXR_NAMESPACE_CLOSE_SCOPE
