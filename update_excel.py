import sys
import pandas as pd
import os

# Get command-line arguments
csv_file = sys.argv[1]  # Path to the CSV file
routing_function = sys.argv[2]  # Routing function name
injection_rate = float(sys.argv[3])  # Injection rate value
latency = float(sys.argv[4])  # Latency value

# Load or create the CSV file
if os.path.exists(csv_file):
    df = pd.read_csv(csv_file, index_col=0)  # Load existing CSV with first column as index
    df.columns = [float(col) if col.replace('.', '', 1).isdigit() else col for col in df.columns]
else:
    # Create a new DataFrame with routing functions as rows and injection rates as columns
    injection_rates = [round(x, 2) for x in list(frange(0.01, 0.70, 0.01))] 
    df = pd.DataFrame(index=["custom", "dor", "valiant", "romm", "min_adapt"], columns=injection_rates)
    df.index.name = "Routing Function"  # Set the index name

# Ensure the routing function exists in the index
if routing_function not in df.index:
    df.loc[routing_function] = [None] * len(df.columns)  # Initialize row if missing

# Update the correct cell
df.at[routing_function, injection_rate] = latency

# Save back to CSV
df.to_csv(csv_file)

print(f"Updated CSV: {csv_file} - Routing: {routing_function}, Rate: {injection_rate}, Latency: {latency}")


# Helper function to create floating point ranges
def frange(start, stop, step):
    while start < stop:
        yield round(start, 2)
        start += step
