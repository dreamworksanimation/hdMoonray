// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdlib>

namespace hdMoonray {

bool getEnv(const char* name, bool dflt)
{
    const char* v = std::getenv(name);
    if (not v) return dflt;
    return *v && *v != '0' && *v != 'f' && *v != 'F';
}

float getEnv(const char* name, float dflt)
{
    const char* v = std::getenv(name);
    if (not v) return dflt;
    return float(strtod(v,0));
}

int getEnv(const char* name, int dflt)
{
    const char* v = std::getenv(name);
    if (not v) return dflt;
    return int(strtol(v,0,0));
}

const char* getEnv(const char* name, const char* dflt)
{
    const char* v = std::getenv(name);
    return v ? v : dflt ? dflt : "";
}

}
