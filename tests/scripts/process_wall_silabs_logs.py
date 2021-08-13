import re
import matplotlib.pyplot as plt

distances = ['1m', '2m'] # ['1m', '2m']
colors = ["b", "r"]
signals = ['chan_disabled']  # ['minus20db', 'minus10db', '4db']
# times = {
#     "4db": [("00:39", "10:41"), ("11:07", "21:14")],
#     "minus10db": [("02:37", "12:40"), ("13:03", "23:05")],
#     "minus20db": [("01:00", "11:02"), ("11:28", "21:30")],
# }

times = {
    "chan_disabled": [("00:45", "10:47"), ("11:16", "22:00")]
}


def main():
    data = {}
    for signal in signals:
        temp = {}
        for distance in distances:
            temp[distance] = []
        data[signal] = temp
    for signal in signals:
        with open(f"silabs_wall/{signal}.txt", "r") as f:
            contents = f.read()
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

    # data is now a dict containing three elements (minus20db, minus10db, 4db)
    # each bucket should be its own graph

    # each bucket contains 2 elements (1m, 2m)
    # want to plot each one of these as a series
    for signal_strength, graph_data in data.items():
        plt.figure()
        plt.title(f"Distance vs RSSI through wall at {signal_strength} for {distance}")
        plt.xlabel("Time (s)")
        plt.ylabel("RSSI (dBm)")
        plt.xlim(0, 600)
        plt.ylim(-110, -30)
        for distance, series in graph_data.items():
            base_time = convert_time_to_ms(series[0].split(" ")[0].split(":")[1:])
            time_series = []
            rssi_series = []
            for el in series:
                vals = el.split(" ")
                time = convert_time_to_ms(vals[0].split(":")[1:]) - base_time
                rssi = int(vals[1])
                time_series.append(time)
                rssi_series.append(rssi)
            # plot this
            print(len(time_series))
            plt.scatter(time_series, rssi_series, label=distance, s=1)
        plt.legend()
        plt.show()

    # buckets = {}
    # for signal in signals:
    #     bucket = {
    #         "times": []
    #     }
    #     for distance in distances:
    #         bucket[distance] = []
    #     buckets[signal] = bucket

    # for signal, val1 in data.items():
    #     for dists, val2 in val1.items():
    #         base_time = convert_time_to_ms(val2[0].split(" ")[0].split(":")[1:3])
    #         temp = [f"{round(convert_time_to_ms(el.split(' ')[0].split(':')[1:3]) - base_time, 3)} {el.split(' ')[1]}" for el in val2
    #                 if round(convert_time_to_ms(el.split(' ')[0].split(':')[1:3]) - base_time, 3) < 600]
    #         time_series = [el.split(" ")[0] for el in temp]
    #         rssi_series = [el.split(" ")[1] for el in temp]
    #         buckets[signal]["times"].extend(time_series)
    #         buckets[signal][dists] = rssi_series


def get_rssi(line):
    return int(line.split(" ")[-1])


def get_time(line):
    return convert_time_to_s(get_time_from_line(line))


def get_time_from_line(line):
    time = line.split(" ")[1]
    time_arr = time.split(":")[1:3]
    return ":".join(time_arr).split(".")[0]


def get_relevant_data(line):
    parts = line.split(" ")
    return parts[1] + " " + parts[-1]


def contains_dongle_track(line):
    return "dongle_track" in line


def check_line_not_empty(line):
    return line != ""


def regex_match(line):
    pattern = re.compile(
        "\[I\] 0:[0-9][0-9]:([0-9][0-9].[0-9][0-9][0-9]) ([0-9])* \.\./src/dongle.c:685 :dongle_track: dongle_track -([0-9][0-9])")
    if pattern.match(line):
        return True
    else:
        return False


def get_filenames():
    names = []
    for signal in signals:
        name = f"silabs_wall/{signal}.txt"
        names.append(name)
    return names


def convert_time_to_ms(time):
    time_arr = [float(t) for t in time]
    return 60 * time_arr[0] + time_arr[1]


def convert_time_to_s(time):
    time_arr = time.split(":")
    time_arr = [int(time) for time in time_arr]
    return 60 * time_arr[0] + time_arr[1]


if __name__ == "__main__":
    main()
