// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <sdk/sdk.h>
#include <mcrt_dataio/client/receiver/ClientReceiverFb.h>

namespace hdMoonray {

class RenderSettings;

class ArrasSettings
{
public:
    ArrasSettings();

    arras4::log::Logger::Level getLogLevel();
    std::string getUrl(arras4::sdk::SDK&);
    arras4::client::SessionDefinition getSessionDefinition();
    arras4::client::SessionOptions getSessionOptions();

    void applySettings(const RenderSettings&);
    bool getReconnectRequired() { return mReconnectRequired; }
    void clearReconnectRequired() { mReconnectRequired = false; }
    void setLocalMode(bool flag);
    void setLogLevel(int level) { mLogLevel = level; }
    void setHostCount(int count);
    void setMaxFps(float val);
    void setExecMode(std::string val);
    void setLocalReservedCores(int val);
    int getHostCount() const { return mLocalMode ? 1 : mHostCount; }
    int getMaxConnectRetries() { return mMaxConnectRetries; }
    void setMaxConnectRetries(int i) { mMaxConnectRetries = i; }
    // toggling this value causes a reconnect
    void reconnectTrigger(bool flag);
    void setDenoiseMode(bool enable, bool albedoGuiding, bool normalGuiding);
    mcrt_dataio::ClientReceiverFb::DenoiseMode getDenoiseMode() const { return mDenoiseMode; }

private:
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

    arras4::client::SessionDefinition mSingleHostTemplDef;
    arras4::client::SessionDefinition mMultiHostTemplDef;
};

}
