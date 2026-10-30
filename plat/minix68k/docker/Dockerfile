# The ACK for minix68k (Minix-ST 2.0.4 on the rosco_m68k: 68010, 16-bit int)
# in a container: cross-compile C and BASIC for the board from Linux.
#
#	docker build -t ack-minix68k plat/minix68k/docker
#	docker build --build-arg ACK_REV=<tag or commit> -t ack-minix68k:<tag> \
#		plat/minix68k/docker
#	docker run --rm -v "$PWD":/src --user "$(id -u):$(id -g)" ack-minix68k \
#		ack -mminix68k -O -o prog prog.c
#	docker run --rm -v "$PWD":/src ack-minix68k minix68k-emu prog
#
# The image clones this repository at ACK_REV, builds it and runs its minix68k
# tests (a few minutes). The whole source and build tree stays in /opt/ack:
# the minix project's mkpass.py reads /opt/ack/.obj/build.ninja to build the
# passes as Minix programs. ACKDIR points at the staging tree, so ack finds
# its passes and libraries; minix68k-emu is the emulator the tests use.

FROM debian:trixie-slim@sha256:f6b08b09a05725f9431502d3b5714f5c4576e82c1a84fa1debe0c24a7e0c3ec4

ARG ACK_URL=https://github.com/maarten-boot/ack.git
ARG ACK_REV=minix68k-20261030

RUN apt-get update \
	&& apt-get install -y --no-install-recommends \
		bison ca-certificates flex gcc git libc6-dev lua5.4 make \
		ninja-build python3 \
	&& rm -rf /var/lib/apt/lists/*

RUN git clone -q "$ACK_URL" /opt/ack \
	&& git -C /opt/ack checkout -q "$ACK_REV" \
	&& make -C /opt/ack -j"$(nproc)" LUA=lua5.4 \
	&& ln -s /opt/ack/.obj/plat/minix68k/emu/+emu/emu /usr/local/bin/minix68k-emu

ENV ACKDIR=/opt/ack/.obj/staging
ENV PATH=/opt/ack/.obj/staging/bin:$PATH

WORKDIR /src
