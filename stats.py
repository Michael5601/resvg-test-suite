#!/usr/bin/env python3

import csv
import json
import subprocess
import sys
import os
from collections import defaultdict
from pathlib import Path

UNKNOWN      = 0
PASSED       = 1
FAILED       = 2
CRASHED      = 3

class RowData:
    def __init__(self, name, flags):
        self.name = name
        self.flags = flags


def get_group(name):
    return name.split('/')[0]  # Extract group name before the first "/"


is_svg2_only = "--svg2" in sys.argv

forbiddenSVG2Files = {"not-UTF-8-encoding.svg",
                      "invalid-id-attribute-2.svg",
                      "non-ASCII-character.svg",
                      "compound-emojis-and-coordinates-list.svg",
                      "compound-emojis.svg",
                      "emojis.svg",
                      "escaped-text-4.svg",
                      "rotate-with-multiple-values-and-complex-text.svg",
                      "zalgo.svg",
                      "on-tspan-with-arabic.svg",
                      "complex.svg",
                      "writing-mode=tb.svg",
                      "tb-and-punctuation.svg"}
svg2_files = []
if is_svg2_only:
    files = list(Path('tests').rglob('*.svg'))
    for file in files:
        with open(file, 'r') as f:
            if (file.name in forbiddenSVG2Files): 
                continue

            if "(SVG 2)" in f.read():
                svg2_files.append(str(file).replace('tests\\', ''))

for i in range(len(svg2_files)):
    svg2_files[i] = svg2_files[i].replace("\\", "/")
    
rows = []
with open('results.csv', 'r') as f:
    for row in csv.reader(f):
        if row[0] == 'title':
            continue

        file_name = row[0]

        if is_svg2_only and file_name not in svg2_files:
            continue
        
        # Skip UB
        if int(row[1]) == UNKNOWN:
            continue

        flags = [int(row[1]), int(row[2]), int(row[3]), int(row[4])]
        rows.append(RowData(file_name, flags))

# Total passed for all tests
passed_total = [0, 0, 0, 0]

# Passed results grouped by group name
group_passed = defaultdict(lambda: [0, 0, 0, 0])

for row in rows:
    group = get_group(row.name)
    for idx, flag in enumerate(row.flags):
        if flag == PASSED:
            passed_total[idx] += 1
            group_passed[group][idx] += 1

# Generate chart data for total results
barh_data = {
    "items_font": {
        "family": "Arial",
        "size": 12
    },
    "items": [
        {
            "name": "Batik 1.17",
            "value": passed_total[0]
        },
        {
            "name": "JSVG 1.6.1",
            "value": passed_total[1]
        },
        {
            "name": "svgSalamander 1.1.4",
            "value": passed_total[2]
        },
        {
            "name": "EchoSVG 1.2.2",
            "value": passed_total[3]
        }
    ],
    "hor_axis": {
        "title": "Tests passed",
        "round_tick_values": True,
        "width": 700,
        "max_value": len(rows)
    }
}

with open('chart.json', 'w') as f:
    f.write(json.dumps(barh_data, indent=4))

# Generate chart data for groups
group_data = {}
for group, passed in group_passed.items():
    group_data[group] = {
        "Batik 1.17": passed[0],
        "JSVG 1.6.1": passed[1],
        "svgSalamander 1.1.4": passed[2],
        "EchoSVG 1.2.2": passed[3]
    }

with open('group_results.json', 'w') as f:
    f.write(json.dumps(group_data, indent=4))

if is_svg2_only:
    out_path = 'site/images/chart-svg2.svg'
else:
    out_path = 'site/images/chart.svg'

try:
    subprocess.check_call(['./barh', 'chart.json', out_path])
except FileNotFoundError:
    print('Error: \'barh\' executable is not found.\n'
          'You should build https://github.com/RazrFalcon/barh '
          'and link resultig binary to the current directory.')
