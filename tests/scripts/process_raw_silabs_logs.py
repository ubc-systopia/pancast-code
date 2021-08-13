import re
import csv

signals = ["minus40db", "minus20db", "minus10db", "4db"]
distances = ["0.5m", "1.0m", "2.0m", "4.0m"]
times = {
    "4db": [("06:30", "16:52"), ("17:15", "27:24"), ("28:11", "38:51"), ("39:17", "50:10")],
    "minus10db": [("00:11", "10:14"), ("10:33", "20:35"), ("20:47", "31:11"), ("31:34", "42:47")],
    "minus20db": [("00:17", "10:18"), ("10:34", "20:35"), ("20:47", "30:50"), ("31:09", "42:00")],
    "minus40db": [("00:28", "13:00"), ("13:11", "23:14"), ("23:35", "34:13"), ("34:24", "45:00")]
}


def main():
    # initialize data bucket
    data = {}
    for signal in signals:
        temp = {}
        for distance in distances:
            temp[distance] = []
        data[signal] = temp
    # read files
    for signal in signals:
        file = open("silabs_raw/" + signal + ".txt")
        contents = file.read()
        elements = contents.splitlines()
        elements = list(filter(contains_dongle_track, elements))
        elements = list(filter(check_line_not_empty, elements))
        elements = list(filter(regex_match, elements))

        for dist_pos, time_range in enumerate(times[signal]):
            for element in elements:
                if convert_time_to_s(time_range[0]) <= get_time(element) <= convert_time_to_s(time_range[1]):
                    # this entry belongs to this time range
                    dist = distances[dist_pos]
                    data[signal][dist].append(get_relevant_data(element))

    # create buckets
    buckets = {}
    for signal in signals:
        bucket = {
            "times": []
        }
        for distance in distances:
            bucket[distance] = []
        buckets[signal] = bucket

    # standardize times and write to buckets
    for signal, val1 in data.items():
        for dists, val2 in val1.items():
            with open(f"silabs/{dists}_{signal}.txt", "w") as f:
                base_time = convert_time_to_ms(val2[0].split(" ")[0].split(":")[1:3])
                temp = [f"{round(convert_time_to_ms(el.split(' ')[0].split(':')[1:3]) - base_time, 3)} {el.split(' ')[1]}" for el in val2
                        if round(convert_time_to_ms(el.split(' ')[0].split(':')[1:3]) - base_time, 3) < 600]
                time_series = [el.split(" ")[0] for el in temp]
                rssi_series = [el.split(" ")[1] for el in temp]
                buckets[signal]["times"].extend(time_series)
                buckets[signal][dists] = rssi_series

    enable_write = 1
    if enable_write:
        with open("silabs_telemetry.csv", "w") as f:
            writer = csv.writer(f)
            headers = []
            for signal in signals:
                header_group = [f"{signal} RSSI {dist} (dBm)" for dist in distances]
                header_group.insert(0, f"{signal} Time (ms)")
                headers.extend(header_group)
            writer.writerow(headers)
            while buckets != {}:
                row = []
                for signal in signals:
                    if buckets[signal] == {}:
                        row.extend(['N/A'])
                        row.extend(['127' for _ in distances])
                    else:
                        idx = 2 + len(distances) - len(buckets[f"{signal}"].keys())
                        group = [buckets[f"{signal}"]["times"].pop(0)]
                        group.extend(['127' for _ in distances])
                        group[idx] = buckets[f"{signal}"][distances[idx - 1]].pop(0)
                        if not buckets[f"{signal}"][distances[idx - 1]]:  # all empty
                            del buckets[f"{signal}"][distances[idx - 1]]
                        if not buckets[f"{signal}"]["times"]:
                            buckets[f"{signal}"] = {}
                        row.extend(group)
                writer.writerow(row)
                should_stop = True
                for sign in buckets:
                    if buckets[sign] != {}:
                        should_stop = False
                if should_stop:
                    buckets = {}

def get_relevant_data(line):
    parts = line.split(" ")
    return parts[1] + " " + parts[-1]


def regex_match(line):
    pattern = re.compile(
        "\[I\] 0:[0-9][0-9]:([0-9][0-9].[0-9][0-9][0-9]) ([0-9])* \.\./src/dongle.c:685 :dongle_track: dongle_track -([0-9][0-9])")
    if pattern.match(line):
        return True
    else:
        return False


def get_rssi(line):
    return int(line.split(" ")[-1])


def get_time(line):
    return convert_time_to_s(get_time_from_line(line))


def contains_dongle_track(line):
    return "dongle_track" in line


def check_line_not_empty(line):
    return line != ""


def get_time_from_line(line):
    time = line.split(" ")[1]
    time_arr = time.split(":")[1:3]
    return ":".join(time_arr).split(".")[0]


def convert_time_to_ms(time):
    time_arr = [float(t) for t in time]
    return 60 * time_arr[0] + time_arr[1]


def convert_time_to_s(time):
    time_arr = time.split(":")
    time_arr = [int(time) for time in time_arr]
    return 60 * time_arr[0] + time_arr[1]


if __name__ == "__main__":
    main()
