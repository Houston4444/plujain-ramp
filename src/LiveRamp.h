
#include "Ramp.h"

class LiveRamp : public Ramp
{
public:
    LiveRamp(double rate);
    ~LiveRamp() {}
    static LV2_Handle instantiate(const LV2_Descriptor* descriptor, double samplerate, const char* bundle_path, const LV2_Feature* const* features);
    static void connect_port(LV2_Handle instance, uint32_t port, void *data);
    float get_tempo();
    uint32_t get_mode();
    float get_enter_threshold();
    float get_leave_threshold();
    void send_midi_start_stop(bool start, uint32_t frame);
    float *sync_bpm;
    float *tempo;
};
