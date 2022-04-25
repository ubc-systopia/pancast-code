#!/usr/local/bin/python3

import sys, os
import ssl
import http.client
import json
import argparse

DEFAULT_SERVER_IP = "127.0.0.1"
DEFAULT_SERVER_PORT = 8081

# ephemeral ID
IDX_EPHID = 0
# beacon identifier
IDX_BEACON_ID = 1
# beacon's location identifier
IDX_LOCATION_ID = 2
# index of the encounter entry in dongle's log datastructure
IDX_LOG_ENTRY_CNT = 3
# beacon's clock in the first record of this encounter
IDX_BEACON_CLOCK = 4
# duration w.r.t. beacon's clock until the last record of this encounter
IDX_BEACON_INTERVAL = 5
# dongle's clock in the first record of this encounter
IDX_DONGLE_CLOCK = 6
# duration w.r.t. dongle's clock until the last record of this encounter
IDX_DONGLE_INTERVAL = 7
# RSSI value of the bluetooth signal over which encounter received
IDX_RSSI = 8


def init_conn(server_ip, server_port):
    try:
        _create_unverified_https_context = ssl._create_unverified_context
    except AttributeError:
        # Legacy Python that doesn't verify HTTPS certificates by default
        pass
    else:
        # Handle target environment that doesn't support
        # HTTPS verification
        ssl._create_default_https_context = _create_unverified_https_context

    conn = http.client.HTTPSConnection(
            "{}:{}".format(server_ip, server_port))

    if (conn):
#        print("{!s}".format(conn))
        print("Connected to server {}:{}".format(server_ip, server_port))

#    conn.request("GET", "/")
#    r1 = conn.getresponse()
#    print(r1.status, r1.reason)

    return conn


def parse_input(ifile):
    f = open(ifile, "r")
    buf = f.readlines()
    f.close()

    enctr_raw_arr = []
    i = len(buf) - 1
    start = 0
    done = 0
    while (i >= 0):
        line = buf[i].strip('\r').strip('\n')
        i -= 1

        if ("UPLOAD LOG START" in line):
            start = 1
            j = i+3
            while (j < len(buf)):
                line = buf[j].strip('\r').strip('\n')
                j += 1

                if ("read" not in line):
                    continue

                if ("UPLOAD LOG END" in line):
#                    print("line {}: {}".format(j-1, line))
                    done = 1
                    break

                enctr_raw_arr.append(line.split(' ')[2:])

        if (done == 1):
            break

    print("# encounters: {}".format(len(enctr_raw_arr)))
#    print(enctr_raw_arr[0])
#    print(enctr_raw_arr[-1])

    return enctr_raw_arr


def upload(enctr_raw_arr):
    json_data_arr = []
    for i in range(len(enctr_raw_arr)):
        '''
        data = {}
        data['EphemeralID'] = 'deadbeeffeedbeadaddadeaf987654'
        data['DongleClock'] = 22
        data['BeaconClock'] = 135
        data['BeaconId'] = 572653569
        data['LocationID'] = 572653569
        json_data_arr.append(data)

        data = {}
        data['EphemeralID'] = 'deadbeeffeedbeadaddadeaf987655'
        data['DongleClock'] = 24
        data['BeaconClock'] = 145
        data['BeaconId'] = 572653569
        data['LocationID'] = 572653569
        json_data_arr.append(data)
        '''

        data = {}
        data['EphemeralID'] = enctr_raw_arr[i][IDX_EPHID]
        data['BeaconID'] = int(enctr_raw_arr[i][IDX_BEACON_ID], 16)
        data['LocationID'] = int(enctr_raw_arr[i][IDX_BEACON_ID], 16)
        data['DongleClock'] = int(enctr_raw_arr[i][IDX_DONGLE_CLOCK])
        data['BeaconClock'] = int(enctr_raw_arr[i][IDX_BEACON_CLOCK])
        json_data_arr.append(data)


    reqbuf = {}
    reqbuf['Entries'] = json_data_arr
    reqbuf['Type'] = 0
    reqbuf2 = json.dumps(reqbuf)
    print("JSON buf size: {}".format(len(reqbuf2)))

    return reqbuf2


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Dongle uploader")
    parser.add_argument("-s", "--serverip", dest="server_ip",
            default=DEFAULT_SERVER_IP,
            help="IP address of Pancast server")
    parser.add_argument("-p", "--serverport", dest="server_port",
            default=DEFAULT_SERVER_PORT,
            help="Port of Pancast server")
    parser.add_argument("-i", "--ifile", dest="ifile",
            help="Input encounter log file")

    args = parser.parse_args()
    server_ip = args.server_ip
    server_port = args.server_port
    ifile = args.ifile

    enctr_raw_arr = parse_input(ifile)
    reqbuf2 = upload(enctr_raw_arr)

    conn = init_conn(server_ip, server_port)

    conn.request("GET", "/upload", reqbuf2)
    r2 = conn.getresponse()
    print(r2.status, r2.reason)
