// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "pxr/pxr.h"

#include "pxr/usdImaging/usdImaging/lightAdapter.h"

PXR_NAMESPACE_OPEN_SCOPE


class UsdPrim;

class GeometryLightAdapter : public UsdImagingLightAdapter {
public:
    typedef UsdImagingLightAdapter BaseAdapter;

    GeometryLightAdapter()
        : UsdImagingLightAdapter()
    {}

    virtual ~GeometryLightAdapter();

    virtual SdfPath Populate(UsdPrim const& prim,
                     UsdImagingIndexProxy* index,
                     UsdImagingInstancerContext const* instancerContext = NULL);

    virtual bool IsSupported(UsdImagingIndexProxy const* index) const;

#if PXR_VERSION >= 2011
    // in 0.20.11+ we can re-implement Get in the adapter to return
    // the path value of "rel geometry"
    virtual VtValue Get(UsdPrim const& prim,
                        SdfPath const& cachePath,
                        TfToken const& key,
                        UsdTimeCode time
#if PXR_VERSION >= 2105
                        , VtIntArray*
#endif
) const;
#endif
protected:
    virtual void _RemovePrim(SdfPath const& cachePath,
                             UsdImagingIndexProxy* index) final;

};

PXR_NAMESPACE_CLOSE_SCOPE
