DE ?= 440
ASTRO_PATH = ~/.astro
JPLEPH_LINK = $(ASTRO_PATH)/JPLEPH
NOVAS_URL = https://aa.usno.navy.mil/software/novas/novas_c/novasc3.1.tar.gz
NOVAS_TGZ := $(shell basename $(NOVAS_URL))
CURLOPTS = --connect-timeout 20 --no-keepalive
ifeq ($(DE),440)
DE_URL := ftp://ssd.jpl.nasa.gov/pub/eph/planets/Linux/de430/linux_p1550p2650.440
DE_CKSUM = 29915576d0a6555766b99485ac3056ee415e86df4fce282611c31afb329ad062
else
ifeq ($(DE),430)
DE_URL := ftp://ssd.jpl.nasa.gov/pub/eph/planets/Linux/de430/linux_p1550p2650.430
DE_CKSUM = 0deb23ca9269496fcbab9f84bec9f3f090e263bfb99c62428b212790721de126
else
ifeq ($(DE),431)
DE_URL := ftp://ssd.jpl.nasa.gov/pub/eph/planets/Linux/de431/lnxm13000p17000.431
DE_CKSUM = fe3d0323d26ada11f8d8228fda9ca590c7eb00cee8b22dff1839f74f5be71149
else
$(error Unsupported DE)
endif
endif
endif
DE_FILE := $(shell basename $(DE_URL))

.PHONY: astro_path clean veryclean

all: $(JPLEPH_LINK) .Cdist.is_patched
	make -C Cdist && \
	make -C ephutil && \
	make -C planets && \
	make -C tropical

clean:
	make -C ephutil clean && \
	make -C planets clean && \
	make -C tropical clean

veryclean: clean
	rm -f .Cdist.is_patched $(DE_FILE) $(JPLEPH_LINK) $(NOVAS_TGZ) && \
	rm -fr Cdist

astro_path:
	mkdir -p $(ASTRO_PATH)

$(DE_FILE):
	@echo "Downloading $@"
	curl $(CURLOPTS) -O $(DE_URL)
	@if [ ! -f $@ ]; then \
		echo "Problem downloading $@" && false; \
	elif [ "$$(sha256sum $(DE_FILE) | cut -f1 -d' ')" != "$(DE_CKSUM)" ]; then \
		echo "$@ is corrupted" && false; \
	fi

$(JPLEPH_LINK): astro_path $(DE_FILE)
	ln -sf $$(readlink --canonicalize $(DE_FILE)) $@

.Cdist.is_patched: $(NOVAS_TGZ)
	tar -xzf $(NOVAS_TGZ)
	@if [ ! -d Cdist ]; then \
		echo "Problem untarring $(NOVAS_TGZ)" && false; \
	else \
		echo "Patching NOVAS-C"; \
		cd Cdist && \
		patch -p1 < ../support/novasc3.1-linux64.patch && \
		cp ../support/Makefile.Cdist Makefile && \
		cd .. && \
		touch $@; \
	fi

$(NOVAS_TGZ):
	@echo "Reading NOVAS-C"
	curl $(CURLOPTS) -O $(NOVAS_URL)
	@if [ ! -f $@ ]; then \
		echo "Problem downloading $@" && false; \
	fi

