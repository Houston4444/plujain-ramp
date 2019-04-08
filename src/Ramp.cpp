#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <lv2.h>
#include <map>
#include <iostream>
#include <vector>


#include "Ramp.h"



enum {BYPASS, FIRST_WAITING_PERIOD, WAITING_SIGNAL, FIRST_PERIOD, EFFECT, OUTING};
enum {MODE_ACTIVE, MODE_THRESHOLD, MODE_HOST_TRANSPORT, MODE_MIDI, MODE_MIDI_BYPASS};

float audio_memory[960000]; /*max 10s of audio save in 96000 */
uint32_t max_audio = 12000;
// uint32_t 

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


Ramp::Ramp(){
    is_live_ramp = false;
}

uint32_t Ramp::get_mode(){
    return MODE_HOST_TRANSPORT;
}

float Ramp::get_enter_threshold(){
    return 0.0f;
}

float Ramp::get_leave_threshold(){
    return 0.0f;
}

LV2_Handle Ramp::instantiate(const LV2_Descriptor* descriptor, double samplerate, const char* bundle_path, 
                  const LV2_Feature* const* features)
{
    Ramp *plugin = new Ramp();
    
    plugin->samplerate = samplerate;
    
    plugin->period_count = 0;
    plugin->period_length = 12000;
    plugin->period_death = 12000;
    plugin->taken_beat_offset = 0;
    plugin->current_offset = 0;
    
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
    
    plugin->instance_started_since = 0;
    plugin->start_sent_after_start = false;
    
    plugin->host_was_playing = false;
    
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

    enum {IN, CTRL_IN, OUT, ACTIVE, PRE_START, PRE_START_UNITS, BEAT_OFFSET, HOST_TEMPO,
      DIVISION, MAX_DURATION, HALF_SPEED, DOUBLE_SPEED,
      ATTACK, SHAPE, DEPTH, VOLUME, SUB_SUBOCTAVE, SUBOCTAVE, PLUGIN_PORT_COUNT};

    switch (port)
    {
        case IN:
            plugin->in = (float*) data;
            break;
        case CTRL_IN:
            plugin->midi_in = (const LV2_Atom_Sequence*) data;
            break;
        case OUT:
            plugin->out = (float*) data;
            break;
        case ACTIVE:
            plugin->active = (float*) data;
            break;
        case PRE_START:
            plugin->pre_start = (float*) data;
            break;
        case PRE_START_UNITS:
            plugin->pre_start_units = (float*) data;
            break;
        case BEAT_OFFSET:
            plugin->beat_offset = (float*) data;
            break;
        case HOST_TEMPO:
            plugin->host_tempo = (float*) data;
            break;
        case DIVISION:
            plugin->division = (float*) data;
            break;
        case MAX_DURATION:
            plugin->max_duration = (float*) data;
            break;
        case HALF_SPEED:
            plugin->half_speed = (float*) data;
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
        case SUB_SUBOCTAVE:
            plugin->sub_suboctave = (float*) data;
            break;
        case SUBOCTAVE:
            plugin->suboctave = (float*) data;
            break;
    }
}

/**********************************************************************************************************************************************************/

void Ramp::set_running_step(uint32_t step, uint32_t frame)
{
    last_global_factor_mem = last_global_factor;
    stop_request = false;
    running_step = step;
    
    std::cout << step << std::endl;
    
    switch(step)
    {
        case BYPASS:
            octaves_running = false;
            current_volume = 1;
            current_depth = 0;
            break;
        case FIRST_WAITING_PERIOD:
            period_cut = period_count;
            ex_period_length = period_length;
            oct_period_factor_mem = oct_period_factor;
            period_count = 0;
            period_length = default_fade;
            waiting_enter_threshold = true;
            break;
        case WAITING_SIGNAL:
            octaves_running = false;
            period_count = 0;
            period_length = default_fade;
            break;
        case FIRST_PERIOD:
            period_cut = period_count;
//             ex_period_length = period_length;
            oct_period_factor_mem = oct_period_factor;
            n_period = 1;
            current_offset = 0;
            start_period();
            current_shape = RAIL(*shape, -4, 4);
            current_depth = RAIL(*depth, 0, 1);
            
            has_pre_start = bool(*pre_start > 0.5f);
            
            send_midi_start_stop(true, frame);
            
            waiting_enter_threshold = false;
//             start_first_period(frame);
            break;
        case EFFECT:
            octaves_running = true;
            taken_beat_offset = 0;
            break;
        case OUTING:
            period_cut = period_count;
            ex_period_length = period_length;
            oct_period_factor_mem = oct_period_factor;
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
    return float(*host_tempo);
}

float Ramp::get_division()
{
    ternary = false;
    
    switch (RAIL(ROUND(*division), 0, 14)){
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
    if (running_step == FIRST_PERIOD or running_step == EFFECT){
        ;
    } else {
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
    
    int tmp_period_length;
    uint32_t pre_start_n = ROUND(*pre_start);
    
    if (running_step == FIRST_PERIOD and pre_start_n > 0){
        tmp_period_length =  pre_start_n
                             * int((float(60.0f / tempo_now) * samplerate) / RAIL(ROUND(*pre_start_units), 1, 8));
    } else {
        tmp_period_length = int((float(60.0f / tempo_now) * float(samplerate)) * tmp_division);
    
        if (ternary){
            if (n_period == 1){
                tmp_period_length = tmp_period_length - taken_by_groove;
            } else {
                taken_by_groove = int(0.33333333333 * tmp_period_length);
                tmp_period_length = tmp_period_length + taken_by_groove;
            }
        }
    }
    
    current_offset = (float(60.0f/tempo_now) * samplerate * 0.125)
                     * RAIL(*beat_offset, -1, 1); /* 0.125 for beat/8 */
    tmp_period_length += current_offset - taken_beat_offset;
    
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
    ex_period_length = period_length;
    
    period_audio_start += period_count;
    if (period_audio_start >= 480000){
        period_fake_start = period_audio_start;
        period_audio_start = 0;
    }
    
//     std::cout << period_audio_start << std::endl;
    
    period_count = 0;
    taken_beat_offset = current_offset;
    
    if (n_period == 0){
        n_period = 1;
    } else {
        n_period = 0;
    }
    
    period_length = get_period_length();
    period_length_at_start = period_length;
    
    if (running_step == BYPASS or running_step == OUTING){
        return;
    }
    
    set_period_death();
    
    period_peak = (float(*attack) * float(samplerate)) / 1000;
            
    if (period_peak >= period_death - default_fade){
        period_peak = period_death - default_fade;
    }
    
    ex_volume = current_volume;
    current_volume = powf(10.0f, (*volume)/20.0f);
    if (*volume <= -80.0f){
        current_volume = 0;
    }
    
    if (running_step == WAITING_SIGNAL){
        ex_depth = current_depth;
        current_depth = RAIL(*depth, 0, 1);
    }
    
    current_sub_suboctave = powf(10.0f, (*sub_suboctave)/20.0f);
    if (*sub_suboctave <= -80.0f){
        current_sub_suboctave = 0.0f;
    }
    
    current_suboctave = powf(10.0f, (*suboctave)/20.0f);
    if (*suboctave <= -80.0f){
        current_suboctave = 0.0f;
    }
    
    current_upoctave = powf(10.0f, (*upoctave)/20.0f);
    if (*upoctave <= -80.0f){
        current_upoctave = 0.0f;
    }
}


void Ramp::start_first_period(uint32_t frame)
{
    n_period = 1;
    current_offset = 0;
    start_period();
    current_shape = RAIL(*shape, -4, 4);
    current_depth = RAIL(*depth, 0, 1);
    
    has_pre_start = bool(*pre_start > 0.5f);
    
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
        shape = float(shape * 3/4);
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

float Ramp::get_shut_octave_factor()
{
    float tmp_period_factor = 0.0f;
    
    if ((ex_period_length - period_cut) < default_fade){
        int fade_len = ex_period_length - period_cut;
        
        if (period_count <= fade_len){
            tmp_period_factor = oct_period_factor_mem
                                - (period_count/float(fade_len))
                                    * oct_period_factor_mem;
        }
    } else if (period_count <= default_fade) {
        tmp_period_factor = oct_period_factor_mem
                            - (period_count/float(default_fade))
                                * oct_period_factor_mem;
    }
    
    return tmp_period_factor;
}


float Ramp::get_octave_image_value(float speed, bool leaving){
    if (speed <= 0){
        return 0.0f;
    }
    
    int frame;
    if (speed <= 1){
        frame = period_audio_start + period_count * speed;
    } else {
        frame = period_audio_start + period_length_at_start
                - (period_length_at_start - period_count) * speed;
    }
    
    if (leaving){
        frame -= ex_period_length;
        frame += period_cut;
    }
    
    
    if (frame > period_audio_start + period_count){
        return 0.0f;
    }
    
    if (frame < 0){
        frame += period_fake_start;
        if (frame < period_audio_start + period_count){
            return 0.0f;
        }
    }
    
    if (frame > 960000){
        return 0.0f;
    }
    
    float value = audio_memory[frame];
    if (speed <= 1){
        int delta = period_count % int(1/speed);
        value += (audio_memory[frame+1] - audio_memory[frame]) * delta * speed;
    }
    
    return value;
}


float Ramp::get_effect_up_octave_value()
{
//     uint32_t frame = 0;
//     
// //     if (period_count < (period_length * 3/4.0f)){
// //         int tmp_frame = period_audio_start - (period_length * 3) + period_count * 4;
// //         
// //         if (tmp_frame < 0){
// //             tmp_frame = period_fake_start - (period_length * 3) + period_count * 4;
// //             
// //             if (tmp_frame < (period_audio_start + period_count)){
// //                 return 0.0f;
// //             }
// //         }
// //         
// //         frame = uint32_t(tmp_frame);
// //     } else {
// //         frame = period_audio_start + (period_count - (period_length * 3/4.0f)) * 4;
// //     }
//     
//     if ((period_count * 2) < period_length){
//         int tmp_frame = period_audio_start - period_length + period_count * 2;
//         
//         if (tmp_frame < 0){
//             tmp_frame = period_fake_start - period_length + period_count * 2;
//             
//             if (tmp_frame < (period_audio_start + period_count)){
//                 return 0.0f;
//             }
//         }
//         
//         frame = uint32_t(tmp_frame);
//     } else {
//         frame = period_audio_start + (period_count - period_length/2) * 2;
//     }
//     
//     return audio_memory[frame];
    return get_octave_image_value(2.0, false);
}

float Ramp::get_effect_suboctave_value()
{
//     uint32_t frame = period_audio_start + period_count / 2;
// 
// //     if (ternary and n_period == 1){
// //         frame += ex_period_length;
// //     }
//     
//     float octaved_value = audio_memory[frame];
//     
//     if (period_count % 2){
//         octaved_value += (audio_memory[frame + 1] - audio_memory[frame]) / 2.0f;
//     }
//     
//     return octaved_value;
    return get_octave_image_value(0.5, false);
}


float Ramp::get_effect_sub_suboctave_value()
{
//     uint32_t frame = period_audio_start + period_count / 4;
//     
// //     if (ternary and n_period == 1){
// //         frame += ex_period_length;
// //     }
//     
//     float octaved_value = audio_memory[frame];
//     uint8_t delta = period_count % 4;
//     octaved_value += (audio_memory[frame +1] - octaved_value) * delta/4.0f;
//     return octaved_value;
    return get_octave_image_value(0.25, false);
}


float Ramp::get_shut_up_octave_value()
{
    return get_octave_image_value(2.0, true);
//     float octaved_value = 0.0f;
//     
//     if (period_count <= (ex_period_length - period_cut)){
//         int count = period_cut + period_count;
//         octaved_value = audio_memory[count*2];
//     }
//     
//     return octaved_value;
}
    

float Ramp::get_shut_suboctave_value()
{
//     float octaved_value = 0.0f;
//     
//     if (period_count <= (ex_period_length - period_cut)){
//         int count = period_cut + period_count;
//         octaved_value = audio_memory[count/2];
//         
//         if (count % 2){
//             octaved_value += (audio_memory[1 + count/2] - audio_memory[count/2]) / 2.0f;
//         }
//     }
//     
//     return octaved_value;
    return get_octave_image_value(0.5, true);
}


float Ramp::get_shut_sub_suboctave_value()
{
//     float octaved_value = 0.0f;
//     
//     if (period_count <= (ex_period_length - period_cut)){
// //         int count = period_audio_start - ex_period_length + period_cut + period_count;
//         
//         int count = period_cut + period_count;
//         int frame = period_audio_start - ex_period_length + count/4;
//         if (frame < 0){
//             frame += period_fake_start;
//             if (frame < period_audio_start + period_count){
//                 return 0.0f;
//             }
//         }
//         octaved_value = audio_memory[frame];
//         uint8_t delta = count % 4;
//         octaved_value += (audio_memory[frame +1] - audio_memory[frame]) / float(delta);
//     }
//     
//     return octaved_value;
    return get_octave_image_value(0.25, true);
}


void Ramp::send_midi_start_stop(bool start, uint32_t frame)
{
    return;
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
    
    if (plugin->is_live_ramp){
        /* set midi_out for midi messages */
        const uint32_t capacity = plugin->midi_out->atom.size;
        lv2_atom_forge_set_buffer (&plugin->forge, (uint8_t*)plugin->midi_out, capacity);
        lv2_atom_forge_sequence_head (&plugin->forge, &plugin->frame, 0);
    }
        
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
    
    bool active_state = bool(*plugin->active > 0.5f);
    
    /* check mode change */
    
    uint32_t mode = plugin->get_mode();
    
    if (mode != plugin->current_mode
            or (active_state and not plugin->ex_active_state)){
        plugin->current_mode = mode;
//         std::string s = std::to_string(*plugin->mode);
//         char const *pchar = s.c_str();
//         lv2_log_error (&plugin->logger,
//                        pchar);
        if (active_state){
            if (mode == MODE_ACTIVE or mode == MODE_HOST_TRANSPORT){
                plugin->set_running_step(FIRST_PERIOD);
            } else if (mode == MODE_MIDI_BYPASS){
                plugin->set_running_step(OUTING);
            } else {
                plugin->set_running_step(FIRST_WAITING_PERIOD);
            }
        }
    }
    
    
	
	int start_sample = -1;
	
    
	/* check midi input start/stop signal */
    if (active_state and plugin->is_live_ramp){
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
    
    if (active_state
        and plugin->current_mode == MODE_HOST_TRANSPORT
        and not (plugin->host_speed <= 0)){
            if (not plugin->host_was_playing){
//                 std::cout << "azpeo" << std::endl;
//                 std::cout << plugin->bar_beats << std::endl;
//                 std::string s = std::to_string(plugin->bar_beats);
//                 char const *pchar = s.c_str();
//                 lv2_log_error (&plugin->logger, pchar);
                plugin->set_running_step(FIRST_PERIOD);
            }
            plugin->host_was_playing = true;
    }  else {
        plugin->host_was_playing = false;
    }
    
    if (plugin->ex_active_state and not active_state){
        plugin->set_running_step(OUTING);
    }
    
    float enter_threshold = plugin->get_enter_threshold();
    float leave_threshold = plugin->get_leave_threshold();
    
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
        
        float oct_period_factor = 0;
        
        float suboctave_value = 0;
        float sub_suboctave_value = 0;
        float up_octave_value = 0;
        
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
                
                if (plugin->octaves_running){
                    oct_period_factor = plugin->get_shut_octave_factor();
                    suboctave_value = plugin->get_shut_suboctave_value() * oct_period_factor;
                    sub_suboctave_value = plugin->get_shut_sub_suboctave_value() * oct_period_factor;
                    up_octave_value = plugin->get_shut_up_octave_value() * oct_period_factor;
                }
                break;
                
            case WAITING_SIGNAL:
                period_factor = 0;
                oct_period_factor = 0;
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
                        oct_period_factor = plugin->get_shut_octave_factor();
                        suboctave_value = plugin->get_shut_suboctave_value() * oct_period_factor
                                          + plugin->get_effect_suboctave_value()
                                            * plugin->period_count/float(plugin->period_peak);
                        sub_suboctave_value = plugin->get_shut_sub_suboctave_value()
                                                * oct_period_factor
                                              + plugin->get_effect_sub_suboctave_value()
                                                * plugin->period_count/float(plugin->period_peak);
                        up_octave_value = 0.0f;
                        
                    } else {
                        period_factor = plugin->get_fall_period_factor();
                        oct_period_factor = period_factor;
                        suboctave_value = plugin->get_effect_suboctave_value() * period_factor;
                        sub_suboctave_value = plugin->get_effect_sub_suboctave_value() * period_factor;
                        up_octave_value = 0.0f;
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
                    
                    oct_period_factor = plugin->get_shut_octave_factor();
                }
                break;
                
            case EFFECT:
                if (plugin->period_count < plugin->period_peak){
                    period_factor = float(plugin->period_count)/float(plugin->period_peak);
                    v = plugin->ex_volume
                        + period_factor * (plugin->current_volume - plugin->ex_volume);
                         
                } else {
                    if (plugin->period_count == plugin->period_peak){
                        plugin->current_shape = float(RAIL(*plugin->shape, -4, 4));
                        plugin->current_depth = float(RAIL(*plugin->depth, 0, 1));
                        
                        int tmp_period_length = plugin->get_period_length();
                        if (tmp_period_length >= (plugin->period_peak + plugin->default_fade)){
                            plugin->period_length = tmp_period_length;
                        }
                        plugin->set_period_death();
                    }
                    
                    period_factor = plugin->get_fall_period_factor();
                }
                
                oct_period_factor = period_factor;
                suboctave_value = plugin->get_effect_suboctave_value() * period_factor;
                sub_suboctave_value = plugin->get_effect_sub_suboctave_value() * period_factor;
                up_octave_value = plugin->get_effect_up_octave_value() * period_factor;
                break;
                
            case OUTING:
                v = 1;
                d = 1;
                period_factor = plugin->last_global_factor_mem \
                                + (plugin->period_count/float(plugin->default_fade)) \
                                    * (1 - plugin->last_global_factor_mem);
                
                if (plugin->octaves_running){
                    oct_period_factor = plugin->get_shut_octave_factor();
                    suboctave_value = plugin->get_shut_suboctave_value() * oct_period_factor;
                    sub_suboctave_value = plugin->get_shut_sub_suboctave_value() * oct_period_factor;
                    up_octave_value = plugin->get_shut_up_octave_value() * oct_period_factor;
                }
                break;
        }
        
        plugin->last_global_factor = (1 - (1-period_factor) * d) * v;
        plugin->oct_period_factor = oct_period_factor;
        
        audio_memory[plugin->period_audio_start + plugin->period_count] = plugin->in[i];
        
        plugin->out[i] = plugin->in[i] * plugin->last_global_factor
                        + sub_suboctave_value * plugin->current_sub_suboctave
                        + suboctave_value * plugin->current_suboctave
                        + up_octave_value * plugin->current_upoctave;
                        
        plugin->out_test[i] = plugin->last_global_factor - 0.5;
        plugin->out_test2[i] = plugin->oct_period_factor - 0.5;
//             plugin->out[i] = octaved_value * period_factor;
        
//         if (plugin->period_count <= plugin->period_length/2){
//             audio_memory[plugin->period_count] = plugin->in[i];
//         }
        
        
//         plugin->out[i] = plugin->in[i] * plugin->last_global_factor;
        
        
        
//         if (plugin->running_step != BYPASS){
        plugin->period_count++;
        
        if (plugin->period_count == plugin->period_length){
//             if (plugin->running_step == FIRST_PERIOD){
//                 plugin->set_running_step(EFFECT);
//             } else if (plugin->running_step == FIRST_WAITING_PERIOD){
//                 plugin->set_running_step(WAITING_SIGNAL);
//             } else if (plugin->running_step == OUTING){
//                 plugin->set_running_step(BYPASS);
//             }
            
            switch(plugin->running_step){
                case FIRST_WAITING_PERIOD:
                    plugin->set_running_step(WAITING_SIGNAL);
                    break;
                case FIRST_PERIOD:
                    plugin->set_running_step(EFFECT);
                    break;
                case OUTING:
                    plugin->set_running_step(BYPASS);
                    break;
            }
            
            plugin->start_period();
            
            if (active_state
                and plugin->running_step == EFFECT
                and not plugin->start_sent_after_start){
                /* send start if plugin just loaded because slave instances
                    * may have not been ready for a previous start */
                    plugin->send_midi_start_stop(true, i);
            }
            
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
//         }
    }
    
    if (plugin->is_live_ramp){
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
    }
    
    plugin->ex_active_state = active_state;
    
    if (plugin->instance_started_since <= (2 * plugin->samplerate)){
        plugin->instance_started_since += n_samples;
    }
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
 
