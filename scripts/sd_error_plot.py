# python3 sd_error_plot.py <-10dbm_log> <4dbm_log> <-20dbm_log>

import sys
import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
import numpy as np

distances = [1, 2, 5, 10]
z = 1.96

def main():

  filename1 = sys.argv[1]
  filename2 = sys.argv[2]
  filename3 = sys.argv[3]

  silabs_10 = get_silabs_data(filename1)
  silabs_4 = get_silabs_data(filename2)

  nordic_20 = get_nordic_data(filename3)
  nordic_10 = get_nordic_data(filename1)
  nordic_4 = get_nordic_data(filename2)

  silabs_10_rssi = get_rssi_lists(silabs_10)
  silabs_4_rssi = get_rssi_lists(silabs_4)

  nordic_20_rssi = get_rssi_lists(nordic_20)
  nordic_10_rssi = get_rssi_lists(nordic_10)
  nordic_4_rssi = get_rssi_lists(nordic_4)

  # Silabs Plot
  plt.figure()
  plt.title("Mean RSSI and 95% confidence interval, SD-SB")
  plt.xlim(0,11)
  plt.ylim(-110, -30)
  plt.xticks([0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10])
  plt.ylabel("Mean RSSI (dBm)")
  plt.xlabel("Distance (m)")
  for i, rssi_list in enumerate(silabs_10_rssi):
    data_series = np.array(rssi_list)
    mean = data_series.mean()
    std = data_series.std()
    distnum = get_dist_from_index(i)
    ci = z * std
    plt.errorbar(distnum, mean, ci, label="-10dbm", fmt="o", color='blue', capsize=2)
  for i, rssi_list in enumerate(silabs_4_rssi):
    data_series = np.array(rssi_list)
    mean = data_series.mean()
    std = data_series.std()
    distnum = get_dist_from_index(i)
    ci = z * std
    plt.errorbar(distnum, mean, ci, label="4dbm", fmt="o", color='red', capsize=2)

  red_patch = mpatches.Patch(color='red', label='4dbm')
  blue_patch = mpatches.Patch(color='blue', label='-10dbm')
  plt.legend(handles=[red_patch, blue_patch])
  plt.show()

  # Nordic Plot
  plt.figure()
  plt.title("Mean RSSI and 95% confidence interval SD-NB")
  plt.xlim(0,11)
  plt.ylim(-110, -30)
  plt.xticks([0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10])
  plt.ylabel("Mean RSSI (dBm)")
  plt.xlabel("Distance (m)") 
  for i, rssi_list in enumerate(nordic_20_rssi):
    data_series = np.array(rssi_list)
    mean = data_series.mean()
    std = data_series.std()
    distnum = get_dist_from_index(i)
    ci = z * std
    plt.errorbar(distnum, mean, ci, label="-20dbm", fmt="o", color='green', capsize=2)
  for i, rssi_list in enumerate(nordic_10_rssi):
    data_series = np.array(rssi_list)
    mean = data_series.mean()
    std = data_series.std()
    distnum = get_dist_from_index(i)
    ci = z * std
    plt.errorbar(distnum, mean, ci, label="-10dbm", fmt="o", color='blue', capsize=2)
  for i, rssi_list in enumerate(nordic_4_rssi):
    data_series = np.array(rssi_list)
    mean = data_series.mean()
    std = data_series.std()
    distnum = get_dist_from_index(i)
    ci = z * std
    plt.errorbar(distnum, mean, ci, label="4dbm", fmt="o", color='red', capsize=2)

  red_patch = mpatches.Patch(color='red', label='4dbm')
  blue_patch = mpatches.Patch(color='blue', label='-10dbm')
  green_patch = mpatches.Patch(color='green', label='-20dbm')
  plt.legend(handles=[red_patch, blue_patch, green_patch])
  plt.show()
  

def filter_lines(elements):
  new_array = []
  for x in elements:
    if "eph ID:" in x:
      new_array.append(x)
  return new_array

def filter_lines_by_id(elements, b_id):
  new_array = []
  for x in elements:
    if b_id in x:
      if len(x.split(" ")) == 23:
        new_array.append(x)
  return new_array

def get_dist_from_index(i):
  if i == 0:
    return 1
  if i == 1:
    return 2
  if i == 2:
    return 5
  if i == 3:
    return 10

def get_rssi_lists(data):
  rssi_lists = []
  for d in data:
    for x in d:
      if len(x.split(" ")) != 23:
        print(x)
    rssi_list = [int(x.split(" ")[22]) for x in d]
    rssi_lists.append(rssi_list)
  return rssi_lists

def get_silabs_data(filename):
  with open(filename, 'r') as f:
    contents = f.read()
    elements = contents.splitlines()
  
    filtered_elements = filter_lines(elements)
    
    group_2m = filter_lines_by_id(filtered_elements, "0x22220004")
    group_1m = filter_lines_by_id(filtered_elements, "0x22220005")
    group_5m = filter_lines_by_id(filtered_elements, "0x22220006")
    group_10m = filter_lines_by_id(filtered_elements, "0x22220007")

    return [group_1m, group_2m, group_5m, group_10m]

def get_nordic_data(filename):
  with open(filename, 'r') as f:
    contents = f.read()
    elements = contents.splitlines()
  
    filtered_elements = filter_lines(elements)
    
    group_2m = filter_lines_by_id(filtered_elements, "0x22220000")
    group_1m = filter_lines_by_id(filtered_elements, "0x22220001")
    group_5m = filter_lines_by_id(filtered_elements, "0x22220002")
    group_10m = filter_lines_by_id(filtered_elements, "0x22220003")

    return [group_1m, group_2m, group_5m, group_10m]
if __name__ == '__main__':
  main()
