#ifndef CONVERT_HPP
#define CONVERT_HPP

#include "MidiFile.h"
#include "pxtone/pxtnService.h"

// The primary conversion function.
MidiFile pxtn_to_midi(const pxtnService &pxtn);

#endif // CONVERT_HPP
