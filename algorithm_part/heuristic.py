import subprocess
import random
import math
from simulation_functions import read_bus_data, calculate_centroids, generate_enb_positions_from_ue_data, write_attachments_to_file, write_positions_to_file, calculate_distance





def random_ue_enb_attachments(ue_positions, num_enbs):
    attachments = {}
    for ue_index in range(len(ue_positions)):
        random_enb = random.randint(0, num_enbs - 1)
        attachments[ue_index] = random_enb
    return attachments

def generate_ue_enb_attachments(ue_positions, enb_positions):
    attachments = {}
    for ue_index, ue_pos in enumerate(ue_positions):
        best_signal_strength = float('inf')
        best_enb = None
        for enb_index, enb_pos in enumerate(enb_positions):
            distance = calculate_distance(ue_pos, enb_pos)
            if distance < best_signal_strength:
                best_signal_strength = distance
                best_enb = enb_index
        attachments[ue_index] = best_enb
    return attachments


def run_ns3_simulation(num_ues, num_enbs, alg):
    command = "./ns3 run scratch-simulator -- --numberOfUes=" + str(num_ues) + ", --numberOfEnbs=" + str(num_enbs) + ", --resultsFile=" + "simulation_results_" + str(alg) +".txt"
    subprocess.run(command, shell=True)

def main():
    num_enbs = 4

    filename = '/home/mzlm/Documents/master/cell/2014-10/2014-10-24.txt'
    ue_positions = read_bus_data(filename, max_lines=140)
    num_ues = len(ue_positions) 

    enb_positions = generate_enb_positions_from_ue_data(ue_positions, num_enbs)

    alg , attachments = "heuristic" , generate_ue_enb_attachments(ue_positions, enb_positions)
    #alg, attachments = "random" , random_ue_enb_attachments(ue_positions, num_enbs)
    write_attachments_to_file(attachments)

        # Write UE and eNodeB positions to files
    write_positions_to_file(ue_positions, "ue_positions.txt")
    write_positions_to_file(enb_positions, "enb_positions.txt")

    run_ns3_simulation(num_ues, num_enbs, alg)

if __name__ == "__main__":
    main()
