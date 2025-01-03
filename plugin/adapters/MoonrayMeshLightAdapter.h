// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <tbb/tbb_machine.h> // This must be included before any pxr headers

#include "pxr/pxr.h"
#include "pxr/usdImaging/usdImaging/api.h"
#include "pxr/usdImaging/usdImaging/lightAdapter.h"

PXR_NAMESPACE_OPEN_SCOPE


class UsdPrim;

class MoonrayMeshLightAdapter : public UsdImagingLightAdapter {
public:
    typedef UsdImagingLightAdapter BaseAdapter;

    MoonrayMeshLightAdapter()
        : UsdImagingLightAdapter()
    {}
    
    USDIMAGING_API
    virtual ~MoonrayMeshLightAdapter();
    
    USDIMAGING_API
    virtual SdfPath Populate(UsdPrim const& prim,
                     UsdImagingIndexProxy* index,
                     UsdImagingInstancerContext const* instancerContext = NULL);
    USDIMAGING_API
    virtual bool IsSupported(UsdImagingIndexProxy const* index) const;

    // in 0.20.11+ we can re-implement Get in the adapter to return
    // the path value of "rel geometry"
    virtual VtValue Get(UsdPrim const& prim,
                        SdfPath const& cachePath,
                        TfToken const& key,
                        UsdTimeCode time, 
                        VtIntArray*
) const;
protected:
    virtual void _RemovePrim(SdfPath const& cachePath,
                             UsdImagingIndexProxy* index) final;

};

PXR_NAMESPACE_CLOSE_SCOPE
