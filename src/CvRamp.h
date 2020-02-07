#ifndef CVRAMP_H_INCLUDED
#define CVRAMP_H_INCLUDED

#include "Ramp.h"

class CvRamp : public Ramp
{
public:
    CvRamp(double rate);
    ~CvRamp() {}
    static LV2_Handle instantiate(const LV2_Descriptor* descriptor, double samplerate, const char* bundle_path, const LV2_Feature* const* features);
    static void connect_port(LV2_Handle instance, uint32_t port, void *data);
}; 

#endif // CVRAMP_H_INCLUDED
