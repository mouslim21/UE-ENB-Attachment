import csv
import math
from sklearn.cluster import KMeans



def read_bus_data(filename, delimiter=',', max_lines=65535):
    ue_positions = []
    with open(filename, 'r') as file:
        reader = csv.reader(file, delimiter=delimiter)
        next(reader)  # Skip header line if there is one
        for count, row in enumerate(reader):
            if count >= max_lines:  # Break the loop if the maximum number of lines is reached
                break
            if row:  # Check if row is not empty
                lat, lon = float(row[4]), float(row[5])
                print("UE : " + str(lat) + " , " +str(lon))
                ue_positions.append((lat, lon))
    return ue_positions

def calculate_centroids(positions, num_clusters):
    kmeans = KMeans(n_clusters=num_clusters, n_init=10)
    kmeans.fit(positions)
    centroids = kmeans.cluster_centers_
    return centroids

def generate_enb_positions_from_ue_data(ue_positions, num_enbs):
    filename = '/home/mzlm/Documents/master/cell/2014-10/2014-10-24.txt'
    ue_positions = read_bus_data(filename, max_lines=100) 
    centroids = calculate_centroids(ue_positions, num_enbs)
    enb_positions = [(centroids[i][0], centroids[i][1]) for i in range(len(centroids))]
    return enb_positions

def write_attachments_to_file(attachments, filename="attachment_decisions.txt"):
    with open(filename, 'w') as file:
        for ue, enb in attachments.items():
            file.write(f"{ue} {enb}\n")

def write_positions_to_file(positions, filename):
    with open(filename, 'w') as file:
        for pos in positions:
            file.write(f"{pos[0]} {pos[1]}\n")

def calculate_distance(position1, position2):
    return math.sqrt((position1[0] - position2[0]) ** 2 + (position1[1] - position2[1]) ** 2)