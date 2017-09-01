#ifndef DATA_HPP
#define DATA_HPP

#include <iostream>

#include "historical.hpp"

struct Press {
  int vel, length;
};

std::ostream &operator<<(std::ostream &o, const Press &p);

struct Woice {
  Woice() : drum(false), num(0) {}
  bool drum;
  int num;
};

struct Unit {
  // not a historical since it's not exactly a time-varying quantity, but a
  // bunch of presses at different times.
  std::map<int, Press> presses;

  Historical<int> notes, portas;
  Historical<double> tunings;

  // other quantities that easily map to MIDI
  Historical<int> volume, voice, group, pan_v, pan_t;

  Unit()
      : notes(48), portas(0), tunings(0), volume(104), voice(0), group(0),
        pan_v(64), pan_t(64){};
};

#endif // DATA_HPP
