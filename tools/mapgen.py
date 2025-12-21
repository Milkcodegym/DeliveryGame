import xml.etree.ElementTree as ET
import math
import random
import os

# --- CONFIGURATION ---
# We use '..' to go up one folder from 'src' to the main project folder
OUTPUT_DIR = os.path.join("..", "resources", "maps") 
OUTPUT_FILE = os.path.join(OUTPUT_DIR, "real_city.map")
INPUT_FILE = "map.osm" 

SCALE = 111139.0  # Meters per degree

# --- HELPER: GPS to Local X/Z ---
def gps_to_local(lat, lon, origin_lat, origin_lon):
    x = (lon - origin_lon) * SCALE * math.cos(math.radians(origin_lat))
    z = (origin_lat - lat) * SCALE 
    return x, z

def convert_osm():
    # 0. CHECK INPUT
    if not os.path.exists(INPUT_FILE):
        print(f"ERROR: Could not find '{INPUT_FILE}'.")
        print(f"Make sure map.osm is inside the 'src' folder (where this script is).")
        return

    print(f"Parsing {INPUT_FILE}...")
    tree = ET.parse(INPUT_FILE)
    root = tree.getroot()

    # 1. Calculate Origin
    lats, lons = [], []
    for node in root.findall('node'):
        lats.append(float(node.attrib['lat']))
        lons.append(float(node.attrib['lon']))
    
    if not lats:
        print("Error: No nodes found in OSM file.")
        return

    origin_lat = (min(lats) + max(lats)) / 2
    origin_lon = (min(lons) + max(lons)) / 2
    
    # 2. Process Nodes
    osm_id_to_index = {}
    nodes_list = [] 
    
    print("Converting Nodes...")
    # Optimization: Only keep nodes used in ways
    relevant_node_ids = set()
    for way in root.findall('way'):
        for nd in way.findall('nd'):
            relevant_node_ids.add(nd.attrib['ref'])

    node_counter = 0
    for node in root.findall('node'):
        osm_id = node.attrib['id']
        if osm_id in relevant_node_ids:
            lat = float(node.attrib['lat'])
            lon = float(node.attrib['lon'])
            x, z = gps_to_local(lat, lon, origin_lat, origin_lon)
            
            osm_id_to_index[osm_id] = node_counter
            nodes_list.append((x, z))
            node_counter += 1

    # 3. Process Ways
    edges_list = []
    buildings_list = []

    print("Processing Ways...")
    for way in root.findall('way'):
        tags = {tag.attrib['k']: tag.attrib['v'] for tag in way.findall('tag')}
        refs = [nd.attrib['ref'] for nd in way.findall('nd')]
        
        if any(ref not in osm_id_to_index for ref in refs):
            continue

        # Roads
        if 'highway' in tags:
            width = 3.5 
            if tags['highway'] in ['primary', 'secondary']: width = 6.0
            
            for i in range(len(refs) - 1):
                start_idx = osm_id_to_index[refs[i]]
                end_idx = osm_id_to_index[refs[i+1]]
                edges_list.append((start_idx, end_idx, width))

        # Buildings
        elif 'building' in tags:
            points = []
            for ref in refs:
                idx = osm_id_to_index[ref]
                points.append(nodes_list[idx])
            
            height = 8.0 + random.random() * 15.0 
            if 'levels' in tags:
                try: height = float(tags['levels']) * 3.5
                except: pass
            
            r = random.randint(100, 200)
            g = random.randint(100, 150)
            b = random.randint(100, 150)
            
            buildings_list.append({
                'height': height,
                'color': (r, g, b),
                'points': points
            })

    # 4. SAFETY CHECK: Create Directories if missing
    if not os.path.exists(OUTPUT_DIR):
        print(f"Creating directory: {OUTPUT_DIR}")
        os.makedirs(OUTPUT_DIR)

    # 5. Write Output
    print(f"Writing to {OUTPUT_FILE}...")
    with open(OUTPUT_FILE, 'w') as f:
        f.write("NODES:\n")
        for i, (x, z) in enumerate(nodes_list):
            f.write(f"{i}: {x:.2f} {z:.2f}\n")
        
        f.write("\nEDGES:\n")
        for start, end, width in edges_list:
            f.write(f"{start} {end} {width:.2f}\n")
            
        f.write("\nBUILDINGS:\n")
        for b in buildings_list:
            f.write(f"{b['height']:.2f} {b['color'][0]} {b['color'][1]} {b['color'][2]}")
            for px, pz in b['points']:
                f.write(f" {px:.2f} {pz:.2f}")
            f.write("\n")

    print(f"SUCCESS! Map saved to {OUTPUT_FILE}")

if __name__ == "__main__":
    convert_osm()