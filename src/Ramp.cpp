#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <lv2.h>
#include <map>
#include <iostream>
#include <vector>
#include <float.h>


#include "Ramp.h"

enum {BYPASS, FIRST_WAITING_PERIOD, WAITING_SIGNAL, FIRST_PERIOD, EFFECT, OUTING};
enum {MODE_ACTIVE, MODE_THRESHOLD, MODE_HOST_TRANSPORT, MODE_MIDI, MODE_MIDI_BYPASS};

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
        if (not plugin->host_div){
            plugin->host_div = 4.00;
        }
        
		plugin->host_speed = ((LV2_Atom_Float*)speed)->body;
        plugin->host_bpm   = ((LV2_Atom_Float*)bpm)->body;
        
		plugin->bar_beats  = _bar * _bpb * (4.0 / plugin->host_div) + _beat * (4.0 / plugin->host_div);
        plugin->bar_beats = (_bar * _bpb + _beat) * (4.0 / plugin->host_div);
		plugin->host_info  = true;
        plugin->beats = _beat;
        plugin->bar = _bar;
        
        
        if (plugin->host_speed and not plugin->host_was_playing){
            /* beat_start_ref is used to know on which beat we should work when division > 1beat
             * choice is to use the first beat of the next bar */
            plugin->beat_start_ref = _bpb * (_bar +1) * (4.0 / plugin->host_div);
        }
	}
}


Ramp::Ramp(double rate){
    samplerate = rate;
    
    period_count = 0;
    default_fade = int(0.005 * samplerate);  /*  5ms */
    threshold_time = int(0.05 * samplerate); /* 50ms */
    period_length = default_fade;
    ex_period_length = period_length;
    period_length_at_start = period_length;
    ex_period_length_at_start = period_length;
    period_death = period_length;
    period_peak = default_fade;
    period_cut = 0;
    period_audio_start = 0;
    period_last_reset = 0;
    period_random_offset = 0.00;
    
    running_step = BYPASS;
    current_mode = MODE_HOST_TRANSPORT;
    
    ex_active_state = false;
    
    waiting_enter_threshold = true;
    leave_threshold_exceeded = false;
    stop_request = false;
    
    current_shape = 0.0f;
    current_depth = 1.0f;
    current_volume = 1.0f;
    current_bypass_volume = 1.0f; /* used only for ramp CV, else it will stay on 1.0 */
    ex_volume = 1.0f;
    ex_depth = 1.0f;
    last_global_factor = 1.0f;
    last_global_factor_mem = 1.0f;
    oct_period_factor = 0.0f;
    oct_period_factor_mem = 0.0f;
    current_speed_effect_1 = 0.5f;
    current_speed_effect_1_vol = 0.0f;
    current_speed_effect_2 = 2.0f;
    current_speed_effect_2_vol = 0.0f;
    
    has_pre_start = false;
    ternary = false;
    
    instance_started_since = 0;
    start_sent_after_start = false;
    
    host_was_playing = false;
    
    host_info = false;
    host_bpm = 120.00f;
    bar_beats = 0.0;
    host_speed = 0.0f;
    host_div = 4;
    beats = 0.0f;
    bar = 0;
    
    note_pressed = false;
    active_note = 30;
    
    restart_countdown = 0;
    waiting_restart_on_bar = false;
    
    bar_beats_hot_node = 0.0;
    bar_beats_target = 0.25;
    bar_beats_period_start = 0.0;
    
    
    is_live_ramp = false;
    is_cv_ramp = false;
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

uint8_t Ramp::get_midi_note(){
    return 0x00;
}

LV2_Handle Ramp::instantiate(const LV2_Descriptor* descriptor, double samplerate, const char* bundle_path, 
                  const LV2_Feature* const* features)
{
    Ramp *plugin = new Ramp(samplerate);
    
    int i;
	for (i=0; features[i]; ++i) {
		if (!strcmp (features[i]->URI, LV2_URID__map)) {
			plugin->map = (LV2_URID_Map*)features[i]->data;
		} else if (!strcmp (features[i]->URI, LV2_LOG__log)) {
			plugin->log = (LV2_Log_Log*)features[i]->data;
		}
	}
	
	plugin->last_velocity = 100;
	plugin->bar_beats = 0.00;
    plugin->beat_start_ref = 0;
    
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
//     Ramp *plugin;
//     plugin = (Ramp *) instance;
//     plugin->send_midi_note_off(0);
}
        

/**********************************************************************************************************************************************************/

void Ramp::connect_port(LV2_Handle instance, uint32_t port, void *data)
{
    Ramp *plugin;
    plugin = (Ramp *) instance;

    enum {IN, CTRL_IN, OUT, ACTIVE, PRE_START, PRE_START_UNITS, BEAT_OFFSET, RANDOM_OFFSET,
          HOST_TEMPO, DIVISION, MAX_DURATION, HALF_SPEED, DOUBLE_SPEED,
          ATTACK, SHAPE, RANDOM_SHAPE, DEPTH, VOLUME,
          SPEED_EFFECT_1, SPEED_EFFECT_1_VOL, SPEED_EFFECT_2, SPEED_EFFECT_2_VOL, PLUGIN_PORT_COUNT};

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
        case RANDOM_OFFSET:
            plugin->random_offset = (float*) data;
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
        case RANDOM_SHAPE:
            plugin->random_shape = (float*) data;
            break;
        case DEPTH:
            plugin->depth = (float*) data;
            break;
        case VOLUME:
            plugin->volume = (float*) data;
            break;
        case SPEED_EFFECT_1:
            plugin->speed_effect_1 = (float*) data;
            break;
        case SPEED_EFFECT_1_VOL:
            plugin->speed_effect_1_vol = (float*) data;
            break;
        case SPEED_EFFECT_2:
            plugin->speed_effect_2 = (float*) data;
            break;
        case SPEED_EFFECT_2_VOL:
            plugin->speed_effect_2_vol = (float*) data;
            break;
    }
}

/**********************************************************************************************************************************************************/

void Ramp::set_running_step(uint32_t step, uint32_t frame)
{
    last_global_factor_mem = last_global_factor;
    oct_period_factor_mem = oct_period_factor;
    stop_request = false;
    running_step = step;
    
    switch(step)
    {
        case BYPASS:
            current_volume = 1;
            current_depth = 0;
            break;
        case FIRST_WAITING_PERIOD:
            start_period();
            waiting_enter_threshold = true;
            break;
        case WAITING_SIGNAL:
            break;
        case FIRST_PERIOD:
            start_period();
            current_shape = RAIL(*shape, -4, 4);
            current_depth = RAIL(*depth, 0, 1);
            has_pre_start = bool(*pre_start > 0.5f);
            send_midi_start_stop(true, frame);
            waiting_enter_threshold = false;
            break;
        case EFFECT:
            break;
        case OUTING:
            waiting_enter_threshold = false;
            send_midi_start_stop(false);
            start_period();
            break;
    }
}

void Ramp::set_running_step(uint32_t step)
{
    set_running_step(step, 0);
}

float Ramp::get_volume_factor()
{
    if (*volume <= -80.0f){
        return 0.0f;
    }
    
    return powf(10.0f, (*volume)/20.0f);
}

float Ramp::get_inactive_volume_factor()
{
    return 1.0f;
}

float Ramp::get_tempo()
{
    if (host_info){
        return host_bpm;
    } else {
        return float(*host_tempo);
    }
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
            return 1/3.0f;
        case 11:
            return 0.25;
        case 12:
            ternary = true;
            return 0.25;
        case 13:
            return 1/6.0f;
        case 14:
            return 0.125;
    }
    
    return 0.25;
}

void Ramp::set_period_properties()
{
    if (running_step == FIRST_PERIOD or running_step == EFFECT){
        ;
    } else {
        period_length = default_fade;
        period_death = period_length;
        period_peak = period_length / 2;
        period_hot_node_count = 0;
        period_hot_node_ratio = 0.0f;
        return;
    }
    
    if (period_count == 0 or period_count == period_peak){
        float tmp_division = get_division();
        
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
        
        current_division = tmp_division;
        current_pre_start = ROUND(*pre_start);
        current_pre_start_units = RAIL(ROUND(*pre_start_units), 1, 8);
        current_max_duration = *max_duration;
        
        if (period_count == 0){
            current_attack = *attack;
        } else if (period_count == period_peak){
            current_beat_offset = RAIL(*beat_offset, -1, 1);
        }
            
    }
    
    float bb_pre_start = float(current_pre_start) / float(current_pre_start_units) ;
    float bb_offset = 0.125 * float(current_beat_offset); /* 0.125 for beat/8 */
    
    if (running_step == FIRST_PERIOD){
        if (period_count == 0){
            bar_beats_hot_node = 0.00;
            bar_beats_target = 0.00;
        } else {
            bar_beats_hot_node += (period_count - period_hot_node_count)
                                        / double((float(60.0f / current_tempo) * samplerate));
            current_tempo = get_tempo();
        }
    }
        
    if (running_step == FIRST_PERIOD and current_pre_start > 0){
        bar_beats_target = bb_pre_start + bb_offset;                 
    } else {
        if (period_count == 0){
            bar_beats_hot_node = bar_beats_target;
            
        } else if (period_count == period_peak){
            bar_beats_hot_node += (period_count - period_hot_node_count)
                                        / double((float(60.0f / current_tempo) * samplerate));
        } else {
            if (current_mode == MODE_HOST_TRANSPORT and host_speed){
                if (period_length - period_count < 64
                    and bar_beats >= bar_beats_target
                    and bar_beats - bar_beats_target <= double( (64/double(48000)) * (60/double(current_tempo)) * samplerate)){
                        /* ignore hot message which wants to start a new period right now
                        * when the current period is very closed to be finished */
                        current_tempo = host_bpm;
                        return;
                }
                
                current_tempo = host_bpm;
                bar_beats_hot_node = bar_beats;
            } else {
                bar_beats_hot_node += (period_count - period_hot_node_count)
                                        / double((float(60.0f / current_tempo) * samplerate));
                current_tempo = get_tempo();
            }
        }
        
        /* set period_hot_node_ratio and define the mininum time needed to finish the current period */
        int needed_count = 0;
        
        if (period_count == 0){
            period_hot_node_ratio = 0.0f;
            needed_count = 0;
            
        } else if (period_count < period_peak){
            period_hot_node_ratio += float((1.0f - float(period_hot_node_ratio))
                                        * (float(period_count - period_hot_node_count)
                                            / float(period_peak - period_hot_node_count)));
            needed_count = int(float(1.0f - float(period_hot_node_ratio)) * MIN(period_peak, default_fade) + default_fade);
            
        } else if (period_count == period_peak){
            period_hot_node_ratio = 1.0f;
            needed_count = default_fade;

        } else if (period_count < period_death){
            period_hot_node_ratio -= float(period_hot_node_ratio)
                                    * float(period_count - period_hot_node_count)
                                        / float(period_death - period_hot_node_count);
                                        
            needed_count = int(float(period_hot_node_ratio) * default_fade * 0.5f);
            
        } else {
            period_hot_node_ratio = 0.0f;
            needed_count = 0;
        }
        
        if (period_count < threshold_time){ 
            needed_count = MAX(needed_count, threshold_time - period_count);
        }
        
        double bar_beats_mini_need = needed_count / double((float(60.0f / current_tempo) * samplerate));
            
        /* define bar_beats_target, the musical start of the next period */
        if (ternary){
            float base_time = float(current_division) * 2;
            double pos_in_div = fmod(bar_beats_hot_node - bb_offset - bb_pre_start, base_time) / base_time;
            bool second_time = bool(pos_in_div >= 2/double(3.0));
            bar_beats_target = bar_beats_hot_node
                                + bb_pre_start
                                + bb_offset
                                + period_random_offset;
                                
            if (not second_time){
                bar_beats_target += double(base_time) * 2/double(3.0) - fmod(bar_beats_hot_node, base_time);
                
                while (bar_beats_target <= bar_beats_hot_node){
                    bar_beats_target += base_time;
                }
                while(bar_beats_target - bar_beats_hot_node > double(base_time)){
                    bar_beats_target -= base_time;
                }
                
                if (bar_beats_target - bar_beats_hot_node < bar_beats_mini_need
                        or bar_beats_target - bar_beats_period_start < base_time / double(3)){
                    bar_beats_target += base_time / double(3);
                }
                
            } else {
                bar_beats_target += base_time - fmod(bar_beats_hot_node, base_time);
                
                while (bar_beats_target <= bar_beats_hot_node){
                    bar_beats_target += base_time;
                }
                
                while(bar_beats_target - bar_beats_hot_node > base_time){
                    bar_beats_target -= base_time;
                }
                
                if (bar_beats_target - bar_beats_hot_node < bar_beats_mini_need
                     or bar_beats_target - bar_beats_period_start < base_time / double(6)){
                    bar_beats_target += base_time * 2/double(3);
                }
            }
        } else {
            bar_beats_target = bar_beats_hot_node
                                + current_division
                                - fmod(bar_beats_hot_node, current_division)
                                + bb_pre_start
                                + bb_offset
                                + period_random_offset;
            
            if (current_division > 1.00){
                bar_beats_target += fmod(beat_start_ref, current_division);
            }

            while (bar_beats_target <= bar_beats_hot_node){
                bar_beats_target += current_division;
            }
                
            while (bar_beats_target - bar_beats_hot_node > current_division){
                bar_beats_target -= current_division;
            }
            
            if (bar_beats_target - bar_beats_hot_node < bar_beats_mini_need
                or bar_beats_target - bar_beats_period_start < current_division / 2.0){
                bar_beats_target += current_division;
            }
        }
    }
    
    
    /* finally, set period properties (length, death and peak) */
    period_length = period_count
                    + int((float(60.0f / current_tempo) * float(samplerate))
                          * (bar_beats_target - bar_beats_hot_node));
    
    if (period_count < period_death){
        period_death = int((float(60.0f / current_tempo) * float(samplerate)) * current_max_duration);
        period_death = MIN(period_length, period_death);
    }
    
    if (period_count < period_peak){
        period_peak = (current_attack * float(samplerate)) / 1000;
        period_peak = MIN(period_peak, period_death - default_fade);
    }
    
    period_hot_node_count = period_count;
}
    
    
void Ramp::start_period()
{
    ex_period_length = period_length;
    
    period_audio_start += period_count;
    if (period_audio_start >= 480000){
        period_last_reset = period_audio_start;
        period_audio_start = 0;
    }
    
    period_cut = period_count;
    period_count = 0;
    
    bar_beats_hot_node = bar_beats_target;
    bar_beats_period_start = bar_beats_hot_node;
    period_random_offset = 0.125 * (2.0f * float(rand() / (RAND_MAX + 1.)) - 1.0f) * RAIL(*random_offset, 0, 1);
    
    current_tempo = get_tempo();
    set_period_properties();
    ex_period_length_at_start = period_length_at_start;
    period_length_at_start = period_length;
    
    if (running_step == BYPASS or running_step == OUTING){
        return;
    }
    
    ex_volume = current_volume;
    current_volume = get_volume_factor();
    
    if (running_step == WAITING_SIGNAL){
        ex_depth = current_depth;
        current_depth = RAIL(*depth, 0, 1);
    }
    
    if (is_cv_ramp){
        current_bypass_volume = get_inactive_volume_factor();
    } else {
        current_speed_effect_1 = float(*speed_effect_1);
        current_speed_effect_1_vol = powf(10.0f, (*speed_effect_1_vol)/20.0f);
        current_speed_effect_2 = float(*speed_effect_2);
        current_speed_effect_2_vol = powf(10.0f, (*speed_effect_2_vol)/20.0f);
    }
}


void Ramp::set_shape()
{
    current_shape = float(*shape);
//     float bb_offset = 0.125 * float(current_beat_offset);
//     float in_div = abs(fmod(bar_beats_target - current_division - bb_offset, *swing_inside) / *swing_inside - 0.5); 
//     current_shape += 4 * (1 - in_div);
    float randoshape = float(RAIL(*random_shape, 0, 1)) * float((8 * float(rand() / (RAND_MAX + 1.))) - 4.0);
    current_shape += randoshape;
    current_shape = float(RAIL(current_shape, -4, 4));
    int n_max = (period_death - period_peak) / default_fade;
    
    if (n_max < 1){
        current_shape = 0.0f;
    } else if (n_max < 2){
        current_shape *= 0.25f;
    } else if (n_max < 3){
        current_shape *= 0.50f;;
    } else if (n_max < 4){
        current_shape *= 0.75f;
    }
}

float Ramp::get_fall_period_factor()
{
    if (period_count > period_death){
        return 0.0f;
    }
    float period_factor = 1.0f;
    
    float period_fall_ratio = (1.0f - float(period_hot_node_ratio))
                              + float(period_hot_node_ratio)
                                * (float(period_count - period_hot_node_count)
                                   / (float(period_death - period_hot_node_count)) );
    
    float pre_factor = 1.0f - float(period_fall_ratio);
    float shape = current_shape;
    
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
                                * (1 - period_count/float(fade_len));
        }
    } else if (period_count <= default_fade) {
        tmp_period_factor = oct_period_factor_mem
                            * (1 - period_count/float(default_fade));
    }
    return tmp_period_factor;
}


float Ramp::get_octave_image_value(float speed, bool leaving){
    if (speed == 0){
        return 0.0f;
    }
    
    float float_frame;
    int start = period_audio_start;
    int count = period_count;
    int length_at_start = period_length_at_start;
    
    if (leaving){
        count += period_cut;
        start -= period_cut;
        length_at_start = ex_period_length_at_start;
    }
    
    if (speed <= 1){
        float_frame = start + count * speed;
    } else {
        float_frame = start + length_at_start
                - (length_at_start - count) * speed;
    }
    
    if (float_frame > period_audio_start + period_count){
        /* calling futur frame, impossible */
        return 0.0f;
    }
    
    if (float_frame < 0){
        float_frame += period_last_reset;
        if (float_frame < period_audio_start + period_count){
            return 0.0f;
        }
    }
    
    if (float_frame > 960000){
        return 0.0f;
    }

    int frame1 = int(float_frame);
    int frame2 = frame1 +1;
//     if (frame1 == period_last_reset -1){
//         frame2 = 0;
//     }
    
    float frame_offset = float_frame - frame1;
    float value = audio_memory[frame1] * (1 - frame_offset) + audio_memory[frame2] * frame_offset;
    
    return value;
}

void Ramp::send_midi_start_stop(bool start, uint32_t frame)
{
    return;
}

void Ramp::send_midi_start_stop(bool start)
{
    send_midi_start_stop(start, 0);
}

void Ramp::send_midi_note(uint32_t frame)
{
    return;
}

void Ramp::send_midi_note_off(uint32_t frame)
{
    return;
}

/**********************************************************************************************************************************************************/

void Ramp::run(LV2_Handle instance, uint32_t n_samples)
{
    Ramp *plugin;
    plugin = (Ramp *) instance;
    plugin->block_id +=1;
    
    int hot_change_sample = -1;
    
    if (plugin->is_live_ramp){
        /* set midi_out for midi messages */
        const uint32_t capacity = plugin->midi_out->atom.size;
        lv2_atom_forge_set_buffer (&plugin->forge, (uint8_t*)plugin->midi_out, capacity);
        lv2_atom_forge_sequence_head (&plugin->forge, &plugin->frame, 0);
        
        if (plugin->current_mode != MODE_HOST_TRANSPORT){
            if (fabs(plugin->get_tempo() - plugin->current_tempo) > FLT_EPSILON){
                hot_change_sample = 0;
            }
        }
    }
    
    /* process control events (for host transport) */
    LV2_Atom_Event* ev = lv2_atom_sequence_begin (&(plugin->midi_in)->body);
    while (!lv2_atom_sequence_is_end (&(plugin->midi_in)->body, (plugin->midi_in)->atom.size, ev)) {
        if (ev->body.type == plugin->uris.atom_Blank || ev->body.type == plugin->uris.atom_Object) {
            const LV2_Atom_Object* obj = (LV2_Atom_Object*)&ev->body;
            if (obj->body.otype == plugin->uris.time_Position) {
                if (plugin->current_mode == MODE_HOST_TRANSPORT){
                    update_position(plugin, obj);
                    if (plugin->host_speed){
                        hot_change_sample = ev->time.frames;
                    }
                }
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
        and (plugin->host_speed > 0)){
            plugin->host_was_playing = true;
    }  else {
        plugin->host_was_playing = false;
    }
    
    if (plugin->ex_active_state and not active_state){
        plugin->set_running_step(OUTING);
        plugin->send_midi_note_off(0);
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
            /* if there is a start_sample found, prefer node (0.0f) before in the buffer limit */
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
        /* save audio sample */
        plugin->audio_memory[plugin->period_audio_start + plugin->period_count] = plugin->in[i];
        
        float period_factor = 1.0f;
        float v = plugin->current_volume;
        float d = plugin->current_depth;
        
        float oct_period_factor = 0.0f;
        float speed_effect_1_value = 0.0f;
        float speed_effect_2_value = 0.0f;
        
        if (start_sample == int(i)){
            plugin->set_running_step(FIRST_PERIOD, i);
        }
        
        if (hot_change_sample == int(i)){
            if (not bool(plugin->period_count == 0 or plugin->period_count == plugin->period_peak)){
                plugin->set_period_properties();
            }
        }
        
        switch(plugin->running_step)
        {
            case BYPASS:
                v = plugin->current_bypass_volume;
                d = 0.0f;
                break;
                
            case FIRST_WAITING_PERIOD:
                v = 1;
                d = 1;
                period_factor = plugin->last_global_factor_mem
                                - (plugin->last_global_factor_mem
                                    - (1 - plugin->current_depth) * plugin->current_volume)
                                  * plugin->period_count/float(plugin->period_length);
                
                oct_period_factor = plugin->get_shut_octave_factor();
                speed_effect_1_value = plugin->get_octave_image_value(plugin->current_speed_effect_1, true);
                speed_effect_2_value = plugin->get_octave_image_value(plugin->current_speed_effect_2, true);
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
                    /* first period is played (not muted) */
                    
                    if (plugin->period_count == 0){
                        plugin->send_midi_note(i);
                    }
                    
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
                        speed_effect_1_value = plugin->get_octave_image_value(plugin->current_speed_effect_1, true)
                                                * oct_period_factor
                                               + plugin->get_octave_image_value(plugin->current_speed_effect_1, false)
                                                * plugin->period_count/float(plugin->period_peak);
                        speed_effect_2_value = plugin->get_octave_image_value(plugin->current_speed_effect_2, true)
                                                * oct_period_factor
                                               + plugin->get_octave_image_value(plugin->current_speed_effect_2, false)
                                                * plugin->period_count/float(plugin->period_peak);
                    } else {
                        if (plugin->period_count == plugin->period_peak){
                            plugin->set_shape();
                            plugin->current_depth = float(RAIL(*plugin->depth, 0, 1));
                            plugin->set_period_properties();
                        }   
                        
                        period_factor = plugin->get_fall_period_factor();
                        oct_period_factor = period_factor;
                        speed_effect_1_value = plugin->get_octave_image_value(plugin->current_speed_effect_1, false)
                                               * oct_period_factor;
                        speed_effect_2_value = plugin->get_octave_image_value(plugin->current_speed_effect_2, false)
                                               * oct_period_factor;
                    }
                    
                    if (plugin->period_count == plugin->period_death){
                        plugin->send_midi_note_off(i);
                    }
                    
                } else {
                    /* first period is muted */
                    
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
                    speed_effect_1_value = plugin->get_octave_image_value(plugin->current_speed_effect_1, true)
                                           * oct_period_factor;
                    speed_effect_2_value = plugin->get_octave_image_value(plugin->current_speed_effect_2, true)
                                           * oct_period_factor;
                }
                break;
                
            case EFFECT:
                if (plugin->period_count == 0){
                    plugin->send_midi_note(i);
                }
                
                if (plugin->period_count < plugin->period_peak){
                    period_factor = float(plugin->period_hot_node_ratio)
                                    + float(1 - plugin->period_hot_node_ratio)
                                        * (float(plugin->period_count - plugin->period_hot_node_count)
                                           / float(plugin->period_peak - plugin->period_hot_node_count)); 
                    
                    v = plugin->ex_volume
                        + period_factor * (plugin->current_volume - plugin->ex_volume);
                         
                } else {
                    if (plugin->period_count == plugin->period_peak){
                        plugin->set_shape();
                        plugin->current_depth = float(RAIL(*plugin->depth, 0, 1));
                        plugin->set_period_properties();
                    }
                    
                    period_factor = plugin->get_fall_period_factor();
                }
                
                if (plugin->period_count == plugin->period_death){
                    plugin->send_midi_note_off(i);
                }
                
                oct_period_factor = period_factor;
                speed_effect_1_value = plugin->get_octave_image_value(plugin->current_speed_effect_1, false)
                                        * oct_period_factor;
                speed_effect_2_value = plugin->get_octave_image_value(plugin->current_speed_effect_2, false)
                                       * oct_period_factor;
                break;
                
            case OUTING:
                v = 1;
                d = 1;
                period_factor = plugin->last_global_factor_mem
                                + (plugin->current_bypass_volume - plugin->last_global_factor_mem)
                                  * plugin->period_count/float(plugin->default_fade);
                
                oct_period_factor = plugin->oct_period_factor_mem
                                    * (1 - plugin->period_count/float(plugin->default_fade));
                speed_effect_1_value = plugin->get_octave_image_value(plugin->current_speed_effect_1, true)
                                       * oct_period_factor;
                speed_effect_2_value = plugin->get_octave_image_value(plugin->current_speed_effect_2, true)
                                       * oct_period_factor;
                break;
        }
        
        plugin->last_global_factor = (1 - (1-period_factor) * d) * v;
        plugin->oct_period_factor = oct_period_factor;
        
        if (plugin->is_cv_ramp){
            plugin->out[i] = plugin->last_global_factor * 10.0f;
        } else {
            plugin->out[i] = plugin->in[i] * plugin->last_global_factor
                            + speed_effect_1_value * plugin->current_speed_effect_1_vol
                            + speed_effect_2_value * plugin->current_speed_effect_2_vol;
        }
                            
        plugin->period_count++;
        
        if (plugin->period_count >= plugin->period_length){
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
        
        if (plugin->period_count == (plugin->period_length - plugin->threshold_time)){
            plugin->peak_in_threshold = 0.0f;
        }
        
        if (plugin->period_count > (plugin->period_length - plugin->threshold_time)){
            plugin->peak_in_threshold = MAX(plugin->peak_in_threshold, abs(plugin->in[i]));
            
            if (! plugin->waiting_enter_threshold
                    and ! plugin->leave_threshold_exceeded
                    and plugin->current_mode == MODE_THRESHOLD
                    and abs(plugin->in[i] >= leave_threshold)){
                plugin->leave_threshold_exceeded = true;
            }
        }
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
 
