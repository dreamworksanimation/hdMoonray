// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <tbb/tbb_machine.h> // fix for icc-19/tbb bug
#include <pxr/base/gf/vec2f.h>
#include <pxr/base/gf/vec2d.h>
#include <pxr/base/gf/vec3f.h>
#include <pxr/base/gf/vec3d.h>
#include <pxr/base/gf/vec4f.h>
#include <pxr/base/gf/vec4d.h>
#include <pxr/base/gf/matrix4f.h>
#include <pxr/base/gf/matrix4d.h>

#include <pxr/base/vt/value.h>
#include <pxr/base/vt/array.h>

#include <scene_rdl2/scene/rdl2/SceneObject.h>

using namespace scene_rdl2::rdl2;

namespace hdMoonray { namespace ValueConverter {

// copy usd attribute to scene_rdl2 attribute
void setAttribute(SceneObject* sceneObj, const Attribute*, const pxr::VtValue& val);
//void setAttribute(SceneObject* sceneObj, const std::string& name, const pxr::VtValue& val);

// reset the attribute to default
void setDefault(SceneObject* sceneObj, const Attribute*);
//void setDefault(SceneObject* sceneObj, const std::string& name);

// set a binding on an attribute (also sets value to unit)
void setBinding(SceneObject* sceneObject, const Attribute*,SceneObject* binding);

}}

