# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/home/zenithtek/esp/esp-idf/components/bootloader/subproject"
  "/media/zenithtek/Windows/Users/jnana/OneDrive - Zenith Tek/Projects/FY_2026-27/Home_Automation/home_automation/build/bootloader"
  "/media/zenithtek/Windows/Users/jnana/OneDrive - Zenith Tek/Projects/FY_2026-27/Home_Automation/home_automation/build/bootloader-prefix"
  "/media/zenithtek/Windows/Users/jnana/OneDrive - Zenith Tek/Projects/FY_2026-27/Home_Automation/home_automation/build/bootloader-prefix/tmp"
  "/media/zenithtek/Windows/Users/jnana/OneDrive - Zenith Tek/Projects/FY_2026-27/Home_Automation/home_automation/build/bootloader-prefix/src/bootloader-stamp"
  "/media/zenithtek/Windows/Users/jnana/OneDrive - Zenith Tek/Projects/FY_2026-27/Home_Automation/home_automation/build/bootloader-prefix/src"
  "/media/zenithtek/Windows/Users/jnana/OneDrive - Zenith Tek/Projects/FY_2026-27/Home_Automation/home_automation/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/media/zenithtek/Windows/Users/jnana/OneDrive - Zenith Tek/Projects/FY_2026-27/Home_Automation/home_automation/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/media/zenithtek/Windows/Users/jnana/OneDrive - Zenith Tek/Projects/FY_2026-27/Home_Automation/home_automation/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
