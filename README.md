# ptmidi

This attempts to be a more complete `.ptcop` to `.mid` converter, using the not-so-recently-released [source code](http://pxtone.org/wp-content/uploads/2016/08/pxtone-source-code-170212a.zip) of the pxtone parser and player. Some properties and features:

* Portamentos and key corrects fully supported, using a lot of pitch bends and bend range adjustments.
* Associations between voices and MIDI programs through a voice naming scheme.
  * If the voice name starts with an `M` and is followed by a number, that voice will correspond to that program number, 0-indexed.
  * If the voice name starts with a `D` and is followed by a number, that voice will correspond to the percussion sound with that note number, 0-indexed.
* Every unit is roughly on its own track and channel, except ones assigned to drums.

Some limitations and unsupported things:

* Different types of panning are unsupported - currently MIDI pan comes directly from volume panning. Not sure how to do time panning or have any meaningful two-parameter panning with MIDI.
* Groups and group effects are unsupported - perhaps overdrive and echo could map to some reverb and chorus, or an added channel of echos?
* All and only percussion sounds are played on channel 10. As a result, only note properties like length and velocity, not channel properties like volume, are kept.
* Notes should not shift past an octave away from the key it started on, since the maximum pitch bend range is an octave.

See the example for feature uses. 
