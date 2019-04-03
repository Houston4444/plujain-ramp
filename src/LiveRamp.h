
#include "Ramp.h"

class LiveRamp : public Ramp
{
public:
    LiveRamp();
    ~LiveRamp() {}
    static LV2_Handle instantiate(const LV2_Descriptor* descriptor, double samplerate, const char* bundle_path, const LV2_Feature* const* features);
    static void connect_port(LV2_Handle instance, uint32_t port, void *data);
    float get_tempo();
    uint32_t get_mode();
    float get_enter_threshold();
    float get_leave_threshold();
    void send_midi_start_stop(bool start, uint32_t frame);
//     const LV2_Atom_Sequence *midi_in;
//     LV2_Atom_Sequence *midi_out;
//     float *mode;
//     float *enter_threshold;
//     float *leave_threshold;
    float *sync_bpm;
    float *tempo;
};
