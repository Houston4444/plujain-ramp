#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <lv2.h>
#include <map>
#include <iostream>

#include "LiveRamp.h"
#include "CvRamp.h"
#include "CvLiveRamp.h"
/**********************************************************************************************************************************************************/


#define PLUGIN_URI "http://plujain/plugins/ramp"
#define PLUGIN_URI_LIVE "http://plujain/plugins/ramp_live"
#define PLUGIN_URI_CV "http://plujain/plugins/ramp_cv"
#define PLUGIN_URI_LIVE_CV "http://plujain/plugins/ramp_live_cv"


/**********************************************************************************************************************************************************/

static const LV2_Descriptor Descriptor = {
    PLUGIN_URI,
    Ramp::instantiate,
    Ramp::connect_port,
    Ramp::activate,
    Ramp::run,
    Ramp::deactivate,
    Ramp::cleanup,
    Ramp::extension_data
};

static const LV2_Descriptor DescriptorLive = {
    PLUGIN_URI_LIVE,
    LiveRamp::instantiate,
    LiveRamp::connect_port,
    LiveRamp::activate,
    LiveRamp::run,
    LiveRamp::deactivate,
    LiveRamp::cleanup,
    LiveRamp::extension_data
};

static const LV2_Descriptor DescriptorCv = {
    PLUGIN_URI_CV,
    CvRamp::instantiate,
    CvRamp::connect_port,
    CvRamp::activate,
    CvRamp::run,
    CvRamp::deactivate,
    CvRamp::cleanup,
    CvRamp::extension_data
};

static const LV2_Descriptor DescriptorCvLive = {
    PLUGIN_URI_LIVE_CV,
    CvLiveRamp::instantiate,
    CvLiveRamp::connect_port,
    CvLiveRamp::activate,
    CvLiveRamp::run,
    CvLiveRamp::deactivate,
    CvLiveRamp::cleanup,
    CvLiveRamp::extension_data
};
/**********************************************************************************************************************************************************/


/**********************************************************************************************************************************************************/

LV2_SYMBOL_EXPORT
const LV2_Descriptor* lv2_descriptor(uint32_t index)
{
    if (index == 0) return &Descriptor;
    else if (index == 1) return &DescriptorLive;
    else if (index == 2) return &DescriptorCv;
    else if (index == 3) return &DescriptorCvLive;
    else return NULL;
}

/**********************************************************************************************************************************************************/

