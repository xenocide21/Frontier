#!/usr/bin/env python3
"""
FRONTIER galaxy generator (M16a).

Writes src/galaxy_data.h and src/galaxy_data.c: every star system, its links,
its station and planet, the dock descriptions and the hidden anomalies. The four
systems of the early builds keep their ids and their words: AURORA (0, the UFS
capital), VESTA (1, the independent hub), KESTREL (2, the FSA capital) and CINDER
(3, the Syndicates' home port).

The map is 400 x 240 units: the Core in the west, the Outer Rim in the east. The
Veyr nebula will lie beyond x = 400, past the Rim, in a later build.

Run:  python3 tools/galaxy/gen.py [--png out.png]
The same seed always gives the same galaxy.
"""
import math, random, sys

SEED = 1987
W, H = 400, 240
random.seed(SEED)

UFS, IND, FSA, SYN, PLAYER, NONE, VEYR = 0, 1, 2, 3, 4, 5, 6
OWN_C = {UFS: 'OWN_FEDERATION', IND: 'OWN_INDEPENDENT', FSA: 'OWN_REBEL', SYN: 'OWN_SYNDICATE', NONE: 'OWN_NONE', VEYR: 'OWN_VEYR'}
BANDS = ['BAND_CORE', 'BAND_MID', 'BAND_FRONTIER', 'BAND_RIM']
ECON = ['ECON_AGRICULTURAL', 'ECON_INDUSTRIAL', 'ECON_MINING', 'ECON_MILITARY', 'ECON_SETTLEMENT', 'ECON_TRADE_HUB']
AGR, IND_E, MIN, MIL, SET, HUB = range(6)

NAMES = """ALDERA BRISK CALLOW DENEB ELARA FARROW GALLOWAY HESPER IONA JUNO KARST LUMEN MERIDIAN NADIR
OBERON PALLAS QUILL RHEA SABLE TALLIS UMBRA VANTAGE WICKHAM XANDER YARROW ZEPHYR ASHFORD BELLWETHER
CORVUS DRAKE EMBER FENWICK GRISTLE HALCYON ISOLDE JARROW KELVIN LARKSPUR MORROW NOVAK ORRIN PERDITA
QUORRA RAVEN SOLACE THORNE ULLR VESPER WINTER HADRIAN CASSIA DUSK MIRE BRAKE SHALE TINDER SCORIA
MAGNUS OSPREY PELL RUSTWATER GANTRY LODESTAR TEMPEST HOLLOW""".split()
EMPTY_NAMES = """THE STILLS|DEADWATER|GREY REACH|THE HUSH|COLD DEEP|BLACK SHOAL|THE GAP|NULL POINT|EMPTY QUARTER|
DROWNED LIGHT|THE SILENCE|WIDOW'S WELL|LONG DARK|ASH VOID|THE DRIFT""".replace('\n', '').split('|')

# The four from the early builds: id, name, x, y, owner, security, econ, band
CORES = [
    ('AURORA', 60, 120, UFS, 3, (IND_E, AGR), 0),
    ('VESTA', 150, 88, IND, 2, (HUB, IND_E), 1),
    ('KESTREL', 236, 176, FSA, 1, (MIL, MIN), 2),
    ('CINDER', 338, 112, SYN, 0, (SET, MIN), 3),
]

def band_of(x):
    return 0 if x < 105 else 1 if x < 195 else 2 if x < 290 else 3

# ---- Positions: the cores, then Poisson-disc samples round them ----------------
pts = [(c[1], c[2]) for c in CORES]
tries = 0
while len(pts) < 64 and tries < 20000:
    tries += 1
    x, y = random.uniform(12, W - 12), random.uniform(12, H - 12)
    if all(math.hypot(x - a, y - b) >= 25 for a, b in pts):
        pts.append((x, y))
N = len(pts)

# ---- Owners: nearest capital, weighted; then holes of empty space ---------------
caps = [(60, 120, UFS, 1.0), (236, 176, FSA, 0.8), (338, 112, SYN, 1.1), (150, 88, IND, 1.15)]
owner = []
for i, (x, y) in enumerate(pts):
    if i < 4:
        owner.append(CORES[i][3]); continue
    best = min(caps, key=lambda c: math.hypot(x - c[0], y - c[1]) * c[3] + random.uniform(-8, 8))
    owner.append(best[2])
for i in range(4, N):                     # independent free worlds scattered along the borders
    x, y = pts[i]
    if owner[i] != IND and random.random() < 0.10:
        owner[i] = IND
for i in range(4, N):                     # empty systems: no planet, no port
    d_cap = min(math.hypot(pts[i][0] - c[0], pts[i][1] - c[1]) for c in caps)
    if d_cap > 40 and random.random() < (0.30 if band_of(pts[i][0]) >= 2 else 0.16):
        owner[i] = NONE

# ---- Links: a planar web of short routes, then thinned into chokepoints ---------
def cross(p1, p2, p3, p4):
    def ccw(a, b, c): return (c[1] - a[1]) * (b[0] - a[0]) - (b[1] - a[1]) * (c[0] - a[0])
    if len({p1, p2, p3, p4}) < 4: return False
    return (ccw(p1, p3, p4) * ccw(p2, p3, p4) < 0) and (ccw(p1, p2, p3) * ccw(p1, p2, p4) < 0)

MAXJ = 5
cand = sorted(((math.hypot(pts[a][0] - pts[b][0], pts[a][1] - pts[b][1]), a, b)
               for a in range(N) for b in range(a + 1, N)), key=lambda t: t[0])
edges = set()
deg = [0] * N
for d, a, b in cand:
    if d > 58: break
    if deg[a] >= MAXJ or deg[b] >= MAXJ: continue
    if any(cross(pts[a], pts[b], pts[c], pts[e]) for c, e in edges): continue
    edges.add((a, b)); deg[a] += 1; deg[b] += 1

def connected(es):
    adj = {i: [] for i in range(N)}
    for a, b in es: adj[a].append(b); adj[b].append(a)
    seen, st = {0}, [0]
    while st:
        v = st.pop()
        for w in adj[v]:
            if w not in seen: seen.add(w); st.append(w)
    return len(seen) == N

# Borders become chokepoints; the inside of a territory gets twisty.
order = list(edges); random.shuffle(order)
for a, b in order:
    e = (a, b)
    border = owner[a] != owner[b]
    p = 0.72 if border else 0.38
    if a < 4 or b < 4: p *= 0.5              # capitals stay reachable
    if random.random() < p:
        trial = edges - {e}
        da = sum(1 for x in trial if a in x); db = sum(1 for x in trial if b in x)
        if da >= 1 and db >= 1 and connected(trial):
            if (a < 4 and da < 2) or (b < 4 and db < 2): continue
            edges = trial
assert connected(edges)
adj = {i: [] for i in range(N)}
for a, b in sorted(edges): adj[a].append(b); adj[b].append(a)

# ---- Names ----------------------------------------------------------------------
pool = [n for n in NAMES if n not in ('AURORA', 'VESTA', 'KESTREL', 'CINDER')]
random.shuffle(pool)
epool = EMPTY_NAMES[:]; random.shuffle(epool)
names = []
for i in range(N):
    if i < 4: names.append(CORES[i][0])
    elif owner[i] == NONE: names.append(epool.pop() if epool else pool.pop())
    else: names.append(pool.pop())

def dist_to_cap(i, own):
    for c in caps:
        if c[2] == own: return math.hypot(pts[i][0] - c[0], pts[i][1] - c[1])
    return 999

# ---- Per system ------------------------------------------------------------------
ECON_BY = {UFS: [IND_E, AGR, MIL, HUB, AGR], FSA: [MIL, MIN, MIN, SET, AGR],
           IND: [HUB, AGR, SET, IND_E, MIN], SYN: [SET, MIN, MIN, SET, IND_E]}
STATION_FMT = {UFS: ['{n} STATION', '{n} ORBITAL', '{n} DOCKYARD', 'PORT {n}'],
               FSA: ['FORT {n}', '{n} REDOUBT', 'CAMP {n}', '{n} WATCH'],
               IND: ['{n} EXCHANGE', '{n} FREEPORT', '{n} MARKET', '{n} HALT'],
               SYN: ['{n} OUTPOST', '{n} DEN', '{n} SCRAPYARD', '{n} HOLE']}
PLANET_FMT = ['{n} PRIME', '{n} II', '{n} IV', 'NEW {n}', '{n} MINOR', '{n} V']
RGB = [(50, 110, 170), (150, 120, 70), (120, 70, 55), (95, 95, 100), (60, 130, 80), (140, 60, 120),
       (170, 150, 110), (70, 90, 150), (130, 140, 60), (110, 50, 50)]

DESC = {
 UFS: [["A CLEAN, ORDERLY UFS STATION. FLAGS IN EVERY CONCOURSE.", "CUSTOMS OFFICERS CHECK EVERY MANIFEST TWICE.", "THE FOOD IS BLAND AND THE PATROLS ARE CLOSE."],
       ["A FEDERATION RELAY POST, BUSY WITH CONVOYS.", "POSTERS REMIND YOU THAT LOYALTY IS SAFETY.", "THE BAR SHUTS AT CURFEW. NOBODY ARGUES."],
       ["UFS NAVY CONTRACTORS RUN THIS PORT LIKE A SHIP.", "SPOTLESS DECKS, LONG QUEUES, GOOD REPAIRS.", "A RECRUITING DESK STANDS BY THE AIRLOCK."]],
 FSA: [["AN FSA STRONGPOINT, BUILT FROM WHAT THEY HAD.", "VOLUNTEERS STAND GUARD IN MISMATCHED ARMOUR.", "THEY SHARE WHAT LITTLE THEY HAVE WITH FRIENDS."],
       ["A FREE SYSTEMS OUTPOST ON A DUSTY ORBIT.", "MURALS OF FALLEN PILOTS COVER THE WALLS.", "EVERY CREW HERE HAS LOST SOMEONE TO THE UFS."],
       ["A REBEL SUPPLY DEPOT, ALWAYS SHORT OF PARTS.", "THE QUARTERMASTER BARTERS HARD BUT FAIR.", "SONGS ABOUT FREEDOM DRIFT FROM THE MESS."]],
 IND: [["A FREE PORT OWNED BY NOBODY IN PARTICULAR.", "TRADERS FROM EVERY FACTION DRINK AT ONE BAR.", "THE RULE HERE IS SIMPLE: BUSINESS FIRST."],
       ["A FAMILY-RUN STATION, PATCHED FOR GENERATIONS.", "THE OWNERS KNOW EVERY CAPTAIN BY NAME.", "PRICES ARE FAIR AND THE COFFEE IS REAL."],
       ["A COOPERATIVE OF MINERS AND HAULERS.", "THEY VOTE ON EVERYTHING, EVEN THE MUSIC.", "STRANGERS ARE WELCOME IF THEY PAY THEIR WAY."]],
 SYN: [["A SYNDICATE DEN WELDED FROM DEAD SHIPS.", "EVERYTHING IS FOR SALE, AND SO IS EVERYONE.", "KEEP YOUR HAND NEAR YOUR CREDITS."],
       ["A SMUGGLERS' HOLE IN THE LEE OF A GAS GIANT.", "THE DOCKMASTER TAKES A CUT OF EVERY CARGO.", "NOBODY HERE HAS A REAL NAME."],
       ["A RIM SCRAPYARD THAT GREW INTO A TOWN.", "CUTTING TORCHES BURN ALL NIGHT IN THE BAYS.", "THE SYNDICATE KEEPS THE PEACE, ITS OWN WAY."]],
}
PDESC = [["A WIND-SCOURED WORLD WITH ONE LANDING FIELD.", "THE SETTLERS TRADE WHAT THEY GROW.", "REPAIRS ARE SLOW BUT HONEST."],
         ["A FROZEN MOON OF ICE MINES AND DOMES.", "THE PORT SMELLS OF COOLANT AND COFFEE.", "NO SHIPYARD, BUT THE MARKET IS BUSY."],
         ["A HOT, HAZY WORLD OF REFINERIES.", "FLARE STACKS LIGHT THE NIGHT SIDE.", "THE SPACEPORT SELLS PARTS, NOT SHIPS."],
         ["A GREEN WORLD OF TERRACED FARMS.", "FOOD LEAVES BY THE SHIPLOAD EVERY DAY.", "THE LOCALS ARE WARY OF ARMED SHIPS."],
         ["A ROCKY WORLD RIDDLED WITH OLD SHAFTS.", "CONTRACT CREWS WORK IT IN SHIFTS.", "THE PORT IS A RING OF CRANES AND HANGARS."]]
CORE_PLACES = [
    (["THE AURORA DOCK AUTHORITY RUNS THIS OLD RING STATION.", "YARD, REPAIR BAYS AND A LOUNGE THAT NEVER CLOSES.", "EVERY CAPTAIN IN THE SYSTEM PASSES THROUGH HERE."],
     ["A COLD OCEAN WORLD WITH ONE BIG SPACEPORT.", "MINERS AND FISHERS WORK THE POLAR FLATS.", "NO SHIPYARD, BUT THE PORT FIXES ANYTHING THAT LANDS."]),
    (["VESTA EXCHANGE: THE BUSIEST MARKET ON THE MID BELT.", "BROKERS, SHIPWRIGHTS AND MERCENARY CAPTAINS ALL", "DO BUSINESS IN THE SAME LOUD CONCOURSE."],
     ["A DRY WORLD OF FOUNDRIES AND ASSEMBLY LINES.", "THE SMOKE CAN BE SEEN FROM ORBIT.", "THE SPACEPORT SELLS PARTS, NOT SHIPS."]),
    (["FORT KESTREL: THE FSA'S CAPITAL, HOLDING THE LINE.", "THEY BUY AMMUNITION AND FUEL AT ANY PRICE AND ASK", "FEW QUESTIONS ABOUT WHERE YOU CAME FROM."],
     ["A RUSTED MINING WORLD, WORKED BY CONTRACT CREWS.", "ORE LEAVES BY THE SHIPLOAD. FOOD AND PARTS ARE SHORT.", "THE SPACEPORT IS A PATCHED FIELD OF CRANES."]),
    (["CINDER OUTPOST: A BOLTED-TOGETHER SETTLEMENT ON THE RIM.", "THE OUTER RIM SYNDICATES TAKE A CUT OF EVERYTHING; THE LOUNGE", "IS WHERE THE REAL BUSINESS IS DONE."],
     ["A GREY, AIRLESS ROCK DOTTED WITH MINING DOMES.", "NOBODY ASKS WHOSE SHIP YOU ARE FLYING.", "REPAIRS ARE FAST AND NOT ALWAYS GENTLE."]),
]
CORE_PORTS = [("HOME STATION", "AURORA"), ("VESTA EXCHANGE", "VESTA PRIME"), ("FORT KESTREL", "KESTREL IV"), ("CINDER OUTPOST", "CINDER")]
ANOMALIES = ["A DERELICT SURVEY PROBE", "A FROZEN GENERATION SHIP", "A RINGED ROGUE ASTEROID", "A DEAD VEYR HUSK",
             "A SILENT LISTENING POST", "THE WRECK OF A UFS CRUISER", "A CRYSTAL DUST CLOUD", "AN ABANDONED SYNDICATE CACHE",
             "A TUMBLING ALIEN MONOLITH", "A GHOST BEACON", "A SHATTERED MOONLET", "A LOST COLONY BARGE"]

systems = []
for i in range(N):
    x, y = pts[i]
    own = owner[i]
    rec = {'name': names[i], 'x': int(round(x)), 'y': int(round(y)), 'owner': own, 'band': band_of(x)}
    if i < 4:
        c = CORES[i]
        rec.update(security=c[4], econ=c[5], band=c[6], station=CORE_PORTS[i][0], planet=CORE_PORTS[i][1])
    elif own == NONE:
        rec.update(security=0, econ=(SET, SET), station=None, planet=None)
    else:
        dcap = dist_to_cap(i, own)
        base_sec = {UFS: 3, FSA: 2, IND: 2, SYN: 0}[own]
        sec = max(0, base_sec - (1 if dcap > 70 else 0) - (1 if dcap > 130 else 0))
        e = random.sample(ECON_BY[own], 2)
        n = names[i]
        rec.update(security=sec, econ=(e[0], e[1]), station=random.choice(STATION_FMT[own]).format(n=n),
                   planet=random.choice(PLANET_FMT).format(n=n))
    systems.append(rec)

# ---- Pirate activity (M16c): rare at home, worse toward the edges ------------------
# Hops from each territory's capital, travelling only through that territory. A
# system its owner can't reach through its own space is an enclave: cut off from
# the navy, pirates thrive there.
from collections import deque
CAPITAL = {UFS: 0, FSA: 2, IND: 1, SYN: 3}
def own_hops(i):
    own = systems[i]['owner']
    if own not in CAPITAL: return None
    start = CAPITAL[own]
    dist = {start: 0}; q = deque([start])
    while q:
        v = q.popleft()
        for w in adj[v]:
            if w not in dist and systems[w]['owner'] == own:
                dist[w] = dist[v] + 1; q.append(w)
    return dist.get(i)          # None: an enclave

activity = []
for i, s in enumerate(systems):
    own = s['owner']
    h = own_hops(i)
    if own == NONE: a = 6
    elif own == SYN: a = 5
    elif h is None: a = 3 if own == IND else 5             # cut off from home (free ports fend for themselves)
    elif own == UFS: a = min(1 + h, 4)
    elif own == FSA: a = min(2 + h, 4)
    else: a = min(2 + h // 2, 4)                          # independents
    activity.append(a)
    if i >= 4 and own in (UFS, FSA):                      # security follows: the navy can't hold an enclave
        if h is None: s['security'] = 1 if own == UFS else 0
        else: s['security'] = max(0, (3 if own == UFS else 2) - (h + 1) // 2)

# ---- The Veyr nebula (M17a): five hidden systems past the Rim -------------------
# Their links stay closed until the hive's position is found. Two Rim systems sit
# on the edge of the nebula: that is where the Veyr come through.
NEBULA = [('THE VEIL', 418, 150), ('CHITIN DRIFT', 440, 104), ('BROOD REACH', 446, 196),
          ('SPAWNING DEEP', 462, 148), ('VEYR PRIME', 472, 96)]
def nearest(x, y):
    return min(range(N), key=lambda i: math.hypot(systems[i]['x'] - x, systems[i]['y'] - y) if systems[i]['owner'] != NONE else 1e9)
rim_a = nearest(400, 140)               # the Veil's door
rim_b = nearest(395, 215)               # Brood Reach's door
NEB0 = N
for k, (nm, x, y) in enumerate(NEBULA):
    systems.append({'name': nm, 'x': x, 'y': y, 'owner': VEYR, 'band': 3, 'security': 0, 'econ': (SET, SET),
                    'station': ('THE HIVE MIND' if nm == 'VEYR PRIME' else nm + ' SPIRE'),
                    'planet': (nm if nm == 'VEYR PRIME' else nm + ' NEST'), 'hidden': 1})
    adj[NEB0 + k] = []
    activity.append(6)
def link(a, b):
    adj[a].append(b); adj[b].append(a); edges.add((min(a, b), max(a, b)))
link(rim_a, NEB0 + 0); link(rim_b, NEB0 + 2)
link(NEB0 + 0, NEB0 + 1); link(NEB0 + 0, NEB0 + 2); link(NEB0 + 1, NEB0 + 3); link(NEB0 + 2, NEB0 + 3); link(NEB0 + 3, NEB0 + 4)
link(NEB0 + 1, NEB0 + 4)
N_ALL = len(systems)
VEYR_PLACES = [
    ["A SPIRE OF VEYR CHITIN, STILL WARM.", "THE CORRIDORS BREATHE. SOMETHING SKITTERS BELOW.", "YOUR CREW REFUSE TO SLEEP HERE."],
    ["A DEAD NEST WORLD UNDER A GREEN SKY.", "RIDGES OF RESIN CRUST OVER OLD BURROWS.", "THE PORT IS WHATEVER YOU BUILT ON IT."]]

def place_svcs(i):
    s = systems[i]
    if s['station'] is None: return ('0', '0')
    if s.get('hidden'):
        if s['name'] == 'VEYR PRIME':                     # the hive's yard (M17b): Veyr hulls once it is yours
            return ('(SVC_REPAIR | SVC_SHIPYARD | SVC_TRADE)', '(SVC_REPAIR | SVC_TRADE)')
        return ('(SVC_REPAIR | SVC_TRADE)', '(SVC_REPAIR | SVC_TRADE)')
    own = s['owner']
    if i in (0, 1, 3): st = 'STATION_ALL'
    elif i == 2: st = 'NO_SHIPS'
    else:
        yard = random.random() < (0.45 if s['security'] >= 2 else 0.3)
        st = 'STATION_ALL' if yard else ('NO_SHIPS' if random.random() < 0.8 else '(SVC_REPAIR | SVC_TRADE)')
    pl = 'NO_SHIPS'
    if own == SYN:
        st += ' | SVC_BLACK'; pl += ' | SVC_BLACK'
    return (st, pl)

# ---- Output ----------------------------------------------------------------------
def cstr(s): return 'NULL' if s is None else '"' + s.replace('"', '\\"') + '"'

out = []
out.append('/* Generated by tools/galaxy/gen.py (seed %d). Do not edit: change the script and run it again. */' % SEED)
out.append('#include <stddef.h>\n#include "galaxy.h"\n#include "place.h"\n#include "world.h"\n')
out.append('#define STATION_ALL (SVC_REPAIR | SVC_SHIPYARD | SVC_TRADE | SVC_LOUNGE | SVC_GUILD)')
out.append('#define NO_SHIPS    (SVC_REPAIR | SVC_TRADE | SVC_LOUNGE)\n')
out.append('const StarSystem galaxy_start[GALAXY_SYSTEMS] = {')
anom = []
for i, s in enumerate(systems):
    links = adj[i]
    hid = s.get('hidden', 0)
    # station at the origin; the planet off to one side; arrival points toward each neighbour
    if hid:
        px, py, rad = [(1200, -900, 170), (-1300, 700, 150), (900, 1300, 160), (-1100, -1100, 180), (1400, 600, 260)][i - NEB0]
    elif s['station']:
        pa = random.uniform(0, 2 * math.pi)
        pr = random.randint(1300, 1700)
        rad = random.randint(110, 180)
        px, py = int(pr * math.cos(pa)), int(pr * math.sin(pa))
        if i < 4:
            px, py, rad = [(1500, -700, 180), (-1400, 800, 150), (300, -1600, 160), (1500, 1200, 120)][i]
    else:
        px, py, rad = 30000, 30000, 0
    jx, jy = [], []
    for k in links:
        a = math.atan2(systems[k]['y'] - s['y'], systems[k]['x'] - s['x'])
        for t in range(12):
            r = 2000
            x, y = int(r * math.cos(a)), int(r * math.sin(a))
            if math.hypot(x - px, y - py) > rad + 650: break
            a += 0.4
        jx.append(x); jy.append(y)
    while len(jx) < MAXJ: jx.append(0); jy.append(0)
    rgb = RGB[i % len(RGB)] if i >= 4 else [(50, 110, 170), (150, 120, 70), (120, 70, 55), (95, 95, 100)][i]
    if hid: rgb = (90, 150, 60)                         # Veyr worlds: a sick green
    pir = activity[i]                                    # pirate activity 0..6 (M16c)
    mer = 0 if not s['station'] else 2
    base = 'NAV_PLANET' if s['owner'] in (FSA, SYN, VEYR) else 'NAV_STATION'
    out.append('    {   /* %d */' % i)
    out.append('        %s, %d, %d, %s, %s, %d,' % (cstr(s['name']), s['x'], s['y'], BANDS[s['band']], OWN_C[s['owner']], s['security']))
    out.append('        { %s, %s },' % (ECON[s['econ'][0]], ECON[s['econ'][1]]))
    out.append('        %d, { %s },' % (len(links), ', '.join(str(k) for k in links) or '0'))
    out.append('        %s, %s,' % (cstr(s['station']), cstr(s['planet'])))
    out.append('        %d, %d, %d,' % (px, py, rad))
    out.append('        { %s }, { %s },' % (', '.join(map(str, jx)), ', '.join(map(str, jy))))
    out.append('        { %d, %d, %d },' % rgb)
    if hid: mer = 0
    out.append('        %d, %d, %s, %d' % (pir, mer, base, hid))
    out.append('    },')
    ax, ay = 0, 0
    if hid:
        anom.append((-2600, 2200, 'A VEYR BROOD CHAMBER')); continue
    while True:
        a = random.uniform(0, 2 * math.pi); r = random.randint(2300, 3000)
        ax, ay = int(r * math.cos(a)), int(r * math.sin(a))
        if math.hypot(ax - px, ay - py) > rad + 600 and all(math.hypot(ax - x, ay - y) > 500 for x, y in zip(jx, jy)): break
    anom.append((ax, ay, ANOMALIES[i % len(ANOMALIES)] if i >= 4 else ANOMALIES[i]))
out.append('};\n')

out.append('const Place galaxy_places[GALAXY_SYSTEMS][2] = {')
for i, s in enumerate(systems):
    st, pl = place_svcs(i)
    if s['station'] is None:
        out.append('    { { NAV_STATION, 0, ART_STATION, { "", "", "" } }, { NAV_PLANET, 0, ART_SPACEPORT, { "", "", "" } } },   /* %s: empty */' % s['name'])
        continue
    if s.get('hidden'): d1, d2 = VEYR_PLACES
    elif i < 4: d1, d2 = CORE_PLACES[i]
    else: d1, d2 = random.choice(DESC[s['owner']]), random.choice(PDESC)
    out.append('    {   /* %s */' % s['name'])
    out.append('        { NAV_STATION, %s, ART_STATION, { %s } },' % (st, ', '.join(cstr(t) for t in d1)))
    out.append('        { NAV_PLANET, %s, ART_SPACEPORT, { %s } }' % (pl, ', '.join(cstr(t) for t in d2)))
    out.append('    },')
out.append('};\n')
out.append('const s16 galaxy_anomaly_xy[GALAXY_SYSTEMS][2] = {')
out.append('    ' + ', '.join('{ %d, %d }' % (a[0], a[1]) for a in anom))
out.append('};')
out.append('const char *const galaxy_anomaly_name[GALAXY_SYSTEMS] = {')
out.append('    ' + ',\n    '.join(cstr(a[2]) for a in anom))
out.append('};')

root = __file__.rsplit('/tools/', 1)[0]
open(root + '/src/galaxy_data.c', 'w').write('\n'.join(out) + '\n')
open(root + '/src/galaxy_data.h', 'w').write(
    '/* Generated by tools/galaxy/gen.py. */\n#ifndef FRONTIER_GALAXY_DATA_H\n#define FRONTIER_GALAXY_DATA_H\n'
    '#define GALAXY_SYSTEMS %d\n#define SYS_MAX_JUMPS  %d\n#define VEYR_PRIME     %d   /* the hive world */\n'
    '#define VEYR_NEBULA    %d   /* the first nebula system (the Veil) */\n#define VEYR_DOOR_A    %d\n#define VEYR_DOOR_B    %d\n#endif\n'
    % (N_ALL, MAXJ, NEB0 + 4, NEB0, rim_a, rim_b))

cnt = {}
for s in systems: cnt[s['owner']] = cnt.get(s['owner'], 0) + 1
print('systems', N, 'links', len(edges), 'owners', {k: cnt[k] for k in sorted(cnt)})
for i, s in enumerate(systems[:N]):
    print('  %-14s own %d sec %d pirates %d hops %s' % (s['name'], s['owner'], s['security'], activity[i], own_hops(i)))

if '--png' in sys.argv:
    from PIL import Image, ImageDraw
    sc = 3
    im = Image.new('RGB', (W * sc + 40, H * sc + 40), (8, 10, 20))
    d = ImageDraw.Draw(im)
    col = {UFS: (90, 170, 255), IND: (120, 230, 140), FSA: (230, 80, 210), SYN: (255, 90, 90), NONE: (110, 110, 110), VEYR: (140, 220, 60)}
    W = 490
    im = Image.new('RGB', (W * sc + 40, H * sc + 40), (8, 10, 20)); d = ImageDraw.Draw(im)
    for a, b in edges:
        d.line([(20 + systems[a]['x'] * sc, 20 + systems[a]['y'] * sc), (20 + systems[b]['x'] * sc, 20 + systems[b]['y'] * sc)], fill=(60, 80, 120), width=2)
    for i, s in enumerate(systems):
        x, y = 20 + s['x'] * sc, 20 + s['y'] * sc
        r = 7 if i < 4 else 4
        d.ellipse([x - r, y - r, x + r, y + r], fill=col[s['owner']])
        d.text((x - 12, y + 6), s['name'], fill=(200, 205, 215))
    im.save(sys.argv[sys.argv.index('--png') + 1])
