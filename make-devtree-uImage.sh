#!/bin/sh

set -e

touch arch/arm/boot/dts/omap4-panda.dts && kmake omap4-panda.dtb
touch arch/arm/boot/dts/omap4-sdp.dts && kmake omap4-sdp.dtb
kmake uImage modules && kmake uImage-dtb.omap4-panda uImage-dtb.omap4-sdp

