// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#include "ValueConverter.h"

#include <scene_rdl2/render/logging/logging.h>

#include <pxr/usd/sdf/assetPath.h>

namespace hdMoonray {

using scene_rdl2::logging::Logger;

static void _clearBinding(SceneObject* sceneObj, const Attribute* attribute)
{
    if (attribute->isBindable())
        sceneObj->setBinding(*attribute,nullptr);
}

template<typename T>
static void _set(SceneObject* sceneObj, const Attribute* attribute, const T& value) {
    sceneObj->set(AttributeKey<T>(*attribute), value);
}

// Need class wrapper to allow partial specialization
template <typename T, typename H>
struct RefConverter { static const T& _(const H&); };

// cases where hydra and rdl2 use the same type
template<typename T>
struct RefConverter<T,T> { static const T& _(const T& v) { return v; }};

// cases where the types can be cast in-place
#define PTRCAST(T,H)                                                     \
    template<> struct RefConverter<T,H> { static const T& _(const H& v) { return reinterpret_cast<const T&>(v); }}

PTRCAST(Bool, bool);
PTRCAST(Double, double);
PTRCAST(Float, float);
PTRCAST(Int, int);
PTRCAST(Long, long);
PTRCAST(String, std::string);

PTRCAST(Vec2f, pxr::GfVec2f);
PTRCAST(Vec2d, pxr::GfVec2d);
PTRCAST(Vec3f, pxr::GfVec3f);
PTRCAST(Rgb,   pxr::GfVec3f);
PTRCAST(Rgb,   pxr::GfVec4f); // does not work for vector<Rgb>!
PTRCAST(Vec3d, pxr::GfVec3d);
PTRCAST(Vec4f, pxr::GfVec4f);
PTRCAST(Rgba,  pxr::GfVec4f);
PTRCAST(Vec4d, pxr::GfVec4d);
PTRCAST(Mat4f, pxr::GfMatrix4f);
PTRCAST(Mat4d, pxr::GfMatrix4d);

// Token to string
template<> struct RefConverter<String, pxr::TfToken> {
    static const String& _(const pxr::TfToken& v) { return v.GetString(); }
};

// use first entry of vector
template<> struct RefConverter<Float, pxr::GfVec3f> {
    static const Float& _(const pxr::GfVec3f& v) { return v[0]; }
};

template <typename T, typename H>
static const T& _convertRef(const H& h) { return RefConverter<T,H>::_(h); }

template <typename T, typename H>
static bool _setAttributeRef(SceneObject* sceneObj, const Attribute* attribute, const pxr::VtValue& val)
{
    if (val.IsHolding<H>()) {
        _set(sceneObj, attribute, _convertRef<T,H>(val.UncheckedGet<H>()));
        _clearBinding(sceneObj,attribute);
        return true;
    } else {
        return false;
    }
}

// Non-reference version for when a new object must be constructed
template <typename T, typename H>
struct Converter { static T _(const H& v) { return v; } };

// Arrays of the types that can be cast in-place
template<typename T, typename H>
struct Converter<std::vector<T>, pxr::VtArray<H>>
{
    static std::vector<T> _(const pxr::VtArray<H>& v) {
        const T* p = &(RefConverter<T,H>::_(v[0]));
        return std::vector<T>(p, p + v.size());
    }
};

// Rdl2 uses a deque rather than a vector for bool arrays
template<>
struct Converter<BoolVector, pxr::VtArray<bool>>
{
    static BoolVector _(const pxr::VtArray<bool>& v) {
        return BoolVector(&v[0], &v[0] + v.size());
    }
};

// float->double converters
template<>
struct Converter<Vec3d, pxr::GfVec3f>
{
    static Vec3d _(const pxr::GfVec3f& v) {
        return Vec3d(v[0],v[1],v[2]);
    }
};

template <typename T, typename H>
static T _convert(const H& h) { return Converter<T,H>::_(h); }

template <typename T, typename H>
static bool _setAttribute(SceneObject* sceneObj, const Attribute* attribute, const pxr::VtValue& val)
{
    if (val.IsHolding<H>()) {
        _set(sceneObj, attribute, _convert<T,H>(val.UncheckedGet<H>()));
        _clearBinding(sceneObj,attribute);
        return true;
    } else {
        return false;
    }
}

void
ValueConverter::setAttribute(SceneObject* sceneObj, const Attribute* attribute, const pxr::VtValue& val)
{
    switch(attribute->getType()) {
    case TYPE_BOOL:
        if (val.IsHolding<long>()) {
            sceneObj->set(AttributeKey<Bool>(*attribute), static_cast<bool>(val.UncheckedGet<long>()));
            return;
        }
        if (_setAttributeRef<Bool, bool>(sceneObj, attribute, val)) return;
        if (_setAttribute<Bool, int>(sceneObj, attribute, val)) return; // used by show_specular by Houdini
        break;
    case TYPE_INT:
        if (attribute->isEnumerable()) {
            const std::string* key = nullptr;
            if (val.IsHolding<pxr::TfToken>()) {
                key = &(val.UncheckedGet<pxr::TfToken>().GetString());
            } else if (val.IsHolding<std::string>()) {
                key = &(val.UncheckedGet<std::string>());
            } else if (val.IsHolding<long>() || val.IsHolding<int>()) {
                const int intVal = val.IsHolding<long>() ?
                                   static_cast<int>(val.UncheckedGet<long>()) :
                                   val.UncheckedGet<int>();
                int index = 0;
                for (auto it = attribute->beginEnumValues(); it != attribute->endEnumValues(); ++it) {
                    if (index == intVal) {
                        sceneObj->set(AttributeKey<Int>(*attribute), it->first);
                        return;
                    }
                    ++index;
                }
                break;
            } else {
                break; // go print normal error message
            }
            for (auto it = attribute->beginEnumValues(); it != attribute->endEnumValues(); ++it) {
                if (it->second == *key) {
                    _set(sceneObj, attribute, it->first);
                    _clearBinding(sceneObj,attribute);
                    return;
                }
            }
            Logger::error(sceneObj->getName(), '.', attribute->getName(),
                          ": Invalid enum key '", *key, "'");
            return;
        } else  if (val.IsHolding<long>()) {
            sceneObj->set(AttributeKey<Int>(*attribute), static_cast<int>(val.UncheckedGet<long>()));
            return;
        } else {
            if (_setAttributeRef<Int, int>(sceneObj, attribute, val)) return;
        }
        break;
    case TYPE_LONG:
        if (_setAttributeRef<Long, long>(sceneObj, attribute, val)) return;
        break;
    case TYPE_FLOAT:
        if (val.IsHolding<int>()) {
            const float floatVal = static_cast<float>(val.UncheckedGet<int>());
            sceneObj->set(AttributeKey<Float>(*attribute), floatVal);
            return;
        } else if (val.IsHolding<long>()) {
            const float floatVal = static_cast<float>(val.UncheckedGet<long>());
            sceneObj->set(AttributeKey<Float>(*attribute), floatVal);
            return;
        } else {
            if (_setAttributeRef<Float, float>(sceneObj, attribute, val)) return;
            if (_setAttribute<Float, double>(sceneObj, attribute, val)) return;
            // handle incorrect types in Input bindings
            if (_setAttributeRef<Float, pxr::GfVec3f>(sceneObj, attribute, val)) return;
        }
        break;
    case TYPE_DOUBLE:
        if (_setAttributeRef<Double, double>(sceneObj, attribute, val)) return;
        if (_setAttribute<Double, float>(sceneObj, attribute, val)) return;
        break;
    case TYPE_STRING:
        if (attribute->isFilename() &&
            val.IsHolding<pxr::SdfAssetPath>()) {
            const pxr::SdfAssetPath& assetPath = val.UncheckedGet<pxr::SdfAssetPath>();
            _set(sceneObj, attribute, assetPath.GetResolvedPath());
            return;
        }
        if (_setAttributeRef<String, std::string>(sceneObj, attribute, val)) return;
        if (_setAttributeRef<String, pxr::TfToken>(sceneObj, attribute, val)) return;
        break;
    case TYPE_RGB:
        if (_setAttributeRef<Rgb, pxr::GfVec3f>(sceneObj, attribute, val)) return;
        if (_setAttributeRef<Rgb, pxr::GfVec4f>(sceneObj, attribute, val)) return; // in seaside scene preview shaders
        break;
    case TYPE_RGBA:
        if (_setAttributeRef<Rgba, pxr::GfVec4f>(sceneObj, attribute, val)) return;
        break;
    case TYPE_VEC2F:
        if (_setAttributeRef<Vec2f, pxr::GfVec2f>(sceneObj, attribute, val)) return;
        break;
    case TYPE_VEC2D:
        if (_setAttributeRef<Vec2d, pxr::GfVec2d>(sceneObj, attribute, val)) return;
        break;
    case TYPE_VEC3F:
        if (_setAttributeRef<Vec3f, pxr::GfVec3f>(sceneObj, attribute, val)) return;
        break;
    case TYPE_VEC3D:
        if (_setAttributeRef<Vec3d, pxr::GfVec3d>(sceneObj, attribute, val)) return;
        if (_setAttribute<Vec3d, pxr::GfVec3f>(sceneObj, attribute, val)) return; // Sdf forces 3d attributes to be 3f
        break;
    case TYPE_VEC4F:
        if (_setAttributeRef<Vec4f, pxr::GfVec4f>(sceneObj, attribute, val)) return;
        break;
    case TYPE_VEC4D:
        if (_setAttributeRef<Vec4d, pxr::GfVec4d>(sceneObj, attribute, val)) return;
        break;
    case TYPE_MAT4F:
        if (_setAttributeRef<Mat4f, pxr::GfMatrix4f>(sceneObj, attribute, val)) return;
        break;
    case TYPE_MAT4D:
        if (_setAttributeRef<Mat4d, pxr::GfMatrix4d>(sceneObj, attribute, val)) return;
        break;
    case TYPE_SCENE_OBJECT:
        // not supported yet
        break;
    case TYPE_BOOL_VECTOR:
        if (_setAttribute<BoolVector, pxr::VtArray<bool>>(sceneObj, attribute, val)) return;
        break;
    case TYPE_INT_VECTOR:
        if (_setAttribute<IntVector, pxr::VtArray<int>>(sceneObj, attribute, val)) return;
        break;
    case TYPE_LONG_VECTOR:
        if (_setAttribute<LongVector, pxr::VtArray<long>>(sceneObj, attribute, val)) return;
        break;
    case TYPE_FLOAT_VECTOR:
        if (_setAttribute<FloatVector, pxr::VtArray<float>>(sceneObj, attribute, val)) return;
        break;
    case TYPE_DOUBLE_VECTOR:
        if (_setAttribute<DoubleVector, pxr::VtArray<double>>(sceneObj, attribute, val)) return;
        break;
    case TYPE_STRING_VECTOR:
        if (_setAttribute<StringVector, pxr::VtArray<std::string>>(sceneObj, attribute, val)) return;
        break;
    case TYPE_RGB_VECTOR:
        if (_setAttribute<RgbVector, pxr::VtArray<pxr::GfVec3f>>(sceneObj, attribute, val)) return;
        break;
    case TYPE_RGBA_VECTOR:
        if (_setAttribute<RgbaVector, pxr::VtArray<pxr::GfVec4f>>(sceneObj, attribute, val)) return;
        break;
    case TYPE_VEC2F_VECTOR:
        if (_setAttribute<Vec2fVector, pxr::VtArray<pxr::GfVec2f>>(sceneObj, attribute, val)) return;
        break;
    case TYPE_VEC2D_VECTOR:
        if (_setAttribute<Vec2dVector, pxr::VtArray<pxr::GfVec2d>>(sceneObj, attribute, val)) return;
        break;
    case TYPE_VEC3F_VECTOR:
        if (_setAttribute<Vec3fVector, pxr::VtArray<pxr::GfVec3f>>(sceneObj, attribute, val)) return;
        break;
    case TYPE_VEC3D_VECTOR:
        if (_setAttribute<Vec3dVector, pxr::VtArray<pxr::GfVec3d>>(sceneObj, attribute, val)) return;
        break;
    case TYPE_VEC4F_VECTOR:
        if (_setAttribute<Vec4fVector, pxr::VtArray<pxr::GfVec4f>>(sceneObj, attribute, val)) return;
        break;
    case TYPE_VEC4D_VECTOR:
        if (_setAttribute<Vec4dVector, pxr::VtArray<pxr::GfVec4d>>(sceneObj, attribute, val)) return;
        break;
    case TYPE_MAT4F_VECTOR:
        if (_setAttribute<Mat4fVector, pxr::VtArray<pxr::GfMatrix4f>>(sceneObj, attribute, val)) return;
        break;
    case TYPE_MAT4D_VECTOR:
        if (_setAttribute<Mat4dVector, pxr::VtArray<pxr::GfMatrix4d>>(sceneObj, attribute, val)) return;
        break;
    case TYPE_SCENE_OBJECT_VECTOR:
    case TYPE_SCENE_OBJECT_INDEXABLE:
        // not supported yet
        break;
    case TYPE_UNKNOWN:
        break;
    }
    Logger::error(sceneObj->getName(), '.', attribute->getName(),
                  ": cannot convert ", val.GetTypeName(), " to ", attributeTypeName(attribute->getType()));
}

void
ValueConverter::setDefault(SceneObject* sceneObj, const Attribute* attribute)
{
    sceneObj->resetToDefault(attribute);
    _clearBinding(sceneObj,attribute);
}

void _setUnit(SceneObject* sceneObj, const Attribute* attr)
{
     switch(attr->getType()) {
     case TYPE_BOOL:   _set(sceneObj, attr, true); break;
     case TYPE_INT:    _set(sceneObj, attr, 1); break;
     case TYPE_LONG:   _set(sceneObj, attr, (long)1); break;
     case TYPE_FLOAT:  _set(sceneObj, attr, 1.0f); break;
     case TYPE_DOUBLE: _set(sceneObj, attr, 1.0);  break;
     case TYPE_RGB:    _set(sceneObj, attr, Rgb(1,1,1)); break;
     case TYPE_RGBA:   _set(sceneObj, attr, Rgba(1,1,1,1)); break;
     case TYPE_VEC2F:  _set(sceneObj, attr, Vec2f(1,1)); break;
     case TYPE_VEC2D:  _set(sceneObj, attr, Vec2d(1,1)); break;
     case TYPE_VEC3F:  _set(sceneObj, attr, Vec3f(1,1,1)); break;
     case TYPE_VEC3D:  _set(sceneObj, attr, Vec3d(1,1,1));break;
     case TYPE_VEC4F:  _set(sceneObj, attr, Vec4f(1,1,1,1)); break;
     case TYPE_VEC4D:  _set(sceneObj, attr, Vec4d(1,1,1,1)); break;
     default:
         Logger::error("setUnit not implemented for ", attributeTypeName(attr->getType()));
         break;
     }
}

void ValueConverter::setBinding(SceneObject* sceneObj, const Attribute* attr, SceneObject* binding)
{
    _setUnit(sceneObj,attr);
    if (attr->isBindable())
        sceneObj->setBinding(attr->getName(),binding);
}

}
