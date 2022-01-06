import matplotlib.pyplot as plt

distances_silabs = ["0.5m", "1.0m", "2.0m", "4.0m", "5.0m", "6.0m", "7.0m", "8.0m", "9.0m", "10.0m"]
signals_silabs = ["minus20db", "minus10db", "4db"]


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

    for signal, graph_data in data.items():
        plt.figure()
        plt.title(f"Distance vs RSSI through wall at {signal} for Silabs dongle")
        plt.xlabel("Time (s)")
        plt.ylabel("RSSI (dBm)")
        plt.xlim(0, 600)
        plt.ylim(-110, -30)
        for distance, series in graph_data.items():
            time_series = [x[0] for x in series]
            rssi_series = [x[1] for x in series]
            plt.scatter(time_series, rssi_series, label=distance, s=1)
        plt.legend()
        plt.show()





def convert_time_to_s(timestamp):
    print(timestamp)
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


if __name__ == "__main__":
    collect_silabs_data()
