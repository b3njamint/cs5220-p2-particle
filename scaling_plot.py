import os
import re
import string
import matplotlib.pyplot as plt

def get_time(out_file):
    with open(out_file, 'r') as file:
        data = file.read()
    
    match = re.search(r'Ran in (\d+\.\d+) seconds', data)
    if match:
        return float(match.group(1))
    else:
        return None

def plot_core(data):
    cores = list(data.keys())
    times = list(data.values())

    plt.figure(figsize=(10, 6))
    plt.plot(cores, times, marker='o')
    plt.title('Execution Time vs. Core Count')
    plt.xlabel('Number of Cores')
    plt.ylabel('Execution Time (seconds)')
    plt.grid(True)
    plt.xticks(cores)
    plt.savefig('scaling_plot_cores.png')
    plt.show()

def plot_particle(data):
    h = list(data.keys())
    times = list(data.values())

    plt.figure(figsize=(10, 6))
    plt.loglog(h, times, marker='o')
    plt.title('Log-Log Execution Time vs. Particle Size')
    plt.xlabel('Log Particle Size (h)')
    plt.ylabel('Log Execution Time (seconds)')
    plt.grid(True)
    plt.xticks(h)
    plt.savefig('scaling_plot_particles.png')
    plt.show()

def main():
    data = {}
    
    # for cores in [1, 2, 4, 8, 16, 32, 64]:
    #     out_file = f'sph_outputs_cores/sph_{cores}.out'
    #     if os.path.exists(out_file):
    #         time = get_time(out_file)
    #         if time is not None:
    #             data[cores] = time
    #         else:
    #             print(f"Could not get time from {out_file}")
    #     else:
    #         print(f"{out_file} does not exist")

    # plot_core(data)

    char_to_h = {c : h for c, h in zip(list(string.ascii_lowercase[:9]), [5, 2, 1, 5e-1, 2e-1, 1e-1, 5e-2, 2e-2, 1e-2, 1e-3])}

    for c in list(string.ascii_lowercase[:7]):
        out_file = f'sph_outputs_particles/sph_{c}.out'
        if os.path.exists(out_file):
            time = get_time(out_file)
            if time is not None:
                data[char_to_h[c]] = time
            else:
                print(f"Could not get time from {out_file}")
        else:
            print(f"{out_file} does not exist")

    plot_particle(data)

if __name__ == "__main__":
    main()
