# ptmidi
Yet Another ptcop to midi converter

This uses the not-so-recently-released source code of the pxtone parser and player to convert a `.ptcop` to a `.mid` file. Some properties and features:

* Portamentos and key corrects supported, using a lot of pitch bends.
* Associations between voices and MIDI programs through a voice naming scheme.
* Every unit is roughly on its own track and channel, except ones assigned to drums.
