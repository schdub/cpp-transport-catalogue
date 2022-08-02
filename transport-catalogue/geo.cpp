#define _USE_MATH_DEFINES
#include "geo.h"

#include <cmath>

namespace geo {

double ComputeDistance(const Coordinates & from, const Coordinates & to) {
    if (from == to) {
        return 0;
    }
    static const double dr = 3.1415926535 / 180.;
    static const double EARTH_RADIUS = 6371000;
    return std::acos(std::sin(from.lat * dr)
         * std::sin(to.lat * dr)
         + std::cos(from.lat * dr)
         * std::cos(to.lat * dr)
         * std::cos(std::abs(from.lng - to.lng) * dr))
         * EARTH_RADIUS;
}

}  // namespace geo