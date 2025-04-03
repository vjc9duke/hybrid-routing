#!/bin/bash

for csv_file in *.csv
do
  echo "Processing $csv_file"
  python3 make_graph.py $csv_file
done

echo "Done"