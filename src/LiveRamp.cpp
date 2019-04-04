#include "LiveRamp.h"

// #define MIN(a,b) ( (a) < (b) ? (a) : (b) )
// #define MAX(a,b) ( (a) > (b) ? (a) : (b) )
// #define RAIL(v, min, max) (MIN((max), MAX((min), (v))))
// #define ROUND(v) (uint32_t(v + 0.5f))

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

LiveRamp::LiveRamp() : Ramp(){
    is_live_ramp = true;
}

uint32_t LiveRamp::get_mode(){
    return ROUND(RAIL(*mode, 0, 4));
}

float LiveRamp::get_enter_threshold(){
    return powf(10.0f, (*enter_threshold)/20.0f);
}

float LiveRamp::get_leave_threshold(){
    float tmp_lt = powf(10.0f, (*leave_threshold)/20.0f);
    if (*leave_threshold <= -80.0f){
        tmp_lt = 0.0f;
    }
    return tmp_lt;
}

void LiveRamp::send_midi_start_stop(bool start, uint32_t frame)
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
    
    if (instance_started_since > (2 * samplerate)){
        start_sent_after_start = true;
    }
}

void LiveRamp::connect_port(LV2_Handle instance, uint32_t port, void *data)
{
    LiveRamp *plugin;
    plugin = (LiveRamp *) instance;
    
    enum {IN, MIDI_IN, OUT, MIDI_OUT,
      ACTIVE, MODE, ENTER_THRESHOLD, LEAVE_THRESHOLD, PRE_START, PRE_START_UNITS, BEAT_OFFSET,
      SYNC_BPM, HOST_TEMPO, TEMPO, DIVISION, MAX_DURATION, HALF_SPEED, DOUBLE_SPEED,
      ATTACK, SHAPE, DEPTH, VOLUME, SUB_SUBOCTAVE, SUBOCTAVE, OUT_TEST, OUT_TEST2, PLUGIN_PORT_COUNT};

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
        case BEAT_OFFSET:
            plugin->beat_offset = (float*) data;
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
        case OUT_TEST:
            plugin->out_test = (float*) data;
            break;
        case OUT_TEST2:
            plugin->out_test2 = (float*) data;
            break;
    }
}

LV2_Handle LiveRamp::instantiate(const LV2_Descriptor* descriptor, double samplerate, const char* bundle_path, 
                  const LV2_Feature* const* features)
{
    LiveRamp *plugin = new LiveRamp();
    
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
    plugin->current_sub_suboctave = 0.0f;
    plugin->current_suboctave = 0.0f;
    plugin->ex_volume = 1.0f;
    plugin->ex_depth = 1.0f;
    plugin->last_global_factor = 1.0f;
    plugin->last_global_factor_mem = 1.0f;
    plugin->oct_period_factor = 0.0f;
    plugin->oct_period_factor_mem = 0.0f;
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


float LiveRamp::get_tempo()
{
    float tempo_now;
    if (*sync_bpm > 0.5){
        tempo_now = *host_tempo;
    } else {
        tempo_now = *tempo;
    }
    
    return tempo_now;
} 
