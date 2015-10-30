## GeographicLib's geoid height function in Python, C, and JavaScript

[GeographicLib][1] is a library that contains, among many other
features, one especially useful feature: computing the height of the
[WGS84][2] geoid above the ellipsoid.

At various times over the course of the past 5 years, I needed this
feature in projects that were written in Python, C, and JavaScript.
Since the guts of the code is not too long, I translated the feature
from GeographicLib's C++ into each of those other 3 languages.

 * The Python implementation was written on 2009-11-07
 * The C implementation was written on 2011-06-10
 * The JavaScript implementation was written on 2015-10-29

Since GeographicLib is licensed under the LGPL, all of this derived
work is as well.

-Kim Vandry <vandry@TZoNE.ORG> 2015-10-30

[1]: http://geographiclib.sourceforge.net/
[2]: https://en.wikipedia.org/wiki/World_Geodetic_System
