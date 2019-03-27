-- README FOR PLUJAIN-RAMP --

Plujain-Ramp is a mono rhythmic tremolo LV2 Audio Plugin.
Each period of the tremolo is made of one short fade in and one long fade out.
There is currently no GUI, but it's not really needed.

A tremolo is just a plugin that moves the volume, so there are no particular dependencies except these ones needed to build a LV2 plugin.

To build and install just type:
make
sudo make install

This plugin contains one audio input/output, and one midi input/output to allow user to synchronize multiples instances together.

Now let see the control ports and what they do:

Active:
    toggle Bypass/effect. This port will be used by allmost hosts to activate/deactivate the plugin.
    If Bypass is not checked, the effect will be bypassed.
    When Active is unchecked, plugin sends a Midi Stop signal to midi out.

Mode:
    Always Active:
        Effect is always active, a Midi Start signal in midi_in will restart effect to the begginning.
        Midi Stop signals are ignored.

    Start With Threshold:
        At start, out volume will be at the down of the Depth. If Depth is at 100%, signal will be muted.
        When signal in audio_in exceed "Input Threshold", effect is started and a Midi Start signal is sent to midi_out).
        If at the end of the effect period (last 50ms), signal in audio_in doesn't exceed "Leave Threshold", "Input Threshold" can restart effect again.

    Host Transport:
        Still experimental. For the moment, effect will start at Transport Play. There is certainly better to do.
    
    Midi In Slave:
        At start, out volume will be at the down of the Depth. If Depth is at 100%, signal will be muted.
        When midi_in receives a Midi Start signal, effect will start.
        When midi_in receives a Midi Stop signal, out volume will return to the down of the depth.

    Midi In Slave / Stop is Bypass:
        Same as Midi In Slave except that at start effect is bypassed, a stop signal in midi_in will make it go back to bypass.

Enter Threshold:
    Only used in "Start With Threshold Mode", see "Mode/Start With Threshold"

Leave Threshold:
    Only used in "Start With Threshold Mode", see "Mode/Start With Threshold"

Pre Start:
    Duration of the first effect period. Very useful when instance is slave. 
    In "Midi In Slave" Mode,output volume will be at the down on the depth during this period.
    Unit is decided by "Pre Start Units".

Pre Start Units:
    Unit of "Pre Start".

Beat Offset:
    Shift the effect placement from -1/8beat to 1/8beat. Applied directly, but also changes the first period duration. Useful to make groove slave instances.

Sync Tempo:
    Set Tempo to the tempo host.

Tempo:
    If you don't know what a tempo is, not sure that this plugin will help you !
    Ignored if "Sync Tempo" is checked.

Division:
    Duration of the effect period. Probably the most important control !
    On "beat/2 t" and "beat/4 t", it plays effect with a syncope.
    
Max Duration:
    Max duration (in beats) of the effect period.
    After this max duration, volume will be at the down of the depth until next period.
    While Max Duration exceed Division, it doesn't changes anything.
    
Half Speed:
    Double period duration. Here to be assignated for live.
    If division is on "beat/2 t" or "beat/4 t", it plays only the highlight beat.
    
Double Speed:
    Divise period duration by 2. Here to be assignated for live.
    If division is on "beat/2 t" or "beat/4 t", it respectively plays beat/3 or beat/6.
    
Attack:
    Duration of the fade in of the period in milliseconds.
    Short duration could create artefacts. 
    A long duration make a smooth effect, a bit like a reverse effect.
    If this exceed the real max duration of the period, the surplus will be ignored.
    
Shape:
    Shape of the fade out of the period.
    on 0 it will be a linear fade.
    negative values makes a faster curved fade out.
    Positive values makes a slower curved fade out.
    
Depth:
    Depth of the effect. At 100% signal will be totally muted at the end of the real max duration period.
    
Volume:
    Volume of the effect. At -80dB, signal is totally muted. This way you can use this instance only for managing the other ones, Practical if you need many other effects when you use this tremolo.







