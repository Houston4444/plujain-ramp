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

#define MIN(a,b) ( (a) < (b) ? (a) : (b) )
#define MAX(a,b) ( (a) > (b) ? (a) : (b) )
#define RAIL(v, min, max) (MIN((max), MAX((min), (v))))
#define ROUND(v) (uint32_t(v + 0.5f))

typedef struct {
	LV2_URID atom_Blank;
	LV2_URID atom_Object;
	LV2_URID atom_Sequence;
	LV2_URID midi_MidiEvent;
	LV2_URID atom_Float;
	LV2_URID atom_Int;
	LV2_URID atom_Long;
	LV2_URID time_Position;
	LV2_URID time_bar;
	LV2_URID time_barBeat;
	LV2_URID time_beatUnit;
	LV2_URID time_beatsPerBar;
	LV2_URID time_beatsPerMinute;
	LV2_URID time_speed;
} PluginURIs;

class Ramp
{
public:
    Ramp(double rate);
    virtual ~Ramp() {}
    static LV2_Handle instantiate(const LV2_Descriptor* descriptor, double samplerate, const char* bundle_path, const LV2_Feature* const* features);
    static void activate(LV2_Handle instance);
    static void deactivate(LV2_Handle instance);
    static void connect_port(LV2_Handle instance, uint32_t port, void *data);
    static void run(LV2_Handle instance, uint32_t n_samples);
    static void cleanup(LV2_Handle instance);
    static const void* extension_data(const char* uri);
    void set_running_step(uint32_t step);
    void set_running_step(uint32_t step, uint32_t frame);
    virtual float get_tempo();
    float get_division();
    int get_period_length();
    void set_period_death();
    void start_period();
    void start_first_period(uint32_t frame);
    float get_fall_period_factor();
    virtual void send_midi_start_stop(bool start);
    virtual void send_midi_start_stop(bool start, uint32_t frame);
    virtual uint32_t get_mode();
    virtual float get_enter_threshold();
    virtual float get_leave_threshold();
    float get_shut_octave_factor();
    float get_octave_image_value(float speed, bool leaving);
    float get_effect_up_octave_value();
    float get_effect_suboctave_value();
    float get_effect_sub_suboctave_value();
    float get_shut_up_octave_value();
    float get_shut_suboctave_value();
    float get_shut_sub_suboctave_value();
    
    float *in;
    const LV2_Atom_Sequence *midi_in;
    float *out;
    LV2_Atom_Sequence *midi_out;
    float *active;
    float *mode;
    float *enter_threshold;
    float *leave_threshold;
    float *pre_start;
    float *pre_start_units;
    float *beat_offset;
//     float *sync_bpm;
//     float *tempo;
    float *host_tempo;
    float *division;
    float *max_duration;
    float *half_speed;
    float *double_speed;
    float *attack;
    float *shape;
    float *depth;
    float *volume;
    float *speed_effect_1;
    float *speed_effect_1_vol;
    float *speed_effect_2;
    float *speed_effect_2_vol;
    
    double samplerate;
    
    float audio_memory[960000];
    
    int period_count;
    int period_length;
    int period_length_at_start;
    int ex_period_length_at_start;
    int period_death;
    int period_peak;
    int default_fade;
    int threshold_time;
    int taken_beat_offset;
    int current_offset;
    int ex_period_length;
    int period_cut;
    int period_audio_start;
    int period_last_reset;
    
    uint32_t running_step;
    uint32_t current_mode;
    
    bool ex_active_state;
    
    bool waiting_enter_threshold;
    bool leave_threshold_exceeded;
    bool stop_request;
    
    float current_shape;
    float current_depth;
    float current_volume;
    float ex_volume;
    float ex_depth;
    float last_global_factor;
    float last_global_factor_mem;
    float oct_period_factor;
    float oct_period_factor_mem;
    float current_speed_effect_1;
    float current_speed_effect_1_vol;
    float current_speed_effect_2;
    float current_speed_effect_2_vol;
    bool has_pre_start;
    uint8_t n_period;
    bool ternary;
    int taken_by_groove;
    
    uint32_t instance_started_since;
    bool start_sent_after_start;
    
    bool host_was_playing;
    
    /* Host Time */
	bool     host_info;
	float    host_bpm;
	double   bar_beats;
	float    host_speed;
	int      host_div;
    float    beats;
    int64_t  bar;
    
    uint32_t restart_countdown;
    bool waiting_restart_on_bar;
    
    /* LV2 Output */
	LV2_Log_Log* log;
	LV2_Log_Logger logger;
    
    LV2_Atom_Forge forge;
	LV2_Atom_Forge_Frame frame;
    
    LV2_URID_Map* map;
    
	PluginURIs uris;
    
    bool is_live_ramp;
}; 
