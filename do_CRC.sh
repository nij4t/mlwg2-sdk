#!/bin/sh
cd CRC_tool
cp ../images/root_uImage ./
./CRC_tool root_uImage WMTM-171GN
mv ./CRC_root_uImage ../images/
rm -f root_uImage
ver=`grep " VER" ../user/gt_utilities/version.h | cut -d '"' -f 2`
mv ../images/CRC_root_uImage ../images/Kingston_widrive_v$ver.bin
chmod 777 ../images/Kingston_widrive_v$ver.bin
cp ../images/Kingston_widrive_v$ver.bin ../images/mlw5200fw_v$ver.bin
cp ../images/Kingston_widrive_v$ver.bin ../images/mlwG2_v$ver.bin
