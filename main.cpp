#include "MidiFile.h"
#include "pxtone/pxtnService.h"
#include <iostream>
#include <queue>

struct Note {
  int vel, key;
};

struct PitchEvent {
  int pitch, time;
};

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
    const pxtnUnit &unit = *pxtn.Unit_Get(i);
    midifile.addTrackName(i + 1, 0, unit.get_name_buf(nullptr));
    // 104 = default volume
    midifile.addController(i + 1, 0, i + 1, 11, 104);
  }

  // Note: assuming records are non-decreasing in clock
  int last_clock = -1;
  std::queue<const EVERECORD *> records;
  std::vector<Note> notes(pxtn.Unit_Num());
  // std::vector<PitchEvent> pitches(pxtn.Unit_Num());
  for (const EVERECORD *p = pxtn.evels->get_Records(); p; p = p->next) {
    // process velocities & keys for this instant first.
    if (p->clock < last_clock) {
      std::cerr << "records not in time-ascending order" << std::endl;
      return -1;
    } else if (p->clock > last_clock) {
      for (; !records.empty(); records.pop()) {
        const EVERECORD *p = records.front();
        int track = p->unit_no + 1;
        int channel = p->unit_no + 1;
        if (channel == 9) ++channel; // to avoid drums
        switch (p->kind) {
        case EVENTKIND_ON: {
          int key = notes[p->unit_no].key / 256 - 39;
          int vel = notes[p->unit_no].vel;
          midifile.addNoteOn(track, p->clock, channel, key, vel);
          midifile.addNoteOff(track, p->clock + p->value, channel, key);
          break;
        }

        case EVENTKIND_VOLUME:
          // 11 = Expression MSB, 43 = Expression LSB
          midifile.addController(track, p->clock, channel, 11, p->value);
          break;

        case EVENTKIND_PAN_VOLUME:
          // TODO: do some function of time and volume
          // 10 = Pan MSB, 42 = Pan LSB
          midifile.addController(track, p->clock, channel, 10, p->value);
          break;

        case EVENTKIND_TUNING:
          // TODO: extend this once you do portamentos
          // 1 = Modulation MSB, 33 = Modulation LSB, 6 = Pitch bend range
          midifile.addController(track, p->clock, channel, 1, p->value);
          break;

        default:
          break;
        }
      }
      last_clock = p->clock;
    }
    switch (p->kind) {
    case EVENTKIND_VELOCITY:
      notes[p->unit_no].vel = p->value;
      break;
    case EVENTKIND_KEY:
      notes[p->unit_no].key = p->value;
      break;
    default:
      records.push(p);
    }
    if (p->kind == EVENTKIND_VELOCITY)
      notes[p->unit_no].vel = p->value;
    else
      records.push(p);
  }

  midifile.sortTracks();
  midifile.write(string(filename) + ".mid");
  return 0;
}