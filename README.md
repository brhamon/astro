# astro

The astro project seeks to help programmers new to vector
astronomy work with the latest planetary ephemerides
published by the NASA Jet Propulsion Laboratory, using
the NOVAS-C library.

NOVAS-C and the JPL Ephemerides are free to download from the publishers.
Neither is provided here, although a setup script downloads both.

Included in this project are two sample programs:

* `planets` -- Calculates the apparent position of the Sun
and planets from an observation point on Earth
* `tropical` -- Displays upcoming tropical moments (equinox and solstice)

### Basic vector astronomy using NOVAS

"The Naval Observatory Vector Astronomy Software (NOVAS) is a source-code
library in FORTRAN, C and Python that provides common astrometric quantities
and transformations. It can supply, in one or two function calls, the
instantaneous celestial position of any star or planet in a variety of
coordinate systems."
_NOVAS C 3.1 Guide_

This project uses the NOVAS-C library to calculate the sky positions of
the planets, Sun and Moon, from an observer at a fixed location on Earth;
and predicts future solstices and equinoxes.

The latest version of the library is NOVAS-C 3.1.
The setup script will also apply fixes for the two (as of 2018-Mar-25)
[known issues](http://aa.usno.navy.mil/software/novas/novas_faq.php)
in NOVAS-C 3.1.

This project uses the general purpose planetary ephemeris file DE430, published
15-Aug-2013 by the National Aeronautics and Space Administration
Jet Propulsion Laboratory (NASA JPL, or simply "JPL"). The JPL also publishes
the DE431 ephemeris which covers a time range of more than 30,000 years.
Those wishing to work with planetary positions more than 500 years ago, or 500 years
in the future, should download DE431.

More information about these JPL products is provided in
[The Planetary and Lunar Ephemerides DE430 and DE431](https://naif.jpl.nasa.gov/pub/naif/generic_kernels/spk/planets/de430_and_de431.pdf).

### NOVAS-C References

The United States Naval Observatory publishes NOVAS-C. More information is avaiable
at the following links:

[Naval Observatory Vector Astronomy Software](http://aa.usno.navy.mil/software/novas/novas_info.php)

[C Edition](http://aa.usno.navy.mil/software/novas/novas_c/novasc_info.php)

[Tarball](http://aa.usno.navy.mil/software/novas/novas_c/novasc3.1.tar.gz)


### Set Up Development Tree

The Bash script `setup-linux64.sh` will download and prepare the
NOVAS-C sources from the United States Naval Observatory and DE430
from JPL. (Examine the sources, and change the value of `WANT_DE431`
to `1`, if you wish to use it instead of DE430.)

The files are downloaded via FTP from the JPL and United States
Naval Observatory sites. You may have to run the setup script
several times before it completes successfully.

`./setup-linux64.sh`

## Mac (Darwin) support

This project has been tested on MacOS, using gcc.

## What about Microsoft Windows?

This code should build and run on Windows using Microsoft
Visual Studio, however, you will need to create the solution (.sln)
file. It has not yet been tested on this platform.

A pull request providing successful results on Windows would be welcome.

# Building applications with NOVAS-C and JPL DE430

This project includes two very simple applications to illustrate
how to use NOVAS-C and JPL DE430. The features in these programs is
intentionally limited in order to reduce dependencies on platform-specific
and third-party libraries.

## Planets

The `planets` program displays the positions of the planets from an
observer on Earth.

```
make -C planets
planets/planets -h
watch planets/planets
```

## Tropical

The `tropical` program displays upcoming tropical moments
(equinox and solstice).

It uses the latitude of the subsolar point on Earth in its
calculations. A local maximum or minimum latitude is the
solstice, and the crossing of the Equator is the equinox.

```
make -C tropical
tropical/tropical
```

The output will look like:
```
Ephemeris: JPL Planetary Ephemeris DE430/LE430
June solstice occurs at 2018-06-21 19:52:06 UTC
  Subsolar point: {23°26'04.7"N 117°33'47.5"W} [{23.4346361922, -117.5631956988}]
September equinox occurs at 2018-09-23 00:42:53 UTC
  Subsolar point: {0°01'09.3"N 167°24'43.0"E} [{0.0192365702, 167.4119554468}]
December solstice occurs at 2018-12-21 11:21:39 UTC
  Subsolar point: {23°26'04.6"S 9°05'41.4"E} [{-23.4346196427, 9.0948333703}]
March equinox occurs at 2019-03-20 22:00:25 UTC
  Subsolar point: {0°00'02.5"N 148°14'49.4"W} [{0.0006938732, -148.2470566509}]
June solstice occurs at 2019-06-22 04:21:21 UTC
  Subsolar point: {23°26'05.4"N 115°07'59.0"E} [{23.4348224251, 115.1330580705}]
September equinox occurs at 2019-09-23 09:13:52 UTC
  Subsolar point: {0°01'21.6"S 39°39'06.1"E} [{-0.0226555423, 39.6516846115}]
December solstice occurs at 2019-12-21 18:33:14 UTC
  Subsolar point: {23°26'07.7"S 98°47'52.7"W} [{-23.4354646112, -98.7979609006}]
March equinox occurs at 2020-03-20 03:52:35 UTC
  Subsolar point: {0°00'03.3"N 123°42'36.8"E} [{0.0009214862, 123.7102242062}]
```

# Citation and Further Reading

After cloning this repository and running the setup script, the
following file will be present:

`Cdist/NOVAS_C3.1_Guide.pdf`

That document provides the definitive reference to NOVAS-C, and is
therefore required reading if you want to get the best results.

Bangert, J., Puatua, W., Kaplan, G., Bartlett, J., Harris, W., Fredericks, A., & Monet, A. (2011) User's Guide to NOVAS Version C3.1 (Washington, DC: USNO).
