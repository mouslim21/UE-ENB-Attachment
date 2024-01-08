import matplotlib.pyplot as plt

def read_results(file_path):
    results = []
    with open(file_path, 'r') as file:
        for line in file:
            parts = line.split()
            if len(parts) >= 3:
                num_users = int(parts[1])
                throughput = float(parts[2])
                results.append((num_users, throughput))
    return results

# File paths for the results
heuristic_file = 'simulation_results_heuristic.txt'
genetic_file = 'simulation_results_ga.txt'
random_file = 'simulation_results_random.txt'

# Read the results from the files
heuristic_results = read_results(heuristic_file)
genetic_algorithm_results = read_results(genetic_file)
random_results = read_results(random_file)

# Extracting X and Y values
heuristic_x, heuristic_y = zip(*heuristic_results)
ga_x, ga_y = zip(*genetic_algorithm_results)
random_x, random_y = zip(*random_results)

# Plotting
plt.figure(figsize=(10, 6))
plt.plot(heuristic_x, heuristic_y, label='Heuristic', marker='o')
plt.plot(ga_x, ga_y, label='Genetic Algorithm', marker='s')
plt.plot(random_x, random_y, label='Random', marker='^')

plt.xlabel('Number of Users')
plt.ylabel('Throughput')
plt.title('Comparison of Throughput for Different Approaches')
plt.legend()
plt.grid(True)
plt.show()
