#!/bin/sh
# Run from the astro directory
mkdir -p ephemeris/ascii/de430
mkdir -p ephemeris/fortran
curl -O http://aa.usno.navy.mil/software/novas/novas_c/novasc3.1.tar.gz
tar -xzf novasc3.1.tar.gz
cd Cdist
patch -p1 < novasc3.1-linux64.patch
cd ../ephemeris/fortran
echo "Reading ephemeris/fortran"
for p in $(curl --list-only ftp://ssd.jpl.nasa.gov/pub/eph/planets/fortran/); do
    curl -O ftp://ssd.jpl.nasa.gov/pub/eph/planets/fortran/$p
done
cd ../ascii/de430
echo "Reading ascii/de430"
for p in $(curl --list-only ftp://ssd.jpl.nasa.gov/pub/eph/planets/ascii/de430/); do
    curl -O ftp://ssd.jpl.nasa.gov/pub/eph/planets/ascii/de430/$p
done
echo "Done"
