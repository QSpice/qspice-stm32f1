#ifndef __SPICE_H__
#define __SPICE_H__

struct Spice {
    const char* name;
    float weight;

    Spice(const char* name, float weight) {
      this->name = name;
      this->weight = weight;
    }

};

#endif
