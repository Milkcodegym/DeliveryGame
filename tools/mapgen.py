import xml.etree.ElementTree as ET
import math
import random
import os

# --- CONFIGURATION ---
OUTPUT_DIR = os.path.join("..", "resources", "maps") 
OUTPUT_FILE = os.path.join(OUTPUT_DIR, "real_city.map")
INPUT_FILE = "map.osm" 

SCALE = 111139.0 

# --- POI TYPE MAPPING (Commercial Only) ---
POI_TYPE_NONE = 0
POI_TYPE_FUEL = 1
POI_TYPE_FAST_FOOD = 2
POI_TYPE_CAFE = 3
POI_TYPE_BAR = 4
POI_TYPE_MARKET = 5 
POI_TYPE_SUPERMARKET = 6
POI_TYPE_RESTAURANT = 7

def gps_to_local(lat, lon, origin_lat, origin_lon):
    x = (lon - origin_lon) * SCALE * math.cos(math.radians(origin_lat))
    z = (origin_lat - lat) * SCALE 
    return x, z

def clean_name(name):
    if not name: return ""
    clean = name.replace(" ", "_").encode('ascii', 'ignore').decode('ascii')
    return clean

def get_name_from_tags(tags):
    if 'name' in tags: return clean_name(tags['name'])
    if 'brand' in tags: return clean_name(tags['brand'])
    return None

def get_poi_type(tags):
    amenity = tags.get('amenity', '')
    shop = tags.get('shop', '')
    if amenity == 'fast_food': return POI_TYPE_FAST_FOOD
    if amenity == 'cafe': return POI_TYPE_CAFE
    if amenity in ['bar', 'pub']: return POI_TYPE_BAR
    if amenity == 'restaurant': return POI_TYPE_RESTAURANT
    if amenity == 'fuel': return POI_TYPE_FUEL
    if shop == 'convenience': return POI_TYPE_MARKET
    if shop == 'supermarket': return POI_TYPE_SUPERMARKET
    return POI_TYPE_NONE

def parse_speed(speed_str):
    if not speed_str: return 50
    clean_str = ''.join(filter(str.isdigit, speed_str))
    if clean_str: return int(clean_str)
    return 50

def parse_width_tag(width_str):
    if not width_str: return None
    try:
        clean_str = width_str.replace('m', '').replace(' ', '').replace(',', '.')
        return float(clean_str)
    except ValueError:
        return None

def convert_osm():
    if not os.path.exists(INPUT_FILE):
        print(f"ERROR: Could not find '{INPUT_FILE}'.")
        return

    print(f"Parsing {INPUT_FILE}...")
    tree = ET.parse(INPUT_FILE)
    root = tree.getroot()

    # 1. Calculate Origin
    lats, lons = [], []
    for node in root.findall('node'):
        lats.append(float(node.attrib['lat']))
        lons.append(float(node.attrib['lon']))
    
    if not lats: return

    origin_lat = (min(lats) + max(lats)) / 2
    origin_lon = (min(lons) + max(lons)) / 2
    
    # 2. Parse Nodes DB
    node_db = {} 
    print("Converting Nodes...")
    for node in root.findall('node'):
        osm_id = node.attrib['id']
        lat = float(node.attrib['lat'])
        lon = float(node.attrib['lon'])
        x, z = gps_to_local(lat, lon, origin_lat, origin_lon)
        tags = {tag.attrib['k']: tag.attrib['v'] for tag in node.findall('tag')}
        node_db[osm_id] = {'x': x, 'z': z, 'tags': tags}

    # Data Collections
    nodes_export = []     
    node_id_map = {}      
    edges_export = []     
    buildings_export = [] 
    areas_export = []     
    pois_export = []      
    poi_coords = [] 

    def add_poi(p_type, x, z, name):
        if p_type == POI_TYPE_NONE: return
        if name is None or len(name) < 2: return 
        if "Unknown" in name: return 
        
        for px, pz in poi_coords:
            dist = math.sqrt((x-px)**2 + (z-pz)**2)
            if dist < 5.0: return 
        
        pois_export.append((p_type, x, z, name))
        poi_coords.append((x, z))

    def register_node(osm_id):
        if osm_id not in node_id_map:
            if osm_id not in node_db: return -1 
            data = node_db[osm_id]
            
            flags = 0
            if 'highway' in data['tags']:
                h = data['tags']['highway']
                if h == 'traffic_signals': flags = 1
                elif h == 'stop': flags = 2
            
            new_idx = len(nodes_export)
            nodes_export.append((new_idx, data['x'], data['z'], flags))
            node_id_map[osm_id] = new_idx
        return node_id_map[osm_id]

    print("Processing Ways...")
    for way in root.findall('way'):
        tags = {tag.attrib['k']: tag.attrib['v'] for tag in way.findall('tag')}
        refs = [nd.attrib['ref'] for nd in way.findall('nd')]
        
        # A. ROADS
        if 'highway' in tags:
            road_type = tags['highway']
            if road_type in ['footway', 'path', 'steps', 'pedestrian']: continue 

            # --- 1. SET DEFAULTS BY TYPE FIRST (Fallback) ---
            est_width = 3.5 
            if road_type == 'motorway': est_width = 15.0  
            elif road_type == 'trunk': est_width = 10.0
            elif road_type in ['primary', 'primary_link']: est_width = 8.0
            elif road_type in ['secondary', 'secondary_link']: est_width = 7.0
            elif road_type == 'tertiary': est_width = 6.0
            elif road_type in ['residential', 'living_street']: est_width = 5.0
            elif road_type == 'service': est_width = 4.0

            # --- 2. TRY 'lanes' TAG (Better Accuracy) ---
            lanes_count = tags.get('lanes')
            if lanes_count:
                try:
                    l = int(lanes_count)
                    # Use 3.25m per lane if lane count is known
                    est_width = l * 3.25 
                except ValueError:
                    pass 

            # --- 3. TRY 'width' TAG (Best Accuracy) ---
            explicit_width = parse_width_tag(tags.get('width'))
            if explicit_width:
                est_width = explicit_width

            # --- 4. EXPORT ---
            oneway = 1 if tags.get('oneway') == 'yes' else 0
            maxspeed = parse_speed(tags.get('maxspeed'))
            lanes = int(tags.get('lanes', 1))

            for i in range(len(refs) - 1):
                u = register_node(refs[i])
                v = register_node(refs[i+1])
                if u != -1 and v != -1:
                    edges_export.append((u, v, est_width, oneway, maxspeed, lanes))

        # B. BUILDINGS
        elif 'building' in tags:
            points = []
            for ref in refs:
                idx = register_node(ref)
                if idx != -1:
                    points.append((nodes_export[idx][1], nodes_export[idx][2]))
            
            if len(points) < 3: continue

            height = 8.0 + random.random() * 5.0
            if 'building:levels' in tags:
                try: height = float(tags['building:levels']) * 3.5
                except: pass

            r, g, b = 200, 200, 200
            buildings_export.append({
                'height': height,
                'color': (r, g, b),
                'points': points
            })

            # Check if this building is a Commercial POI
            poi_type = get_poi_type(tags)
            if poi_type != POI_TYPE_NONE:
                avg_x = sum(p[0] for p in points) / len(points)
                avg_z = sum(p[1] for p in points) / len(points)
                name = get_name_from_tags(tags)
                add_poi(poi_type, avg_x, avg_z, name)

        # C. AREAS (Parks & Water)
        elif 'leisure' in tags or 'natural' in tags or 'landuse' in tags:
            area_type = -1 
            color = (0, 0, 0)
            if tags.get('leisure') == 'park' or tags.get('landuse') == 'grass':
                area_type = 0; color = (50, 150, 50) 
            elif tags.get('natural') == 'water' or tags.get('landuse') == 'basin':
                area_type = 1; color = (50, 100, 200)
            
            if area_type != -1:
                points = []
                for ref in refs:
                    if ref in node_db:
                        d = node_db[ref]
                        points.append((d['x'], d['z']))
                if len(points) > 2:
                    areas_export.append({'type': area_type, 'color': color, 'points': points})

    # --- PASS 4: SCAN ALL STANDALONE NODES FOR POIS ---
    print("Scanning Standalone POIs...")
    for osm_id, data in node_db.items():
        tags = data['tags']
        poi_type = get_poi_type(tags)
        
        if poi_type != POI_TYPE_NONE:
            name = get_name_from_tags(tags)
            add_poi(poi_type, data['x'], data['z'], name)

    # WRITE
    if not os.path.exists(OUTPUT_DIR): os.makedirs(OUTPUT_DIR)
    
    with open(OUTPUT_FILE, 'w') as f:
        f.write("NODES:\n")
        for i, x, z, flags in nodes_export:
            f.write(f"{i}: {x:.2f} {z:.2f} {flags}\n")
        
        f.write("\nEDGES:\n")
        for u, v, w, ow, spd, ln in edges_export:
            f.write(f"{u} {v} {w:.2f} {ow} {spd} {ln}\n")
            
        f.write("\nBUILDINGS:\n")
        for b in buildings_export:
            f.write(f"{b['height']:.2f} {b['color'][0]} {b['color'][1]} {b['color'][2]}")
            for px, pz in b['points']:
                f.write(f" {px:.2f} {pz:.2f}")
            f.write("\n")

        f.write("\nAREAS:\n")
        for a in areas_export:
            f.write(f"{a['type']} {a['color'][0]} {a['color'][1]} {a['color'][2]}")
            for px, pz in a['points']:
                f.write(f" {px:.2f} {pz:.2f}")
            f.write("\n")

        f.write("\nL:\n")
        for p_type, x, z, name in pois_export:
            f.write(f"L {p_type} {x:.2f} {z:.2f} {name}\n")

    print(f"SUCCESS! Map saved to {OUTPUT_FILE}")

if __name__ == "__main__":
    convert_osm()