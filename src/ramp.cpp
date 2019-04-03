#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <lv2.h>
#include <map>
#include <iostream>



// #include "Ramp.h"
#include "LiveRamp.h"
/**********************************************************************************************************************************************************/


#define PLUGIN_URI "http://plujain/plugins/ramp"
#define PLUGIN_URI_LIVE "http://plujain/plugins/ramp_live"







/**********************************************************************************************************************************************************/



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
/**********************************************************************************************************************************************************/


/**********************************************************************************************************************************************************/

LV2_SYMBOL_EXPORT
const LV2_Descriptor* lv2_descriptor(uint32_t index)
{
    if (index == 0) return &Descriptor;
    else if (index == 1) return &DescriptorLive;
    else return NULL;
}

/**********************************************************************************************************************************************************/

