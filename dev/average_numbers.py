import sys
import statistics

# Check if the filename argument is provided
if len(sys.argv) < 2:
    print("Error: Please provide a filename as an argument.")
    exit(1)

# Open the file in read mode
filename = sys.argv[1]
numbers = []
with open(filename, "r") as f:
    # Read each line in the file
    for line in f:
        # Split the line into words
        words = line.split()
        if len(words) == 0:
            continue

        # Check if the first word is a number
        if words[0].isdigit():
            # Convert the first word to a number and add it to the list
            number = int(words[0])
            numbers.append(number)

# Calculate the average and standard deviation of the numbers
if len(numbers) > 0:
    # Sort the numbers in ascending order
    sorted_numbers = sorted(numbers)

    # Calculate first quartile (Q1)
    qs = statistics.quantiles(sorted_numbers, n=5)
    median = statistics.median(sorted_numbers)

    average = statistics.mean(numbers)
    standard_deviation = statistics.stdev(numbers)

    print(f"Avg ± SD: {average:2.0f} ± {standard_deviation:2.0f}")
    print("Quantiles:", qs)
    print("Second quartile (median):", median)
else:
    print("No numbers found in the file.")
