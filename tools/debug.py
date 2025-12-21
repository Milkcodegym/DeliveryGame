import sys
print("1. Python is running!")

import os
print(f"2. Current working directory: {os.getcwd()}")

# Check if input file exists
INPUT_FILE = "map.osm"
if not os.path.exists(INPUT_FILE):
    print(f"ERROR: Could not find '{INPUT_FILE}' in this folder!")
    print("   -> Please make sure map.osm is in the same folder as this script.")
    input("Press Enter to exit...")
    sys.exit()

print(f"3. Found {INPUT_FILE} successfully.")

import xml.etree.ElementTree as ET
print("4. Imported XML library.")

try:
    print("5. Parsing XML file... (this might take a second)")
    tree = ET.parse(INPUT_FILE)
    root = tree.getroot()
    print("6. XML parsed successfully.")
    
    node_count = len(root.findall('node'))
    print(f"7. Found {node_count} nodes in the file.")
    
    if node_count == 0:
        print("WARNING: The OSM file seems empty or has no nodes!")
        
    way_count = len(root.findall('way'))
    print(f"8. Found {way_count} ways (roads/buildings).")

    print("9. Script finished logic. (If you see this, it worked!)")

except Exception as e:
    print(f"CRASH: An error occurred: {e}")

input("Press Enter to close...")