#!/bin/bash

set -e

ESDK=${EPIPHANY_HOME}
ELIBS="-L ${ESDK}/tools/host/lib"
EINCS="-I ${ESDK}/tools/host/include"
ELDF=${ESDK}/bsps/current/fast.ldf

SCRIPT=$(readlink -f "$0")
EXEPATH=$(dirname "$SCRIPT")
cd $EXEPATH

CROSS_PREFIX=
case $(uname -p) in
	arm*)
		# Use native arm compiler (no cross prefix)
		CROSS_PREFIX=
		;;
	   *)
		# Use cross compiler
		CROSS_PREFIX="arm-linux-gnueabihf-"
		;;
esac

# Get C source files from CTF metadata
barectf -O src ctf/metadata

# Build HOST side application
${CROSS_PREFIX}gcc src/barectf_tracing.c -o Debug/barectf_tracing.elf ${EINCS} ${ELIBS} -le-hal -le-loader -lpthread

# Build DEVICE side program
e-gcc -T ${ELDF} src/e_barectf_tracing.c -o Debug/e_barectf_tracing.elf -le-lib

# Convert ebinary to SREC file
e-objcopy --srec-forceS3 --output-target srec Debug/e_barectf_tracing.elf Debug/e_barectf_tracing.srec

