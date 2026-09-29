/*-----------------------------------------------------------/
/ ConfigParam.h
/------------------------------------------------------------/
/ Copyright (c) 2026, Elehobica
/ Released under the BSD-2-Clause
/ refer to https://opensource.org/licenses/BSD-2-Clause
/-----------------------------------------------------------*/

#pragma once

#include "FlashParam.h"

typedef enum {
    CFG_ID_VERSION = FlashParamNs::CFG_ID_BASE,
    CFG_ID_BAUDRATE,
} ParamId_t;

//=================================
// Interface of ConfigParam class
//=================================
struct ConfigParam : FlashParamNs::FlashParam
{
    static ConfigParam& instance() {  // Singleton
        static ConfigParam instance;
        return instance;
    }
    // Parameter<T>                      instance        id               name            default  size
    FlashParamNs::Parameter<std::string> P_CFG_VERSION  {CFG_ID_VERSION,  "CFG_VERSION",  "0.0.0", 16};
    FlashParamNs::Parameter<uint32_t>    P_CFG_BAUDRATE {CFG_ID_BAUDRATE, "CFG_BAUDRATE", 115200};

    void initialize(bool preserveStoreCount = false) override {
        FlashParamNs::FlashParam::initialize(preserveStoreCount);
        P_CFG_VERSION.loadDefault();  // always set default version
    }
};
