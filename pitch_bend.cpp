#include "pitch_bend.hpp"

static double lerp(double a, double b, int num, int denom) {
  return (a * (denom - num) + b * num) / denom;
}

// Changing portamento times within a note is complicated. From what I gather
// looking at the source code (pxtnUnit::Tone_Increment_Key) and experiments,
// what happens at a mid-note portamento change is:
//
// 1) If there was previously a [non-zero] portamento time, and the portamento
// finished, this portamento time change, and all future changes for this note,
// do nothing (as expected).
//
// 2) Otherwise, there was a [non-zero] portamento time just before the note
// started, but the portamento didn't finish [OR the previous portamento time
// was 0]. (In addition, condition 1 was never fulfilled by any previous
// portamento -- i.e., no previous portamento slide finished). Then then note
// plays afterwards as if the new portamento time covered the whole note.
//
// For example, consider 2 whole notes were together in sequence, the second a 4
// semitones lower, and say portamento time was originally 4 beats, and a beat
// into the second whole note, portamento time changed to 2 beats. The first
// quarter note would smoothly move to 1 semitone below the first whole note,
// but then jump to 2 semitones below, then smoothly move to 4 semitones below
// for the second whole note.
//
// I don't think the 2nd case behaviour is intended. Or if it is, it shouldn't
// depend on whether or not the previous portamento time was 0 or not (why
// should there be a jump if there was 0-time portamento vs. if there was a
// 0.00001-time portamento?).
//
// I'm emulating the 2 points described above, but getting rid of the
// distinction between non-zero and zero portamento time (i.e., what was said
// but with everything in []s removed). This is mostly faithful to the original
// but with the case most likely to be an error gone.
//
// To avoid all of this likely undesired behaviour, write ptcop files with
// (possibly empty) note changes at each portamento time change.
Historical<double> porta_pitch_offsets(const std::map<int, Press> &presses,
                                       const Historical<int> &notes,
                                       const Historical<int> &portas) {
  Historical<double> offsets(0);
  for (const auto & [ press_time, press ] : presses) {
    if (offsets.at_time(press_time) != 0) offsets[press_time] = 0;
    int base_key = notes.at_time(press_time);
    auto key_bound = notes.lower_bound(press_time + press.length);
    auto key_it = notes.upper_bound(press_time);
    while (key_it != key_bound) {
      const auto & [ key_time, key ] = *key_it;
      double curr_off = offsets.at_time(key_time);
      double dest_off = key - base_key;

      ++key_it;
      if (dest_off == curr_off) continue;

      // available length for this note
      int avail_note_length = press_time + press.length - key_time;
      if (key_it != key_bound && key_it->first - key_time < avail_note_length)
        avail_note_length = key_it->first - key_time;

      constexpr int incr = 10;
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
        if (porta <= avail_porta_length) {
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
  return offsets;
}

// Given two historicals, sets the first equal to the "sum" of the two.
void add_pitch_offset(Historical<double> &base, const Historical<double> &add) {
  auto new_it = add.begin();
  while (new_it != add.end()) {
    const auto & [ new_time, new_pitch ] = *new_it;
    double current_offset = base.at_time(new_time);
    ++new_it;

    auto offset_it = base.emplace(new_time, current_offset).first;
    while (offset_it != base.end() &&
           (new_it == add.end() || offset_it->first < new_it->first)) {
      offset_it->second += new_pitch;
      ++offset_it;
    }
  }
}
