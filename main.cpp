#include "MidiFile.h"
#include "pxtone/pxtnService.h"
#include <cmath>
#include <iostream>
#include <map>

struct PitchEvent {
  int pitch, clock, end_clock;
};

template <typename T> using Historical = std::map<int, T>;

template <typename T> auto at_time(Historical<T> hist, int time) {
  auto it = hist.upper_bound(time);
  if (it == hist.begin())
    return hist.end();
  else
    return --it;
}

struct Press {
  int vel, length;
};

struct Unit {
  Historical<Press> presses;
  Historical<int> notes, volume, porta, voice, group, pan_v, pan_t;
  Historical<float> tuning;
};

void print_event(const EVERECORD *p) {
  std::cout << (int)p->unit_no << "\t" << (int)p->kind << "\t" << p->clock
            << "\t" << p->value << std::endl;
}

int main(int argc, char **args) {
  // read file
  if (argc != 2) {
    std::cerr << "need filename" << std::endl;
    return 0;
  }
  char *filename = args[1];
  pxtnDescriptor desc;
  desc.set_file_r(fopen(filename, "r"));
  pxtnService pxtn;
  pxtn.init();
  pxtn.read(&desc);

  MidiFile midifile;
  midifile.addTracks(pxtn.Unit_Num());
  midifile.setTicksPerQuarterNote(480);

  // Set track 0 metadata
  const char *song_name = pxtn.text->get_name_buf(nullptr);
  if (!song_name) song_name = "";
  midifile.addTrackName(0, 0, song_name);
  midifile.addTempo(0, 0, pxtn.master->get_beat_tempo());

  // Create other tracks from units
  for (int i = 0; i < pxtn.Unit_Num(); ++i) {
  }

  int last_clock = -1;
  std::vector<Unit> units(pxtn.Unit_Num());
  for (const EVERECORD *p = pxtn.evels->get_Records(); p; p = p->next) {
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
      units[p->unit_no].notes.emplace(p->clock, p->value / 256 - 27);
      break;

    case EVENTKIND_VOLUME:
      units[p->unit_no].volume.emplace(p->clock, p->value);
      break;

    case EVENTKIND_PAN_VOLUME:
      units[p->unit_no].pan_v.emplace(p->clock, p->value);
      break;

    case EVENTKIND_PAN_TIME:
      units[p->unit_no].pan_t.emplace(p->clock, p->value);
      break;

    case EVENTKIND_PORTAMENT:
      units[p->unit_no].porta.emplace(p->clock, p->value);
      break;

    case EVENTKIND_VOICENO:
      units[p->unit_no].voice.emplace(p->clock, p->value);
      break;

    case EVENTKIND_GROUPNO:
      units[p->unit_no].group.emplace(p->clock, p->value);
      break;

    case EVENTKIND_TUNING:
      auto &hist = units[p->unit_no].tuning;
      hist.emplace(p->clock, reinterpret_cast<const float &>(p->value));
      break;

    default:
      std::cout << "warning: unhandled event - ";
      print_event(p);
      break;
    }
  }

  for (int i = 0; i < pxtn.Unit_Num(); ++i) {
    int track = i + 1;
    int channel = (i + 1 >= 10 ? i + 2 : i + 1); // skip the drum channel

    const pxtnUnit &unit = *pxtn.Unit_Get(i);
    midifile.addTrackName(track, 0, unit.get_name_buf(nullptr));
    // configure cc6 to set pitch bend range
    midifile.addController(track, 0, channel, 100, 0);
    midifile.addController(track, 0, channel, 101, 0);
    // 104 = default volume
    midifile.addController(track, 0, channel, 11, 104);
    midifile.addPitchBend(track, 0, channel, 0);

    for (const auto & [ time, volume ] : units[i].volume)
      midifile.addController(track, time, channel, 11, volume);

    for (const auto & [ time, pan_v ] : units[i].pan_v)
      midifile.addController(track, time, channel, 10, pan_v);

    for (const auto & [ time, press ] : units[i].presses) {
      auto key_it = at_time(units[i].notes, time);
      // 48 is a default
      int key = (key_it == units[i].notes.end() ? 48 : key_it->second);
      midifile.addNoteOn(track, time, channel, key, press.vel);
      midifile.addNoteOff(track, time + press.length, channel, key);
    }

    for (const auto & [ time, tuning ] : units[i].tuning) {
      // 6 = Pitch bend range
      float value = reinterpret_cast<const float &>(tuning);
      double note_offset = std::log2(value) * 12;
      int pitch_bend_range = std::max<int>(1, std::ceil(std::abs(note_offset)));
      double abs_offset = note_offset / pitch_bend_range;
      midifile.addController(track, time, channel, 6, pitch_bend_range);
      midifile.addPitchBend(track, time, channel, abs_offset);
    }

    /*case EVENTKIND_KEY: {
    // key has not yet been updated. create pitch bend line
    int track = p->unit_no + 1;
    int channel = p->unit_no + 1;
    int start_pitch = pitches[p->unit_no].pitch;
    int start_clock = pitches[p->unit_no].clock;
    int port_time = 240; // half a beat
    int pitch_diff = notes[p->unit_no].key - start_pitch;
    int pitch_bend_range =
        std::max<int>(1, std::ceil(std::abs(pitch_diff / 256)));
    midifile.addController(track, p->clock, channel, 6, pitch_bend_range);
    for (int i = 0; i <= std::min<int>(port_time, p->clock); i += 10) {
      double abs_offset =
          pitch_diff * i / (port_time * 256. * pitch_bend_range);
      std::cout << abs_offset << std::endl;
      midifile.addPitchBend(track, start_clock + i, channel, abs_offset);
    }

    notes[p->unit_no].key = p->value;
    break;
  }*/
  }

  midifile.sortTracks();
  midifile.write(string(filename) + ".mid");
  return 0;
}
