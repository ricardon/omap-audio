#!/bin/sh

set -e

touch arch/arm/boot/dts/*omap*
touch arch/arm/boot/dts/*twl*

kmake omap4-panda.dtb
kmake omap4-sdp.dtb
kmake uImage modules && kmake uImage-dtb.omap4-panda uImage-dtb.omap4-sdp

