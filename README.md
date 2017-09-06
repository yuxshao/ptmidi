# ptmidi

This attempts to be a more complete `.ptcop` to `.mid` converter, using the not-so-recently-released [source code](http://pxtone.org/wp-content/uploads/2016/08/pxtone-source-code-170212a.zip) of the pxtone parser and player.

## Features

* Most unit properties like volume, panning, and portamento are fully supported (in the case of portamento and key correct, this was through a lot of pitch bend and bend range adjustments).
* Associations between voices and MIDI programs through a voice naming scheme (see example for use).
  * If the voice name starts with an `M` and is followed by a number, that voice will correspond to that program number, 0-indexed.
  * If the voice name starts with a `D` and is followed by a number, that voice will correspond to the percussion sound with that note number, 0-indexed.
* Every unit is roughly on its own track and channel, except ones assigned to drums.

## Limitations

* Different types of panning are unsupported - currently MIDI pan comes directly from volume panning. Not sure how to do time panning or have any meaningful two-parameter panning with MIDI.
* Groups and group effects are unsupported - perhaps overdrive and echo could map to some reverb and chorus, or an added channel of echos?
* All and only percussion sounds are played on channel 10. As a result, only note properties like length and velocity, not channel properties like volume, are kept.
* Notes should not shift past an octave away from the key it started on, since the maximum pitch bend range is an octave.

## Use

If on Windows, run `ptmidi.exe` and select the file to generate another file with `.mid` appended to it. The Visual Studio solution should also hopefully build.

On other systems, if you have make and gcc 7 or above, run `make` to build the `ptmidi` command line executable, then run `./ptmidi {YOUR-FILE}.ptcop` to generate `{YOUR-FILE}.ptcop.mid`. The `ptmidi` binary might also work in lieu of building.
