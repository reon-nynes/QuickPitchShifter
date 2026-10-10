# QuickPitchShifter

An audio effect for streamers who change the pitch of live, general-purpose audio.

## Language

**Pitch shift**:
A change in the perceived pitch of audio, expressed as a musical interval.
_Avoid_: Pitch correction, automatic tuning

**Semitone**:
One twelfth of an octave. Positive pitch shifts raise pitch; negative pitch shifts lower it.

**Live stream**:
A continuous audio feed processed as it arrives for a broadcast or monitoring path.

**Processing latency**:
The delay the effect adds between receiving audio and producing its corresponding output.
_Avoid_: Control response time

**Tonality limit**:
A frequency reference that determines how the high-frequency portion of a pitch shift preserves the source's timbre.
_Avoid_: Filter cutoff

**Formant compensation**:
Preservation of the source's broad spectral envelope while its pitch changes, helping retain its vocal or instrumental character.
_Avoid_: Automatic tuning

**Formant base**:
An expected fundamental frequency used as a reference for formant analysis.
_Avoid_: Formant shift

**Analysis block**:
The duration of audio considered together for spectral pitch analysis.
_Avoid_: Host buffer size

**Overlap**:
The ratio of analysis block duration to the interval between successive analyses.
_Avoid_: Overlap percentage

**Bypass**:
Unshifted audio with the active effect's processing delay retained.
_Avoid_: Zero-semitone setting

**Applied configuration**:
The analysis block and overlap settings currently in use by the effect.
_Avoid_: Pending settings
