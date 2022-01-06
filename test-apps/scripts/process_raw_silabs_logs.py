import re

signals = ["ext_minus20db", "ext_minus10db", "ext_4db"]
distances = ["5.0m", "6.0m", "7.0m", "8.0m", "9.0m", "10.0m"]
times = {
    "ext_4db": [("0:0:22", "0:10:30"), ("0:11:02", "0:21:04"), ("0:21:36", "0:32:44"), ("0:33:22", "0:43:24"),
                ("0:44:00", "0:54:02"), ("0:54:46", "1:04:58")],
    "ext_minus10db": [("0:0:20", "0:10:22 "), ("0:10:48", "0:20:50"), ("0:21:26", "0:31:28"), ("0:32:02", "0:42:04"),
                      ("0:42:39", "0:52:41"), ("0:53:34", "1:03:36")],
    "ext_minus20db": [("0:0:16", "0:10:18"), ("0:10:48", "0:20:50"), ("0:21:24", "0:34:11"), ("0:36:17", "0:46:36"),
                      ("0:47:51", "0:57:58"), ("0:58:40", "1:08:42")]
}


# distances = ["0.5m", "1.0m", "2.0m", "4.0m"]
# times = {
#     "4db": [("06:30", "16:52"), ("17:15", "27:24"), ("28:11", "38:51"), ("39:17", "50:10")],
#     "minus10db": [("00:11", "10:14"), ("10:33", "20:35"), ("20:47", "31:11"), ("31:34", "42:47")],
#     "minus20db": [("00:17", "10:18"), ("10:34", "20:35"), ("20:47", "30:50"), ("31:09", "42:00")],
#     "minus40db": [("00:28", "13:00"), ("13:11", "23:14"), ("23:35", "34:13"), ("34:24", "45:00")]
# }


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

    for signal, val1 in data.items():
        for dists, val2 in val1.items():
            with open(f"silabs/{dists}_{signal[4:]}.txt", "w") as f:
                base_time = convert_time_to_ms(val2[0].split(" ")[0].split(":"))
                temp = [
                    f"{round(convert_time_to_ms(el.split(' ')[0].split(':')) - base_time, 3)} {el.split(' ')[1]}"
                    for el in val2
                    if round(convert_time_to_ms(el.split(' ')[0].split(':')) - base_time, 3) < 600]
                f.write("\n".join(temp))

            #     temp = [f"{round(convert_time_to_ms(el.split(' ')[0].split(':')[1:3]) - base_time, 3)} {el.split(' ')[1]}" for el in val2
            #             if round(convert_time_to_ms(el.split(' ')[0].split(':')[1:3]) - base_time, 3) < 600]
            #     time_series = [el.split(" ")[0] for el in temp]
            #     rssi_series = [el.split(" ")[1] for el in temp]
            #     buckets[signal]["times"].extend(time_series)
            #     buckets[signal][dists] = rssi_series

    # create buckets (this turns the data into a csv file. we don't want that for now because
    # what we want is to turn this data into different files to concatenate them with the extended experiment data)
    # buckets = {}
    # for signal in signals:
    #     bucket = {
    #         "times": []
    #     }
    #     for distance in distances:
    #         bucket[distance] = []
    #     buckets[signal] = bucket
    #
    # # standardize times and write to buckets
    # for signal, val1 in data.items():
    #     for dists, val2 in val1.items():
    #         with open(f"silabs/{dists}_{signal}.txt", "w") as f:
    #             base_time = convert_time_to_ms(val2[0].split(" ")[0].split(":")[1:3])
    #             temp = [f"{round(convert_time_to_ms(el.split(' ')[0].split(':')[1:3]) - base_time, 3)} {el.split(' ')[1]}" for el in val2
    #                     if round(convert_time_to_ms(el.split(' ')[0].split(':')[1:3]) - base_time, 3) < 600]
    #             time_series = [el.split(" ")[0] for el in temp]
    #             rssi_series = [el.split(" ")[1] for el in temp]
    #             buckets[signal]["times"].extend(time_series)
    #             buckets[signal][dists] = rssi_series
    #
    # enable_write = 1
    # if enable_write:
    #     with open("silabs_telemetry.csv", "w") as f:
    #         writer = csv.writer(f)
    #         headers = []
    #         for signal in signals:
    #             header_group = [f"{signal} RSSI {dist} (dBm)" for dist in distances]
    #             header_group.insert(0, f"{signal} Time (ms)")
    #             headers.extend(header_group)
    #         writer.writerow(headers)
    #         while buckets != {}:
    #             row = []
    #             for signal in signals:
    #                 if buckets[signal] == {}:
    #                     row.extend(['N/A'])
    #                     row.extend(['127' for _ in distances])
    #                 else:
    #                     idx = 2 + len(distances) - len(buckets[f"{signal}"].keys())
    #                     group = [buckets[f"{signal}"]["times"].pop(0)]
    #                     group.extend(['127' for _ in distances])
    #                     group[idx] = buckets[f"{signal}"][distances[idx - 1]].pop(0)
    #                     if not buckets[f"{signal}"][distances[idx - 1]]:  # all empty
    #                         del buckets[f"{signal}"][distances[idx - 1]]
    #                     if not buckets[f"{signal}"]["times"]:
    #                         buckets[f"{signal}"] = {}
    #                     row.extend(group)
    #             writer.writerow(row)
    #             should_stop = True
    #             for sign in buckets:
    #                 if buckets[sign] != {}:
    #                     should_stop = False
    #             if should_stop:
    #                 buckets = {}


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
    time_arr = time.split(":")
    return ":".join(time_arr).split(".")[0]


def convert_time_to_ms(time):
    time_arr = [float(t) for t in time]
    return 3600 * time_arr[0] + 60 * time_arr[1] + time_arr[2]


def convert_time_to_s(time):
    time_arr = time.split(":")
    time_arr = [int(time) for time in time_arr]
    return 3600 * time_arr[0] + 60 * time_arr[1] + time_arr[2]


if __name__ == "__main__":
    main()
