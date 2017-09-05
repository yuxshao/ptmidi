#include <cctype>
#include <cmath>
#include <iostream>

#include "MidiFile.h"
#include "historical.hpp"
#include "pitch_bend.hpp"
#include "pttypes.hpp"
#include "pxtone/pxtnService.h"

int main(int argc, char **args) {
  // read file
  if (argc != 2) {
    std::cerr << "need filename" << std::endl;
    return 0;
  }
  char *filename = args[1];
  FILE *file = fopen(args[1], "r");
  pxtnDescriptor desc;
  desc.set_file_r(file);
  pxtnService pxtn;
  pxtn.init();
  pxtn.read(&desc);
  fclose(file);

  // Get woice and units MIDI correspondence
  std::vector<Woice> woices = Woice::get_woices(pxtn);
  std::vector<Unit> units = Unit::get_units(pxtn);

  MidiFile midifile;
  midifile.addTracks(pxtn.Unit_Num());
  midifile.setTicksPerQuarterNote(480);

  // Set track 0 metadata
  const char *song_name = pxtn.text->get_name_buf(nullptr);
  if (!song_name) song_name = "";
  midifile.addTrackName(0, 0, song_name);
  midifile.addTempo(0, 0, pxtn.master->get_beat_tempo());
  midifile.addTimeSignature(0, 0, pxtn.master->get_beat_num(), 4);

  for (int i = 0; i < pxtn.Unit_Num(); ++i) {
    int track = i + 1;
    int channel = (i >= 9 ? i + 1 : i); // skip the drum channel

    const pxtnUnit &unit = *pxtn.Unit_Get(i);
    midifile.addTrackName(track, 0, unit.get_name_buf(nullptr));
    // mark cc6 as setting pitch bend range using cc100 and cc101
    midifile.addController(track, 0, channel, 100, 0);
    midifile.addController(track, 0, channel, 101, 0);

    // All these pxtone parameters go from 0 to *128*, so we need to cap at 127
    for (const auto & [ time, volume ] : units[i].volume)
      midifile.addController(track, time, channel, 11, std::min(volume, 127));

    for (const auto & [ time, pan_v ] : units[i].pan_v)
      midifile.addController(track, time, channel, 10, std::min(pan_v, 127));

    // note to self: when doing pan_t, also cap at 127

    for (const auto & [ time, voice ] : units[i].voice) {
      const Woice &woice = woices[voice];
      if (!woice.drum) midifile.addPatchChange(track, time, channel, woice.num);
    }

    for (const auto & [ time, press ] : units[i].presses) {
      const Woice &woice = woices[units[i].voice.at_time(time)];
      int real_channel = (woice.drum ? 9 : channel);
      int key = (woice.drum ? woice.num : units[i].notes.at_time(time));
      midifile.addNoteOn(track, time, real_channel, key,
                         std::min(press.vel, 127));
      midifile.addNoteOff(track, time + press.length, real_channel, key);
    }

    Historical<double> pitch_offsets =
        porta_pitch_offsets(units[i].presses, units[i].notes, units[i].portas);
    add_pitch_offset(pitch_offsets, units[i].tunings);

    for (const auto & [ time, offset ] : pitch_offsets) {
      int pitch_bend_range = (std::floor(std::abs(offset) / 4) + 1) * 4;
      double abs_offset = offset / pitch_bend_range;
      midifile.addController(track, time, channel, 6, pitch_bend_range);
      midifile.addPitchBend(track, time, channel, abs_offset);
    }
  }

  midifile.sortTracks();
  midifile.write(string(filename) + ".mid");
  return 0;
}
