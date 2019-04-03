
#include "Ramp.h"

class LiveRamp : public Ramp
{
public:
    LiveRamp();
    ~LiveRamp() {}
    float get_tempo();
    static void connect_port(LV2_Handle instance, uint32_t port, void *data);
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
