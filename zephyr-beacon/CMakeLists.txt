# Find Zephyr. This also loads Zephyr's build system.
cmake_minimum_required(VERSION 3.13.1)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(pancast_beacon)

zephyr_include_directories(./ ./src)
# Add your source file to the "app" target. This must come after
# find_package(Zephyr) which defines the target.
target_sources(app PRIVATE src/constants.h)
target_sources(app PRIVATE src/log.h)
target_sources(app PRIVATE src/settings.h)
target_sources(app PRIVATE src/stats.h)
target_sources(app PRIVATE src/util.h)
target_sources(app PRIVATE src/test.h)
target_sources(app PRIVATE src/beacon.h)
target_sources(app PRIVATE src/beacon.c)
target_sources(app PRIVATE src/storage.h)
target_sources(app PRIVATE src/storage.c)
target_sources(app PRIVATE sha-2/sha-256.h)
target_sources(app PRIVATE sha-2/sha-256.c)
target_sources(app PRIVATE src/led.h)
target_sources(app PRIVATE src/led.c)
target_sources(app PRIVATE src/button.h)
target_sources(app PRIVATE src/button.c)
