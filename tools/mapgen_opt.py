import xml.etree.ElementTree as ET
import math
import random
import os

# --- CONFIGURATION ---
OUTPUT_DIR = os.path.join("..", "resources", "maps") 
OUTPUT_FILE = os.path.join(OUTPUT_DIR, "whole_city.map")
INPUT_FILE = "wholemap.osm" 

SCALE = 111139.0 

# --- OPTIMIZATION SETTINGS (RELAXED) ---

# 1. Radius Clipping: Set to 0 to DISABLE it completely.
#    (This was likely deleting the "wrong" half of your city if the center calculation was off)
MAX_RADIUS_METERS = 0  

# 2. Road Filtering: Set to False to see ALL drivable roads.
#    (Many city streets are tagged as 'service' or 'unclassified' in OSM)
EXCLUDE_SERVICE_ROADS = False 

# 3. Building Culling: Lowered to 5.0m (Keep small shops/kiosks)
MIN_BUILDING_AREA = 2.0 

# 4. Geometry Simplification: Lowered to 0.5m
#    (High values like 1.5m can turn small square houses into triangles or lines, causing deletion)
SIMPLIFY_TOLERANCE = 0.1 

# 5. Nature Simplification: Keep this aggressive (it saves the most space with least visual impact)
NATURE_SIMPLIFY_TOLERANCE = 10.0 
MIN_NATURE_AREA = 200.0

# --- POI TYPE MAPPING ---
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

# --- GEOMETRY HELPERS ---
def simplify_points(points, tolerance):
    if len(points) < 3: return points
    new_points = [points[0]]
    last_p = points[0]
    tol_sq = tolerance * tolerance
    
    for i in range(1, len(points)):
        p = points[i]
        dist_sq = (p[0] - last_p[0])**2 + (p[1] - last_p[1])**2
        if dist_sq > tol_sq or i == len(points) - 1:
            new_points.append(p)
            last_p = p
            
    return new_points

def calculate_area(points):
    if len(points) < 3: return 0.0
    area = 0.0
    for i in range(len(points)):
        j = (i + 1) % len(points)
        area += points[i][0] * points[j][1]
        area -= points[j][0] * points[i][1]
    return abs(area) / 2.0

def convert_osm():
    if not os.path.exists(INPUT_FILE):
        print(f"ERROR: Could not find '{INPUT_FILE}'.")
        return

    print(f"Parsing {INPUT_FILE} with Optimizations...")
    tree = ET.parse(INPUT_FILE)
    root = tree.getroot()

    # 1. Calculate Origin (Using First Node found)
    # This prevents bad centering if outliers exist
    first_node = root.find('node')
    if first_node is None:
        print("ERROR: No nodes found in input file!")
        return

    origin_lat = float(first_node.attrib['lat'])
    origin_lon = float(first_node.attrib['lon'])
    print(f"Origin Anchor set to: {origin_lat}, {origin_lon}")
    
    # 2. Parse Nodes DB
    node_db = {} 
    print("Converting Nodes...")
    for node in root.findall('node'):
        osm_id = node.attrib['id']
        lat = float(node.attrib['lat'])
        lon = float(node.attrib['lon'])
        x, z = gps_to_local(lat, lon, origin_lat, origin_lon)
        
        # OPTIMIZATION: Radius Clipping
        if MAX_RADIUS_METERS > 0:
            dist_sq = x*x + z*z
            if dist_sq > (MAX_RADIUS_METERS * MAX_RADIUS_METERS):
                continue 
        
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
        
        # Duplicate check
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
    count_roads = 0
    count_buildings = 0
    
    for way in root.findall('way'):
        tags = {tag.attrib['k']: tag.attrib['v'] for tag in way.findall('tag')}
        refs = [nd.attrib['ref'] for nd in way.findall('nd')]
        
        # --- A. ROADS (Robust Segment Logic) ---
        if 'highway' in tags:
            road_type = tags['highway']
            # Only ignore things that are DEFINITELY not cars
            if road_type in ['footway', 'path', 'steps', 'pedestrian', 'track', 'cycleway', 'corridor', 'elevator']: 
                continue 
            
            # (Optional) Service filtering
            if EXCLUDE_SERVICE_ROADS and road_type == 'service': continue

            # Width Estimation
            est_width = 4.0 # Default for 'unclassified'
            if road_type == 'motorway': est_width = 15.0  
            elif road_type == 'trunk': est_width = 10.0
            elif road_type in ['primary', 'primary_link']: est_width = 8.0
            elif road_type in ['secondary', 'secondary_link']: est_width = 7.0
            elif road_type == 'tertiary': est_width = 6.0
            elif road_type in ['residential', 'living_street']: est_width = 5.0
            elif road_type == 'service': est_width = 4.0

            # Override with specific tags
            lanes = tags.get('lanes')
            if lanes:
                try: est_width = int(lanes) * 3.25 
                except: pass 
            
            explicit_width = parse_width_tag(tags.get('width'))
            if explicit_width: est_width = explicit_width

            oneway = 1 if tags.get('oneway') == 'yes' else 0
            maxspeed = parse_speed(tags.get('maxspeed'))
            lanes_int = int(tags.get('lanes', 1))

            # --- THE FIX: Salvage valid segments ---
            # Instead of failing the whole road, we collect continuous runs of valid nodes.
            current_segment = []
            
            for r in refs:
                idx = register_node(r)
                if idx != -1:
                    current_segment.append(idx)
                else:
                    # Node missing! Flush current segment as a road and start over
                    if len(current_segment) > 1:
                        for i in range(len(current_segment) - 1):
                            edges_export.append((current_segment[i], current_segment[i+1], est_width, oneway, maxspeed, lanes_int))
                        count_roads += 1
                    current_segment = [] # Reset
            
            # Flush remaining segment
            if len(current_segment) > 1:
                for i in range(len(current_segment) - 1):
                    edges_export.append((current_segment[i], current_segment[i+1], est_width, oneway, maxspeed, lanes_int))
                count_roads += 1

        # --- B. BUILDINGS (Broad Tag Matching) ---
        # Checks for 'building', 'building:part', or 'wall' to catch more geometry
        elif 'building' in tags or 'building:part' in tags or tags.get('barrier') == 'wall':
            raw_points = []
            
            for ref in refs:
                if ref in node_db:
                    d = node_db[ref]
                    raw_points.append((d['x'], d['z']))
            
            # Only need 3 points for a triangle
            if len(raw_points) < 3: continue

            # Relaxed Area Check
            if calculate_area(raw_points) < MIN_BUILDING_AREA: continue

            # Relaxed Simplification
            final_points = simplify_points(raw_points, SIMPLIFY_TOLERANCE)
            if len(final_points) < 3: continue

            height = 8.0 + random.random() * 5.0
            if 'building:levels' in tags:
                try: height = float(tags['building:levels']) * 3.5
                except: pass
            elif 'height' in tags:
                try: height = parse_width_tag(tags['height']) # Reuse width parser for "10m" string
                except: pass
                if not height: height = 10.0

            # Color randomization based on ID (deterministic)
            # Use hash of first ref to keep color consistent
            color_seed = hash(refs[0]) % 50
            r = 180 + color_seed
            g = 180 + color_seed
            b = 190 + color_seed

            buildings_export.append({
                'height': height,
                'color': (r, g, b),
                'points': final_points
            })
            count_buildings += 1

            # Commercial POI Check
            poi_type = get_poi_type(tags)
            if poi_type != POI_TYPE_NONE:
                avg_x = sum(p[0] for p in final_points) / len(final_points)
                avg_z = sum(p[1] for p in final_points) / len(final_points)
                name = get_name_from_tags(tags)
                add_poi(poi_type, avg_x, avg_z, name)

        # --- C. AREAS (Nature) ---
        elif 'leisure' in tags or 'natural' in tags or 'landuse' in tags or 'amenity' in tags:
            area_type = -1 
            color = (0, 0, 0)
            
            t_leisure = tags.get('leisure', '')
            t_landuse = tags.get('landuse', '')
            t_natural = tags.get('natural', '')
            t_amenity = tags.get('amenity', '')

            if t_leisure in ['park', 'garden', 'pitch'] or t_landuse in ['grass', 'village_green', 'meadow'] or t_natural == 'wood':
                area_type = 0; color = (50, 150, 50) 
            elif t_natural == 'water' or t_landuse == 'basin':
                area_type = 1; color = (50, 100, 200)
            elif t_landuse in ['residential', 'commercial']:
                # Optional: Draw faint ground color for districts?
                # area_type = 2; color = (230, 230, 230) 
                pass
            
            if area_type != -1:
                points = []
                for ref in refs:
                    if ref in node_db:
                        d = node_db[ref]
                        points.append((d['x'], d['z']))
                
                if len(points) > 2:
                    if calculate_area(points) > MIN_NATURE_AREA:
                        points = simplify_points(points, NATURE_SIMPLIFY_TOLERANCE)
                        if len(points) > 2:
                            areas_export.append({'type': area_type, 'color': color, 'points': points})

    # --- PASS 4: SCAN STANDALONE POIS ---
    print("Scanning Standalone POIs...")
    for osm_id, data in node_db.items():
        tags = data['tags']
        poi_type = get_poi_type(tags)
        if poi_type != POI_TYPE_NONE:
            name = get_name_from_tags(tags)
            add_poi(poi_type, data['x'], data['z'], name)

    # --- [CRITICAL] RE-CENTER MAP TO (0,0) ---
    if not nodes_export:
        print("ERROR: No nodes collected. Check input file or radius settings.")
        return

    print("Calculating Map Center...")
    # nodes_export entries are (idx, x, z, flags)
    min_x = min(n[1] for n in nodes_export)
    max_x = max(n[1] for n in nodes_export)
    min_z = min(n[2] for n in nodes_export)
    max_z = max(n[2] for n in nodes_export)
    
    center_x = (min_x + max_x) / 2.0
    center_z = (min_z + max_z) / 2.0
    
    print(f"Shift applied: X{-center_x:.2f}, Z{-center_z:.2f}")
    
    # Shift Nodes
    for i in range(len(nodes_export)):
        idx, x, z, flags = nodes_export[i]
        nodes_export[i] = (idx, x - center_x, z - center_z, flags)
        
    # Shift Buildings
    for b in buildings_export:
        new_points = []
        for px, pz in b['points']:
            new_points.append((px - center_x, pz - center_z))
        b['points'] = new_points
        
    # Shift Areas
    for a in areas_export:
        new_points = []
        for px, pz in a['points']:
            new_points.append((px - center_x, pz - center_z))
        a['points'] = new_points
        
    # Shift POIs
    for i in range(len(pois_export)):
        p_type, x, z, name = pois_export[i]
        pois_export[i] = (p_type, x - center_x, z - center_z, name)

    # --- WRITE ---
    print(f"Stats: {len(nodes_export)} nodes, {count_roads} roads, {count_buildings} buildings.")
    if not os.path.exists(OUTPUT_DIR): os.makedirs(OUTPUT_DIR)
    
    with open(OUTPUT_FILE, 'w') as f:
        f.write("NODES:\n")
        # Save precision to .1f to save space (10cm accuracy is enough for games)
        for i, x, z, flags in nodes_export:
            f.write(f"{i}: {x:.1f} {z:.1f} {flags}\n")
        
        f.write("\nEDGES:\n")
        for u, v, w, ow, spd, ln in edges_export:
            f.write(f"{u} {v} {w:.1f} {ow} {spd} {ln}\n")
            
        f.write("\nBUILDINGS:\n")
        for b in buildings_export:
            f.write(f"{b['height']:.1f} {b['color'][0]} {b['color'][1]} {b['color'][2]}")
            for px, pz in b['points']:
                f.write(f" {px:.1f} {pz:.1f}")
            f.write("\n")

        f.write("\nAREAS:\n")
        for a in areas_export:
            f.write(f"{a['type']} {a['color'][0]} {a['color'][1]} {a['color'][2]}")
            for px, pz in a['points']:
                f.write(f" {px:.1f} {pz:.1f}")
            f.write("\n")

        f.write("\nL:\n")
        for p_type, x, z, name in pois_export:
            f.write(f"L {p_type} {x:.1f} {z:.1f} {name}\n")

    print(f"SUCCESS! Optimized map saved to {OUTPUT_FILE}")

if __name__ == "__main__":
    convert_osm()