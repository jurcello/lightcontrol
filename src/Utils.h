//
//  Utils.h
//  PhotonicDirector
//
//  Created by Jur de Vries on 25/10/2017.
//

#ifndef Utils_h
#define Utils_h

#include <uuid/uuid.h>
#include "cinder/CinderMath.h"

using namespace cinder;

// I know that this is not system indepedent, but this can easily be fixed once needed.
namespace photonic {
    inline std::string generate_uuid()
    {
        uuid_t uuid;
        uuid_generate_random(uuid);
        char s[37];
        uuid_unparse(uuid, s);
        return s;
    }

    inline double getBellIntensity(double distance, double width) {
        return math<double>::exp(-(distance)/(2*(math<double>::pow(width, 2))));
    }

    inline ColorA normalizeColor(ColorA color) {
        float max = color.r;
        if (color.g > max) {
            max = color.g;
        }
        if (color.b > max) {
            max = color.b;
        }
        return color / max;
    }

    inline std::string midiNoteToString(int midinote) {
        int octave = int (midinote / 12) - 1;
        const std::string name = "C C#D D#E F F#G G#A A#B ";
        return name.substr((midinote % 12) * 2, 2) + std::to_string(octave);
    }
}

#endif /* Utils_h */
