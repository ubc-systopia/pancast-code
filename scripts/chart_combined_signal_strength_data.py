import matplotlib.pyplot as plt

# Experiment setup for long distance advertisement tests
from constants import distance_permutations, signal_permutations, folders_ext, parameters
import numpy as np
import math
from scipy import stats

distances_silabs = ["0.5m", "1.0m", "2.0m", "4.0m", "5.0m", "6.0m", "7.0m", "8.0m", "9.0m", "10.0m"]
signals_silabs = ["minus20db", "minus10db", "4db"]
z = 1.96  # roughly corresponds to 95% confidence interval


def main():
    phone_data = collect_phone_data()
    silabs_data = collect_silabs_data()
    data = phone_data
    data["silabs_ext"] = silabs_data
    for folder, graph_data_major in data.items():
        for signal, graph_data_minor in graph_data_major.items():
            plt.figure()
            plt.title(f"Mean RSSI and 95% confidence interval for {folder} at {signal}")
            plt.xlim(0, 12)
            plt.ylim(-110, -30)
            plt.ylabel("Mean RSSI (dBm)")
            plt.xlabel("Distance")
            # ci_set = []
            for i, (distance, series) in enumerate(graph_data_minor.items()):
                rssi_series = [x[1] for x in series]
                data_series = np.array(rssi_series)
                mean = data_series.mean()
                std = data_series.std(ddof=1)  # standard normal dist (I think)
                distnum = float(distance[:-1])
                # interval = stats.norm.interval(0.95, loc=mean, scale=std)
                # ci_set.append(interval)
            # print(find_maximum_disjoint_sets(ci_set))
                ci = z * std
                plt.errorbar(distnum, mean, ci, label=distance, fmt="o")
            plt.legend()
            plt.show()


def find_maximum_disjoint_sets(intervals):
    unchosen_intervals = sort_by_uppermost_bound(intervals)
    accept = []
    while len(unchosen_intervals) != 0:
        chosen_interval = unchosen_intervals.pop(0)
        accept.append(chosen_interval)
        unchosen_intervals = [x for x in unchosen_intervals if not has_conflict(chosen_interval, x)]
    return len(accept)


def has_conflict(i1, i2):
    overlap = max(0, min(i1[1], i2[1]) - max(i1[0], i2[0]))
    return overlap != 0


def sort_by_uppermost_bound(intervals):
    interval_copy = intervals.copy()
    interval_copy.sort(key=lambda x: x[1])
    return interval_copy


def collect_phone_data():
    data = initialize_bucket_phone()

    filenames = get_all_filenames()
    for filename in filenames:
        file = open(filename)
        contents = file.read()
        elements = contents.splitlines()
        if len(elements) != 0:
            start_time = get_timestamp(elements[0])
        else:
            start_time = [0, 0, 0]
        end_time = [int(start_time[0]), int(start_time[1]) + 10, float(start_time[2])]
        if int(end_time[1]) > 60:
            end_time[0] += 1
            end_time[1] -= 60
        filtered_entries = [x for x in elements if is_before_time(x, end_time)]

        time_list = [time_to_milliseconds(get_timestamp(x)) - time_to_milliseconds(start_time) for x in
                     filtered_entries]
        if len(time_list) > 0:
            base = time_list[0]
            time_list = [time - base for time in time_list]

        rssi_list = [x.split("TELEMETRY: [")[1].split(",")[0] for x in filtered_entries]
        rssi_list = [int(x) for x in rssi_list]
        tuple_list = [(time_list[i], rssi_list[i]) for i in range(0, len(time_list))]
        filename_parts = filename.split("/")
        folder = filename_parts[0]
        signal = filename_parts[1]
        distance = filename_parts[2].split("_")[2].split(".")[0]
        if distance == "0":
            distance = "0.5m"
        data[folder][signal][distance] = tuple_list
    return data
    # for folder, graph_data_major in data.items():
    #     for signal_strength, graph_data_minor in graph_data_major.items():
    #         plt.figure()
    #         plt.title(f"Distance vs RSSI through wall at {signal_strength} for {folder[:-4]} phone")
    #         plt.xlabel("Time (s)")
    #         plt.ylabel("RSSI (dBm)")
    #         plt.xlim(0, 600)
    #         plt.ylim(-110, -30)
    #         for distance, series in graph_data_minor.items():
    #             time_series = [x[0] for x in series]
    #             rssi_series = [x[1] for x in series]
    #             plt.scatter(time_series, rssi_series, label=distance, s=1)
    #         plt.legend()
    #         plt.show()


def collect_silabs_data():
    data = initialize_bucket_silabs()

    for signal in signals_silabs:
        for distance in distances_silabs:
            filename = f"silabs/{signal}/{distance}.txt"
            with open(filename, "r") as f:
                contents = f.read()
                elements = contents.splitlines()
                element_list = [(float(x.split(" ")[0]), int(x.split(" ")[1])) for x in elements]
                data[signal][distance] = element_list

    # for signal, graph_data in data.items():
    #     plt.figure()
    #     plt.title(f"Distance vs RSSI through wall at {signal} for Silabs dongle")
    #     plt.xlabel("Time (s)")
    #     plt.ylabel("RSSI (dBm)")
    #     plt.xlim(0, 600)
    #     plt.ylim(-110, -30)
    #     for distance, series in graph_data.items():
    #         time_series = [x[0] for x in series]
    #         rssi_series = [x[1] for x in series]
    #         plt.scatter(time_series, rssi_series, label=distance, s=1)
    #     plt.legend()
    #     plt.show()

    return data


def convert_time_to_s(timestamp):
    time_parts = timestamp.split(":")
    time_parts = [float(x) for x in time_parts]
    time = 3600 * time_parts[0] + 60 * time_parts[1] + time_parts[2]
    time = round(time, 3)
    return time


def initialize_bucket_silabs():
    data = {}
    for signal in signals_silabs:
        signal_bucket = {}
        for distance in distances_silabs:
            signal_bucket[distance] = []
        data[signal] = signal_bucket
    return data


def initialize_bucket_phone():
    data = {}
    for folder in folders_ext:
        folder_bucket = {}
        for signal in signal_permutations:
            signal_bucket = {}
            for distance in distance_permutations:
                signal_bucket[distance] = []
            folder_bucket[signal] = signal_bucket
        data[folder] = folder_bucket
    return data


def is_before_time(result, time):
    result_time = get_timestamp(result)
    if result_time[0] < time[0]:
        return True
    if result_time[0] > time[0]:
        return False
    if result_time[1] < time[1]:
        return True
    if result_time[1] > time[1]:
        return False
    if result_time[2] < time[2]:
        return True
    if result_time[2] > time[2]:
        return False
    else:
        return True


def time_to_milliseconds(time) -> float:
    return time[0] * 3600 + time[1] * 60 + time[2]


def get_timestamp(entry):
    time = entry.split(" ")[1].split(":")
    time = [int(time[0]), int(time[1]), float(time[2])]
    return list(time)


def get_all_filenames():
    names = []
    for folder in folders_ext:
        for signal in signal_permutations:
            for distance in distance_permutations:
                # patchwork fix to accommodate collecting data between silabs board and phones in one script
                name = f"{folder}/{signal}/raw_samsung_{distance}.txt"  # temporary change
                if folder == "silabs":
                    name = f"{folder}/{signal}/{distance}.txt"
                names.append(name)
    return names


def get_all_headers():
    headers = []
    for folder in folders_ext:
        for signal in signal_permutations:
            for distance in distance_permutations:
                for data_type in parameters:
                    header = f"{folder}_{signal}_{distance}_{data_type}"
                    headers.append(header)
    return headers


if __name__ == '__main__':
    main()
