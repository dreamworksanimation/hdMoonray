// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#include "HdmLog.h"
#include <sstream>
#include <iostream>
#include <fstream>
#include <cstdlib>

namespace {

const char* HDM_LOG_ENV_VAR = "HDM_LOG_FILE";

class HdmLogInfo 
{
public:
    static std::ostream* getStream();
    HdmLogInfo();
    ~HdmLogInfo() { if (mOwnsStream) delete mLogStream; }
private:
    bool mOwnsStream;
    std::ostream* mLogStream;
};

HdmLogInfo::HdmLogInfo() 
    : mLogStream(nullptr)
    , mOwnsStream(false)
{
    char* ev = std::getenv(HDM_LOG_ENV_VAR);
    if (ev == nullptr) return;
    std::string evs(ev);
    if (evs == "stdout") {
        mLogStream = &std::cout;
        return;
    }
    if (evs == "stderr") {
        mLogStream = &std::cerr;
        return;
    }
    std::ofstream* stream = new std::ofstream(evs);
    if (stream->is_open()) {
        std::cout << "Writing log to file: " << evs << std::endl;
        mOwnsStream = true;
        mLogStream = stream;
    } else {
        std::cerr << "Failed to open hdm log file: " << evs << std::endl;
        delete stream;
    }
}

std::ostream* HdmLogInfo::getStream() 
{
    static HdmLogInfo info;
    return info.mLogStream;
}

}
namespace hdMoonray {

void hdmLogSyncStart(const std::string& type, const pxr::SdfPath& id, pxr::HdDirtyBits *dirtyBits)
{
    if (std::ostream* stream = HdmLogInfo::getStream()) {
        // generate a single string to prevent interleaving of messages from multiple threads
        std::stringstream s;
        s << "SyncStart " << type << " " << id << " " << std::hex << *dirtyBits << std::endl;
        (*stream) << s.str();
        stream->flush();
    }
}

void hdmLogSyncEnd(const pxr::SdfPath& id)
{
    if (std::ostream* stream = HdmLogInfo::getStream()) {
        std::stringstream s;
        s << "SyncEnd " << id << std::endl;
        (*stream) << s.str();
        stream->flush();
    }
}

void hdmLogRenderBuffer(const std::string& msg,const pxr::SdfPath& id)
{
    if (std::ostream* stream = HdmLogInfo::getStream()) {
        std::stringstream s;
        s << "RenderBuffer " << msg << " " << id << std::endl;
        (*stream) << s.str();
        stream->flush();
    }
}

void hdmLogArras(const std::string& msg)
{
    if (std::ostream* stream = HdmLogInfo::getStream()) {
        std::stringstream s;
        s << "Arras " << msg << std::endl;
        (*stream) << s.str();
        stream->flush();
    }
}

}
