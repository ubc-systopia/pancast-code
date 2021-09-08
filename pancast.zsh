PANCAST_HOME="$HOME/Work/pancast"
ZEPHYR="$PANCAST_HOME/zephyrproject/zephyr"
ZEPHYR_OUT="$ZEPHYR/build/zephyr"
DEVICE_INFO="$PANCAST_HOME/pancast-code/configs"

build () {
	tmp="$(pwd)"
	cd "$ZEPHYR"
	west build -p auto -b nrf52dk_nrf52832 "$PANCAST_HOME/pancast-code/$1" -- -Wno-dev -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
	cd "$tmp"
	built="./$1.hex"
	echo "created $built"
	cp "$ZEPHYR_OUT/zephyr.hex" "$built"
	if [[ ! -z "$2" ]]; then
		echo "device id specified"
		devName="${1}_$2"
		echo "creating $devName"
		config="$DEVICE_INFO/${devName}_config.hex"
		if [[ -f "$config" ]]; then
			mergehex -m "$built" "$config" -o "$devName.hex"
		else
			echo "no hex file for device"
		fi
	fi
}

deploy () {
#	build "$1"
	if 		[[ "$1" == "beacon" ]]; then
			sn=682013321
	elif	[[ "$1" == "terminal" ]]; then
			sn=682013321
	elif	[[ "$1" == "dongle" ]]; then
			sn=682213153
	fi
	nrfjprog="/usr/local/bin//nrfjprog"
	remote_ip="10.0.0.16"
	remote_user="rlindsay"
	remote_dir="/Users/$remote_user/tmp"
	scp "$1.hex" "$remote_user@$remote_ip":"$remote_dir/$1.hex"
	if [[ $? != 0 ]]; then
		return 1
	fi
	ssh "$remote_user@$remote_ip" "'$nrfjprog' --program '$remote_dir/$1.hex' --sectoranduicrerase -f NRF52 --snr $sn && '$nrfjprog' --snr $sn --reset	"
}

flash () {
	if 		[[ "$1" == "beacon" ]]; then
			sn=682213153
	elif	[[ "$1" == "terminal" ]]; then
			sn=682013321
	elif	[[ "$1" == "dongle" ]]; then
			sn=682213153
	fi
   nrfjprog --program $1.hex --sectoranduicrerase -f NRF52 --snr $sn && nrfjprog --snr $sn --reset 
}

