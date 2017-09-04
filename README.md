# astro

The astro project seeks to help programmers new to vector
astronomy obtain current, accurate ephemeris data; and create
a platform-compatible data file. Once the ephemeris and
library are built, small sample applications are included
to verify correctness and demonstrate the library's
capabilities.

This project leverages the latest general purpose JPL Ephemeris (DE430) and
NOVAS-C release (3.1).  NOVAS-C, and the JPL Ephemeris
and utility software are free to download from the publishers.
Neither is provided here.

Instead, this project scripts the process and provides
sample executables. The scripting is developed for and tested on generic
Linux platforms.

Included in this source code repository are two sample programs
which use NOVAS-C to calculate the apparent position of the Sun
and planets from an observation point on Earth, and a program to
display upcoming tropical moments (equinox and solstice).

The goal of this project is to encourage amatuer astronomical
observations and applications for the highly accurate vector
astronomy that NOVAS-C and JPL DE430 provide.

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

The latest version of the library, as of 2015-12-30, is NOVAS-C 3.1.
The setup script will also apply fixes for the two (as of 2015-Jan-09)
[known issues](http://aa.usno.navy.mil/software/novas/novas_faq.php)
in NOVAS-C 3.1.

This project uses the general purpose planetary ephemeris file DE430, published
15-Aug-2013 by the National Aeronautics and Space Administration
Jet Propulsion Laboratory (NASA JPL, or simply "JPL"). Following the instructions
below, you will download the text files that make up JPL DE430, along with FORTRAN
code that parses these and produces a binary ephemeris file.
Once the binary ephemeris is built and tested, you will use
it to build example programs from the NOVAS-C distribution.

Two small applications included in this project can
be built to demonstrate some of the capabilities of NOVAS-C. (See below.)

![Screen shot of planets](demo.png)

### Instructions for creating the binary JPL DE430

These instructions were tested on FC19-25 x86_64 Linux.

### NOVAS-C References

For your reference... and to make sure things haven't changed too much.

[Naval Observatory Vector Astronomy Software](http://aa.usno.navy.mil/software/novas/novas_info.php)

[C Edition](http://aa.usno.navy.mil/software/novas/novas_c/novasc_info.php)

[Tarball](http://aa.usno.navy.mil/software/novas/novas_c/novasc3.1.tar.gz)


### Install FORTRAN Compiler

Assumes that you have the typical GCC tools already. Also add the
cURL library and development headers (for Bulletin A fetching code).

`sudo yum install libgfortran gcc-gfortran libcurl libcurl-devel`

And on newer FC releases:

`sudo dnf install libgfortran gcc-gfortran libcurl libcurl-devel`


### Set Up Development Tree

The Bash script `setup-linux64.sh` will download and prepare the
NOVAS-C sources from the United States Naval Observatory and DE430
from JPL.

The files are downloaded via FTP from these external sites. The
sites tend to be busy, so you may have to run the setup script
several times before it completes successfully. It keeps track
of its progress using touchfiles, so you should be able to run
it again (sometimes repeatedly) after it stops on an FTP error.

`./setup-linux64.sh`

### Build asc2eph, JPLEPH and test

`make -C ephemeris/fortran test`

The output will display (among other things) the _jpl values_, _user value_ and _difference_.
The output may end with:

`Note: The following floating-point exceptions are signalling: IEEE_UNDERFLOW_FLAG IEEE_DENORMAL`

The exit code is normal, so this message can (probably) be ignored for now. What is important
are the numbers in the "difference" column.  A successful run will have no difference greater
than about `0.71054E-14`

The output, `JPLEPH` is created in `ephemeris/fortran`, but after passing the test is moved
to ~/.astro. You now have the binary ephemeris for your platform.

### Build NOVAS-C examples

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

You have built and tested NOVAS-C with JPL DE430.

## Doesn't JPL supply binary ephemerides?

Yes, JPL offers binary versions of DE430 on their FTP server.
In particular, the [Linux](ftp://ssd.jpl.nasa.gov/pub/eph/planets/Linux/de430/linux_p1550p2650.430)
binary ephemeris should match identically the ephemeris you just built
on 64-bit Linux systems. To confirm, run the following command:

```
$ sha256sum ~/.astro/JPLEPH
0deb23ca9269496fcbab9f84bec9f3f090e263bfb99c62428b212790721de126  ~/.astro/JPLEPH
```

The value above matches exactly the sha256sum of the file `linux_p1550p2650.430`
dated 2013-Aug-15 from the JPL FTP site. Even though this file is available,
building your own binary ephemeris is worth the effort
because it validates the FORTRAN back-end, which is used both for
generating and consuming the binary ephemeris.

Retaining the FORTRAN back end in NOVAS-C makes the software more
modular and testable. This is absolutely a requirement for mission-
critical applications, such as flying spacecraft or maneuvering
satellites. If you're working on applications of that sort, you
will be comfortable working with NOVAS-C's FORTRAN back end.

This project encourages less scientific developers to use NOVAS-C
for amateur astronomy, therefore a patch to replace the back end
with native C would definitely be in scope. Such an effort would
also open up the project to developers running Microsoft Visual
Studio.

## What about Microsoft Windows?

This project (as yet) will only build on Microsoft Windows systems
if Cygwin is installed and used to load GCC, including its FORTRAN
support libraries. It currently does not support a native
build in Microsoft Visual Studio.

A pull request providing that ability would be welcome.

# Building applications with NOVAS-C and JPL DE430

This project includes two very simple applications to illustrate
how to use NOVAS-C and JPL DE430. The features in these programs is
intentionally limited in order to reduce dependencies on platform-specific
and third-party libraries. A more feature-rich application in C++
with tight dependency on Boost libraries is coming soon.

## Planets

The `planets` program displays the positions of the planets from an
observer on Earth.

You will want to edit `planets.c` to use your geodetic location.
Search for `REPLACE WITH YOUR LOCATION`
Update the latitude, longitude, height, temperature and pressure
with values appropriate for your location. Latitude is expressed in
degrees north of the Equator. Longitude is expressed in degrees east
of the Prime Meridian. Height is expressed in meters above sea level.
Temperature is expressed in degrees Celsius. Pressure is expressed
in millibars.

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
Using solsys version 2


September equinox occurs at 2017-09-22 20:05:42 UTC
  Subsolar point: {0°00'03.3"S 123°17'42.7"W} [{-0.0009103414, -123.2951996969}]
December solstice occurs at 2017-12-21 06:08:38 UTC
  Subsolar point: {23°26'02.3"S 87°21'18.5"E} [{-23.4339762762, 87.3551439222}]
March equinox occurs at 2018-03-20 16:11:34 UTC
  Subsolar point: {0°00'04.7"S 61°02'23.2"W} [{-0.0013095105, -61.0397648868}]
June solstice occurs at 2018-06-21 21:01:06 UTC
  Subsolar point: {23°26'04.4"N 134°48'40.7"W} [{23.4345547085, -134.8112933746}]
September equinox occurs at 2018-09-23 01:50:38 UTC
  Subsolar point: {0°00'03.3"N 150°28'05.7"E} [{0.0009268029, 150.4682396496}]
December solstice occurs at 2018-12-21 12:30:43 UTC
  Subsolar point: {23°26'05.3"S 8°09'55.8"W} [{-23.4348010124, -8.1655028688}]
March equinox occurs at 2019-03-20 22:00:34 UTC
  Subsolar point: {0°00'02.7"N 148°16'59.2"W} [{0.0007503193, -148.2831158729}]
June solstice occurs at 2019-06-22 02:55:10 UTC
  Subsolar point: {23°26'05.9"N 136°40'28.8"E} [{23.4349703858, 136.6746679012}]
September equinox occurs at 2019-09-23 07:49:46 UTC
  Subsolar point: {0°00'00.3"N 60°40'44.7"E} [{0.0000735089, 60.6790762663}]
```

# Citation and Further Reading

After cloning this repository and running the setup script, the
following file will be present:

`Cdist/NOVAS_C3.1_Guide.pdf`

That document provides the definitive reference to NOVAS-C, and is
therefore required reading if you want to get the best results.

Bangert, J., Puatua, W., Kaplan, G., Bartlett, J., Harris, W., Fredericks, A., & Monet, A. (2011) User's Guide to NOVAS Version C3.1 (Washington, DC: USNO).
