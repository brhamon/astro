#!/bin/bash
# Run from the astro directory
get_files() {
# $1 = touchfile
# $2 = local subdirectory
# $3 = remote subdirectory
	local CLIST FLIST=""
	if [[ ! -f $1 ]]; then
		mkdir -p $2
		pushd $2
		echo "Reading $2"
		FLIST="$(curl $MY_CURLOPTS --list-only ftp://ssd.jpl.nasa.gov/$3/)"
		if [[ -z "$FLIST" ]]; then
			echo "Error obtaining $3 directory listing."
			exit 1
		fi
		while [[ -n "$FLIST" ]]; do
			CLIST=""
			for p in $FLIST; do
				if [[ ! -f $p ]]; then
					echo "Need $p"
					CLIST="$CLIST -O ftp://ssd.jpl.nasa.gov/$3/$p"
				fi
			done
			if [[ -z "$CLIST" ]]; then
				break
			fi
			echo $CLIST | xargs curl $MY_CURLOPTS
			CLIST=""
			for p in $FLIST; do
				if [[ ! -f $p ]]; then
					CLIST="$CLIST $p"
				fi
			done
			FLIST="$CLIST"
			if [[ -n "$FLIST" ]]; then
				echo "Retrying for $FLIST"
			fi
		done
		popd
		touch $1
	fi
}

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
	ln -s ../ephemeris/fortran/jplsubs.f
	ln -s ../ephemeris/fortran/fsizer4.c
	popd
	touch .Cdist.is_patched
fi
get_files .ephemeris.fortran ephemeris/fortran pub/eph/planets/fortran
if [[ ! -f .ephemeris.fortran.is_patched ]]; then
	pushd ephemeris/fortran
	if [[ ! -f testeph_orig.f ]]; then
		cp testeph.f testeph_orig.f
		sed -e '/^[ \t]\{1,\}SUBROUTINE PLEPH/,$!d' testeph_orig.f > jplsubs.f
		sed -e '/^[ \t]\{1,\}SUBROUTINE FSIZER1/,$d' testeph_orig.f > testeph.f
		patch -p1 < ../../support/fortran.patch
	fi
	cp ../../support/Makefile.fortran Makefile
	cp ../../support/fsizer4.c .
	popd
	touch .ephemeris.fortran.is_patched
fi
get_files .ephemeris.ascii.de430 ephemeris/ascii/de430 pub/eph/planets/ascii/de430
echo "Done"
# vim:set ts=4 sts=4 sw=4 cindent noexpandtab:
