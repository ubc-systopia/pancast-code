# Find Zephyr. This also loads Zephyr's build system.
cmake_minimum_required(VERSION 3.13.1)
find_package(Zephyr)
project(pancast_beacon)


# Add your source file to the "app" target. This must come after
# find_package(Zephyr) which defines the target.
target_sources(app PRIVATE src/beacon.h)
target_sources(app PRIVATE src/beacon.c)
target_sources(app PRIVATE src/storage.c)