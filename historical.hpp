#ifndef HISTORICAL_HPP
#define HISTORICAL_HPP

// A structure that keeps track of a time-varying quantity (starting at time 0).
#include <climits>
#include <iostream>
#include <map>

template <typename T> class Historical : public std::map<int, T> {
public:
  // We (softly) enforce an initial value for easier file writing and pitch
  // processing later.
  Historical(T initial) : std::map<int, T>({{0, initial}}){};

  // Get the value of the quantity at the given time.
  T at_time(int time) const {
    // first transition after time
    auto it = std::map<int, T>::upper_bound(time);
    if (it == std::map<int, T>::begin())
      throw std::domain_error("no value at this time");
    else
      return (--it)->second;
  }
};

template <typename T>
std::ostream &operator<<(std::ostream &o, const Historical<T> &hist) {
  for (const auto & [ time, value ] : hist)
    o << time << ":\t" << value << std::endl;
  return o;
}

#endif // HISTORICAL_HPP
