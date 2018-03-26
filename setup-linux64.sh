#!/bin/bash
# Run from the astro directory
declare DE430_FILE="linux_p1550p2650.430"
declare DE431_FILE="lnxm13000p17000.431"
# NOTE: You may want to use DE431. If so, change the next line to 1
declare -i WANT_DE431=0
declare ASTRO_PATH=~/.astro
declare JPLEPH_LINK=$ASTRO_PATH/JPLEPH
declare MY_CURLOPTS="--connect-timeout 20 --no-keepalive"

if [[ ! -f .Cdist.is_patched ]]; then
	if [[ -d Cdist ]]; then
		rm -fr Cdist
	fi
	echo "Reading NOVAS-C"
	curl $MY_CURLOPTS -O http://aa.usno.navy.mil/software/novas/novas_c/novasc3.1.tar.gz
	tar -xzf novasc3.1.tar.gz
	echo "Patching NOVAS-C"
	pushd Cdist
	patch -p1 < ../support/novasc3.1-linux64.patch
	cp ../support/Makefile.Cdist Makefile
	for p in jplbin.h jpleph.h jplint.h jpleph.c jplint.c; do
		cp ../support/$p .
	done
	popd
	touch .Cdist.is_patched
fi
echo "Reading DE430 ephemeris"

if [[ ! -f $DE430_FILE ]] || \
	   [[ "$(sha256sum $DE430_FILE | cut -f1 -d' ')" != \
		"0deb23ca9269496fcbab9f84bec9f3f090e263bfb99c62428b212790721de126" ]]; then
	echo "Downloading $DE430_FILE"
	curl $MY_CURLOPTS -O ftp://ssd.jpl.nasa.gov/pub/eph/planets/Linux/de430/$DE430_FILE
fi
if (( WANT_DE431 != 0 )); then
	if [[ ! -f $DE431_FILE ]] || \
		[[ "$(sha256sum $DE431_FILE | cut -f1 -d' ')" != \
		"fe3d0323d26ada11f8d8228fda9ca590c7eb00cee8b22dff1839f74f5be71149" ]]; then
		echo "Downloading $DE431_FILE"
		curl $MY_CURLOPTS -O ftp://ssd.jpl.nasa.gov/pub/eph/planets/Linux/de431/$DE431_FILE
	fi
fi
mkdir -p $ASTRO_PATH
if [[ -f $JPLEPH_LINK ]] && [[ ! -s $JPLEPH_LINK ]]; then
	rm -f $JPLEPH_LINK
fi
if (( WANT_DE431 != 0 )); then
	ln -sf $(pwd)/$DE431_FILE $JPLEPH_LINK
else
	ln -sf $(pwd)/$DE430_FILE $JPLEPH_LINK
fi
echo "Done"
# vim:set ts=4 sts=4 sw=4 cindent noexpandtab:
