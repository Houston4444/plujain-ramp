-- README FOR PLUJAIN-RAMP --

Plujain-Ramp is a mono rhythmic tremolo LV2 Audio Plugin.<br>
Each period of the tremolo is made of one short fade in and one long fade out,<br>
so we can say it generates musical pulses.<br>
It can also generate pitched periods (see Speed Effect).<br>
There is currently no GUI, but it's not really needed.<br>

You can ear examples of how can sound the plugin here:<br>
https://soundcloud.com/user-420463073/sets/plujain-ramp-examples

There are no particular dependencies except these ones needed to build an LV2 plugin.

To build and install just type: <br>
`$ make clean && make` <br>
`$ [sudo] make install`

This plugin is provided in two versions: <strong>Plujain Ramp</strong> and <strong>Plujain Ramp Live</strong>.<br>
<strong>Plujain Ramp</strong> contains one audio input/output, 
tempo is based on host and it follows tranport. It's for use in DAW.<br>
<strong>Plujain Ramp Live</strong> additionally contains one midi input/output
to allow user to synchronize multiples instances together.
It also can start effect with a threshold.<br>

To synchronize multiples instances of Plujain Ramp Live, the better method is to use one instance as the master one.<br>
The midi output of this master instance has to be connected to all midi inputs of slave instances.<br>
The master instance should be in "always active" or "Start with threshold" mode, the slaves instances in "Midi in Slave".<br>
Tip: if you want to apply this effect to distorded or high reverb effects, but you want a reactiv "Start with threshold" mode,<br>you can simply connect the instrument source to the audio input of the master instance and not connect its audio output,<br> then set one slave instance to "Midi in slave/Stop is bypass" mode.<br> 



Now let see the control ports and what they do:

----------

<strong>Active:</strong>
    toggle Bypass/effect. This port will be used by allmost hosts to activate/deactivate the plugin.
    If this control is not checked, the effect will be bypassed.
    When Active is unchecked, plugin sends a Midi Stop signal to midi out.

    
----------
    
<strong>Mode:</strong> (Plujain Ramp Live only)

    
    
    Always Active:
        Effect is always active, a Midi Start signal in midi_in will restart effect to the begginning.
        Midi Stop signals are ignored.
        Pre-start is tretead as a period.

    Start With Threshold:
        At start, out volume will be at the down of the Depth.
        If Depth is at 100%, signal will be muted.
        When signal in audio_in exceed "Input Threshold",
        effect is started and a Midi Start signal is sent to midi_out.
        If at the end of the effect period (last 50ms),
        signal in audio_in doesn't exceed "Leave Threshold", 
        effect will be still alive but restarted the next time "Input Threshold" is reached.
        Pre-start is tretead as a period.

    Host Transport:
        Effect will be always active but restarted with the transport.
        If transport "play" doesn't start at the begginning of a bar, effect will be restarted on the next bar.
        During Pre-start, volume is at the down of the depth (depth at 100% => Mute, depth at 0 => Bypass).
    
    Midi In Slave:
        At start, out volume will be at the down of the Depth. 
        If Depth is at 100%, signal will be muted.
        When midi_in receives a Midi Start signal, effect will start.
        When midi_in receives a Midi Stop signal,
        out volume will return to the down of the depth.
        During Pre-start, volume is at the down of the depth (depth at 100% => Mute, depth at 0 => Bypass).

    Midi In Slave / Stop is Bypass:
        Same as Midi In Slave except that at start the effect is bypassed.
        a stop signal in midi_in will make it go back to bypass.
        Pre-start is tretead as a period.


----------        
        
<strong>Enter Threshold:</strong> (Plujain Ramp Live only)
    Only used in "Start With Threshold Mode", see "Mode/Start With Threshold"

<strong>Leave Threshold:</strong> (Plujain Ramp Live only)
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
    
<strong>Sync Tempo:</strong> (Plujain Ramp Live only)
    Set Tempo to the host tempo.

<strong>Tempo:</strong> (Plujain Ramp Live only)
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

<strong>Speed Effect 1:</strong>
    Speed effect adds sound. It's not dependent of depth or volume.
    For example on "Octave -" , it plays the first half of the period two times slowly.
    This will gives an octaver consuming very few DSP resources.
    If the speed is faster than 1x (for example Octave+), it will sounds more as a pitched delay than as a pitch,
    except that the delay time depends on when the attack appears !
    Effect will take the required audio time in the past to fill the period :
    for Octave+ (x2 speed), it will take all the previous period (more exactly the same time as the period before the period start)
    and the current period and stretch it.
    This explains why an attack at the start of the period will be repeated at half,
    and at the start of the next period.
    An attack at half of the period will be repeated at 3/4, and at 1/4 of the next period.
    Hard to anticipate but often beautiful.

<strong>Speed Effect 1 Vol:</strong>
    Volume of the speed effect 1 below. At -80dB, signal is totally muted.
    
<strong>Speed Effect 2:</strong>
    Same as Speed Effect 1.
    
<strong>Speed Effect 2 Vol:</strong>
    Volume of the speed effect 2 below. At -80dB, signal is totally muted.



