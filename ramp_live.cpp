#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <lv2.h>
#include <map>
#include <iostream>

#include <lv2/lv2plug.in/ns/lv2core/lv2.h>
#include <lv2/lv2plug.in/ns/ext/atom/atom.h>
#include <lv2/lv2plug.in/ns/ext/atom/forge.h>
#include <lv2/lv2plug.in/ns/ext/log/logger.h>
#include <lv2/lv2plug.in/ns/ext/midi/midi.h>
#include "lv2/lv2plug.in/ns/ext/time/time.h"
#include <lv2/lv2plug.in/ns/ext/urid/urid.h>

// #include "Ramp.h"
#include "LiveRamp.h"
/**********************************************************************************************************************************************************/


#define PLUGIN_URI "http://plujain/plugins/ramp"
#define PLUGIN_URI_LIVE "http://plujain/plugins/ramp_live"







/**********************************************************************************************************************************************************/


/**********************************************************************************************************************************************************/

static const LV2_Descriptor Descriptor = {
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
    else return NULL;
}

/**********************************************************************************************************************************************************/

