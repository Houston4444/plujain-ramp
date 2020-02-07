#ifndef CVLIVERAMP_H_INCLUDED
#define CVLIVERAMP_H_INCLUDED

#include "LiveRamp.h"

class CvLiveRamp : public LiveRamp
{
public:
    CvLiveRamp(double rate);
    ~CvLiveRamp() {}
    static LV2_Handle instantiate(const LV2_Descriptor* descriptor, double samplerate, const char* bundle_path, const LV2_Feature* const* features);
    static void connect_port(LV2_Handle instance, uint32_t port, void *data);
}; 

#endif // CVLIVERAMP_H_INCLUDED
