# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/home/laroye/esp/esp-idf/components/bootloader/subproject"
  "/home/laroye/Projects/solo/ESP32/luminous_alarm/build/bootloader"
  "/home/laroye/Projects/solo/ESP32/luminous_alarm/build/bootloader-prefix"
  "/home/laroye/Projects/solo/ESP32/luminous_alarm/build/bootloader-prefix/tmp"
  "/home/laroye/Projects/solo/ESP32/luminous_alarm/build/bootloader-prefix/src/bootloader-stamp"
  "/home/laroye/Projects/solo/ESP32/luminous_alarm/build/bootloader-prefix/src"
  "/home/laroye/Projects/solo/ESP32/luminous_alarm/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/laroye/Projects/solo/ESP32/luminous_alarm/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/laroye/Projects/solo/ESP32/luminous_alarm/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
