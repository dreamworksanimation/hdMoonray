// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file RenderOptions.h

#pragma once

#include <string>
#include <utility>
#include <vector>
#include <map>

namespace hd_render {

enum class TimeType
{
    // ways that UsdTimeCode can specify time
    Earliest, Default, DoubleValue
};

// RenderOptions: command line parsing, default values
class RenderOptions
{
public:
    typedef std::pair<std::string, std::string> RenderSetting;

    // Construct render options from command line.  Check for user
    // input errors with the allFlagsValid method.
    RenderOptions(int argc, char *argv[]);

    // Was help requested?
    bool helpRequested() const { return mHelpRequested; }

    // Were all input flags valid?
    bool allFlagsValid() const { return mAllFlagsValid; }

    // Were we mising required flags?
    const std::vector<std::string> &getMissingRequiredFlags() const { return mMissingRequiredFlags; }

    // Return a formatted usage message suitable for printing on
    // stderr or stdout
    std::string usage(const char *argv0) const;

    const std::string &getRenderer() const { return mRenderer; }

    const std::string &getInputSceneFile() const { return mInputSceneFile; }
    const std::string getOutputExrFile() const { return mOutputExrFile; }

    const std::string getCamera() const { return mCamera; }


    unsigned int getWidth() const { return mWidth; }
    unsigned int getHeight() const { return mHeight; }
    float getRes() const { return mRes; }

    unsigned int getRefineLevel() const { return mRefineLevel; }

    TimeType getTimeType() const { return mTimeType; }
    double getTime() const { return mTime; }

    std::string getAov() const { return mAov; }

    bool getPrintRenderSettingsRequested() const { return mPrintRenderSettingsRequested; }
    const std::vector<RenderSetting> &getRenderSettings() const { return mRenderSettings; }

    std::string getDeltaInputSceneFile() const { return mDeltaInputSceneFile; }
    std::string getDeltaOutputExrFile() const { return mDeltaOutputExrFile; }
    const std::vector<RenderSetting> &getDeltaRenderSettings() const { return mDeltaRenderSettings; }

    // This is the hydra "purpose" it can be one or more of render, proxy, and guide
    const std::vector<std::string> &getPurposes() const { return mPurposes; }

    bool hasTraceOpt(const std::string& opt) const { return mTraceFiles.count(opt) > 0; }
    const std::string& getTraceFile(const std::string& opt) const { return mTraceFiles.at(opt); }

    bool useChromeTraceFormat() const { return mUseChromeTraceFormat; }

    bool isTimingEnabled() const { return mEnableTiming; }
    std::string getTimingFile() const { return mTimingFile; }

private:
    // default options
    static const bool sHelpRequested;

    static const char * const sRenderer;

    static const char * const sInputSceneFile;
    static const char * const sOutputExrFile;

    static const char * const sCamera;

    static const char * const sSamplingCamera;

    static const unsigned int sWidth;
    static const unsigned int sHeight;
    static const float sRes;
    static const unsigned int sRefineLevel;

    static const TimeType sTimeType;

    static const char * const sAov;

    static const bool sPrintRenderSettingsRequested;

    static const char * const sDeltaInputSceneFile;
    static const char * const sDeltaOutputExrFile;

    static const bool sUseChromeTraceFormat;

    static const bool sEnableTiming;

    // parsed results
    bool mHelpRequested;

    std::string mRenderer;

    std::string mInputSceneFile;
    std::string mOutputExrFile;

    std::string mCamera;

    unsigned int mWidth;
    unsigned int mHeight;
    float mRes;
    unsigned int mRefineLevel;

    TimeType mTimeType;
    double mTime; // interesting if mTimeType = DoubleValue

    std::string mAov;

    std::vector<RenderSetting> mRenderSettings;
    bool mPrintRenderSettingsRequested;

    std::string mDeltaInputSceneFile;
    std::string mDeltaOutputExrFile;
    std::vector<RenderSetting> mDeltaRenderSettings;

    std::vector<std::string> mPurposes;

    std::map<std::string,std::string> mTraceFiles;
    bool mUseChromeTraceFormat;

    bool mEnableTiming;
    std::string mTimingFile;

    // Indicates a user input error
    bool mAllFlagsValid;
    std::vector<std::string> mMissingRequiredFlags;
};

} // namespace hd_render

