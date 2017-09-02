#include <cctype>
#include <cmath>
#include <iostream>

#include "MidiFile.h"
#include "historical.hpp"
#include "pitch_bend.hpp"
#include "pttypes.hpp"
#include "pxtone/pxtnService.h"

std::ostream &operator<<(std::ostream &o, const EVERECORD &p) {
  return o << (int)p.unit_no << " " << (int)p.kind << "\t" << p.clock << "\t"
           << p.value;
}

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

  // Get woice MIDI correspondence
  std::vector<Woice> woices(pxtn.Woice_Num());
  for (int i = 0; i < pxtn.Woice_Num(); ++i) {
    const pxtnWoice *woice = pxtn.Woice_Get(i);
    if (woice->is_name_buf()) {
      string name(woice->get_name_buf(nullptr));
      // some naive string parsing to match (D|M)[0-9]+.*
      if (name.length() >= 2 && (name[0] == 'D' || name[0] == 'M')) {
        if (isdigit(name[1])) {
          int num = std::stoi(name.substr(1));
          if (num >= 0 && num < 128) {
            woices[i].num = num;
            woices[i].drum = (name[0] == 'D');
          }
        }
      }
    }
  }

  // Build units data
  std::vector<Unit> units(pxtn.Unit_Num());
  for (const EVERECORD *p = pxtn.evels->get_Records(); p; p = p->next) {
    if (p->clock >= pxtn.master->get_last_clock() &&
        pxtn.master->get_last_clock() > 0)
      continue;
    switch (p->kind) {
    case EVENTKIND_ON: {
      auto &hist = units[p->unit_no].presses;
      hist.emplace(p->clock, Press{}).first->second.length = p->value;
      break;
    }

    case EVENTKIND_VELOCITY: {
      auto &hist = units[p->unit_no].presses;
      hist.emplace(p->clock, Press{}).first->second.vel = p->value;
      break;
    }

    case EVENTKIND_KEY:
      units[p->unit_no].notes[p->clock] = p->value / 256 - 27;
      break;

    case EVENTKIND_TUNING: {
      auto &hist = units[p->unit_no].tunings;
      float value = reinterpret_cast<const float &>(p->value);
      units[p->unit_no].tunings[p->clock] = std::log2(value) * 12;
      break;
    }

    case EVENTKIND_PORTAMENT:
      units[p->unit_no].portas[p->clock] = p->value;
      break;

    case EVENTKIND_VOLUME:
      units[p->unit_no].volume[p->clock] = p->value;
      break;

    case EVENTKIND_PAN_VOLUME:
      units[p->unit_no].pan_v[p->clock] = p->value;
      break;

    case EVENTKIND_PAN_TIME:
      units[p->unit_no].pan_t[p->clock] = p->value;
      break;

    case EVENTKIND_VOICENO:
      units[p->unit_no].voice[p->clock] = p->value;
      break;

    case EVENTKIND_GROUPNO:
      units[p->unit_no].group[p->clock] = p->value;
      break;

    default:
      std::cerr << "warning: unhandled event - " << *p << std::endl;
      break;
    }
  }

  MidiFile midifile;
  midifile.addTracks(pxtn.Unit_Num());
  midifile.setTicksPerQuarterNote(480);

  // Set track 0 metadata
  const char *song_name = pxtn.text->get_name_buf(nullptr);
  if (!song_name) song_name = "";
  midifile.addTrackName(0, 0, song_name);
  midifile.addTempo(0, 0, pxtn.master->get_beat_tempo());

  for (int i = 0; i < pxtn.Unit_Num(); ++i) {
    int track = i + 1;
    int channel = (i >= 9 ? i + 1 : i); // skip the drum channel

    const pxtnUnit &unit = *pxtn.Unit_Get(i);
    midifile.addTrackName(track, 0, unit.get_name_buf(nullptr));
    // configure cc6 to set pitch bend range
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
