#pragma once

namespace px {

// A geographic position in decimal degrees (WGS-84).
//
// Latitude is positive north, longitude is positive east.  Coordinate is an
// immutable value type; all members are public because it carries no invariant
// beyond holding two numbers.
struct Coordinate {
  double latitude = 0.0;   // degrees, [-90, 90]
  double longitude = 0.0;  // degrees, [-180, 180]

  // Great-circle distance to another coordinate, in nautical miles (NM),
  // computed with the haversine formula on a spherical earth model.
  double DistanceTo(const Coordinate& other) const;

  // Initial bearing (azimuth) from this coordinate to another, in degrees
  // clockwise from true north [0, 360).
  double BearingTo(const Coordinate& other) const;
};

}  // namespace px
