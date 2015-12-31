# astro

The astro project seeks to help programmers new to vector
astronomy obtain current, accurate ephemeris data; and create
a platform-compatible data file. Once the ephemeris and
library are built, small sample applications are included
to verify correctness and demonstrate the library's
capabilities.

This project leverages the latest JPL Ephemeris (DE430) and
NOVAS-C release (3.1).  The NOVAS C distribution and JPL Ephemeris
and utility software are free to download from the publishers.
Neither is provided here.

Instead, this project will simplify the process and provide
sample executables. The operation is scripted for generic Linux
platforms.

Included in this source code repository are two sample programs
which use NOVAS-C to calculate the apparent position of the Sun
and planets from an observation point on Earth, and a program to
display the next two tropical moments (equinox and solstice).

The goal of this project is to encourage amatuer astronomical
observations and applications for the highly accurate vector
astronomy that NOVAS and JPL Ephemeris DE430 provide.

### Basic vector astronomy using NOVAS

"The Naval Observatory Vector Astronomy Software (NOVAS) is a source-code
library in Fortran, C and Python that provides common astrometric quantities
and transformations. It can supply, in one or two function calls, the
instantaneous celestial position of any star or planet in a variety of
coordinate systems."
_NOVAS C 3.1 Guide_

This project uses the NOVAS C-library to calculate the sky positions of
the planets, Sun and Moon, from an observer at a fixed location on Earth.
The latest version of the library, as of 2015-12-30, is NOVAS-C 3.1.
Included in this repository is a patch file that applies fixes to two
[known issues](http://aa.usno.navy.mil/software/novas/novas_faq.php)
in this release.

The known issues are:

* sidereal_time units bug
* ephem_close does not reset the EPHFILE pointer

This project uses the general purpose planet ephemeris file DE430, published
15-Aug-2013 by the National Aeronautics and Space Administration (NASA)
Jet Propulsion Laboratory (JPL). Following the instructions below, you
will download the text files that make up DE430, along with Fortran
code that parses these and produces a binary ephemeris file for your
architecture. Once the binary DE430 is built and tested, you will use
it to build an example program in the NOVAS C distribution.

Two small applications included in this project can
be built to demonstrate some of the capabilities of NOVAS. (See below.)

![Screen shot of planets](demo.png)

### Instructions for creating binary JPL Ephemeris DE430

These instructions were tested on FC19-22 x86_64 Linux.

### NOVAS References

For your reference... and to make sure things haven't changed too much.

[Naval Observatory Vector Astronomy Software](http://aa.usno.navy.mil/software/novas/novas_info.php)

[C Edition](http://aa.usno.navy.mil/software/novas/novas_c/novasc_info.php)

[Tarball](http://aa.usno.navy.mil/software/novas/novas_c/novasc3.1.tar.gz)


### Install Fortran Compiler

Assumes that you have the typical GCC tools already. Also add the
cURL library and development headers (for Bulletin A fetching code).

`sudo yum install libgfortran gcc-gfortran libcurl libcurl-devel`


### Set Up Development Tree

The Bash script `setup-linux64.sh` will download and prepare the
NOVAS-C sources from the U. S. Naval Observatory and the DE430
ephemeris from the NASA JPL.

The files are downloaded via FTP from these external sites. The
sites tend to be busy, so you may have to run the setup script
several times before it completes successfully.

`./setup-linux64.sh`

### Build asc2eph, JPLEPH and test

`make -C ephemeris/fortran test`

The output will display (among other things) the _jpl values_, _user value_ and _difference_.

A successful run will have no difference greater than about `0.71054E-14`

# Congratulations!

You now have the binary ephemeris for your platform.

### Build NOVAS examples

Build `checkout-stars`

```
cd Cdist
make checkout-stars
```

Run `checkout-stars`

`./checkout-stars > checkout-mp-my.txt`

Compare with USNO output

`diff -U0 checkout-stars-full-usno.txt checkout-mp-my.txt`

Because these test vectors were created with DE405, you'll have *small*
differences in the numeric output. Here's what my diff output looks like:

```
--- checkout-stars-full-usno.txt        2011-01-21 14:48:11.000000000 +0000
+++ checkout-mp-my.txt  2015-03-13 22:26:57.000000000 +0000
@@ -1,2 +0,0 @@
-JPL Ephemeris DE405 open. jd_beg = 2305424.50  jd_end = 2525008.50
-
@@ -4 +2 @@
-RA =  2.446988922  Dec =  89.24635149
+RA =  2.446988912  Dec =  89.24635149
@@ -14 +12 @@
-RA =  2.446988922  Dec =  89.24635149
+RA =  2.446988912  Dec =  89.24635149
@@ -24 +22 @@
-RA =  2.509480139  Dec =  89.25196813
+RA =  2.509480137  Dec =  89.25196813
@@ -27 +25 @@
-RA =  5.531195904  Dec =  -0.30301961
+RA =  5.531195903  Dec =  -0.30301961
@@ -34 +32 @@
-RA =  2.481177533  Dec =  89.24254404
+RA =  2.481177531  Dec =  89.24254404
```

### Build example

The current directory is `Cdist`.

`make example`

_Ignore a few unused variable warnings_

Run the example

`./example > example-my.txt`

Compare with USNO output

`diff -U0 example-usno.txt example-my.txt`

Again, ignore small differences in the numbers. The USNO data was generated
from an older ephemeris. Here's my output:

```
--- example-usno.txt    2011-01-21 14:48:10.000000000 +0000
+++ example-my.txt      2015-03-13 22:37:39.000000000 +0000
@@ -1 +1 @@
-JPL ephemeris DE405 open. Start JD = 2305424.50  End JD = 2525008.50
+Using solsys version 2, no description of JPL ephemeris available
@@ -14 +14 @@
-  11.8915479153          37.6586695456
+  11.8915479153          37.6586695455
@@ -17,3 +17,3 @@
-  17.1390774264         -27.5374448869         0.002710296515
-  17.1031967646         -28.2902502967         0.002703785126
-  17.1031967646         -28.2902502967         0.002703785126
+  17.1390775046         -27.5374455772         0.002710296520
+  17.1031968434         -28.2902509919         0.002703785130
+  17.1031968434         -28.2902509919         0.002703785130
@@ -22 +22 @@
-  81.6891016502         219.2708903405
+  81.6891016821         219.2708890772
@@ -28 +28 @@
- 148.0032235906           1.8288284075         1.664218258879
+ 148.0032234980           1.8288285255         1.664218258927
```

# Congratulations!

You have built and tested NOVAS with JPL DE430.

# Planets

The `planets` program uses the ephemeris and NOVAS-C library to
display the positions of the planets from an observer on Earth.

You will want to edit `planets.c` to use your geodetic location.
Search for `REPLACE WITH YOUR LOCATION`
Update the latitude, longitude, height, temperature and pressure
with values appropriate for your location. Latitude is expressed in
degrees north of the Equator. Longitude is expressed in degrees east
of the Prime Meridian. Height is expressed in meters above sea level.
Temperature is expressed in degrees Celsius. Pressure is expressed
in millibars.

```
cd ../planets
make
watch ./planets
```

# Tropical

The `tropical` program uses the ephemeris and NOVAS-C library to
display the next two tropical moments (equinox and solstice).

It uses the latitude of the subsolar point on Earth in its
calculations. A local maximum or minimum latitude is the
solstice, and the crossing of the Equator is the equinox.

## Citation and Further Reading

`Cdist/NOVAS_C3.1_Guide.pdf`

Bangert, J., Puatua, W., Kaplan, G., Bartlett, J., Harris, W., Fredericks, A., & Monet, A. (2011) User's Guide to NOVAS Version C3.1 (Washington, DC: USNO).
