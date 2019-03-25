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
/**********************************************************************************************************************************************************/

#define PLUGIN_URI "http://plujain/plugins/ramp"
enum {IN, MIDI_IN, OUT, MIDI_OUT,
      ACTIVE, MODE, ENTER_THRESHOLD, LEAVE_THRESHOLD, PRE_START, PRE_START_UNITS,
      SYNC_BPM, HOST_TEMPO, TEMPO, DIVISION, MAX_DURATION, HALF_SPEED, DOUBLE_SPEED,
      ATTACK, SHAPE, DEPTH, VOLUME, PLUGIN_PORT_COUNT};

enum {BYPASS, FIRST_WAITING_PERIOD, WAITING_SIGNAL, FIRST_PERIOD, EFFECT, OUTING};

enum {MODE_ACTIVE, MODE_THRESHOLD, MODE_HOST_TRANSPORT, MODE_MIDI, MODE_MIDI_BYPASS};

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

/**********************************************************************************************************************************************************/

static void
map_mem_uris (LV2_URID_Map* map, PluginURIs* uris)
{
	uris->atom_Blank          = map->map (map->handle, LV2_ATOM__Blank);
	uris->atom_Object         = map->map (map->handle, LV2_ATOM__Object);
	uris->midi_MidiEvent      = map->map (map->handle, LV2_MIDI__MidiEvent);
	uris->atom_Sequence       = map->map (map->handle, LV2_ATOM__Sequence);
	uris->time_Position       = map->map (map->handle, LV2_TIME__Position);
	uris->atom_Long           = map->map (map->handle, LV2_ATOM__Long);
	uris->atom_Int            = map->map (map->handle, LV2_ATOM__Int);
	uris->atom_Float          = map->map (map->handle, LV2_ATOM__Float);
	uris->time_bar            = map->map (map->handle, LV2_TIME__bar);
	uris->time_barBeat        = map->map (map->handle, LV2_TIME__barBeat);
	uris->time_beatUnit       = map->map (map->handle, LV2_TIME__beatUnit);
	uris->time_beatsPerBar    = map->map (map->handle, LV2_TIME__beatsPerBar);
	uris->time_beatsPerMinute = map->map (map->handle, LV2_TIME__beatsPerMinute);
	uris->time_speed          = map->map (map->handle, LV2_TIME__speed);
}


class Ramp
{
public:
    Ramp() {}
    ~Ramp() {}
    static LV2_Handle instantiate(const LV2_Descriptor* descriptor, double samplerate, const char* bundle_path, const LV2_Feature* const* features);
    static void activate(LV2_Handle instance);
    static void deactivate(LV2_Handle instance);
    static void connect_port(LV2_Handle instance, uint32_t port, void *data);
    static void run(LV2_Handle instance, uint32_t n_samples);
    static void cleanup(LV2_Handle instance);
    static const void* extension_data(const char* uri);
    void set_running_step(uint32_t step);
    void set_running_step(uint32_t step, uint32_t frame);
    float get_tempo();
    float get_division();
    int get_period_length();
    void set_period_death();
    void start_period();
    void start_first_period(uint32_t frame);
    float get_fall_period_factor();
    void send_midi_start_stop(bool start);
    void send_midi_start_stop(bool start, uint32_t frame);
    
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
    float *sync_bpm;
    float *tempo;
    float *host_tempo;
    float *division;
    float *max_duration;
    float *half_speed;
    float *double_speed;
    float *attack;
    float *shape;
    float *depth;
    float *volume;
    
    double samplerate;
    
    int period_count;
    int period_length;
    int period_death;
    int period_peak;
    int default_fade;
    int threshold_time;
    
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
    bool has_pre_start;
    int n_period;
    bool ternary;
    int taken_by_groove;
    
    
    
    /* Host Time */
	bool     host_info;
	float    host_bpm;
	double   bar_beats;
	float    host_speed;
	int      host_div;
    
    /* LV2 Output */
	LV2_Log_Log* log;
	LV2_Log_Logger logger;
    
    LV2_Atom_Forge forge;
	LV2_Atom_Forge_Frame frame;
    
    LV2_URID_Map* map;
    
	PluginURIs uris;
};

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

/**********************************************************************************************************************************************************/

static void
update_position (Ramp* plugin, const LV2_Atom_Object* obj)
{
    /* code taken from x42 step sequencer */ 
	const PluginURIs* uris = &plugin->uris;

	LV2_Atom* bar   = NULL;
	LV2_Atom* beat  = NULL;
	LV2_Atom* bunit = NULL;
	LV2_Atom* bpb   = NULL;
	LV2_Atom* bpm   = NULL;
	LV2_Atom* speed = NULL;

	lv2_atom_object_get (
			obj,
			uris->time_bar, &bar,
			uris->time_barBeat, &beat,
			uris->time_beatUnit, &bunit,
			uris->time_beatsPerBar, &bpb,
			uris->time_beatsPerMinute, &bpm,
			uris->time_speed, &speed,
			NULL);

	if (   bpm   && bpm->type == uris->atom_Float
			&& bpb   && bpb->type == uris->atom_Float
			&& bar   && bar->type == uris->atom_Long
			&& beat  && beat->type == uris->atom_Float
			&& bunit && bunit->type == uris->atom_Int
			&& speed && speed->type == uris->atom_Float)
	{
		float    _bpb   = ((LV2_Atom_Float*)bpb)->body;
		int64_t  _bar   = ((LV2_Atom_Long*)bar)->body;
		float    _beat  = ((LV2_Atom_Float*)beat)->body;

		plugin->host_div   = ((LV2_Atom_Int*)bunit)->body;
		plugin->host_bpm   = ((LV2_Atom_Float*)bpm)->body;
		plugin->host_speed = ((LV2_Atom_Float*)speed)->body;

		plugin->bar_beats  = _bar * _bpb + _beat; // * host_div / 4.0 // TODO map host metrum
		plugin->host_info  = true;
	}
}
/**********************************************************************************************************************************************************/

LV2_SYMBOL_EXPORT
const LV2_Descriptor* lv2_descriptor(uint32_t index)
{
    if (index == 0) return &Descriptor;
    else return NULL;
}

/**********************************************************************************************************************************************************/

LV2_Handle Ramp::instantiate(const LV2_Descriptor* descriptor, double samplerate, const char* bundle_path, const LV2_Feature* const* features)
{
    Ramp *plugin = new Ramp();
    
    plugin->samplerate = samplerate;
    
    plugin->period_count = 0;
    plugin->period_length = 12000;
    plugin->period_death = 12000;
    plugin->ex_active_state = false;
    
    plugin->default_fade = int(0.005 * samplerate);  /*  5ms */
    plugin->threshold_time = int(0.05 * samplerate); /* 50ms */
    
    plugin->running_step = WAITING_SIGNAL;
    plugin->current_mode = MODE_ACTIVE;
    
    plugin->waiting_enter_threshold = true;
    plugin->leave_threshold_exceeded = false;
    plugin->stop_request = false;
    
    plugin->current_volume = 1.0f;
    plugin->current_depth = 1.0f;
    plugin->ex_volume = 1.0f;
    plugin->ex_depth = 1.0f;
    plugin->last_global_factor = 1.0f;
    plugin->last_global_factor_mem = 1.0f;
    plugin->has_pre_start = false;
    plugin->n_period = 1;
    plugin->taken_by_groove = 0;
    
    int i;
	for (i=0; features[i]; ++i) {
		if (!strcmp (features[i]->URI, LV2_URID__map)) {
			plugin->map = (LV2_URID_Map*)features[i]->data;
		} else if (!strcmp (features[i]->URI, LV2_LOG__log)) {
			plugin->log = (LV2_Log_Log*)features[i]->data;
		}
	}
	
	lv2_log_logger_init (&plugin->logger, plugin->map, plugin->log);
    
    if (!plugin->map) {
		lv2_log_error (&plugin->logger,
                       "Ramp.lv2 error: Host does not support urid:map\n");
		free (plugin);
		return NULL;
	}
    
    lv2_atom_forge_init (&plugin->forge, plugin->map);
    map_mem_uris(plugin->map, &plugin->uris);
    
    return (LV2_Handle)plugin;
}

/**********************************************************************************************************************************************************/

void Ramp::activate(LV2_Handle instance)
{
    // TODO: include the activate function code here
}

/**********************************************************************************************************************************************************/

void Ramp::deactivate(LV2_Handle instance)
{
    // TODO: include the deactivate function code here
}
        

/**********************************************************************************************************************************************************/

void Ramp::connect_port(LV2_Handle instance, uint32_t port, void *data)
{
    Ramp *plugin;
    plugin = (Ramp *) instance;

    switch (port)
    {
        case IN:
            plugin->in = (float*) data;
            break;
        case MIDI_IN:
            plugin->midi_in = (const LV2_Atom_Sequence*) data;
            break;
        case OUT:
            plugin->out = (float*) data;
            break;
        case MIDI_OUT:
            plugin->midi_out = (LV2_Atom_Sequence*) data;
            break;
        case ACTIVE:
            plugin->active = (float*) data;
            break;
        case MODE:
            plugin->mode = (float*) data;
            break;
        case ENTER_THRESHOLD:
            plugin->enter_threshold = (float*) data;
            break;
        case LEAVE_THRESHOLD:
            plugin->leave_threshold = (float*) data;
            break;
        case PRE_START:
            plugin->pre_start = (float*) data;
            break;
        case PRE_START_UNITS:
            plugin->pre_start_units = (float*) data;
            break;
        case SYNC_BPM:
            plugin->sync_bpm = (float*) data;
            break;
        case HOST_TEMPO:
            plugin->host_tempo = (float*) data;
            break;
        case TEMPO:
            plugin->tempo = (float*) data;
            break;
        case DIVISION:
            plugin->division = (float*) data;
            break;
        case HALF_SPEED:
            plugin->half_speed = (float*) data;
            break;
        case MAX_DURATION:
            plugin->max_duration = (float*) data;
            break;
        case DOUBLE_SPEED:
            plugin->double_speed = (float*) data;
            break;
        case ATTACK:
            plugin->attack = (float*) data;
            break;
        case SHAPE:
            plugin->shape = (float*) data;
            break;
        case DEPTH:
            plugin->depth = (float*) data;
            break;
        case VOLUME:
            plugin->volume = (float*) data;
            break;
    }
}

/**********************************************************************************************************************************************************/

void Ramp::set_running_step(uint32_t step, uint32_t frame)
{
    last_global_factor_mem = last_global_factor;
    stop_request = false;
    running_step = step;
    
    switch(step)
    {
        case BYPASS:
            current_volume = 1;
            current_depth = 0;
            break;
        case FIRST_WAITING_PERIOD:
            period_count = 0;
            period_length = default_fade;
            waiting_enter_threshold = true;
            break;
        case WAITING_SIGNAL:
            period_count = 0;
            period_length = default_fade;
            break;
        case FIRST_PERIOD:
            start_first_period(frame);
            break;
        case EFFECT:
            break;
        case OUTING:
            period_count = 0;
            waiting_enter_threshold = false;
            send_midi_start_stop(false);
            break;
    }
}

void Ramp::set_running_step(uint32_t step)
{
    set_running_step(step, 0);
}


float Ramp::get_tempo()
{
    float tempo_now;
    if (*sync_bpm > 0.5){
        tempo_now = *host_tempo;
    } else {
        tempo_now = *tempo;
    }
    
    return tempo_now;
}

float Ramp::get_division()
{
    ternary = false;
    
    switch (int(*division)){
        case 0:
            return 8;
        case 1:
            return 7;
        case 2:
            return 6;
        case 3:
            return 5;
        case 4:
            return 4;
        case 5:
            return 3;
        case 6:
            return 2;
        case 7:
            return 1;
        case 8:
            return 0.5f;
        case 9:
            ternary = true;
            return 0.5f;
        case 10:
            return 0.33333333333;
        case 11:
            return 0.25;
        case 12:
            ternary = true;
            return 0.25;
        case 13:
            return 0.16666666667;
        case 14:
            return 0.125;
    }
    
    return 0.25;
}

int Ramp::get_period_length()
{
    if (running_step == WAITING_SIGNAL){
        return default_fade;
    }
    
    float tempo_now = get_tempo();
    float tmp_division = float(get_division());
    
    if (ternary){
        if (*half_speed > 0.5f and *double_speed > 0.5f){
            ;
        } else if (*half_speed > 0.5f){
            tmp_division = tmp_division * 2.0f;
            ternary = false;
        } else if (*double_speed > 0.5f){
            tmp_division = tmp_division * (2/3.0f);
            ternary = false;
        }
    } else {
        if (*half_speed > 0.5f){
            tmp_division = tmp_division * 2.0f;
        }
        
        if (*double_speed > 0.5f){
            tmp_division = tmp_division / 2.0f;
        }
    }
    
    int tmp_period_length = int(
        (float(60.0f / tempo_now) * float(samplerate)) * tmp_division);
    
    if (ternary){
        if (n_period == 1){
            tmp_period_length = tmp_period_length - taken_by_groove;
        } else {
            taken_by_groove = int(0.33333333333 * tmp_period_length);
            tmp_period_length = tmp_period_length + taken_by_groove;
        }
    }
    
    if (tmp_period_length < threshold_time){
        tmp_period_length = threshold_time;
    }
    
    return tmp_period_length;
}

void Ramp::set_period_death()
{
    float tempo_now = get_tempo();
    
    int tmp_duration = int(
        (float(60.0f / tempo_now) * float(samplerate)) * *max_duration);
    
    if (tmp_duration > period_length){
        tmp_duration = period_length;
    }
    
    period_death = tmp_duration;
}


void Ramp::start_period()
{
    period_count = 0;
    
    if (n_period == 0){
        n_period = 1;
    } else {
        n_period = 0;
    }
    
    period_length = get_period_length();
    set_period_death();
    
    period_peak = (float(*attack) * float(samplerate)) / 1000;
            
    if (period_peak >= period_death - default_fade){
        period_peak = period_death - default_fade;
    }
    
    ex_volume = current_volume;
    current_volume = powf(10.0f, (*volume)/20.0f);
    
    if (running_step == WAITING_SIGNAL){
        ex_depth = current_depth;
        current_depth = float(*depth);
    }
}


void Ramp::start_first_period(uint32_t frame)
{
    n_period = 1;
    start_period();
    current_shape = float(*shape);
    current_depth = float(*depth);
    
    float tempo_now = get_tempo();
    int pre_start_n = int(*pre_start);
    
    float too_much = float(*pre_start) - pre_start_n;
    if (too_much >= 0.5f){
        pre_start_n += 1;
    }
    
    if (pre_start_n < 1){
        has_pre_start = false;
        period_length = get_period_length();
    } else { 
        has_pre_start = true;
        period_length = pre_start_n * int(
            (float(60.0f / tempo_now) * float(samplerate)) / float(*pre_start_units));
    }
    
    if (period_peak >= period_death - default_fade){
        period_peak = period_death - default_fade;
    }
    
    send_midi_start_stop(true, frame);
    
    waiting_enter_threshold = false;
}

float Ramp::get_fall_period_factor()
{
    if (period_count > period_death){
        return 0.0f;
    }
    
    float period_factor = 1.0f;
    int n_max = (period_death - period_peak) / default_fade;
                    
    float pre_factor = 1.00f - float(period_count - period_peak) 
                       / (float(period_death - period_peak));
    float shape = current_shape;
    
    if (n_max < 1){
        shape = 0.0f;
    } else if (n_max < 2){
        shape = float(shape/4);
    } else if (n_max < 3){
        shape = float(shape/2);
    } else if (n_max < 4){
        shape = float(shape*3/4);
    }
                
    if (current_shape > 0.0f){
        while (shape > 1.0f){
            pre_factor = sin(M_PI_2 * pre_factor);
            shape -= 1.0f;
        }
        
        period_factor = (sin(M_PI_2 * pre_factor) * shape) \
                        + (pre_factor * (1 - shape));
    } else {       
        while (shape < -1.0f){
            pre_factor = sin(M_PI_2 * (pre_factor -1)) +1; 
            shape += 1.0f;
        }
        
        period_factor = (sin(M_PI_2 * (pre_factor -1)) +1) * (-shape) \
                        + pre_factor * (1 + shape);
    }
    
    return period_factor;
}


void Ramp::send_midi_start_stop(bool start, uint32_t frame)
{
    LV2_Atom midiatom;
    midiatom.type = uris.midi_MidiEvent;
    midiatom.size = 3;
    
    uint8_t msg[3];
    msg[0] = 0xfa;
    if (!start){
        msg[0] = 0xfc;
    }
    msg[1] = 0;
    msg[2] = 0;
    
    if (0 == lv2_atom_forge_frame_time (&forge, frame)) return;
	if (0 == lv2_atom_forge_raw (&forge, &midiatom, sizeof (LV2_Atom))) return;
	if (0 == lv2_atom_forge_raw (&forge, msg, 3)) return;
    lv2_atom_forge_pad (&forge, sizeof (LV2_Atom) + 3);
}

void Ramp::send_midi_start_stop(bool start)
{
    send_midi_start_stop(start, 0);
}


/**********************************************************************************************************************************************************/

void Ramp::run(LV2_Handle instance, uint32_t n_samples)
{
    Ramp *plugin;
    plugin = (Ramp *) instance;
    
    bool active_state = bool(*plugin->active > 0.5f);
    
    /* check mode change */
    if (uint32_t(*plugin->mode) != plugin->current_mode
            or (active_state and not plugin->ex_active_state)){
        plugin->current_mode = uint32_t(*plugin->mode);
        
        if (active_state){
            if (plugin->current_mode == MODE_ACTIVE){
                plugin->set_running_step(FIRST_PERIOD);
            } else if (plugin->current_mode == MODE_MIDI_BYPASS){
                plugin->set_running_step(OUTING);
            } else {
                plugin->set_running_step(FIRST_WAITING_PERIOD);
            }
        }
    }
    
    /* set midi_out for midi messages */
    const uint32_t capacity = plugin->midi_out->atom.size;
	lv2_atom_forge_set_buffer (&plugin->forge, (uint8_t*)plugin->midi_out, capacity);
	lv2_atom_forge_sequence_head (&plugin->forge, &plugin->frame, 0);
    
    /* process control events (for host transport) */
	LV2_Atom_Event* ev = lv2_atom_sequence_begin (&(plugin->midi_in)->body);
	while (!lv2_atom_sequence_is_end (&(plugin->midi_in)->body, (plugin->midi_in)->atom.size, ev)) {
		if (ev->body.type == plugin->uris.atom_Blank || ev->body.type == plugin->uris.atom_Object) {
			const LV2_Atom_Object* obj = (LV2_Atom_Object*)&ev->body;
			if (obj->body.otype == plugin->uris.time_Position) {
				update_position(plugin, obj);
			}
		}
		ev = lv2_atom_sequence_next (ev);
	}
	
	int start_sample = -1;
	
	/* check midi input start/stop signal */
    if (active_state){
        LV2_Atom_Event const* midi_ev = (LV2_Atom_Event const*)((uintptr_t)((&(plugin->midi_in)->body) + 1)); // lv2_atom_sequence_begin
        while( // !lv2_atom_sequence_is_end
            (const uint8_t*)midi_ev < ((const uint8_t*) &(plugin->midi_in)->body + (plugin->midi_in)->atom.size)
            ){
            if (midi_ev->body.type == plugin->uris.midi_MidiEvent) {
                uint32_t frame = uint32_t(midi_ev->time.frames);
                
                if (((const uint8_t*)(midi_ev+1))[0] == 0xfa){
                    start_sample = int(frame);
                } else if (((const uint8_t*)(midi_ev+1))[0] == 0xfc){
                    if (plugin->current_mode == MODE_MIDI_BYPASS){
                        plugin->set_running_step(OUTING);
                    } else if (plugin->current_mode == MODE_ACTIVE){
                        ;
                    } else {
                        plugin->stop_request = true;
                    }
                    
                    plugin->send_midi_start_stop(false, frame);
                }
            }
            midi_ev = (LV2_Atom_Event const*) // lv2_atom_sequence_next()
                ((uintptr_t)((const uint8_t*)midi_ev + sizeof(LV2_Atom_Event) + ((midi_ev->body.size + 7) & ~7)));
        }
    }
    
    if (active_state and plugin->current_mode == MODE_HOST_TRANSPORT){
        if (plugin->host_speed > 0){
            if (plugin->running_step < FIRST_PERIOD){
                plugin->set_running_step(FIRST_PERIOD);
            }
        } else {
            if (plugin->running_step == EFFECT){
                plugin->set_running_step(OUTING);
            }
        }
    }
    
    if (plugin->ex_active_state and not active_state){
        plugin->set_running_step(OUTING);
    }
    
    float enter_threshold = powf(10.0f, (*plugin->enter_threshold)/20.0f);
    float leave_threshold = powf(10.0f, (*plugin->leave_threshold)/20.0f);
    if (*plugin->leave_threshold == -80.0f){
        leave_threshold = 0.0f;
    }
    if (leave_threshold > enter_threshold){
        leave_threshold = enter_threshold;
    }
    
    
    bool node_found = false;
    
    if (active_state 
        and plugin->waiting_enter_threshold
        and (plugin->current_mode == MODE_THRESHOLD)){
        /* Search attack sample */
        bool up = true;
        
        for ( uint32_t i = 0; i < n_samples; i++)
        {   
            float value = plugin->in[i];
            
            if (value >= enter_threshold){
                up = bool(value >= 0.0f);
                start_sample = i;
                break;
            }
        }
        
        if (start_sample > 0){
            /* if there is an start_sample found, prefer node (0.0f) before in the buffer limit */
            for ( int j = start_sample-1; j >= 0; j--)
            {
                float value;
                value = plugin->in[j];
                
                if ((up and value <= 0.0f)
                    or ((! up) and value >= 0.0f)){
                        node_found = true;
                        start_sample = j;
                        break;
                }
            }
            
            if (!node_found){
                start_sample = 0;
            }
        }
    }
    
    for ( uint32_t i = 0; i < n_samples; i++)
    {
        float period_factor = 1;
        float v = plugin->current_volume;
        float d = plugin->current_depth;
        
        if (start_sample == int(i)){
            plugin->set_running_step(FIRST_PERIOD, i);
        }
        
        switch(plugin->running_step)
        {
            case BYPASS:
                v = 1.0f;
                d = 0.0f;
                break;
                
            case FIRST_WAITING_PERIOD:
                v = 1;
                d = 1;
                period_factor = plugin->last_global_factor_mem
                                - (plugin->last_global_factor_mem
                                    - (1 - plugin->current_depth) * plugin->current_volume)
                                  * plugin->period_count/float(plugin->period_length);
                break;
                
            case WAITING_SIGNAL:
                period_factor = 0;
                d = plugin->ex_depth \
                    + plugin->period_count/float(plugin->period_length) \
                      * (plugin->current_depth - plugin->ex_depth);
                        
                v = plugin->ex_volume \
                    + plugin->period_count/float(plugin->period_length) \
                      * (plugin->current_volume - plugin->ex_volume);
                break;
                
            case FIRST_PERIOD:
                if (not plugin->has_pre_start
                        or plugin->current_mode == MODE_ACTIVE
                        or plugin->current_mode == MODE_THRESHOLD
                        or plugin->current_mode == MODE_MIDI_BYPASS){
                    if (plugin->period_count < plugin->period_peak){
                        if (plugin->period_peak <= (2 * plugin->default_fade)){
                            /* fade from last_global_factor_mem to peak */
                            v = 1;
                            d = 1;
                            period_factor = plugin->last_global_factor_mem
                                            + (plugin->period_count/float(plugin->period_peak))
                                                * (plugin->current_volume
                                                    - plugin->last_global_factor_mem);
                                                
                        } else if (plugin->period_count <= plugin->default_fade){
                            /* fade from last_global_factor_mem to the down point */
                            v = 1;
                            d = 1;
                            period_factor = plugin->last_global_factor_mem
                                            - (plugin->last_global_factor_mem
                                                - (1 - plugin->current_depth) * plugin->current_volume)
                                                * plugin->period_count/float(plugin->default_fade);
                                                
                        } else {
                            period_factor = float(plugin->period_count - plugin->default_fade) \
                                            / float(plugin->period_peak - plugin->default_fade);
                        }
                    } else {
                        period_factor = plugin->get_fall_period_factor();
                    }
                } else {
                    if (plugin->period_count < plugin->default_fade){
                        v = 1;
                        d = 1;
                        period_factor = plugin->last_global_factor_mem
                                        - (plugin->last_global_factor_mem
                                            - (1 - plugin->current_depth) * plugin->current_volume)
                                            * plugin->period_count/float(plugin->default_fade);
                    } else {
                        period_factor = 0;
                    }
                }
                break;
                
            case EFFECT:
                if (plugin->period_count < plugin->period_peak){
                    period_factor = float(plugin->period_count)/float(plugin->period_peak);
                    v = plugin->ex_volume
                        + period_factor * (plugin->current_volume - plugin->ex_volume);
                         
                } else {
                    if (plugin->period_count == plugin->period_peak){
                        plugin->current_shape = float(*plugin->shape);
                        plugin->current_depth = float(*plugin->depth);
                        
                        int tmp_period_length = plugin->get_period_length();
                        if (tmp_period_length >= (plugin->period_peak + plugin->default_fade)){
                            plugin->period_length = tmp_period_length;
                        }
                        plugin->set_period_death();
                    }
                    
                    period_factor = plugin->get_fall_period_factor();
                }
                break;
                
            case OUTING:
                if (plugin->period_count < plugin->default_fade){
                    v = 1;
                    d = 1;
                    period_factor = plugin->last_global_factor_mem \
                                    + (plugin->period_count/float(plugin->default_fade)) \
                                      * (1 - plugin->last_global_factor_mem);
                } else {
                    plugin->set_running_step(BYPASS);
                }
                break;
        }
        
        plugin->last_global_factor = (1 - (1-period_factor) * d) * v;
        plugin->out[i] = plugin->in[i] * plugin->last_global_factor;
        
        plugin->period_count++;
        
        if (plugin->running_step != BYPASS){
            if (plugin->period_count == plugin->period_length){
                if (plugin->running_step == FIRST_PERIOD){
                    plugin->set_running_step(EFFECT);
                } else if (plugin->running_step == FIRST_WAITING_PERIOD){
                    plugin->set_running_step(WAITING_SIGNAL);
                } 
                
                plugin->start_period();
                
                if (plugin->stop_request){
                    plugin->set_running_step(WAITING_SIGNAL);
                }
                
                
                plugin->waiting_enter_threshold = not bool(plugin->leave_threshold_exceeded);
                plugin->leave_threshold_exceeded = false;
            }

            if (plugin->period_count > (plugin->period_length - plugin->threshold_time)
                and ! plugin->waiting_enter_threshold
                and ! plugin->leave_threshold_exceeded
                and plugin->current_mode == MODE_THRESHOLD
                and abs(plugin->in[i] >= leave_threshold)){
                    plugin->leave_threshold_exceeded = true;
            }
        }
    }
    
    LV2_ATOM_SEQUENCE_FOREACH (plugin->midi_out, ev1) {
		LV2_ATOM_SEQUENCE_FOREACH (plugin->midi_out, ev2) {
			if (ev2 <= ev1) {
				continue;
			}
			if (ev1->time.frames > ev2->time.frames) {
				// swap events
				assert (ev1->body.size == ev2->body.size);
				assert (ev1->body.size == 3);
				int64_t tme = ev1->time.frames;
				uint8_t body[3];
				memcpy (body, (const uint8_t*)(ev1 + 1), 3);
				memcpy ((uint8_t*)(ev1 + 1), (const uint8_t*)(ev2 + 1), 3);
				ev1->time.frames = ev2->time.frames;
				memcpy ((uint8_t*)(ev2 + 1), body, 3);
				ev2->time.frames = tme;
			}
		}
	}
    plugin->ex_active_state = active_state;
}

/**********************************************************************************************************************************************************/

void Ramp::cleanup(LV2_Handle instance)
{
    delete ((Ramp *) instance);
}

/**********************************************************************************************************************************************************/

const void* Ramp::extension_data(const char* uri)
{
    return NULL;
}
