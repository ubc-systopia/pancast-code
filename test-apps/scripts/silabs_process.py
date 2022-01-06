import csv

signals = ["minus40db", "minus20db", "minus10db", "4db"]
distances = ["0.5m", "1.0m", "2.0m", "4.0m"]


def main():
    combined_time = []
    combined_rssi = []
    filenames = generate_all_filenames()
    for filename in filenames:
        with open(filename, "r") as f:
            contents = f.read()
            elements = contents.splitlines()
            time_series = [el.split(" ")[0] for el in elements]
            rssi_series = [el.split(" ")[1] for el in elements]
            combined_time.append(time_series)
            combined_rssi.append(rssi_series)

    # standardize lengths
    # (LOGIC MOVED TO process_raw_silabs_logs.py)


def generate_all_filenames():
    filenames = []
    for signal in signals:
        for distance in distances:
            filenames.append(f"silabs_raw/{distance}_{signal}.txt")
    return filenames


if __name__ == "__main__":
    main()
