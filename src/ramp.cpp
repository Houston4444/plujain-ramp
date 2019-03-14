#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <lv2.h>
#include <iostream>

/**********************************************************************************************************************************************************/

#define PLUGIN_URI "http://plujain/plugins/ramp"
enum {IN, SIDECHAIN, OUT, ACTIVE, MODE, ENTER_THRESHOLD, LEAVE_THRESHOLD, PRE_SILENCE, PRE_SILENCE_UNITS,
      SYNC_BPM, HOST_TEMPO, TEMPO, DIVISION, MAX_DURATION, HALF_SPEED, DOUBLE_SPEED, ATTACK,
      SHAPE, DEPTH, VOLUME, OUT_TEST, PLUGIN_PORT_COUNT};

enum {NONE, WAITING_THRESHOLD, FIRST_PERIOD, EFFECT, OUTING};

enum {MODE_ACTIVE_BP, MODE_ACTIVE_MUTE, MODE_IN_BP, MODE_IN_MUTE, MODE_SIDECHAIN_BP, MODE_SIDECHAIN_MUTE};
/**********************************************************************************************************************************************************/

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
    bool mode_direct_active();
    bool mode_threshold_in();
    bool mode_threshold_sidechain();
    bool mode_mute();
    void enter_effect();
    void leave_effect();
    float get_tempo();
    int get_period_length();
    int get_current_duration();
    void start_period();
    void start_first_period();
    float get_fall_period_factor();
    
    float *in;
    float *sidechain;
    float *out;
    float *active;
    float *mode;
    float *enter_threshold;
    float *leave_threshold;
    float *pre_silence;
    float *pre_silence_units;
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
    float *out_test;
    
    double samplerate;
    int period_count;
    int period_length;
    int current_duration;
    int fade_in;
    int default_fade;
    
    bool ex_active_state;
    bool next_is_active;
    uint32_t running_step;
    bool deactivate_ordered;
    float current_shape;
    float current_depth;
    float current_volume;
    float ex_volume;
    float last_global_factor;
    bool has_pre_start;
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
    
    plugin->period_count = 0;
    plugin->period_length = 12000;
    plugin->current_duration = 12000;
    plugin->next_is_active = false;
    plugin->ex_active_state = false;
    
    plugin->default_fade = 250;
    plugin->samplerate = samplerate;
    
    plugin->running_step = NONE;
    plugin->deactivate_ordered = false;
    plugin->current_volume = 1.0f;
    plugin->ex_volume = 1.0f;
    plugin->last_global_factor = 1.0f;
    plugin->has_pre_start = false;
    
    return (LV2_Handle)plugin;
}

/**********************************************************************************************************************************************************/
bool Ramp::mode_direct_active()
{
    int plugin_mode = int(*mode);
    if (plugin_mode == MODE_ACTIVE_BP or plugin_mode == MODE_ACTIVE_MUTE){
        return true;
    }
    
    return false;
}

bool Ramp::mode_threshold_in()
{
    int plugin_mode = int(*mode);
    if (plugin_mode == MODE_IN_BP or plugin_mode == MODE_IN_MUTE){
        return true;
    }
    
    return false;
}


bool Ramp::mode_threshold_sidechain()
{
    int plugin_mode = int(*mode);
    if (plugin_mode == MODE_SIDECHAIN_BP or plugin_mode == MODE_SIDECHAIN_MUTE){
        return true;
    }
    
    return false;
}

bool Ramp::mode_mute()
{
    int plugin_mode = int(*mode);
    if (plugin_mode == MODE_ACTIVE_MUTE
        or plugin_mode == MODE_IN_MUTE
        or plugin_mode == MODE_SIDECHAIN_MUTE){
            return true;
    }
    
    return false;
}


void Ramp::enter_effect()
{
    period_count = 0;
    deactivate_ordered = false;
    
    if (mode_direct_active()){
        start_first_period();
    } else {
        running_step = WAITING_THRESHOLD;
    }
}

void Ramp::leave_effect()
{
    current_volume = 1.0f;
    
    if (mode_direct_active() or float(*active) < 0.5f){
        running_step = NONE;
    } else {
        running_step = WAITING_THRESHOLD;
    }
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


int Ramp::get_period_length()
{
    float tempo_now = get_tempo();
    
    int tmp_period_length = int(
        (float(60.0f / tempo_now) * float(samplerate)) / *division);
    
    if (*half_speed > 0.5f){
        tmp_period_length = tmp_period_length * 2;
    }
    
    if (*double_speed > 0.5f){
        tmp_period_length = tmp_period_length / 2;
    }
    
    if (tmp_period_length < 2400){
        tmp_period_length = 2400;
    }
    
    return tmp_period_length;
}

int Ramp::get_current_duration()
{
    float tempo_now = get_tempo();
    
    int tmp_duration = int(
        (float(60.0f / tempo_now) * float(samplerate)) / *max_duration);
    
    if (tmp_duration < period_length){
        return tmp_duration;
    } else {
        return period_length;
    }
}


void Ramp::start_period()
{
    period_count = 0;
    period_length = get_period_length();
    current_duration = get_current_duration();
    
    fade_in = (float(*attack) * float(samplerate)) / 1000;
            
    if (fade_in >= current_duration - default_fade){
        fade_in = current_duration - default_fade;
    }
    
    ex_volume = current_volume;
    current_volume = powf(10.0f, (*volume)/20.0f);
}


void Ramp::start_first_period()
{
    start_period();
    current_shape = float(*shape);
    current_depth = float(*depth);
    
    float tempo_now = get_tempo();
    
    if (int(*pre_silence) == 0){
        has_pre_start = false;
        period_length = get_period_length();
    } else { 
        has_pre_start = true;
        period_length = int(*pre_silence) * int(
            (float(60.0f / tempo_now) * float(samplerate)) / *pre_silence_units);
    }
    
    if (fade_in >= current_duration - default_fade){
        fade_in = current_duration - default_fade;
    }
    
    if (mode_mute() and ! has_pre_start){
        current_depth = 1.0f;
        running_step = EFFECT;
    } else {
        running_step = FIRST_PERIOD;
    }
}


float Ramp::get_fall_period_factor()
{
    if (period_count > current_duration){
        return 0.0f;
    }
    
    float period_factor = 1.0f;
    int n_max = (current_duration - fade_in) / default_fade;
                    
    float pre_factor = 1.00f - float(period_count - fade_in) 
                       / (float(current_duration - fade_in));
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
        case SIDECHAIN:
            plugin->sidechain = (float*) data;
            break;
        case OUT:
            plugin->out = (float*) data;
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
        case PRE_SILENCE:
            plugin->pre_silence = (float*) data;
            break;
        case PRE_SILENCE_UNITS:
            plugin->pre_silence_units = (float*) data;
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
        case OUT_TEST:
            plugin->out_test = (float*) data;
            break;
    }
}

/**********************************************************************************************************************************************************/

void Ramp::run(LV2_Handle instance, uint32_t n_samples)
{
    Ramp *plugin;
    plugin = (Ramp *) instance;
    
    float enter_threshold = powf(10.0f, (*plugin->enter_threshold)/20.0f);
    float leave_threshold = powf(10.0f, (*plugin->leave_threshold)/20.0f);
    if (*plugin->leave_threshold == -80.0f){
        leave_threshold = 0.0f;
    }
    if (leave_threshold > enter_threshold){
        leave_threshold = enter_threshold;
    }
    
    bool active_state = bool(*plugin->active > 0.5f);
    
    if (active_state and not plugin->ex_active_state){
        plugin->enter_effect();
    }
    
    if (plugin->ex_active_state and not active_state){
        plugin->deactivate_ordered = true;
        plugin->period_count = 0;
        plugin->running_step = OUTING;
    }
    
    int attack_sample = -1;
    bool node_found = false;
    
    if (plugin->running_step == WAITING_THRESHOLD){
        /* Search attack sample, prefer node (0.0f) before threshold */
        bool up = true;
        
        for ( uint32_t i = 0; i < n_samples; i++)
        {   
            float value;
            if (plugin->mode_threshold_in()){
                value = plugin->in[i];
            } else if (plugin->mode_threshold_sidechain()){
                value = plugin->sidechain[i];
            } else {
                plugin->running_step = FIRST_PERIOD;
                break;
            }
            
            if (value >= enter_threshold){
                up = bool(value >= 0.0f);
                attack_sample = i;
                break;
            }
        }
        
//         if (attack_sample == -1){
//             bool up = bool(plugin->sidechain[n_samples -1] >= 0.0f);
//             
//             bool little_node_found = false;
//             for (int j = n_samples-1; j >= 0 ;j--)
//             {
//                 if ((up and (plugin->sidechain[j] < 0.0f))
//                         or (!up and (plugin->sidechain[j]) >= 0.0f)){
//                     plugin->last_node = j;
//                     plugin->n_period_no_node = 0;
//                     little_node_found = true;
//                     break;
//                 }
//             }
//             
//             if (!little_node_found){
//                 plugin->n_period_no_node++;
//             }
//         }
        
        if (attack_sample > 0){
            for ( int j = attack_sample-1; j >= 0; j--)
            {
                float value;
                if (plugin->mode_threshold_in()){
                    value = plugin->in[j];
                } else if (plugin->mode_threshold_sidechain()){
                    value = plugin->sidechain[j];
                } else {
                    break;
                }
                
                if ((up and value <= 0.0f)
                    or ((! up) and value >= 0.0f)){
                        node_found = true;
                        attack_sample = j;
                        break;
                }
            }
            
            if (!node_found){
                attack_sample = 0;
            }
        }
    }
    
    for ( uint32_t i = 0; i < n_samples; i++)
    {
        float period_factor = 1;
        float v = plugin->current_volume;
        
        if (plugin->running_step == WAITING_THRESHOLD
                and attack_sample == int(i)){
            plugin->start_first_period();
        }
        
        switch(plugin->running_step)
        {
            case NONE:
                period_factor = 1;
                if (plugin->mode_mute()){
                    v = 0;
                } else {
                    v = 1;
                }
                break;
                
            case WAITING_THRESHOLD:
                period_factor = 1;
                if (plugin->mode_mute()){
                    v = 0;
                } else {
                    v = 1;
                }
                break;
                
            case FIRST_PERIOD:
                if (plugin->period_count < plugin->fade_in){
                    if (plugin->fade_in <= (2 * plugin->default_fade)){
                        /* No fade in in this case, just adapt volume */
                        period_factor = 1;
                        v = 1 + ((plugin->current_volume -1) \
                                * float(plugin->period_count)/float(plugin->fade_in));
                        
                    } else if (plugin->period_count <= plugin->default_fade){
                        v = 1;
                        period_factor = 1 - (float(plugin->period_count) \
                                            /float(plugin->default_fade));
                        
                    } else {
                        period_factor = float(plugin->period_count - plugin->default_fade) \
                                        / float(plugin->fade_in - plugin->default_fade);
                    }
                } else {
                    period_factor = plugin->get_fall_period_factor();
                }
                
                if (plugin->has_pre_start and plugin->mode_mute()){
                    plugin->current_depth = 1.0f;
                    v = 0;
                }
                
                break;
                
            case EFFECT:
                if (plugin->period_count < plugin->fade_in){
                    period_factor = float(plugin->period_count)/float(plugin->fade_in);
                    v = plugin->ex_volume + period_factor * (plugin->current_volume - plugin->ex_volume);
                         
                } else {
                    if (plugin->period_count == plugin->fade_in){
                        plugin->current_shape = float(*plugin->shape);
                        plugin->current_depth = float(*plugin->depth);
                        
                        int tmp_period_length = plugin->get_period_length();
                        if (tmp_period_length >= (plugin->fade_in + plugin->default_fade)){
                            plugin->period_length = tmp_period_length;
                        }
                    }
                    
                    period_factor = plugin->get_fall_period_factor();
                }
                break;
                
            case OUTING:
                if (plugin->period_count <= plugin->default_fade){
                    if (plugin->mode_mute()){
                        period_factor = plugin->last_global_factor \
                                        - float(plugin->period_count)/float(plugin->default_fade) \
                                          * plugin->last_global_factor;
                    } else {
                        period_factor = float(plugin->period_count)/float(plugin->default_fade) \
                                    * (1 - plugin->last_global_factor) + plugin->last_global_factor;
                    }
                    v = 1;
                    plugin->current_depth = 1.0f;
                    
                    if (plugin->period_count == plugin->default_fade){
                        plugin->leave_effect();
                    }
                }
                break;
        }
        
        float global_factor = (1 - (1-period_factor) * plugin->current_depth) * v;
        plugin->out[i] = plugin->in[i] * global_factor;
        plugin->out_test[i] = global_factor -0.5;
        
        if (plugin->running_step != OUTING){
            plugin->last_global_factor = global_factor;
        }
        
        if (plugin->running_step >= FIRST_PERIOD){
            plugin->period_count++;
        }
        
        if (plugin->period_count == plugin->period_length){
            plugin->start_period();
            
            if (plugin->running_step == FIRST_PERIOD){
                plugin->running_step = EFFECT;
            }
            
            if (! plugin->next_is_active or plugin->deactivate_ordered){
                plugin->running_step = OUTING;
            }
            
            plugin->next_is_active = false;
        }
        
        if (plugin->mode_direct_active()){
            plugin->next_is_active = true;
        } else {
            if (plugin->period_count > (plugin->period_length -2400)
                and ! plugin->next_is_active
                and ((plugin->mode_threshold_in()
                        and abs(plugin->in[i] >= leave_threshold))
                     or (plugin->mode_threshold_sidechain()
                         and abs(plugin->sidechain[i] >= leave_threshold)))){
                        plugin->next_is_active = true;
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
