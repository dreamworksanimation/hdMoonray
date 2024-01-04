// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#include "RenderSettings.h"
#include "RenderDelegate.h"
#include "ValueConverter.h"

#include <scene_rdl2/scene/rdl2/SceneContext.h>
#include <scene_rdl2/scene/rdl2/SceneVariables.h>
#include <scene_rdl2/render/logging/logging.h>

// If adding or changing the descriptors, the file
// ../houdini/soho/parameters/HdMoonrayRendererPlugin_Viewport.ds must be updated to match

namespace {
using pxr::TfToken;
using pxr::TfStaticData;
TF_DEFINE_PRIVATE_TOKENS(Tokens,
    (useRemoteHosts)
    (remoteHosts)
    (localReservedCores)
    (maxFps)
    (debugMode)
    (disableRender)
    (restartToggle)
    (reloadTexturesToggle)
    (maxConnectRetries)
    (debug)
    (info)
    (logLevel)
    (rdlaOutput)
    (disableLighting)
    (doubleSided)
    (enableDenoise)
    (denoiseAlbedoGuiding)
    (denoiseNormalGuiding)
    (decodeNormals)
    (enableMotionBlur)
    (pruneWillow)
    (pruneFurDeform)
    (pruneCurveDeform)
    (pruneVolume)
    (pruneWrapDeform)
    (forcePolygon)
    (executionMode)
);

std::map<pxr::TfToken, pxr::VtValue> defaultMap;

bool getenvBool(const char* name, bool dflt = false)
{
    const char* v = std::getenv(name);
    if (not v) return dflt;
    return *v && *v != '0' && *v != 'f' && *v != 'F';
}

#if 0
float getenvFloat(const char* name, float dflt)
{
    const char* v = std::getenv(name);
    if (not v) return dflt;
    return float(strtod(v,0));
}
#endif

int getenvInt(const char* name, int dflt)
{
    const char* v = std::getenv(name);
    if (not v) return dflt;
    return int(strtol(v,0,0));
}

}

namespace hdMoonray {

using scene_rdl2::logging::Logger;

const char*
RenderSettings::getenvString(const char* name, const char* dflt)
{
    const char* v = std::getenv(name);
    return v ? v : dflt ? dflt : "";
}

const pxr::HdRenderSettingDescriptorList&
RenderSettings::getDescriptors()
{
    static const int hosts = getenvInt("HDMOONRAY_HOSTS", 0);
    static pxr::HdRenderSettingDescriptorList mDescriptors = {
        { "Use Remote Hosts",     Tokens->useRemoteHosts,   pxr::VtValue(hosts > 0) },
        { "Remote Hosts",         Tokens->remoteHosts,      pxr::VtValue(hosts > 0 ? hosts : 5) },
        { "Local Reserved Cores", Tokens->localReservedCores, pxr::VtValue(getenvInt("HDMOONRAY_LOCAL_RESERVED_CORES",1))},
        { "Max FPS",              Tokens->maxFps,           pxr::VtValue(12.0f) },
        { "Debug Mode",           Tokens->debugMode,        pxr::VtValue(getenvBool("HDMOONRAY_DEBUG_MODE")) },
        { "Disable Render",       Tokens->disableRender,    pxr::VtValue(getenvBool("HDMOONRAY_DISABLE")) },
        { "Restart (toggle)",     Tokens->restartToggle,    pxr::VtValue(false) },
        { "Reload textures (toggle)",Tokens->reloadTexturesToggle,  pxr::VtValue(false) },
        { "Maximum connect retries", Tokens->maxConnectRetries, pxr::VtValue(getenvInt("HDMOONRAY_MAX_CONNECT_RETRIES",2)) },
        { "Show Debug Messages",  Tokens->debug,            pxr::VtValue(getenvBool("HDMOONRAY_DEBUG")) },
        { "Show Info Messages",   Tokens->info,             pxr::VtValue(getenvBool("HDMOONRAY_INFO")) },
        { "Log Level(1-5)",       Tokens->logLevel,         pxr::VtValue(getenvInt("HDMOONRAY_LOGLEVEL", 1)) },
        { "Rdla output",          Tokens->rdlaOutput,       pxr::VtValue(getenvString("HDMOONRAY_RDLA_OUTPUT")) },
        { "Disable Lighting",     Tokens->disableLighting,  pxr::VtValue(getenvBool("HDMOONRAY_DISABLE_LIGHTING")) },
        { "DoubleSided",          Tokens->doubleSided,      pxr::VtValue(getenvBool("HDMOONRAY_DOUBLESIDED")) },
        { "Enable Denoising",     Tokens->enableDenoise,    pxr::VtValue(getenvBool("HDMOONRAY_ENABLE_DENOISE")) },
        { "Denoise Albedo Guiding", Tokens->denoiseAlbedoGuiding, pxr::VtValue(getenvBool("HDMOONRAY_DENOISE_ALBEDO_GUIDING")) },
        { "Denoise Normal Guiding", Tokens->denoiseNormalGuiding, pxr::VtValue(getenvBool("HDMOONRAY_DENOISE_NORMAL_GUIDING")) },
        { "Decode Normals",      Tokens->decodeNormals, pxr::VtValue(false) },
        { "Enable Motion Blur",      Tokens->enableMotionBlur, pxr::VtValue(true)},
        { "Prune Willow",      Tokens->pruneWillow, pxr::VtValue(getenvBool("HDMOONRAY_PRUNE_WILLOW")) },
        { "Prune FurDeform",      Tokens->pruneFurDeform, pxr::VtValue(getenvBool("HDMOONRAY_PRUNE_FURDEFORM")) },
        { "Prune Volumes",      Tokens->pruneVolume, pxr::VtValue(getenvBool("HDMOONRAY_PRUNE_VOLUME")) },
        { "Prune WrapDeform",      Tokens->pruneWrapDeform, pxr::VtValue(getenvBool("HDMOONRAY_PRUNE_WRAPDEFORM")) },
        { "Prune CurveDeform",      Tokens->pruneCurveDeform, pxr::VtValue(getenvBool("HDMOONRAY_PRUNE_CURVEDEFORM")) },
        { "Force Polygon",      Tokens->forcePolygon, pxr::VtValue(getenvBool("HDMOONRAY_FORCE_POLYGON")) },
        { "Enable Motion Blur",      Tokens->enableMotionBlur, pxr::VtValue(true) },
        { "Execution Mode",      Tokens->executionMode, pxr::VtValue(getenvString("HDMOONRAY_EXEC_MODE", "auto")) },

    };
    if (defaultMap.empty()) {
        for (auto&& i : mDescriptors) {
            defaultMap[i.key] = i.defaultValue;
        }
    }
    return mDescriptors;
}

template<typename T>
T RenderSettings::get(const pxr::TfToken& key) const
{
    return mDelegate.GetRenderSetting(key).Cast<T>().GetWithDefault(defaultMap[key].Get<T>());
}

void RenderSettings::apply()
{
    bool info = get<bool>(Tokens->info);
    if (info) {
        Logger::setInfoLevel();
    }

    bool dbg = get<bool>(Tokens->debug);
    if (dbg) {
        Logger::setDebugLevel();
    }

    static const pxr::TfToken houdiniInteractive("houdini:interactive");
    pxr::VtValue val = mDelegate.GetRenderSetting(houdiniInteractive);
    mIsHoudini = not val.IsEmpty();

    // fixme: is there a way to make this work using SceneVariables::sLayer, etc instead of string constants?
    static const std::set<std::string> sDontWrite = {
        "camera", "motion_steps", "enable_motion_blur", "layer", "image_width", "image_height"
    };

    scene_rdl2::rdl2::SceneVariables& sv = mDelegate.acquireSceneContext().getSceneVariables();
    {   UpdateGuard guard(sv);
        // apply all sceneVariable:xyz attributes from the RenderSettings prim
        const SceneClass& sceneClass = sv.getSceneClass();
        for (auto it = sceneClass.beginAttributes(); it != sceneClass.endAttributes(); ++it) {
            const std::string& attrName = (*it)->getName();
            if (sDontWrite.count(attrName)) continue;
            // may want to skip some of them or check for overrides here
            pxr::TfToken key = pxr::TfToken("sceneVariable:" + attrName);
            pxr::VtValue val = mDelegate.GetRenderSetting(key);
            if (not val.IsEmpty()) {
                ValueConverter::setAttribute(&sv, *it, val);
            } else {
                // Try alternate form since .ds file complains about colon
                key = pxr::TfToken("sceneVariable_" + attrName);
                val = mDelegate.GetRenderSetting(key);
                if (not val.IsEmpty()) {
                    ValueConverter::setAttribute(&sv, *it, val);
                } else {
                    // Currently the values never return to empty in Houdini, so this never does
                    // anything.
                    // ValueConverter::setDefault(&sv, *it);
                }
            }
        }

        // apply any overrides from the render options
        sv.set(sv.sDebugKey, dbg);
        sv.set(sv.sInfoKey, info);
    }

    mDelegate.setRdlaOutput(get<std::string>(Tokens->rdlaOutput));
    mDelegate.setDisableLighting(get<bool>(Tokens->disableLighting));
    mDelegate.setDoubleSided(get<bool>(Tokens->doubleSided));
    mDelegate.setDecodeNormals(get<bool>(Tokens->decodeNormals));
    mDelegate.setEnableMotionBlur(get<bool>(Tokens->enableMotionBlur));
    mDelegate.setPruneWillow(get<bool>(Tokens->pruneWillow));
    mDelegate.setPruneFurDeform(get<bool>(Tokens->pruneFurDeform));
    mDelegate.setPruneCurveDeform(get<bool>(Tokens->pruneCurveDeform));
    mDelegate.setPruneWrapDeform(get<bool>(Tokens->pruneWrapDeform));
    mDelegate.setPruneVolume(get<bool>(Tokens->pruneVolume));
    mDelegate.setForcePolygon(get<bool>(Tokens->forcePolygon));
}

RendererImplType
RenderSettings::getRendererImplType() const
{
    if (get<bool>(Tokens->disableRender))
        return RendererImplType::Null;
    else if (get<bool>(Tokens->debugMode))
        return RendererImplType::Rndr;
    else
        return RendererImplType::Arras;
}

bool
RenderSettings::getArrasLocalMode() const
{
    return not get<bool>(Tokens->useRemoteHosts);
}

int
RenderSettings::getArrasLogLevel() const
{
    return  get<int>(Tokens->logLevel);
}

int
RenderSettings::getArrasHostCount() const
{
    return  get<int>(Tokens->remoteHosts);
}

int
RenderSettings::getArrasLocalReservedCores() const
{
    return  get<int>(Tokens->localReservedCores);
}

float
RenderSettings::getArrasMaxFps() const
{
    return  get<float>(Tokens->maxFps);
}

std::string
RenderSettings::getExecutionMode() const
{
    // RenderSettings::getDescriptors() initializes the default value for executionMode
    // as it intializes the defaultMap.
    // This is called before getDescriptors()
    // so the val can be empty and we need to provide a default value
    pxr::VtValue val = mDelegate.GetRenderSetting(Tokens->executionMode);
    if (not val.IsEmpty()) {
        return val.Get<std::string>();
    }else{
        return "auto";
    }
}

bool
RenderSettings::getRestartToggle() const
{
    return  get<bool>(Tokens->restartToggle);
}

bool
RenderSettings::getReloadTexturesToggle() const
{
    return  get<bool>(Tokens->reloadTexturesToggle);
}

int
RenderSettings::getMaxConnectRetries() const
{
    return get<int>(Tokens->maxConnectRetries);
}

bool
RenderSettings::enableDenoise() const
{
    return  get<bool>(Tokens->enableDenoise);
}

bool
RenderSettings::denoiseAlbedoGuiding() const
{
    return  get<bool>(Tokens->denoiseAlbedoGuiding);
}

bool
RenderSettings::denoiseNormalGuiding() const
{
    return  get<bool>(Tokens->denoiseNormalGuiding);
}

bool
RenderSettings::getDecodeNormals() const
{
    return  get<bool>(Tokens->decodeNormals);
}

bool
RenderSettings::getEnableMotionBlur() const
{
    return  get<bool>(Tokens->enableMotionBlur);
}

}
