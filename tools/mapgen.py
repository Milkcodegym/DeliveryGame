import xml.etree.ElementTree as ET
import math
import random
import os

# --- CONFIGURATION ---
OUTPUT_DIR = os.path.join("..", "resources", "maps") 
OUTPUT_FILE = os.path.join(OUTPUT_DIR, "smaller_city.map")
INPUT_FILE = "smaller.osm" 

SCALE = 111139.0 

# --- CRITICAL SETTINGS ---
MAP_RADIUS_LIMIT = 2500.0 
POI_RADIUS_LIMIT = 3000.0

# Optimizations
EXCLUDE_SERVICE_ROADS = True
MIN_BUILDING_AREA = 10.0 
SIMPLIFY_TOLERANCE = 0.3  # Restored to save space

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

# [NEW] Convex Hull Algorithm (Monotone Chain)
# Forces messy building points into a clean, convex polygon.
def convex_hull(points):
    if len(points) <= 2: return points
    # Sort by X, then Y
    points = sorted(set(points), key=lambda p: (p[0], p[1]))
    
    def cross(o, a, b):
        return (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0])

    lower = []
    for p in points:
        while len(lower) >= 2 and cross(lower[-2], lower[-1], p) <= 0:
            lower.pop()
        lower.append(p)

    upper = []
    for p in reversed(points):
        while len(upper) >= 2 and cross(upper[-2], upper[-1], p) <= 0:
            upper.pop()
        upper.append(p)

    return lower[:-1] + upper[:-1]

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

def clean_name(name):
    if not name: return ""
    return name.replace(" ", "_").encode('ascii', 'ignore').decode('ascii')

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

def parse_width_tag(width_str):
    if not width_str: return None
    try:
        clean_str = width_str.replace('m', '').replace(' ', '').replace(',', '.')
        return float(clean_str)
    except ValueError: return None

def convert_osm():
    if not os.path.exists(INPUT_FILE):
        print(f"ERROR: '{INPUT_FILE}' not found.")
        return

    print("Parsing XML...")
    tree = ET.parse(INPUT_FILE)
    root = tree.getroot()

    first_node = root.find('node')
    if first_node is None: return
    origin_lat = float(first_node.attrib['lat'])
    origin_lon = float(first_node.attrib['lon'])

    node_db = {}
    print("Loading Nodes...")
    for node in root.findall('node'):
        osm_id = node.attrib['id']
        lat = float(node.attrib['lat'])
        lon = float(node.attrib['lon'])
        x, z = gps_to_local(lat, lon, origin_lat, origin_lon)
        tags = {tag.attrib['k']: tag.attrib['v'] for tag in node.findall('tag')}
        node_db[osm_id] = {'x': x, 'z': z, 'tags': tags}

    nodes_export = []
    node_id_map = {}
    edges_export = []
    buildings_export = []
    pois_export = []
    
    def register_node(osm_id):
        if osm_id not in node_id_map:
            if osm_id not in node_db: return -1
            d = node_db[osm_id]
            idx = len(nodes_export)
            nodes_export.append([idx, d['x'], d['z'], 0])
            node_id_map[osm_id] = idx
        return node_id_map[osm_id]

    print("Scanning POIs...")
    for osm_id, data in node_db.items():
        tags = data['tags']
        ptype = get_poi_type(tags)
        if ptype != POI_TYPE_NONE:
            name = get_name_from_tags(tags)
            pois_export.append([ptype, data['x'], data['z'], name])

    print("Processing Geometry...")
    for way in root.findall('way'):
        tags = {tag.attrib['k']: tag.attrib['v'] for tag in way.findall('tag')}
        refs = [nd.attrib['ref'] for nd in way.findall('nd')]
        
        if 'highway' in tags:
            rtype = tags['highway']
            if rtype in ['footway', 'path', 'steps', 'pedestrian', 'corridor', 'cycleway', 'track']: continue
            if EXCLUDE_SERVICE_ROADS and rtype == 'service': continue

            width = 6.0
            if rtype == 'residential': width = 5.0
            elif rtype == 'tertiary': width = 7.0
            elif rtype == 'secondary': width = 9.0
            elif rtype == 'primary': width = 12.0
            
            segment = []
            for r in refs:
                idx = register_node(r)
                if idx != -1: segment.append(idx)
                else: 
                    if len(segment) > 1:
                        for i in range(len(segment)-1):
                            edges_export.append([segment[i], segment[i+1], width, 0, 50, 1])
                    segment = []
            if len(segment) > 1:
                for i in range(len(segment)-1):
                    edges_export.append([segment[i], segment[i+1], width, 0, 50, 1])

        elif 'building' in tags or 'building:part' in tags:
            pts = []
            for r in refs:
                if r in node_db: pts.append((node_db[r]['x'], node_db[r]['z']))
            
            if len(pts) < 3: continue
            
            # [FIX] Use Convex Hull to fix geometry order for simple buildings
            # This fixes "bow tie" or twisted polygons that look like triangles
            if len(pts) < 10: 
                pts = convex_hull(pts)
            
            if calculate_area(pts) < MIN_BUILDING_AREA: continue
            
            # Re-enable simplification to fix file size
            pts = simplify_points(pts, SIMPLIFY_TOLERANCE)

            height = 8.0 + random.random()*4.0
            if 'building:levels' in tags:
                try: height = float(tags['building:levels']) * 3.5
                except: pass
            elif 'height' in tags:
                 try: height = float(tags['height'].replace('m',''))
                 except: pass
            
            buildings_export.append({'h': height, 'col': (200,200,200), 'pts': pts})
            
            ptype = get_poi_type(tags)
            if ptype != POI_TYPE_NONE:
                cx = sum(p[0] for p in pts)/len(pts)
                cz = sum(p[1] for p in pts)/len(pts)
                pois_export.append([ptype, cx, cz, get_name_from_tags(tags)])

    print("Centering and Culling...")
    if not nodes_export: return

    avg_x = sum(n[1] for n in nodes_export) / len(nodes_export)
    avg_z = sum(n[2] for n in nodes_export) / len(nodes_export)
    
    for n in nodes_export:
        n[1] -= avg_x
        n[2] -= avg_z

    valid_node_indices = set()
    r_sq = MAP_RADIUS_LIMIT * MAP_RADIUS_LIMIT
    
    for n in nodes_export:
        if (n[1]*n[1] + n[2]*n[2]) < r_sq:
            valid_node_indices.add(n[0]) 

    final_buildings = []
    for b in buildings_export:
        new_pts = [(p[0]-avg_x, p[1]-avg_z) for p in b['pts']]
        if (new_pts[0][0]**2 + new_pts[0][1]**2) < r_sq:
            b['pts'] = new_pts
            final_buildings.append(b)
    
    final_edges = []
    for e in edges_export:
        if e[0] in valid_node_indices or e[1] in valid_node_indices:
            final_edges.append(e)

    final_pois = []
    poi_r_sq = POI_RADIUS_LIMIT * POI_RADIUS_LIMIT
    
    for p in pois_export:
        p[1] -= avg_x
        p[2] -= avg_z
        if (p[1]**2 + p[2]**2) < poi_r_sq:
            final_pois.append(p)

    print(f"Writing File: {len(final_buildings)} blds, {len(final_edges)} roads.")
    
    if not os.path.exists(OUTPUT_DIR): os.makedirs(OUTPUT_DIR)
    with open(OUTPUT_FILE, 'w') as f:
        f.write("NODES:\n")
        # [RESTORED] Using .1f precision to balance size vs accuracy
        for n in nodes_export:
            f.write(f"{n[0]}: {n[1]:.1f} {n[2]:.1f} {n[3]}\n")
            
        f.write("\nEDGES:\n")
        for e in final_edges:
            f.write(f"{e[0]} {e[1]} {e[2]:.1f} {e[3]} {e[4]} {e[5]}\n")
            
        f.write("\nBUILDINGS:\n")
        for b in final_buildings:
            f.write(f"{b['h']:.1f} {b['col'][0]} {b['col'][1]} {b['col'][2]}")
            for p in b['pts']: f.write(f" {int(p[0])} {int(p[1])}")
            f.write("\n")
            
        f.write("\nAREAS:\n\nL:\n") 
        for p in final_pois:
            f.write(f"L {p[0]} {p[1]:.1f} {p[2]:.1f} {p[3]}\n")

    print(f"Done! Saved to {OUTPUT_FILE}")

if __name__ == "__main__":
    convert_osm()