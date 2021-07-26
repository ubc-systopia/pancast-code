import csv


def main():
    file = open("telemetry_data.txt")
    contents = file.read()
    elements = contents.splitlines()
    start_time = get_timestamp(elements[0])
    end_time = [int(start_time[0]), int(start_time[1]) + 10, float(start_time[2])]
    if int(start_time[1]) > 60:
        end_time[0] += 1
        end_time[1] -= 60
    filtered_entries = [x for x in elements if is_before_time(x, end_time)]

    rssi_list = [x.split("TELEMETRY: [")[1].split(",")[0] for x in filtered_entries]
    rssi_list = [int(x) for x in rssi_list]
    headers = ['rssi']
    with open('./telemetry_data.csv', 'w') as f:
        writer = csv.writer(f)
        writer.writerow(headers)
        for rssi in rssi_list:
            writer.writerow([str(rssi)])


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


def get_timestamp(entry):
    time = entry.split(" ")[1].split(":")
    time = [int(time[0]), int(time[1]), float(time[2])]
    return time


if __name__ == '__main__':
    main()
