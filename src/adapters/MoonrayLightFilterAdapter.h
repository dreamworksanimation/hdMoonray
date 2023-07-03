// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "pxr/pxr.h"

#include "pxr/usdImaging/usdImaging/lightFilterAdapter.h"

PXR_NAMESPACE_OPEN_SCOPE


class UsdPrim;

class MoonrayLightFilterAdapter : public UsdImagingLightFilterAdapter {
public:
    typedef UsdImagingLightFilterAdapter BaseAdapter;

    MoonrayLightFilterAdapter()
        : UsdImagingLightFilterAdapter()
    {}

    virtual ~MoonrayLightFilterAdapter();
    // in 0.20.11+ we can re-implement Get in the adapter to return
    // the path value of "rel texture"
#if PXR_VERSION >= 2011
    virtual VtValue Get(UsdPrim const& prim,
                        SdfPath const& cachePath,
                        TfToken const& key,
                        UsdTimeCode time
#if PXR_VERSION >= 2105
                        , VtIntArray*
#endif
) const;
#endif


#if PXR_VERSION < 2105
    virtual SdfPath Populate(UsdPrim const& prim,
                        UsdImagingIndexProxy* index,
                        UsdImagingInstancerContext const* instancerContext = NULL);

        virtual bool IsSupported(UsdImagingIndexProxy const* index) const;

    protected:
        virtual void _RemovePrim(SdfPath const& cachePath,
                                UsdImagingIndexProxy* index) final;
#endif
};

PXR_NAMESPACE_CLOSE_SCOPE
