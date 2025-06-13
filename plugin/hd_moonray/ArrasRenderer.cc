// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#include "ArrasRenderer.h"
#include <hydramoonray/RenderSettings.h>
#include <hydramoonray/HdmLog.h>

#include <scene_rdl2/scene/rdl2/RenderOutput.h>
#include <scene_rdl2/scene/rdl2/BinaryWriter.h>

#include <mcrt_messages/RDLMessage.h>
#include <mcrt_messages/CreditUpdate.h>
#include <mcrt_messages/RenderMessages.h>

#include <iostream>

namespace hdMoonray {

using scene_rdl2::logging::Logger;

ArrasRenderer::ArrasRenderer()
{
    mSceneContext = new scene_rdl2::rdl2::SceneContext();

    arras4::sdk::SDK::configAthenaLogger();

    mSDK.reset(new arras4::sdk::SDK);

    mSDK->setMessageHandler([this](const arras4::api::Message& msg) { messageHandler(msg); });
    mSDK->setExceptionCallback([this](const std::exception& e) { exceptionHandler(e); });
    mSDK->setStatusHandler([this](const std::string& msg) { statusHandler(msg); });

    mFbReceiver.reset(new mcrt_dataio::ClientReceiverFb);

    // Setup ClientReceiver console for debugging purposes. This is no impact on the regular
    // ClientReceiverFb functionality. See more detail at ClientReceiverFb.h comments.
    mFbReceiver->consoleAutoSetup([&](const arras4::api::MessageContentConstPtr msg) -> bool {
            mSDK->sendMessage(msg);
            return true;
        });

}

void
ArrasRenderer::applySettings(const RenderSettings& settings)
{
    mSettings.applySettings(settings);
    arras4::log::Logger::instance().setThreshold(mSettings.getLogLevel());
    if (mCurrentDenoiseMode != mSettings.getDenoiseMode()) {
        mCurrentDenoiseMode = mSettings.getDenoiseMode();
        if (mFbReceiver) {
            mFbReceiver->setBeautyDenoiseMode(mCurrentDenoiseMode);
        }
    }
    if (mCurrentDenoiseEngine != mSettings.getDenoiseEngine()) {
        mCurrentDenoiseEngine = mSettings.getDenoiseEngine();
        if (mFbReceiver) {
            mFbReceiver->setDenoiseEngine(mCurrentDenoiseEngine);
        }
    }
}
void
ArrasRenderer::invalidateAllTextureResources()
{
    if (mSDK) {
        auto p{mcrt::RenderMessages::createInvalidateResourcesMessage({"*"})};
        try {
            mSDK->sendMessage(p);
            forceUpdate();
        } catch (std::exception& ex) {
            Logger::error("Reload textures message send failed: ", ex.what());
            hdmLogArras("reloadTexturesSendFailed");
        }
    }
}

void
ArrasRenderer::restartRenderer()
{
    connect(true);
}

ArrasRenderer::~ArrasRenderer()
{ 
    // ensure no more messages are processed
    // (MOONRAY-5403)
    {
        std::lock_guard<std::mutex> guard(mMutex);
        mFbReceiver.reset();
    }
    delete mSceneContext;
}

bool
ArrasRenderer::connect(bool resetFailureCount)
{
    hdmLogArras("connect");
    if (resetFailureCount) {
        mFailedConnects = 0;
    } else if (mFailedConnects > mSettings.getMaxConnectRetries()) {
        return false;
    }

    // assumes mMutex is held by caller
    std::string url = mSettings.getUrl(*mSDK);
    if (url.empty()) return false;

    arras4::client::SessionDefinition def = mSettings.getSessionDefinition();
    arras4::client::SessionOptions opts = mSettings.getSessionOptions();

    mConnected = false;
    mProgress = -1;

    Logger::info("Moonray delegate is connecting to Arras");
    std::string sessionId;
    try {
        sessionId = mSDK->createSession(def, url, opts);
        if (sessionId.empty()) {
            std::string err("Failed to create an Arras session");
            if (mFailedConnects > 0) err += " (retry " + std::to_string(mFailedConnects) + ")";
            Logger::error(err);
            mFailedConnects++;
            hdmLogArras("endConnectErr");
            return false;
        }
    } catch (const arras4::sdk::SDKException& e) {
        std::string err("Failed to create an Arras session");
        if (mFailedConnects > 0) err += " (retry " + std::to_string(mFailedConnects) + ")";
        err += ": " + std::string(e.what());
        Logger::error(err);
        mFailedConnects++;
        hdmLogArras("endConnectErr");
        return false;
    }

    mFailedConnects = 0;
    mFbReceiver.reset(new mcrt_dataio::ClientReceiverFb);
    // Setup ClientReceiver console for debugging purposes. This is no impact on the regular
    // ClientReceiverFb functionality. See more detail at ClientReceiverFb.h comments.
    mFbReceiver->consoleAutoSetup([&](const arras4::api::MessageContentConstPtr msg) -> bool {
            mSDK->sendMessage(msg);
            return true;
        });
    mCurrentDenoiseMode = mSettings.getDenoiseMode();
    mFbReceiver->setBeautyDenoiseMode(mCurrentDenoiseMode);

    mProgress = -1;
    mFirstMessageSent = false;
    mUpdateActive = true; // make sure endUpdate() sends a message
    mConnected = true;
    hdmLogArras("endConnect");
    Logger::info("Moonray delegate created an Arras session with ID ",sessionId);
    mStatusString = "Connected";
    return true;
}

void
ArrasRenderer::messageHandler(const arras4::api::Message& msg)
{
    hdmLogArras("messageHandler");
    std::lock_guard<std::mutex> guard(mMutex);

    // don't try to process messages while destructing
    // (MOONRAY-5403)
    if (!mFbReceiver) return;

    if (msg.classId() == mcrt::ProgressiveFrame::ID) {

        // A progressive frame may contain stats, an image, or both. Only frames
        // containing images change the output of mFbReceiver : these cause the
        // getProgress and getFbActivityCounter values to change, potentially
        // triggering a new snapshot.

        mcrt::ProgressiveFrame::ConstPtr frameMsg = msg.contentAs<mcrt::ProgressiveFrame>();

        Logger::debug("Received ProgressiveFrame progress=", frameMsg->getProgress()*100,
                      " status=", frameMsg->getStatus(),
                      " numBuffers=", frameMsg->mHeader.mNumBuffers);

        mFbReceiver->decodeProgressiveFrame(*frameMsg, true, [&]() {} /*no-op callback*/);

        // update progress. This may be -1.0 if mFbReceiver has received no images at all.
        // After the first received image, it should remain >= 0.0
        mProgress = mFbReceiver->getProgress();

        // update elapsed seconds
        mElapsedSeconds = mFbReceiver->getElapsedSecFromStart();

        // Determine whether the received frame is a result of the latest RDLMessage update
        // we sent out. (Latency means it is possible to receive a frame from an older RDLMessage;
        // we don't want to incorrectly think that this completes the most recent update)
        // Frames from older updates will have a frame id (aka sync id) less than the
        // frame id sent with the latest message.
        bool fromLatestUpdate = mFbReceiver->getFrameId() >= mLatestUpdateFrameId;

        // does this message complete a frame?
        bool completesFrame = mFbReceiver->getStatus() == mcrt::BaseFrame::FINISHED;

        // latest update is completely rendered if the message completes the frame and is
        // a result of the latest update
        mFrameComplete = completesFrame && fromLatestUpdate;

        updateStatusString(mFbReceiver->getBackendStat(), mFbReceiver->getRenderPrepProgress());

        // acknowledge received frame with a credit message
        mcrt::CreditUpdate::Ptr creditMsg = std::make_shared<mcrt::CreditUpdate>();
        creditMsg->value() = 1;
        try {
            mSDK->sendMessage(creditMsg);
        } catch (std::exception& ex) {
            Logger::error("Credit message send failed: ",ex.what());
            hdmLogArras("creditSendFailed");
        }
    }
    hdmLogArras("endMessageHandler");
}

void 
ArrasRenderer::updateStatusString(mcrt_dataio::ClientReceiverFb::BackendStat stat,
                                  float renderPrepProgress)
{
    // status string gets placed in the RenderStats dictionary by RenderDelegate,
    // and is displayed by Houdini just below the progress (top right)
    if (mFrameComplete) {
        // NB doesn't actually seem to show up in Houdini : I think the app
        // stops checking for stats too quickly after completion...
        mStatusString = "Complete"; return;
    }

    switch(stat) {
        case mcrt_dataio::ClientReceiverFb::BackendStat::IDLE:
            break;
        case mcrt_dataio::ClientReceiverFb::BackendStat::RENDER_PREP_RUN: 
        {
            int percent = (int)(renderPrepProgress*100);
            mStatusString = "Render Prep " + std::to_string(percent) + "%";
            break;
        }
        case mcrt_dataio::ClientReceiverFb::BackendStat::RENDER_PREP_CANCEL:
            mStatusString = "Canceling Prep";
            break;
        case mcrt_dataio::ClientReceiverFb::BackendStat::MCRT:
            mStatusString = "Shading";
            break;
        default:
            mStatusString = "";
    }
}

void
ArrasRenderer::pause()
{
    if (not mPaused) {
        mPaused = true;
        mSDK->pause();
    }
}

void
ArrasRenderer::resume()
{
    if (mPaused) {
        mPaused = false;
        mSDK->resume();
    }
}

void
ArrasRenderer::exceptionHandler(const std::exception& e)
{
    Logger::error(e.what());
}

void
ArrasRenderer::statusHandler(const std::string& msg)
{
    // status message should be JSON
    Json::Value root;
    Json::Reader reader;
    if (reader.parse(msg, root)) {
        Json::Value execStatus = root.get("execStatus", Json::Value());
        if (execStatus.isString()) {
            std::string status = execStatus.asString();
            if (status == "stopped" || status == "stopping") {
                Json::Value stopReason = root.get("execStoppedReason", Json::Value());
                Logger::error("Arras session stopped, ",
                              (stopReason.isString() ? stopReason.asString() : msg));
                mConnected = false;
                mStatusString = "Disconnected";
                return;
            }
        }
    }
    // all other messages are dumped to error
    Logger::error("Arras status: " + msg);
}


// Due to problems with Hydra, the RenderOutput may be null, in which case the request must
// be used to get the type and size of the buffer.
bool
ArrasRenderer::allocate(scene_rdl2::rdl2::RenderOutput* ro, PixelData& pd, const PixelSize& request)
{
    if (not pd.mData) { // don't reallocate until resolve() so display does not blink
        pd.mChannels = isBeauty(ro) ? 4 : request.mChannels;
        pd.mWidth = request.mWidth;
        pd.mHeight = request.mHeight;
        pd.vec.resize(pd.mWidth * pd.mHeight * pd.mChannels);
        pd.mData = pd.vec.data();
    }
    return true;
}


bool
ArrasRenderer::resolve(scene_rdl2::rdl2::RenderOutput* ro, PixelData& pd)
{
    // mProgress < 0 indicates that mFbReceiver hasn't received any images yet,
    if (mProgress < 0) return false;

    // see if no change since last time
    unsigned n = mFbReceiver->getFbActivityCounter();
    if (n == pd.filmActivity) return false;
    pd.filmActivity = n;

    if (isBeauty(ro)) {
        mFbReceiver->getBeautyMTSafe(pd.vec, pd.mWidth, pd.mHeight);
        pd.mChannels = 4;
    } else {
        unsigned n = mFbReceiver->getRenderOutputMTSafe(ro->getName(), pd.vec, pd.mWidth, pd.mHeight);
        if (not n) return false; // ignore occasional bad data
        pd.mChannels = n;
    }
    pd.mData = pd.vec.data();

    return true;
}


void
ArrasRenderer::deallocate(scene_rdl2::rdl2::RenderOutput* ro, PixelData& pd)
{ }


float
ArrasRenderer::getProgress() const
{
    return mProgress > 0.0f ? mProgress : 0.0f;
}

float
ArrasRenderer::getElapsedSeconds() const
{
    return mElapsedSeconds;
}

// warning: multithreaded: Almost all the methods appear to be done by the same thread,
// however beginUpdate() may be done by a different thread. A mutex must be used
// around any parts where a simultaneous beginUpdate() would be bad.

void
ArrasRenderer::beginUpdate()
{
    mUpdateActive = true;
}

void
ArrasRenderer::endUpdate()
{
    // ignore successive endUpdates() with no beginUpdate() between...
    if (!mUpdateActive || mPaused) return;
    {
        std::lock_guard<std::mutex> guard(mMutex);
        if (not mConnected || mSettings.getReconnectRequired()) {
            connect(mSettings.getReconnectRequired());
            mSettings.clearReconnectRequired();
        }

        if (!mConnected || !mSDK->isEngineReady()) return;

        hdmLogArras("sendUpdate");
        Logger::info("Restarting Arras render");

        // send any changes in the scene context as an RDLMessage
        scene_rdl2::rdl2::BinaryWriter writer(*mSceneContext);
        // first message to a session sends complete scene, subsequent messages
        // send only a delta relative to the last update
        writer.setDeltaEncoding(mFirstMessageSent);

        // write our data to RDLB and put it in an RDLMessage.
        mcrt::RDLMessage::Ptr rdlMsg = std::make_shared<mcrt::RDLMessage>();
        writer.toBytes(rdlMsg->mManifest, rdlMsg->mPayload);
        if (rdlMsg->mPayload.size() == 0) {
            // sending an empty message restarts the render, so don't
            // do it unnecessarily. Send anyway if mSendEmptyUpdate is true
            if (!mSendEmptyUpdate) {
                mUpdateActive = false;
                hdmLogArras("endSendUpdateEmpty");
                return;
            }
        }
        mSendEmptyUpdate = false;
        rdlMsg->mForceReload = false;

        // Set the frame id (aka sync id) so that we can tell when we
        // start receiving frames associated with this update
        rdlMsg->mSyncId = ++mLatestUpdateFrameId;

        // Send the render data to the computation
        try {
            mSDK->sendMessage(rdlMsg);
        } catch (std::exception& ex) {
            Logger::error("RDL message send failed: ",ex.what());
            hdmLogArras("sendUpdateFailed");
        }
        mFirstMessageSent = true; // we've now sent the first message
        mFrameComplete = false;   // we don't have a complete render of the last update...

        mUpdateActive = false;
        hdmLogArras("endSendUpdate");
    } // drop the lock before doing commit

    // commit all changes on the context to prep
    // for next delta
    mSceneContext->commitAllChanges();
}

bool
ArrasRenderer::isFrameComplete() const
{
    // Asks whether the current image in mFbReceiver is a complete render of the last update sent
    // out. Sending a new update sets this immediately to false, until a new image comes in that
    // completes the render of the new update.
    return !mUpdateActive && mFrameComplete;
}

}
