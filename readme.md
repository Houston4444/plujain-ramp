-- README FOR PLUJAIN-RAMP --

Plujain-Ramp is a mono rhythmic tremolo LV2 Audio Plugin.<br>
Each period of the tremolo is made of one short fade in and one long fade out.<br>
There is currently no GUI, but it's not really needed.<br>

There are no particular dependencies except these ones needed to build an LV2 plugin.

To build and install just type: <br>
`$ make` <br>
`$ [sudo] make install`

This plugin contains one audio input/output, and one midi input/output to allow user to synchronize multiples instances together.

Now let see the control ports and what they do:

----------

<strong>Active:</strong>
    toggle Bypass/effect. This port will be used by allmost hosts to activate/deactivate the plugin.
    If this control is not checked, the effect will be bypassed.
    When Active is unchecked, plugin sends a Midi Stop signal to midi out.

    
----------
    
<strong>Mode:</strong>

    Always Active:
        Effect is always active, a Midi Start signal in midi_in will restart effect to the begginning.
        Midi Stop signals are ignored.

    Start With Threshold:
        At start, out volume will be at the down of the Depth. If Depth is at 100%, signal will be muted.
        When signal in audio_in exceed "Input Threshold",
        effect is started and a Midi Start signal is sent to midi_out).
        If at the end of the effect period (last 50ms),
        signal in audio_in doesn't exceed "Leave Threshold", 
        "Input Threshold" can restart effect again.

    Host Transport:
        Still experimental. For the moment, effect will start at Transport Play.
        There is certainly better to do.
    
    Midi In Slave:
        At start, out volume will be at the down of the Depth. 
        If Depth is at 100%, signal will be muted.
        When midi_in receives a Midi Start signal, effect will start.
        When midi_in receives a Midi Stop signal,
        out volume will return to the down of the depth.

    Midi In Slave / Stop is Bypass:
        Same as Midi In Slave except that at start effect is bypassed,
        a stop signal in midi_in will make it go back to bypass.


----------        
        
<strong>Enter Threshold:</strong>
    Only used in "Start With Threshold Mode", see "Mode/Start With Threshold"

<strong>Leave Threshold:</strong>
    Only used in "Start With Threshold Mode", see "Mode/Start With Threshold"

----------
    
<strong>Pre Start:</strong>
    Duration of the first effect period. Very useful when instance is slave. 
    In "Midi In Slave" Mode,output volume will be at the down on the depth during this period.
    Unit is decided by "Pre Start Units".

<strong>Pre Start Units:</strong>
    Unit of "Pre Start".

<strong>Beat Offset:</strong>
    Shift the effect placement from -1/8beat to 1/8beat. Applied directly, but also changes the first period duration. Useful to make groove slave instances.

----------
    
<strong>Sync Tempo:</strong>
    Set Tempo to the tempo host.

<strong>Tempo:</strong>
    If you don't know what a tempo is, not sure that this plugin will help you !
    Ignored if "Sync Tempo" is checked.

<strong>Division:</strong>
    Duration of the effect period. Probably the most important control !
    On "beat/2 t" and "beat/4 t", it plays effect with a syncope.
    
<strong>Max Duration:</strong>
    Max duration (in beats) of the effect period.
    After this max duration, volume will be at the down of the depth until next period.
    While Max Duration exceed Division, it doesn't changes anything.
    
----------
    
<strong>Half Speed:</strong>
    Double period duration. Here to be assignated for live.
    If division is on "beat/2 t" or "beat/4 t", it plays only the highlight beat.
    
<strong>Double Speed:</strong>
    Divise period duration by 2. Here to be assignated for live.
    If division is on "beat/2 t" or "beat/4 t", it respectively plays beat/3 or beat/6.
    
----------
    
<strong>Attack:</strong>
    Duration of the fade in of the period in milliseconds.
    Short duration could create artefacts. 
    A long duration make a smooth effect, a bit like a reverse effect.
    If this exceed the real max duration of the period, the surplus will be ignored.
    
<strong>Shape:</strong>
    Shape of the fade out of the period.
    on 0 it will be a linear fade.
    negative values makes a faster curved fade out.
    Positive values makes a slower curved fade out.
    
----------
    
<strong>Depth:</strong>
    Depth of the effect. At 100% signal will be totally muted at the end of the real max duration period.
    
<strong>Volume:</strong>
    Volume of the effect. At -80dB, signal is totally muted. This way you can use this instance only for managing the other ones, Practical if you need many other effects when you use this tremolo.







