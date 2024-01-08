import subprocess
import random
import math
import csv
from sklearn.cluster import KMeans
from simulation_functions import read_bus_data, calculate_centroids, generate_enb_positions_from_ue_data, write_attachments_to_file, write_positions_to_file, calculate_distance
import threading


def wait_for_simulation(simulation_finished):
    # Wait until simulation is finished
    with simulation_finished:
        simulation_finished.wait()
        
class GeneticAlgorithm:
    def __init__(self, num_enbs, num_ues, ue_positions, enb_positions, population_size, crossover_rate, mutation_rate):
        self.num_enbs = num_enbs
        self.num_ues = num_ues
        self.ue_positions = ue_positions
        self.enb_positions = enb_positions
        self.population_size = population_size
        self.crossover_rate = crossover_rate
        self.mutation_rate = mutation_rate
        self.population = self.initialize_population()
        

    def initialize_population(self):
        population = []
        for _ in range(self.population_size):
            chromosome = []
            for ue_pos in self.ue_positions:
                best_signal_strength = float('inf')
                best_enb = None
                for enb_index, enb_pos in enumerate(self.enb_positions):
                    distance = calculate_distance(ue_pos, enb_pos)
                    if distance < best_signal_strength:
                        best_signal_strength = distance
                        best_enb = enb_index
                chromosome.append(best_enb)
            population.append(chromosome)
        return population
    
    

    def select_parents(self):
        parents = random.sample(self.population, 2)
        return parents

    def crossover(self, parent1, parent2):
        if random.random() < self.crossover_rate:
            crossover_point = random.randint(1, self.num_ues - 1)
            child1 = parent1[:crossover_point] + parent2[crossover_point:]
            child2 = parent2[:crossover_point] + parent1[crossover_point:]
            return child1, child2
        return parent1, parent2

    def mutate(self, chromosome):
        for i in range(self.num_ues):
            if random.random() < self.mutation_rate:
                chromosome[i] = random.randint(0, self.num_enbs - 1)
        return chromosome

    def fitness_function(self, chromosome, filename="simulation_results_ga.txt"):
        # Calculate the number of eNodeBs based on the chromosome
        num_enbs = max(chromosome) + 1  # Assuming enb indices start from 0

        # The number of UEs is the length of the chromosome
        num_ues = len(chromosome)

        # Initialize variables to store the read values
        found_throughput, found_delay, found_jitter = 0, 0, 0
        found = False  # Flag to check if the matching line is found

        # Read the file and extract values for the specific chromosome configuration
        with open(filename, 'r') as file:
            for line in file:
                enb, ue, throughput, delay, jitter = line.strip().split()
                enb = int(enb)
                ue = int(ue)

                # Check if the line corresponds to the current chromosome
                if enb == num_enbs and ue == num_ues:
                    found_throughput = float(throughput)
                    found_delay = float(delay)
                    found_jitter = float(jitter)
                    found = True
                    break  # Exit the loop once the matching line is found

        # If a matching line is not found, return a low fitness value
        if not found:
            return 0

        # Calculate fitness based on throughput, delay, and jitter
        fitness = found_throughput  # Start with throughput as the base fitness

        # Thresholds for delay and jitter
        delay_threshold = 0.1  # Example threshold for delay in seconds
        jitter_threshold = 0.01  # Example threshold for jitter in seconds

        # Penalize solutions that exceed thresholds
        if found_delay > delay_threshold:
            fitness -= 10 * (found_delay - delay_threshold)  # Example penalty for delay

        if found_jitter > jitter_threshold:
            fitness -= 100 * (found_jitter - jitter_threshold)  # Example penalty for jitter

        return fitness

        # Return a default low fitness if no matching entry is found
        return 0


    def next_generation(self):
        new_population = []
        while len(new_population) < self.population_size:
            parent1, parent2 = self.select_parents()
            child1, child2 = self.crossover(parent1, parent2)
            new_population.append(self.mutate(child1))
            if len(new_population) < self.population_size:
                new_population.append(self.mutate(child2))
        self.population = new_population

    def run_simulation_and_evaluate(self):
        # Dictionary to store the fitness values for each chromosome
        fitness_values = {}

        for index, chromosome in enumerate(self.population):

            simulation_finished = threading.Condition()  # Initialize the condition object
            simulation_thread = threading.Thread(target=self.run_ns3_simulation,
                args=(simulation_finished, chromosome,))  # here we use NS3 
            simulation_thread.start()

            wait_for_simulation(simulation_finished)            

            # Evaluate the fitness of the chromosome
            fitness = self.fitness_function(chromosome)

            # Store the fitness value
            fitness_values[index] = fitness

            print(f"Chromosome {index}: Fitness = {fitness}")

        # Update the population with chromosomes sorted by their fitness values
        self.population = sorted(self.population, key=lambda x: fitness_values[self.population.index(x)], reverse=True)

        # Optionally, return the best chromosome and its fitness value
        best_chromosome = self.population[0]
        best_fitness = fitness_values[self.population.index(best_chromosome)]
        return best_chromosome, best_fitness

    def run_ns3_simulation(self, simulation_finished, chromosome):        
        # Write the attachments to a file
        write_attachments_to_file(dict(enumerate(chromosome)))
        # Run the simulation
        command = f"./ns3 run scratch-simulator -- --numberOfUes={self.num_ues} --numberOfEnbs={self.num_enbs} , --resultsFile=simulation_results_ga.txt"
        subprocess.run(command, shell=True)

        with simulation_finished:
            simulation_finished.notify()

    

    def get_fitness_scores(self):
        # Calculate fitness for each chromosome in the population
        fitness_scores = [self.fitness_function(chromosome) for chromosome in self.population]
        return fitness_scores

    def get_best_solution(self):
        # Get fitness scores for the current population
        fitness_scores = self.get_fitness_scores()

        # Find the index of the chromosome with the highest fitness
        best_index = fitness_scores.index(max(fitness_scores))

        # Return the best chromosome and its fitness
        return self.population[best_index], fitness_scores[best_index]

def run_ns3_simulation(num_ues, num_enbs, chromosome):        
        # Write the attachments to a file
    write_attachments_to_file(dict(enumerate(chromosome)))
        # Run the simulation
    command = f"./ns3 run scratch-simulator -- --numberOfUes={num_ues} --numberOfEnbs={num_enbs} --resultsFile="  ", --resultsFile=simulation_results_ga.txt" 
    subprocess.run(command, shell=True)

def main():
    num_enbs = 4
    
    filename = '/home/mzlm/Documents/master/cell/2014-10/2014-10-24.txt'
    delimiter = ',' 
    ue_positions = read_bus_data(filename, delimiter, max_lines=120)
    num_ues = len(ue_positions) 
    enb_positions = generate_enb_positions_from_ue_data(ue_positions, num_enbs)

    write_positions_to_file(ue_positions, "ue_positions.txt")
    write_positions_to_file(enb_positions, "enb_positions.txt")

    population_size = 2
    crossover_rate = 0.8
    mutation_rate = 0.05

    ga = GeneticAlgorithm(num_enbs, num_ues, ue_positions, enb_positions, population_size, crossover_rate, mutation_rate)

    num_generations = 2
    for generation in range(num_generations):
        print(f"Generation {generation + 1}")
        ga.run_simulation_and_evaluate()
        ga.next_generation()

    best_solution, best_fitness = ga.get_best_solution()
    print("Best Solution:", best_solution)
    print("Best Fitness:", best_fitness)

    # Convert best solution to UE-eNB attachments
    best_attachments = {ue: enb for ue, enb in enumerate(best_solution)}

    # Write the best attachments to file
    write_attachments_to_file(best_attachments, "attachment_decisions.txt")

    # Run NS-3 simulation with the best solution
    print("Running simulation with the best solution...")
    run_ns3_simulation(num_ues, num_enbs, best_solution)
    print("Simulation completed.")

    # Read the results from the simulation results file
    with open("simulation_results_ga.txt", "r") as file:
        final_results = file.readlines()
        for line in final_results:
            print(line.strip())

if __name__ == "__main__":
    main()
