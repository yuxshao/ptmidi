#include "historical.hpp"
#include "pttypes.hpp"

Historical<double> porta_pitch_offsets(const std::map<int, Press> &presses,
                                       const Historical<int> &notes,
                                       const Historical<int> &portas);

void add_pitch_offset(Historical<double> &base, const Historical<double> &add);
