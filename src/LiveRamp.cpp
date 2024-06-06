#include "LiveRamp.h"

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

LiveRamp::LiveRamp(double rate) : Ramp(rate){
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
    midiatom.size = 1;
    
    uint8_t msg[1];
    msg[0] = 0xfa;
    if (!start){
        msg[0] = 0xfc;
    }
    
    if (0 == lv2_atom_forge_frame_time (&forge, frame)) return;
	if (0 == lv2_atom_forge_raw (&forge, &midiatom, sizeof (LV2_Atom))) return;
	if (0 == lv2_atom_forge_raw (&forge, msg, 1)) return;
    lv2_atom_forge_pad (&forge, sizeof (LV2_Atom) + 1);
    
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
      ATTACK, SHAPE, DEPTH, VOLUME, SPEED_EFFECT_1, SPEED_EFFECT_1_VOL, SPEED_EFFECT_2, SPEED_EFFECT_2_VOL, PLUGIN_PORT_COUNT};

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

LV2_Handle LiveRamp::instantiate(const LV2_Descriptor* descriptor, double samplerate, const char* bundle_path, 
                  const LV2_Feature* const* features)
{
    LiveRamp *plugin = new LiveRamp(samplerate);
    
    plugin->samplerate = samplerate;
    
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
