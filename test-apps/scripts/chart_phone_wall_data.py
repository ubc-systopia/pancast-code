import numpy as np
import matplotlib.pyplot as plt

distances = ['1m', '2m']
colors = ["b", "r"]
signals = ['minus20db', 'minus10db', '4db']
folders = ['google_wall', 'samsung_wall']


def main():
    data = {}
    for folder in folders:
        temp = {}
        for signal in signals:
            temp2 = {}
            for distance in distances:
                temp2[distance] = {'time': [], 'rssi': []}
            temp[signal] = temp2
        data[folder] = temp

    print(len(data['google_wall']['minus10db']['1m']['time']))

    for folder in folders:
        for signal in signals:
            for distance in distances:
                with open(f"{folder}/{signal}/{distance}.txt") as f:
                    contents = f.read()
                    elements = contents.splitlines()
                    if len(elements) != 0:
                        start_time = get_time_in_s(elements[0])
                    else:
                        start_time = 0
                    end_time = start_time + 600
                    filtered_entries = [x for x in elements if is_valid_time(start_time, x, end_time)]
                    components = [get_time_and_rssi(start_time, x) for x in filtered_entries]
                    time_series = [row[0] for row in components]
                    rssi_series = [row[1] for row in components]
                    data[folder][signal][distance]['time'] = time_series
                    data[folder][signal][distance]['rssi'] = rssi_series

    for folder, graph_data_major in data.items():
        for signal, graph_data_minor in graph_data_major.items():
            plt.figure()
            plt.title(f"Distance vs RSSI through wall at {signal} for {folder}")
            plt.xlabel("Time (s)")
            plt.ylabel("RSSI (dBm)")
            plt.xlim(0, 600)
            plt.ylim(-110, -30)
            for distance, series in graph_data_minor.items():
                time_series = []  # default values in case something goes wrong
                rssi_series = []  # yanno, to persuade the compiler :)
                for data_type, data_list in series.items():
                    if data_type == "time":
                        time_series = data_list
                    elif data_type == "rssi":
                        rssi_series = data_list
                plt.scatter(time_series, rssi_series, label=distance)
            plt.legend()
            plt.show()


def get_time_in_s(entry):
    time = entry.split(" ")[1].split(":")
    time = [int(time[0]), int(time[1]), float(time[2])]
    return time[0] * 3600 + time[1] * 60 + time[2]


def get_time_and_rssi(start_time, line):
    parts = line.split()
    time = get_time_in_s(line) - start_time
    rssi = int(parts[6][1:-1])
    return [time, rssi]


def is_valid_time(start_time, el, end_time):
    time = get_time_in_s(el)
    return start_time <= time <= end_time


if __name__ == "__main__":
    main()
