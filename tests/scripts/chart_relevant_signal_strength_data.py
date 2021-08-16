import matplotlib.pyplot as plt

# Experiment setup for long distance advertisement tests
from constants import distance_permutations, signal_permutations, folders_ext, parameters


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

        time_list = [time_to_milliseconds(get_timestamp(x)) - time_to_milliseconds(start_time) for x in filtered_entries]
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

        data[folder][signal][distance] = tuple_list

    for folder, graph_data_major in data.items():
        for signal_strength, graph_data_minor in graph_data_major.items():
            plt.figure()
            plt.title(f"Distance vs RSSI through wall at {signal_strength} for {folder[:-4]} phone")
            plt.xlabel("Time (s)")
            plt.ylabel("RSSI (dBm)")
            plt.xlim(0, 600)
            plt.ylim(-110, -30)
            for distance, series in graph_data_minor.items():
                time_series = [x[0] for x in series]
                rssi_series = [x[1] for x in series]
                plt.scatter(time_series, rssi_series, label=distance, s=1)
            plt.legend()
            plt.show()




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
    collect_phone_data()
