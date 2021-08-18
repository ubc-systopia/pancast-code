import csv
import numpy as np
from constants import distance_permutations, signal_permutations, folders_ext, parameters


def main():
    arr = []
    with open("telemetry_google_ext_data.csv", newline="\r\n") as f:
        reader = csv.reader(f, delimiter=',', quotechar="|")
        for row in reader:
            arr.append(row)
    arr.pop(0)  # get rid of header row
    arr = np.array(arr)

    buckets = {}
    for folder in folders_ext:
        for signal in signal_permutations:
            bucket = {
                "times": []
            }
            for distance in distance_permutations:
                bucket[distance] = []
            buckets[f"{folder}_{signal}"] = bucket

    series_as_rows = arr.T
    for i, _ in enumerate(series_as_rows):
        if i % 2 == 0 and series_as_rows[i][0] != 'N/A':
            # this is a nonempty series
            chart_title = find_type_by_index(i)
            time_series = series_as_rows[i]
            rssi_series = series_as_rows[i + 1]
            occurences = np.where(time_series == 'N/A')[0]
            if len(occurences) != 0:
                end_index = occurences[0]
            else:
                end_index = len(time_series) - 1
            time_series = time_series[0:end_index]
            rssi_series = rssi_series[0:end_index]
            buckets[f"{chart_title[0]}_{chart_title[1]}"]["times"].extend(list(time_series))
            buckets[f"{chart_title[0]}_{chart_title[1]}"][chart_title[2]] = list(rssi_series)

    with open("processed_data.csv", "w") as f:
        writer = csv.writer(f)
        headers = []
        for folder in folders_ext:
            for signal in signal_permutations:
                header_group = [f"{folder} {signal} RSSI {dist} (dBm)" for dist in distance_permutations]
                header_group.insert(0, f"{folder} {signal} Time (ms)")
                headers.extend(header_group)
        writer.writerow(headers)
        while buckets != {}:
            row = []
            for folder in folders_ext:
                for signal in signal_permutations:
                    if buckets[f"{folder}_{signal}"] == {}:
                        row.extend(['N/A'])
                        row.extend(['127' for _ in distance_permutations])
                    else:
                        idx = 2 + len(distance_permutations) - len(buckets[f"{folder}_{signal}"].keys())
                        group = [buckets[f"{folder}_{signal}"]["times"].pop(0)]
                        group.extend(['127' for _ in distance_permutations])
                        group[idx] = buckets[f"{folder}_{signal}"][distance_permutations[idx - 1]].pop(0)
                        if not buckets[f"{folder}_{signal}"][distance_permutations[idx - 1]]:  # all empty
                            del buckets[f"{folder}_{signal}"][distance_permutations[idx - 1]]
                        if not buckets[f"{folder}_{signal}"]["times"]:
                            buckets[f"{folder}_{signal}"] = {}
                        row.extend(group)
            writer.writerow(row)
            should_stop = True
            for key in buckets:
                if buckets[key] != {}:
                    should_stop = False
            if should_stop:
                buckets = {}


def find_type_by_index(index):
    for i, folder in enumerate(folders_ext):
        for j, signal in enumerate(signal_permutations):
            for k, distance in enumerate(distance_permutations):
                for l, data_type in enumerate(parameters):
                    if index == i * len(signal_permutations) * len(distance_permutations) * len(parameters) \
                        + j * len(distance_permutations) * len(parameters) \
                        + k * len(parameters) \
                        + l:
                        return [folder, signal, distance]


if __name__ == "__main__":
    main()
