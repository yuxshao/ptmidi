#include "pttypes.hpp"

std::ostream &operator<<(std::ostream &o, const EVERECORD &p) {
  return o << (int)p.unit_no << " " << (int)p.kind << "\t" << p.clock << "\t"
           << p.value;
}

std::ostream &operator<<(std::ostream &o, const Press &p) {
  return o << "(" << p.vel << ", " << p.length << ")";
}

std::vector<Woice> Woice::get_woices(const pxtnService &pxtn) {
  std::vector<Woice> woices(pxtn.Woice_Num());
  for (int i = 0; i < pxtn.Woice_Num(); ++i) {
    const pxtnWoice *woice = pxtn.Woice_Get(i);
    if (woice->is_name_buf()) {
      std::string name(woice->get_name_buf(nullptr));
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

  return woices;
}

std::vector<Unit> Unit::get_units(const pxtnService &pxtn) {
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

  return units;
}
