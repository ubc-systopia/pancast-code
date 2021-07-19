#!/bin/bash

echo "Running automated raspi-config tasks"

export LANGUAGE=en_CA.UTF-8
export LANG=en_CA.UTF-8

# Via https://gist.github.com/damoclark/ab3d700aafa140efb97e510650d9b1be
# Execute the config options starting with 'do_' below
grep -E -v -e '^\s*#' -e '^\s*$' <<END | \
sed -e 's/$//' -e 's/^\s*/\/usr\/bin\/raspi-config nonint /' | bash -x -
#

# Drop this file in SD card root. After booting run: sudo /boot/setup.sh
# --- Begin raspi-config non-interactive config option specification ---

# Hardware Configuration
do_boot_wait 1            # Turn on waiting for network before booting
do_boot_splash 1          # Disable the splash screen
#do_overscan 0             # Enable overscan
#do_camera 0               # Enable the camera
do_ssh 1                  # Enable remote ssh login

# System Configuration
do_configure_keyboard ca
do_hostname ${host}
do_change_timezone Canada/Vancouver
do_change_locale LANG=en_US.UTF-8

# Don't add any raspi-config configuration options after 'END' line below & don't remove 'END' line
END

echo "Installing curllib"
sudo apt-get install libcurl4-openssl-dev

echo "Installing pthreadlib"
sudo apt-get install libpthread-stubs0-dev

echo "Exporting GPIO"
echo 24 >/sys/class/gpio/export

echo "Restarting to apply changes. After run ssh pi@${host}.local"
# Reboot after all changes above complete
/sbin/shutdown -r now
