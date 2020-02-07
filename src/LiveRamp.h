#ifndef LIVERAMP_H_INCLUDED
#define LIVERAMP_H_INCLUDED


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
    uint8_t get_midi_note();
    void send_midi_start_stop(bool start, uint32_t frame);
    void send_midi_note(uint32_t frame);
    void send_midi_note_off(uint32_t frame);
    float *enter_threshold;
    float *leave_threshold;
    float *sync_bpm;
    float *tempo;
    float *midi_note;
    float *midi_velocity_min;
    float *midi_velocity_max;
    float *midi_inertia;
};

#endif // LIVERAMP_H_INCLUDED
