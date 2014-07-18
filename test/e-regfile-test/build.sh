#!/bin/bash

set -e

ESDK=${EPIPHANY_HOME}
ELIBS=${ESDK}/tools/host/lib
EINCS="-I ${ESDK}/tools/host/include -I ${ESDK}/tools/host/include/uapi"
ELDF=${ESDK}/bsps/current/internal.ldf

SCRIPT=$(readlink -f "$0")
EXEPATH=$(dirname "$SCRIPT")
cd $EXEPATH

# Create the binaries directory
mkdir -p bin/

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

# Build HOST side application
${CROSS_PREFIX}gcc src/e-regfile-test.c -o bin/e-regfile-test.elf ${EINCS} -L ${ELIBS} -le-hal -le-loader -lpthread

