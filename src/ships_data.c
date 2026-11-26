/*
 * FRONTIER - ship definitions.
 *
 * Placeholder numbers, to be tuned by play-testing. Speeds are FP12 pixels per
 * frame at 60 frames per second:
 *
 *   max_speed 12288 = 3.0 px/frame = 180 px/s
 *   accel       164 = 0.04 px/frame^2  (full speed in about 75 frames)
 *   turn_rate    36 = 36/4096 of a turn per frame (one turn in about 1.9 s)
 *   shield_arc_max  shield points in each of the four arcs (front, right, back, left)
 *   shield_recharge points per second per arc once recharging has started
 */
#include "ship.h"
#include "weapon.h"

const ShipDef ship_defs[SHIP_DEF_COUNT] = {
    /* id cls                  mk sprite hull shield rech eng maxspd accel turn reactor cargo crew fuel wpn util mass price brakes */
    {  0, CLASS_TRANSPORT,     0,  0,   120,   40,   8,   40,  12288,  164, 36, 100,   12,  1,  20, 1, 1, 400,   4000, 0 },  /* WAYFARER (the starting ship) */
    {  1, CLASS_LIGHT_FIGHTER, 0,  1,    80,   50,  10,   90,  20480,  410, 80, 140,    2,  1,  15, 2, 1, 300,   9000, 0 },  /* RAIDER */
    {  2, CLASS_FREIGHTER,     0,  2,   300,   80,   6,   20,   8192,   82, 16, 160,   60,  3,  40, 1, 2, 900,  22000, 4 },  /* HAULER */
    {  3, CLASS_TRANSPORT      , 0,  3,   125,    41,   5,   60,  12320,  160, 36, 100,   12,  2,  20, 1, 2, 500,   4250, 0 },  /* FRONTIER RUNNER */
    {  4, CLASS_SCOUT          , 0,  4,    85,    50,   6,   60,  17360,  240, 60, 100,    3,  2,  24, 1, 2, 500,   5600, 0 },  /* WAYFINDER */
    {  5, CLASS_MINING_VESSEL  , 0,  5,   325,    83,   7,   60,   9240,   90, 18, 100,   40,  4,  40, 1, 3, 500,  26000, 4 },  /* ROCKBREAKER */
    {  6, CLASS_SALVAGE_VESSEL , 0,  6,   400,   100,   8,   60,   8680,   80, 16, 100,   40,  5,  40, 1, 3, 500,  39000, 4 },  /* RECLAIMER */
    {  7, CLASS_SCOUT          , 0,  7,   155,    86,   7,   60,  19320,  280, 70, 100,    4,  3,  30, 2, 2, 500,   19250, 0 },  /* UFS WATCHMAN */
    {  8, CLASS_LIGHT_FIGHTER  , 0,  8,    97,    70,   6,   60,  22960,  380, 88, 100,    1,  1,  15, 2, 1, 500,   12250, 0 },  /* UFS TALON */
    {  9, CLASS_HEAVY_FIGHTER  , 0,  9,   212,   130,   9,   60,  17640,  240, 60, 100,    2,  2,  20, 3, 1, 500,   23250, 0 },  /* UFS VANGUARD */
    { 10, CLASS_CORVETTE       , 0, 10,  1050,   466,  22,   60,  12320,  110, 14, 100,   60, 30,  50, 3, 3, 500,  240000, 4 },  /* UFS SENTINEL */
    { 11, CLASS_FRIGATE        , 0, 11,  2875,  1416,  40,   60,   9240,   56,  9, 100,  120, 100,  60, 4, 4, 500,  925000, 4 },  /* UFS BASTION */
    { 12, CLASS_DESTROYER      , 0, 12,  6000,  3000,  40,   60,   7560,   36,  6, 100,  200, 250,  80, 5, 5, 500, 2600000, 4 },  /* UFS RESOLUTE */
    { 13, CLASS_CRUISER        , 0, 13, 12000,  7000,  40,   60,   6440,   24,  4, 100,  250, 250, 100, 6, 6, 500, 6250000, 3 },  /* UFS SOVEREIGN */
    { 14, CLASS_SCOUT, 0, 14, 170, 73, 6, 60, 19320, 280, 70, 100, 4, 2, 30, 1, 3, 500, 18300, 0 },  /* FSA GHOST */
    { 15, CLASS_LIGHT_FIGHTER, 0, 15, 107, 60, 5, 60, 22960, 380, 90, 100, 1, 1, 15, 2, 1, 500, 11650, 0 },  /* FSA WASP */
    { 16, CLASS_HEAVY_FIGHTER, 0, 16, 233, 110, 8, 60, 18200, 220, 58, 100, 2, 2, 20, 3, 1, 500, 22100, 0 },  /* FSA HAMMER */
    { 17, CLASS_CORVETTE, 0, 17, 1155, 396, 19, 60, 12880, 104, 14, 100, 70, 35, 50, 3, 3, 500, 228000, 4 },  /* FSA RENEGADE */
    { 18, CLASS_FRIGATE, 0, 18, 3163, 1204, 34, 60, 9800, 54, 9, 100, 140, 110, 60, 4, 4, 500, 878750, 4 },  /* FSA DEFIANT */
    { 19, CLASS_DESTROYER, 0, 19, 6600, 2550, 34, 60, 8120, 34, 6, 100, 200, 250, 80, 5, 5, 500, 2470000, 4 },  /* FSA REPRISAL */
    { 20, CLASS_CRUISER, 0, 20, 13200, 5950, 34, 60, 6720, 22, 4, 100, 250, 250, 100, 6, 6, 500, 5937500, 3 },  /* FSA LIBERTY */
    { 21, CLASS_SCOUT          , 0, 21,   162,    75,   7,   60,  21840,  440, 95, 100,    0,  0,  30, 2, 0, 500,   40000, 0 },  /* VEYR SKITTER */
    { 22, CLASS_LIGHT_FIGHTER  , 0, 22,   187,    83,   7,   60,  23520,  480, 100, 100,    0,  0,  30, 2, 0, 500,   50000, 0 },  /* VEYR MANTIS */
    { 23, CLASS_HEAVY_FIGHTER  , 0, 23,   462,   208,  12,   60,  17360,  260, 62, 100,    0,  0,  30, 3, 0, 500,  120000, 0 },  /* VEYR REAVER */
    { 24, CLASS_FRIGATE        , 0, 24,  8500,  3500,  40,   60,   8120,   50,  9, 100,    0,  0,  60, 4, 0, 500, 3000000, 4 },  /* VEYR BROODGUARD */
    { 25, CLASS_CRUISER        , 0, 25, 15000,  6666,  40,   60,   6160,   20,  3, 100,    0,  0, 100, 6, 0, 500, 9000000, 3 },  /* VEYR LEVIATHAN */
    /* Outer Rim Syndicate: welded from scrap. Tough hulls, weak shields, slower than the originals. */
    { 26, CLASS_LIGHT_FIGHTER  , 0, 26,   110,    40,   6,   60,  20000,  380, 82, 100,    3,  1,  15, 2, 1, 500,    8000, 0 },  /* ORS SCRAPPER */
    { 27, CLASS_HEAVY_FIGHTER  , 0, 27,   280,    90,   7,   60,  16500,  220, 54, 100,   10,  2,  22, 3, 2, 500,   20000, 0 },  /* ORS JACKAL */
    { 28, CLASS_CORVETTE       , 0, 28,  1400,   320,  15,   60,  11800,  100, 13, 100,   90, 30,  50, 3, 3, 500,  210000, 4 },  /* ORS CHOPSHOP */
    { 29, CLASS_FRIGATE        , 0, 29,  3800,  1000,  30,   60,   8800,   50,  8, 100,  160, 100,  60, 4, 4, 500,  850000, 4 },  /* ORS MARAUDER */
};

/* radius: hit circle and muzzle distance (px); tail: centre to engines on the sprite. */
const ShipLook ship_looks[SHIP_DEF_COUNT] = {
    { 16,  90, 200, 255, "WAYFARER",        24 },
    { 12, 255,  90,  90, "RAIDER",          18 },
    { 22, 200, 180,  90, "HAULER",          30 },
    {  15, 200, 200, 200, "FRONTIER RUNNER",  22 },
    {  13, 200, 200, 200, "WAYFINDER",  18 },
    {  19, 200, 200, 200, "ROCKBREAKER",  27 },
    {  19, 200, 200, 200, "RECLAIMER",  28 },
    {  14, 200, 200, 200, "UFS WATCHMAN",  20 },
    {  10, 200, 200, 200, "UFS TALON",  13 },
    {  13, 200, 200, 200, "UFS VANGUARD",  18 },
    {  30, 200, 200, 200, "UFS SENTINEL",  44 },
    {  40, 200, 200, 200, "UFS BASTION",  59 },
    {  59, 200, 200, 200, "UFS RESOLUTE",  88 },
    {  82, 200, 200, 200, "UFS SOVEREIGN", 123 },
    {  13, 200, 200, 200, "FSA GHOST",  18 },
    {  10, 200, 200, 200, "FSA WASP",  12 },
    {  13, 200, 200, 200, "FSA HAMMER",  18 },
    {  31, 200, 200, 200, "FSA RENEGADE",  46 },
    {  40, 200, 200, 200, "FSA DEFIANT",  60 },
    {  61, 200, 200, 200, "FSA REPRISAL",  91 },
    {  81, 200, 200, 200, "FSA LIBERTY", 121 },
    {  11, 200, 200, 200, "VEYR SKITTER",  15 },
    {  13, 200, 200, 200, "VEYR MANTIS",  18 },
    {  17, 200, 200, 200, "VEYR REAVER",  25 },
    {  40, 200, 200, 200, "VEYR BROODGUARD",  60 },
    {  82, 200, 200, 200, "VEYR LEVIATHAN", 123 },
    {  13, 200, 200, 200, "ORS SCRAPPER",  18 },
    {  14, 200, 200, 200, "ORS JACKAL",  21 },
    {  31, 200, 200, 200, "ORS CHOPSHOP",  50 },
    {  42, 200, 200, 200, "ORS MARAUDER",  62 },
};

const ShipLoadout ship_loadouts[SHIP_DEF_COUNT] = {
    /* primary, missile rack, rockets, seekers, ion, turrets */
    { WEAPON_LASER, WEAPON_ROCKET, 6, 3, 2, 1 },
    { WEAPON_LASER, WEAPON_ROCKET, 10, 4, 2, 0 },
    { WEAPON_LASER, WEAPON_NONE, 0, 0, 0, 2 },
    { WEAPON_LASER, WEAPON_ROCKET, 6, 2, 2, 1 },
    { WEAPON_LASER, WEAPON_ROCKET, 4, 3, 1, 0 },
    { WEAPON_LASER, WEAPON_NONE, 0, 0, 0, 2 },
    { WEAPON_LASER, WEAPON_NONE, 0, 0, 0, 2 },
    { WEAPON_LASER, WEAPON_ROCKET, 6, 4, 2, 0 },
    { WEAPON_LASER, WEAPON_ROCKET, 8, 4, 2, 0 },
    { WEAPON_LASER, WEAPON_ROCKET, 10, 6, 2, 0 },
    { WEAPON_LASER, WEAPON_ROCKET, 20, 8, 4, 3 },
    { WEAPON_LASER, WEAPON_ROCKET, 30, 12, 6, 4 },
    { WEAPON_LASER, WEAPON_ROCKET, 40, 16, 8, 5 },
    { WEAPON_LASER, WEAPON_ROCKET, 60, 20, 10, 6 },
    { WEAPON_LASER, WEAPON_ROCKET, 6, 4, 3, 0 },
    { WEAPON_LASER, WEAPON_ROCKET, 8, 3, 2, 0 },
    { WEAPON_LASER, WEAPON_ROCKET, 12, 4, 2, 0 },
    { WEAPON_LASER, WEAPON_ROCKET, 24, 8, 4, 3 },
    { WEAPON_LASER, WEAPON_ROCKET, 30, 10, 6, 4 },
    { WEAPON_LASER, WEAPON_ROCKET, 40, 14, 8, 5 },
    { WEAPON_LASER, WEAPON_ROCKET, 60, 18, 10, 6 },
    { WEAPON_LASER, WEAPON_NONE, 0, 0, 0, 0 },
    { WEAPON_LASER, WEAPON_NONE, 0, 0, 0, 0 },
    { WEAPON_LASER, WEAPON_NONE, 0, 0, 0, 2 },
    { WEAPON_LASER, WEAPON_NONE, 0, 0, 0, 4 },
    { WEAPON_LASER, WEAPON_NONE, 0, 0, 0, 6 },
    { WEAPON_LASER, WEAPON_ROCKET, 8, 2, 1, 0 },
    { WEAPON_LASER, WEAPON_ROCKET, 12, 4, 2, 0 },
    { WEAPON_LASER, WEAPON_ROCKET, 24, 8, 4, 3 },
    { WEAPON_LASER, WEAPON_ROCKET, 30, 10, 6, 4 },
};

/* Sold at shipyards (civilian hulls only for now; military and Veyr ships come by capture). */
const u8 ship_buyable[SHIP_DEF_COUNT] = { 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

/* Pirate ships (AI) carry only a weak laser. */
const ShipLoadout enemy_loadout = { WEAPON_PIRATE_LASER, WEAPON_NONE, 0, 0, 0, 0 };

/* Merchant ships are unarmed. */
const ShipLoadout unarmed_loadout = { WEAPON_NONE, WEAPON_NONE, 0, 0, 0, 0 };
