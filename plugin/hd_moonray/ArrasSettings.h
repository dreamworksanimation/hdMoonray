// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <pxr/imaging/hd/renderDelegate.h>

#include <sdk/sdk.h>
#include <mcrt_dataio/client/receiver/ClientReceiverFb.h>

namespace hdMoonray {

class RenderSettings;

class ArrasSettings
{
public:
    ArrasSettings();

    void addDescriptors(pxr::HdRenderSettingDescriptorList& descriptorList) const;

    std::string getUrl(arras4::sdk::SDK&);
    arras4::client::SessionDefinition getSessionDefinition();
    arras4::client::SessionOptions getSessionOptions();

    void applySettings(const RenderSettings&);
    bool getReconnectRequired() { return mReconnectRequired; }
    void clearReconnectRequired() { mReconnectRequired = false; }
    int getMaxConnectRetries() { return mMaxConnectRetries; }
    mcrt_dataio::ClientReceiverFb::DenoiseMode getDenoiseMode() const { return mDenoiseMode; }
    mcrt_dataio::ClientReceiverFb::DenoiseEngine getDenoiseEngine() const {return mDenoiseEngine; }
    arras4::log::Logger::Level getLogLevel()
        { return static_cast<arras4::log::Logger::Level>(mLogLevel); }


private:
    void setLocalMode(bool flag);
    void setLogLevel(int level) { mLogLevel = level; }
    void setHostCount(int count);
    void setMaxFps(float val);
    void setExecMode(std::string val);
    void setLocalReservedCores(int val);
    void setMaxConnectRetries(int i) { mMaxConnectRetries = i; }
    void setDenoiseMode(bool enable, bool oidn, bool albedoGuiding, bool normalGuiding);
    void setDenoiseEngine(bool enbale, bool oidn);

    bool mLocalMode = true;
    int mLogLevel = 1;
    int mHostCount = 1;
    float mMaxFps = 12;
    int mLocalReservedCores = 1;
    bool mReconnectTrigger = false;
    bool mReconnectRequired = false;
    int mMaxConnectRetries = 2;
    std::string mExecMode = "auto";
    mcrt_dataio::ClientReceiverFb::DenoiseMode mDenoiseMode = mcrt_dataio::ClientReceiverFb::DenoiseMode::DISABLE;
    mcrt_dataio::ClientReceiverFb::DenoiseEngine mDenoiseEngine = mcrt_dataio::ClientReceiverFb::DenoiseEngine::OPTIX;
    arras4::client::SessionDefinition mSingleHostTemplDef;
    arras4::client::SessionDefinition mMultiHostTemplDef;
};

}
