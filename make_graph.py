import pandas as pd
import matplotlib.pyplot as plt
import argparse
import numpy as np

parser = argparse.ArgumentParser(description="Plot data from a CSV file.")
parser.add_argument("csv_file", type=str, help="Path to the CSV file")
args = parser.parse_args()

file_path = args.csv_file
df = pd.read_csv(file_path)

label = df.columns[0]
thresholds = df[label]
df_numeric = df.drop(columns=[label])
#print("Thresholds: ", thresholds)
x_values = df_numeric.columns.astype(float)

if pd.api.types.is_numeric_dtype(thresholds):
    norm = plt.Normalize(vmin=thresholds.min(), vmax=thresholds.max())
    cmap = plt.get_cmap("viridis")
    use_colormap = True
else:
    use_colormap = False

plt.figure(figsize=(10, 6))
for i, row in df_numeric.iterrows():
    if use_colormap:
        color = cmap(norm(thresholds[i]))
        plt.plot(x_values, row, label=str(thresholds[i]), color=color, linewidth=1.5, alpha=0.7)
    else:
        #print("Here")
        #print(str(thresholds[i]))
        plt.plot(x_values, row, label=str(thresholds[i]), linewidth=1.5, alpha=0.7)

png_filename = file_path.rsplit(".", 1)[0] + ".png"
plt.xlabel("Injection Rate")
plt.ylabel("Average Latency (ns)")
plt.yscale("log")
plt.title(png_filename)
plt.legend()
plt.grid()
plt.savefig(png_filename)
plt.close()
