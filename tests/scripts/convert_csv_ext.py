import csv

# Experiment setup for long distance advertisement tests
from constants import distance_permutations, signal_permutations, folders_ext, parameters


def main():
    filenames = get_all_filenames()
    csv_headers = get_all_headers()
    combined_rssi = []
    combined_time = []
    combined_deltas = []
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

        time_list = [time_to_milliseconds(get_timestamp(x)) for x in filtered_entries]
        if len(time_list) > 0:
            base = time_list[0]
            time_list = [time - base for time in time_list]

        rssi_list = [x.split("TELEMETRY: [")[1].split(",")[0] for x in filtered_entries]
        rssi_list = [int(x) for x in rssi_list]

        # rssi_delta_list = [rssi_list[i + 1] - rssi_list[i] for i in range(len(rssi_list) - 1)]

        combined_rssi.append(rssi_list)
        combined_time.append(time_list)
        # combined_deltas.append(rssi_delta_list)

    # standardize lengths
    max_length = 0
    for rssi_list in combined_rssi:
        if len(rssi_list) > max_length:
            max_length = len(rssi_list)
    for i, _ in enumerate(combined_rssi):
        combined_rssi[i].extend([127 for _ in range(max_length - len(combined_rssi[i]))])
    for i, _ in enumerate(combined_time):
        combined_time[i].extend(['N/A' for _ in range(max_length - len(combined_time[i]))])
    # for i, _ in enumerate(combined_deltas):
    #     combined_deltas[i].extend([999 for _ in range(max_length - len(combined_deltas[i]))])

    for folder in folders_ext:
        with open(f"telemetry_{folder}_data.csv", "w") as f:
            writer = csv.writer(f)
            writer.writerow(csv_headers)
            for i, _ in enumerate(combined_rssi[0]):
                row = []
                for j, _ in enumerate(combined_rssi):
                    row.append(combined_time[j][i])
                    row.append(str(combined_rssi[j][i]))
                    # row.append(combined_deltas[j][i])
                writer.writerow(row)


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
                name = f"{folder}/{signal}/raw_samsung_{distance}.txt"  # temporary change
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
