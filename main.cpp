#include "MidiFile.h"
#include "pxtone/pxtnService.h"
#include <cmath>
#include <iostream>
#include <map>

struct PitchEvent {
  int pitch, clock, end_clock;
};

template <typename T> using Historical = std::map<int, T>;

template <typename T> T at_time(const Historical<T> &hist, int time, T def) {
  auto it = hist.upper_bound(time);
  if (it == hist.begin())
    return def;
  else
    return (--it)->second;
}

struct Press {
  int vel, length;
};

template <typename T>
std::ostream &operator<<(std::ostream &o, const Historical<T> &hist) {
  for (const auto & [ time, value ] : hist)
    o << time << ":\t" << value << std::endl;
  return o;
}

std::ostream &operator<<(std::ostream &o, const Press &p) {
  return o << "(" << p.vel << ", " << p.length << ")";
}

std::ostream &operator<<(std::ostream &o, const EVERECORD &p) {
  return o << (int)p.unit_no << "\t" << (int)p.kind << "\t" << p.clock << "\t"
           << p.value;
}

double lerp(double a, double b, int num, int denom) {
  return (a * (denom - num) + b * num) / denom;
}

struct Unit {
  Historical<Press> presses;
  Historical<int> notes, portas;
  Historical<double> tunings;

  // other quantities that easily map to MIDI
  Historical<int> volume, voice, group, pan_v, pan_t;

  Historical<double> pitch_offsets() {
    // Changing portamento times within a note is complicated. From what I
    // gather looking at the source code (pxtnUnit::Tone_Increment_Key) and
    // experiments, what happens at a mid-note portamento change is:
    //
    // 1) If there was previously a [non-zero] portamento time, and the
    // portamento finished, this portamento time change, and all future changes
    // for this note, do nothing (as expected).
    //
    // 2) Otherwise, there was a [non-zero] portamento time just before the note
    // started, but the portamento didn't finish [OR the previous portamento
    // time was 0]. (In addition, condition 1 was never fulfilled by any
    // previous portamento -- i.e., no previous portamento slide finished). Then
    // then note plays afterwards as if the new portamento time covered the
    // whole note.
    //
    // For example, consider 2 whole notes were together in sequence, the second
    // a 4 semitones lower, and say portamento time was originally 4 beats, and
    // a beat into the second whole note, portamento time changed to 2 beats.
    // The first quarter note would smoothly move to 1 semitone below the first
    // whole note, but then jump to 2 semitones below, then smoothly move to 4
    // semitones below for the second whole note.
    //
    // I don't think the 2nd case behaviour is intended. Or if it is, it
    // shouldn't depend on whether or not the previous portamento time was 0 or
    // not (why should there be a jump if there was 0-time portamento vs. if
    // there was a 0.00001-time portamento?).
    //
    // I'm emulating the 2 points described above, but getting rid of the
    // distinction between non-zero and zero portamento time (i.e., what was
    // said but with everything in []s removed). This is mostly faithful to the
    // original but with the case most likely to be an error gone.
    //
    // To avoid all of this likely undesired behaviour, write ptcop files with
    // (possibly empty) note changes at each portamento time change.
    Historical<double> offsets;
    for (const auto & [ press_time, press ] : presses) {
      if (at_time(offsets, press_time, 0.) != 0) offsets[press_time] = 0;
      int base_key = at_time(notes, press_time, 48);
      auto key_bound = notes.lower_bound(press_time + press.length);
      auto key_it = notes.upper_bound(press_time);
      while (key_it != key_bound) {
        const auto & [ key_time, key ] = *key_it;
        double curr_off = at_time(offsets, key_time, 0.);
        double dest_off = key - base_key;

        ++key_it;
        if (dest_off == curr_off) continue;

        // available length for this note
        int avail_note_length = press_time + press.length - key_time;
        if (key_it != key_bound && key_it->first - key_time < avail_note_length)
          avail_note_length = key_it->first - key_time;

        constexpr int incr = 10;
        portas.emplace(0, 0); // to avoid the case of no portamentos
        auto porta_it = --portas.upper_bound(key_time);
        auto porta_bound = portas.lower_bound(key_time + avail_note_length);
        for (; porta_it != porta_bound; ++porta_it) {
          // restrict porta_time for the for loop later down
          int porta_time = std::max(porta_it->first, key_time);
          int porta = porta_it->second;

          // available length for this block of portamento in this note
          auto next_porta = porta_it;
          ++next_porta;
          int avail_porta_length = avail_note_length;
          if (next_porta != porta_bound &&
              avail_porta_length > next_porta->first - key_time)
            avail_porta_length = next_porta->first - key_time;

          for (int i = porta_time - key_time;
               i < std::min(avail_porta_length, porta); i += incr)
            offsets[key_time + i] = lerp(curr_off, dest_off, i, porta);

          // if this porta finished, don't start the next one.
          if (porta + key_time <= avail_porta_length) {
            offsets[key_time + porta] = dest_off;
            break;
          }
        }

        // porta_it is the first portamento that finished. if it's porta_bound,
        // no portamento finished, in which case we calculate and the correct
        // pitch offset for the end of this note.
        if (porta_it == porta_bound && porta_it != portas.begin()) {
          int porta = (--porta_bound)->second;
          // porta is nonzero, or else it would've finished before the note end.
          offsets[key_time + avail_note_length] =
              lerp(curr_off, dest_off, avail_note_length, porta);
        }
      }
    }

    auto tuning_it = tunings.begin();
    while (tuning_it != tunings.end()) {
      const auto & [ tune_time, tuning ] = *tuning_it;
      double current_offset = at_time(offsets, tune_time, 0.);
      ++tuning_it;

      auto offset_it = offsets.emplace(tune_time, current_offset).first;
      while (
          offset_it != offsets.end() &&
          (tuning_it == tunings.end() || offset_it->first < tuning_it->first)) {
        offset_it->second += tuning;
        ++offset_it;
      }
    }
    return offsets;
  }
};

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

  // Build units data
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

    case EVENTKIND_TUNING: {
      auto &hist = units[p->unit_no].tunings;
      float value = reinterpret_cast<const float &>(p->value);
      hist.emplace(p->clock, std::log2(value) * 12);
      break;
    }

    case EVENTKIND_PORTAMENT:
      units[p->unit_no].portas.emplace(p->clock, p->value);
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

    case EVENTKIND_VOICENO:
      units[p->unit_no].voice.emplace(p->clock, p->value);
      break;

    case EVENTKIND_GROUPNO:
      units[p->unit_no].group.emplace(p->clock, p->value);
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
    int channel = (i >= 10 ? i + 1 : i); // skip the drum channel

    const pxtnUnit &unit = *pxtn.Unit_Get(i);
    midifile.addTrackName(track, 0, unit.get_name_buf(nullptr));
    // configure cc6 to set pitch bend range
    midifile.addController(track, 0, channel, 100, 0);
    midifile.addController(track, 0, channel, 101, 0);
    // TODO: get rid of
    midifile.addController(track, 0, channel, 11, 104); // 104 = default volume
    midifile.addPitchBend(track, 0, channel, 0);

    // All these pxtone parameters go from 0 to *128*, so we need to cap at 127
    for (const auto & [ time, volume ] : units[i].volume)
      midifile.addController(track, time, channel, 11, std::min(volume, 127));

    for (const auto & [ time, pan_v ] : units[i].pan_v)
      midifile.addController(track, time, channel, 10, std::min(pan_v, 127));

    // note to self: when doing pan_t, also cap at 127

    for (const auto & [ time, press ] : units[i].presses) {
      int key = at_time(units[i].notes, time);
      midifile.addNoteOn(track, time, channel, key, std::min(press.vel, 127));
      midifile.addNoteOff(track, time + press.length, channel, key);
    }

    for (const auto & [ time, offset ] : units[i].pitch_offsets()) {
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
