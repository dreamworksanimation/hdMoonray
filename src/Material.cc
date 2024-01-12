// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#include "Material.h"
#include "RenderDelegate.h"
#include "ValueConverter.h"
#include "CoordSys.h"

#include <pxr/base/gf/vec2f.h>
#include <pxr/base/gf/vec2d.h>
#include <pxr/base/gf/vec3f.h>
#include <pxr/base/gf/vec3d.h>
#include <pxr/base/gf/vec4f.h>
#include <pxr/base/gf/vec4d.h>
#include <pxr/base/gf/matrix4f.h>
#include <pxr/base/gf/matrix4d.h>
#include <pxr/imaging/hd/rprim.h>

// nb, must use full name scene_rdl2::rdl2::Material...
#include <scene_rdl2/scene/rdl2/Layer.h>
#include <scene_rdl2/scene/rdl2/Material.h>
#include <scene_rdl2/scene/rdl2/Displacement.h>
#include <scene_rdl2/scene/rdl2/VolumeShader.h>

#include <scene_rdl2/render/logging/logging.h>
#include <iostream>

using namespace scene_rdl2::rdl2;
using namespace scene_rdl2::math;

#ifdef __GNUC__
#define UNUSED __attribute__ ((unused))
#else
#define UNUSED
#endif


using scene_rdl2::logging::Logger;

const pxr::TfToken projectorToken("projector");
const pxr::TfToken proj_camToken("proj_cam");

UNUSED
void
dumpMaterialNetworkMap(const pxr::HdMaterialNetworkMap& networkmap)
{
    // struct HdMaterialNetworkMap {
    //    std::map<TfToken, HdMaterialNetwork> map;
    //    std::vector<SdfPath> terminals;
    std::cout << "=== Material Networks ===" << std::endl;
    for (auto const& iter : networkmap.map) {
        const pxr::TfToken & terminalName = iter.first;
        const pxr::HdMaterialNetwork & network = iter.second;

        std::cout << "Terminal '" << terminalName << "':" << std::endl;

        // struct HdMaterialNetwork {
        //    std::vector<HdMaterialRelationship> relationships;
        //    std::vector<HdMaterialNode> nodes;
        //    TfTokenVector primvars;

        std::cout << "  primvars:";
        for (const pxr::TfToken& primvarName : network.primvars) {
            std::cout << " " << primvarName;
        }
        std::cout << std::endl;

        unsigned i = 0;
        for (const pxr::HdMaterialNode & node : network.nodes) {
            // struct HdMaterialNode {
            //    SdfPath path;
            //    TfToken identifier;
            //    std::map<TfToken, VtValue> parameters;
            std::cout << "  node " << i++ << ": " << node.identifier << " " << node.path << std::endl;
            for (auto const& jter : node.parameters) {
                std::cout << "    " << jter.first << " = " << jter.second
                          << " [" << jter.second.GetTypeName() << "] " << std::endl;
            }
        }

        for (const pxr::HdMaterialRelationship& rel : network.relationships) {
            // struct HdMaterialRelationship {
            //    SdfPath inputId;
            //    TfToken inputName;
            //    SdfPath outputId;
            //    TfToken outputName;
            std::cout << "  " << rel.outputId << "." << rel.outputName
                      << " bound to "<< rel.inputId << "." << rel.inputName
                      <<  std::endl;
        }
        std::cout << "---" << std::endl;
    }
    // This is redundant, it is always the last node in each network. I have no idea why they
    // store this redundant information
    // std::cout << "terminals: " << std::endl;
    // for (const pxr::SdfPath& terminal : networkmap.terminals) {
    //     std::cout << "    " << terminal << std::endl;
    // }

    std::cout << "=========================" << std::endl;
}

SceneObject*
getCoordSysBinding(
    hdMoonray::RenderDelegate& renderDelegate,
    pxr::HdSceneDelegate *sceneDelegate,
    const pxr::HdMaterialNode& node, pxr::TfToken key,
    const pxr::HdRprim* geom
) {
    auto valIt = node.parameters.find(key);
    if (valIt != node.parameters.end()) {
        pxr::TfToken coordSysName = valIt->second.Get<pxr::TfToken>();
        scene_rdl2::rdl2::SceneObject* sceneObject =
            hdMoonray::CoordSys::getBinding(sceneDelegate, renderDelegate, geom->GetId(), coordSysName);
        if (not sceneObject) {
            Logger::error(node.path, ".", key, ": failed to find binding for coordSys ", coordSysName);
        }
        return sceneObject;
    } else if (key == projectorToken) {
        // fix sq9010 s2 and other shots where Mpaint assets are missing the coordSys binding
        scene_rdl2::rdl2::SceneObject* sceneObject =
            hdMoonray::CoordSys::getBinding(sceneDelegate, renderDelegate, geom->GetId(), proj_camToken);
        // no error if not found, as this would be triggered if bindings to shaders are used
        return sceneObject;
    } else {
        return nullptr;
    }
}

SceneObject*
makeMoonrayShader(
    hdMoonray::RenderDelegate& renderDelegate,
    pxr::HdSceneDelegate *sceneDelegate,
    const pxr::HdMaterialNode& node,
    const std::string& nodeName,
    const pxr::HdRprim* geom
) {
    SceneObject* shaderObj = renderDelegate.createSceneObject(node.identifier.GetString(), nodeName);
    if (shaderObj) {
        try {
            SceneObject::UpdateGuard guard(shaderObj);
            const SceneClass& sceneClass = shaderObj->getSceneClass();
            for (auto it = sceneClass.beginAttributes(); it != sceneClass.endAttributes(); ++it) {
                const Attribute* attribute = *it;
                const std::string& attrName = attribute->getName();
                if (geom!=nullptr && attribute->getType() == scene_rdl2::rdl2::TYPE_SCENE_OBJECT) {
                    SceneObject* binding = getCoordSysBinding(
                        renderDelegate, sceneDelegate, node, pxr::TfToken(attrName), geom);
                    shaderObj->set(AttributeKey<SceneObject*>(*attribute), binding);
                } else {
                    auto valIt = node.parameters.find(pxr::TfToken(attrName));
                    if (valIt != node.parameters.end()) {
                        hdMoonray::ValueConverter::setAttribute(shaderObj, attribute, valIt->second);
                    } else {
                        hdMoonray::ValueConverter::setDefault(shaderObj, attribute);
                    }
                }
            }
        } catch (const std::exception& e) {
            Logger::error(node.path, ": ", e.what());
            return nullptr;
        }
    }
    return shaderObj;
}


std::string
getNodeWithChannelName(const std::string& nodePath,
                       const std::string& channelName)
{
    return nodePath + std::string(".") + channelName;
}

//} // namespace {

namespace hdMoonray {

pxr::HdDirtyBits
Material::GetInitialDirtyBitsMask() const
{
    return AllDirty; // as of 20.5 this is DirtyParams | DirtyResource
}

void
Material::Sync(pxr::HdSceneDelegate *sceneDelegate,
               pxr::HdRenderParam   *renderParam,
               pxr::HdDirtyBits     *dirtyBits)
{
    auto& renderDelegate(RenderDelegate::get(renderParam));
    renderDelegate.setStartTime();
    const pxr::SdfPath& id = GetId();
    // std::cout << id << " Sync dirty=" << std::hex << *dirtyBits << std::endl;
    if (*dirtyBits & AllDirty) {
        mResource = sceneDelegate->GetMaterialResource(id);
        mMaterialDirty = mDisplacementDirty = mVolumeShaderDirty = true;
        // update any material that has been created
        if (mMaterial) getMaterial(renderDelegate, sceneDelegate, mGeom);
        if (mDisplacement) getDisplacement(renderDelegate, sceneDelegate, mGeom);
        if (mVolumeShader) getVolumeShader(renderDelegate, sceneDelegate, mGeom);
    }
    *dirtyBits &= ~AllDirty;
}

bool Material::isEnabled() const
{
    return mResource.IsHolding<pxr::HdMaterialNetworkMap>();
}

// Build the tree of shaders, return the final output
// Returns null if the terninal does not exist, returns error shader on other errors
// The geom is for finding coordSys bindings
scene_rdl2::rdl2::SceneObject*
Material::updateTerminal(pxr::TfToken terminalName,
                         RenderDelegate& renderDelegate,
                         pxr::HdSceneDelegate *sceneDelegate,
                         const pxr::HdRprim* geom)
{
    if (not isEnabled()) {
        return nullptr;
    }

    const pxr::HdMaterialNetworkMap& networkmap = mResource.UncheckedGet<pxr::HdMaterialNetworkMap>();
    auto i = networkmap.map.find(terminalName);
    if (i == networkmap.map.end()) {
        return nullptr;
    }

    // dumpMaterialNetworkMap(networkmap);

    const pxr::HdMaterialNetwork& network = i->second;

    // Get a mapping of nodes to their output channels.
    // When creating the nodes below, create one node
    // per channel.   The map only stores nodes with output
    // connections so won't contain the material at the end
    // which has no output connections.
    std::unordered_map< std::string, std::set<std::string> > nodeChannelMap;
    for (const pxr::HdMaterialRelationship& rel : network.relationships) {
        const std::string inputChannel = rel.inputName.GetString();

        // Find the input node
        for (const pxr::HdMaterialNode& node : network.nodes) {
            if (node.path == rel.inputId) {
                // Search for node.path string in map
                auto it = nodeChannelMap.find(node.path.GetString());
                if (it == nodeChannelMap.end()) {
                    // Not found. Create channel set with input name as channel
                    std::set<std::string> channelSet = { inputChannel };
                    std::pair<std::string, std::set<std::string>> entry = { node.path.GetString(), channelSet };
                    nodeChannelMap.insert(entry);
                } else {
                    // Found.  Insert channel name into channelSet
                    it->second.insert(inputChannel);
                }
            }
        }
    }

    // Create the nodes
    scene_rdl2::rdl2::SceneObject* last = nullptr;
    for (const pxr::HdMaterialNode& node : network.nodes) {

        // Don't create UsdUVTexture nodes if their file name parameter is empty
        if (node.identifier == "UsdUVTexture") {
            auto valIt = node.parameters.find(pxr::TfToken("file"));
            if (valIt != node.parameters.end()) {
                if (valIt->second.UncheckedGet<pxr::SdfAssetPath>().GetResolvedPath().empty()) {
                    continue;
                }
            }
        }

        // Create a node for each channel entry
        auto it = nodeChannelMap.find(node.path.GetString());
        if (it != nodeChannelMap.end()) {
            std::set<std::string>& channelSet = it->second;
            for (const std::string& channel : channelSet) {

                const std::string nodeName =
                    getNodeWithChannelName(node.path.GetString(),
                                           channel);

                last = makeMoonrayShader(renderDelegate,
                                         sceneDelegate,
                                         node,
                                         nodeName,
                                         geom);
            }
        } else {
            // If the node isn't in the nodeChannelMap (i.e. materials)
            // then only one node is created using the USD path as the name.
            last = makeMoonrayShader(renderDelegate,
                                     sceneDelegate,
                                     node,
                                     node.path.GetString(),
                                     geom);
        }

        if (not last) break; // give up after first error
    }

    // set bindings (fixme: only works for Moonray shaders)
    for (const pxr::HdMaterialRelationship& rel : network.relationships) {
        // Input connection
        SceneObject* input;

        bool foundNodeWithChannel = false;
        for (const pxr::HdMaterialNode& node : network.nodes) {
            if (node.path == rel.inputId) {

                const std::string nodeWithChannel =
                    getNodeWithChannelName(rel.inputId.GetString(),
                                           rel.inputName.GetString());

                input = renderDelegate.getSceneObject(nodeWithChannel);

                if (input) {
                    foundNodeWithChannel = true;
                }
            }
        }

        // If we didn't find the node with the channel name
        // extension then it must be a node with no outputs
        // (i.e. material) so just use the node name without
        // a channel extension to get it from the render delegate.
        if (!foundNodeWithChannel) {
            input = renderDelegate.getSceneObject(rel.inputId);
        }

        if (!input) {
            continue;
        }

        // Special handling for specific nodes

        // Skip IOR maps
        if (rel.outputName.GetString().find("ior") != std::string::npos) {
            continue;
        }

        // UsdUVTexture map special handling
        if (std::string::npos && input->getSceneClass().getName() == "UsdUVTexture") {
            try {
                UpdateGuard guard(input);

                // Decode normal maps
                if (renderDelegate.renderSettings().getDecodeNormals() &&
                    rel.outputName.GetString().find("normal") != std::string::npos) {

                    input->set(input->getSceneClass().getAttributeKey<Rgb>("scale"), Rgb(2.0f));
                    input->set(input->getSceneClass().getAttributeKey<Rgb>("bias"), Rgb(-1.0f));
                    input->set(input->getSceneClass().getAttributeKey<Int>("sourceColorSpace"), 0);
                }

                // Channel binding
                const std::string channel = rel.inputName.GetString();
                const auto outputModeKey = input->getSceneClass().getAttributeKey<Int>("output_mode");
                int enumValue = input->getSceneClass().getEnumValue(outputModeKey, channel);
                input->set(outputModeKey, enumValue);
            } catch (const std::exception& e) {
                Logger::error(rel.outputId, ": ", e.what());
                last = nullptr;
            }
        }

        // Output
        SceneObject* output;
        // Check if output is in nodeChannelMap
        auto it = nodeChannelMap.find(rel.outputId.GetString());
        if (it != nodeChannelMap.end()) {
            // Create a binding for each channel's node
            for (const std::string& channel : it->second) {
                const std::string outputNodeName =
                    getNodeWithChannelName(rel.outputId.GetString(),
                                           channel);

                output = renderDelegate.getSceneObject(outputNodeName);
                if (!output) {
                    continue;
                }

                try {
                    UpdateGuard guard(output);

                    const scene_rdl2::rdl2::Attribute* attribute(
                        output->getSceneClass().getAttribute(rel.outputName.GetString()));

                    if (attribute->getType() == scene_rdl2::rdl2::TYPE_SCENE_OBJECT) {
                        output->set(AttributeKey<SceneObject*>(*attribute), input);
                    } else {
                        ValueConverter::setBinding(output, attribute, input);
                    }
                } catch (const std::exception& e) {
                    Logger::error(rel.outputId, ": ", e.what());
                    last = nullptr;
                }
            }
        } else {
            output = renderDelegate.getSceneObject(rel.outputId);
            if (!output) {
                continue;
            }

            try {
                UpdateGuard guard(output);

                const scene_rdl2::rdl2::Attribute* attribute(
                    output->getSceneClass().getAttribute(rel.outputName.GetString()));

                if (attribute->getType() == scene_rdl2::rdl2::TYPE_SCENE_OBJECT) {
                    output->set(AttributeKey<SceneObject*>(*attribute), input);
                } else {
                    ValueConverter::setBinding(output, attribute, input);
                }
            } catch (const std::exception& e) {
                Logger::error(rel.outputId, ": ", e.what());
                last = nullptr;
            }
        }
    }

    if (not last && terminalName == pxr::HdMaterialTerminalTokens->surface) last = renderDelegate.errorMaterial();
    return last;
}

scene_rdl2::rdl2::Material*
Material::getMaterial(
    RenderDelegate& renderDelegate,
    pxr::HdSceneDelegate *sceneDelegate,
    const pxr::HdRprim* geom)
{
    if (mMaterialDirty || renderDelegate.getDecodeNormalsChanged()) {
        std::lock_guard<std::mutex> lock(mCreateMutex);
        mGeom = geom;
        scene_rdl2::rdl2::SceneObject* s = updateTerminal(
            pxr::HdMaterialTerminalTokens->surface, renderDelegate, sceneDelegate, geom);
        mMaterial = s ? s->asA<scene_rdl2::rdl2::Material>() : nullptr;
        mMaterialDirty = false;
    }
    return mMaterial;
}

scene_rdl2::rdl2::Displacement*
Material::getDisplacement(
    RenderDelegate& renderDelegate,
    pxr::HdSceneDelegate *sceneDelegate,
    const pxr::HdRprim* geom)
{
    if (mDisplacementDirty) {
        std::lock_guard<std::mutex> lock(mCreateMutex);
        if (mDisplacementDirty) {
            mGeom = geom;
            scene_rdl2::rdl2::SceneObject* s = updateTerminal(
                pxr::HdMaterialTerminalTokens->displacement, renderDelegate, sceneDelegate, geom);
            mDisplacement = s ? s->asA<scene_rdl2::rdl2::Displacement>() : nullptr;
            mDisplacementDirty = false;
        }
    }
    return mDisplacement;
}


scene_rdl2::rdl2::VolumeShader*
Material::getVolumeShader(
    RenderDelegate& renderDelegate,
    pxr::HdSceneDelegate *sceneDelegate,
    const pxr::HdRprim* geom)
{
    if (mVolumeShaderDirty) {
        std::lock_guard<std::mutex> lock(mCreateMutex);
        if (mVolumeShaderDirty) {
            mGeom = geom;
            scene_rdl2::rdl2::SceneObject* s = updateTerminal(
                pxr::HdMaterialTerminalTokens->volume, renderDelegate, sceneDelegate, geom);
            mVolumeShader = s ? s->asA<scene_rdl2::rdl2::VolumeShader>() : nullptr;
            mVolumeShaderDirty = false;
        }
    }
    return mVolumeShader;
}

void
Material::get(
    scene_rdl2::rdl2::LayerAssignment& layerAssignment,
    const pxr::SdfPath& materialId,
    RenderDelegate& renderDelegate,
    pxr::HdSceneDelegate* sceneDelegate,
    const pxr::HdRprim* geom,
    bool volume)
{
    if (not materialId.IsEmpty()) {
        Material* mtlPrim = static_cast<Material*>(
            sceneDelegate->GetRenderIndex().GetSprim(pxr::HdPrimTypeTokens->material, materialId));
        if (not mtlPrim) {
            Logger::error(geom->GetId(), ".material: ", materialId, " has no moonray or fallback shaders");
            layerAssignment.mMaterial = renderDelegate.errorMaterial();
            layerAssignment.mDisplacement = nullptr;
            layerAssignment.mVolumeShader = nullptr;
            return;
        }
        if (mtlPrim->isEnabled()) {
            layerAssignment.mMaterial = mtlPrim->getMaterial(renderDelegate, sceneDelegate, geom);
            layerAssignment.mDisplacement = mtlPrim->getDisplacement(renderDelegate, sceneDelegate, geom);
            layerAssignment.mVolumeShader = mtlPrim->getVolumeShader(renderDelegate, sceneDelegate, geom);
            return;
        }
    }
    // return default material
    layerAssignment.mMaterial = volume ? nullptr : renderDelegate.defaultMaterial();
    layerAssignment.mDisplacement = nullptr;
    layerAssignment.mVolumeShader = volume ? renderDelegate.defaultVolumeShader() : nullptr;
}

}

