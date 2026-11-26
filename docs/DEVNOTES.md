# FRONTIER – Milestone 13c: module tiers, blueprints and fleet refits

A PlayStation 1 homebrew project written in C with
[PSn00bSDK](https://github.com/Lameguy64/PSn00bSDK). This milestone is the
foundation for everything else: one ship flying in real time, a camera, an
endless starfield, and a screen that shows how fast the game is running.
Milestone 2 added weapons, collision and a stress test. Milestone 3 added
four-arc shields, subsystem damage, pirate AI and a repair spot at the origin.
Milestone 4 added disabled ships, a reactor subsystem, merchant ships and
target selection. Milestone 5 added the Aurora system: a station, a
planet, a jump point, a navigation computer and an autopilot. Milestone 6 added docking with the station and the planet, and the first station
menus. Milestone 7 added the place table (description, interior picture
and services per station or planet), credits, a shipyard that sells ships and four
module upgrades, and a pilot name (Chad Beefsteel). Milestone 8 added a game clock,
15 commodities, a market at each place and the trading post. Milestone 9
added four linked star systems, hyperspace jumps that cost fuel and game days, and a galaxy map. Milestone 10 added boarding disabled ships; M10b added self-destruct risk and scrap metal; Milestone 11 added a hangar tab in the shipyard. Milestone 12 added factions, patrols, reputation and star charts. UI pass 1 turned menus into buttons, and M12b brought the new controls, escorts and the Start menu. M12c added the radar, hyperspace from open space, drift flight for light ships, and renamed the factions UFS and FSA. M12d added three missile types, guns that aim within their arc, and point-defence turrets. UI pass 2 redrew the trading post, ship status and galaxy map. Graphics stage 1 replaced the triangle ships with sprites rendered from 3D models; stage 2 added the rest of the catalog. Milestone 13a added the grid ship editor and tracking weapon mounts. Milestone 13b added location damage, warp drives, fuel cells, life support, 3x3 modules, mass limits and the hull upgrades. Milestone 13c (this build) adds module tiers and blueprints, refitting hangar ships, and lets turrets, missile racks and railguns sit on inside squares.

## Status (read this first)

- The source compiles and links cleanly against a PSn00bSDK built from source,
  and `mkpsxiso` produced a valid 2.5 MB disc image (about 40 KB program).
- That build used Ubuntu's Linux MIPS compiler instead of the SDK's own
  `mipsel-none-elf` compiler, plus a small fix-up to the ELF file, so it is a
  workaround. A build with the SDK's release package is the proper one.
- Milestones 1 to 4 were run by the project owner and work, including the
  640x480 interlaced mode. The owner measured the Milestone 4 stress test at up
  to 258 of 262 scanlines with occasional 30 fps frames in an unoptimised build
  (see Build types below). Milestones 7 to 12 have **not** been run by anyone yet
  (it compiled and a disc image was built, nothing more). Milestone 6 docking
  was run by the owner and works at the station; planet docking was not tried.
  Tuning numbers are placeholders.

## Video mode (changed after Milestone 4)

The game now runs in **640x480 interlaced** (NTSC, 60 fields per second), four
times the screen area of the old 320x240. Ships and shots keep their size, so
you see twice as far in each direction. Notes:

- There is a single frame buffer; each frame the GPU redraws only the lines of
  the field that is not on screen (see the comment in `src/gfx.c`).
- Frames must finish inside one vertical blank, so `VBL/FRAME` must stay at 1.
  If it shows 2, the picture will look combed, not just slower.
- Lines and details one pixel high flicker a little on a real TV (and show as
  combing on fast movement). Stars and bars are drawn two pixels tall for this
  reason.
- 480 lines is the NTSC height. 512 lines is the PAL height; PAL is a later task.
- The text is the SDK's 8x8 debug font, so it looks small. A proper font comes
  with the interface work.

## What you need

1. A PSn00bSDK release (Windows or Linux zip) from its GitHub releases page.
   It includes the MIPS compiler, `mkpsxiso` and the build rules.
2. CMake 3.21 or newer.
3. An emulator. The SDK recommends DuckStation or PCSX-Redux.

Set the environment variable `PSN00BSDK_LIBS` to the SDK's `lib/libpsn00b`
folder and add the SDK's `bin` folder to your `PATH` (see the SDK's
`doc/installation.md`).

## Build types

`cmake --preset default .` builds with the SDK preset, which is a Debug build
(`-Og`, slow). The disc images I deliver are built with `-DCMAKE_BUILD_TYPE=Release`
(`-O2`, about 15% smaller and clearly faster). Always measure with Release.

## Build

From this folder:

```
cmake --preset default .
cmake --build ./build
```

The disc image is `build/frontier.bin` with `build/frontier.cue`. Open the
`.cue` file in the emulator.

## Controls (port 1)

Use a DualShock in **analog mode** (Analog button lit) for the sticks and L3. The CONTROLS page of
the Start menu shows which mode the game sees. A plain digital pad can turn, thrust, fire and dock, but not strafe.

| Button | Action |
| --- | --- |
| D-pad or left stick | Turn (left/right) and thrust (up). Down is reverse thrust only on ships with retro-thrusters (the Hauler) |
| Right stick | Strafe |
| Cross | Afterburner (hold) |
| R2 | Fire your guns. Each one tracks your target inside its own arc and fires only if it can bear; with no target they fire along their own facings |
| L2 | Fire the selected missile (limited ammo, restocked while docked) |
| L1 | Change missile type: rocket, seeker, ion |
| R1 | Change combat target |
| Square | Cycle navigation target: station, planet, none |
| Triangle | Dock (in range, slow enough), board a wreck, or jump along the plotted course (clear of planets and stations). Press again while charging to cancel |
| Circle | Autopilot on / off |
| Left stick click (L3) | Lock on to the combat target: turns toward it and keeps about 250 px away. Click again to release |
| Select | Galaxy map (pauses the game) |
| Start | System menu (pauses): Resume, Fleet, Galaxy map, Options, Controls, Save/Load/Quit (not yet) |

In the dock menus Cross selects, Circle goes back (Start also launches), and the shipyard / trading
keys are unchanged. Flight assist, the stress test, peace mode and ship switching moved to Start > Options.

## What to check

1. **The system.** The home station (rotating arms, at the origin) repairs you
   inside 90 px. The planet AURORA (1500, -700) is a striped disc, lit on
   the left, smaller than the screen. It is not solid: you fly straight over it. The jump point (-1800,
   900) is a purple ring with a pulse chasing round it.
2. **Navigation.** L2 cycles station, planet, jump point, none. A green marker
   with the name and distance sits on the target; when it is off screen an arrow
   on the edge of the view points to it, with the label beside it.
3. **Autopilot.** Pick a target and press Triangle. The ship turns, accelerates
   to three times its normal top speed, brakes and stops at the destination
   (planet: 400 px from its centre). It disengages if you touch the controls or
   fire, if a pirate comes within 600 px, or if you are hit. A message tells you
   why. With pirates around it is often refused; press R1 + Start for peace mode.
4. **Jump.** Autopilot to the jump point and it jumps automatically on arrival:
   streaks rush outward, the screen wipes to white and you reappear beside the
   same ring facing away. You cannot be hurt during the jump. Real hyperspace to
   other systems is Milestone 9.
5. **Docking (new).** Fly into the station's 90 px range, or over the planet, and
   the HUD says to press Start. Above 90 px/s it says TOO FAST TO DOCK and Start
   is refused with a message; slow down and it works. Docking stops the ship,
   hides it and opens a panel: SHIP STATUS (hull, shields, class), REPAIR AND
   REARM (instant, fully restores everything), TRADING POST and SHIPYARD (shown
   but locked until M8 and M7), and LAUNCH. D-pad up/down chooses, Cross selects,
   Circle or Start launches. Docking at the planet adds SPACEPORT to the title.
6. **Safe while docked.** Pirates ignore a docked ship and nothing can damage it,
   while the world outside keeps running. On launch you have two seconds of
   protection. The autopilot is cancelled when you dock.
7. **Places (new).** The dock screen now has a picture of the inside of the place
   and three lines of description. The station shows a hangar bay; the planet a
   spaceport with towers. The menu lists only what the place offers: the station
   has Shipyard, Trading Post, Lounge and Mercenary Guild; AURORA has no Shipyard
   and no Guild. Trading, Lounge and Guild are locked entries for now.
8. **Full-screen dock (changed).** Docking now covers the whole display: the star
   field, ships and their health bars are no longer drawn behind the menu.
9. **Shipyard (new).** You start with 10000 credits (placeholder) and earn 150 per
   pirate destroyed. At the station choose SHIPYARD, then switch tabs with D-pad
   left / right or L1 / R1:
   - UPGRADES: four modules (engine, shield, hull plating, laser overcharge),
     three levels each. A purchase applies at once; Ship Status and the HUD show
     the new numbers and you can feel it in flight.
   - BUY: the other ships (Raider, Hauler). Your ship trades in for half its
     price, and your modules carry over.
   - SELL: sell the top level of a module back for half the price paid. Selling
     spare ships waits for the fleet (Milestone 11).
10. **Pilot name (new).** The pilot is Chad Beefsteel. On SHIP STATUS press Square
    to rename: up / down changes a letter, left / right moves (right at the end
    adds a letter, up to 16), Cross saves, Circle cancels. Not saved to a memory
    card yet.
11. **Trading post and clock (new, M8).** TRADING POST is now open at the station
    and at AURORA. One game hour is 60 real seconds; DAY and time show on the dock
    screen and the HUD. The list shows 15 commodities with buy price, sell price,
    stock and what you hold. Up / down choose; Cross buys one, Square sells one,
    hold R1 with either for ten, Triangle sells all of the highlighted item.
    Prices follow stock: scarce goods cost more, plentiful goods less, and what
    you buy or sell moves the price. You sell for 92% of the posted price.
12. **Where to profit.** The station is industrial: Machinery, Industrial Parts and
    Electronics are cheap there; Metals, Raw Minerals and Fuel are dear. AURORA is
    agricultural: Food and Water are cheap there; Machinery and Medical Supplies
    are dear. A full hold of Machinery from the station to AURORA is the obvious
    first run (about 70 credits a unit before your own sales push the price). The
    Wayfarer holds 12, the Raider 2, the Hauler 60. Prices drift back toward
    their natural level every game hour, so a market you sold into recovers.
13. **Cargo risks.** Your hold shows on Ship Status and the HUD. If your ship is
    destroyed the cargo is lost. You cannot buy a ship whose hold is smaller than
    the cargo you carry.
14. **Hyperspace and galaxy (new, M9).** Four systems: AURORA, VESTA, KESTREL, CINDER.
    AURORA links to VESTA and KESTREL; KESTREL links on to CINDER. Each system has a
    station, a planet and one jump point per link (named "JUMP: <DEST>"). Fly into a jump
    ring and press Start, or choose the jump point with L2 and press Triangle (the
    autopilot flies there and jumps). A jump costs fuel (1 + distance/10) and several game
    days; the HUD shows FUEL and the day. Refuel at any dock (REFUEL, 5 credits a unit).
    Select opens the galaxy map: D-pad cycles systems, Cross plots a course (the nav
    pointer then aims at the first jump point, and after each arrival at the next one),
    Square clears it. Try AURORA -> KESTREL -> CINDER. Each system has its own markets,
    so prices differ; goods you carry can be sold at a profit across systems.
15. **Salvage (M10 and M10b).** Disable a pirate or merchant without destroying it. Fly
    slowly within 110 px with no active pirate within 400 px; the HUD says "START:
    BOARD WRECK". The game pauses on a menu: CLAIM SHIP (to your hangar, max 8),
    SALVAGE (strips the hull into SCRAP METAL cargo, sold at any trading post; needs free
    hold space), STRIP CARGO AND CREDITS (random loot, once per wreck) or LEAVE. **Every
    action can set off the wreck's self-destruct**: you escape but the wreck is lost
    (claim 25%, salvage 20%, strip 15%, leave 5%). Strip and leave can be repeated.
    Please test several wrecks to see the odds feel right.
16. **Hangar (new, M11).** At a station or planet with a shipyard, the shipyard has a fourth
    tab, FLEET, listing claimed ships. Cross swaps into the highlighted ship (it is
    refitted, your modules carry over, and your old ship takes its place in the hangar);
    Square sells it for its trade-in value (half price). The hangar is at no particular
    place yet and escorts come with the guild later. Swapping repairs the ship.
17. **Factions (new, M12).** AURORA is Federation (4 blue patrol ships), KESTREL is now
    Rebel (3 magenta patrols); VESTA and CINDER have none. Patrols hunt pirates. Shooting a patrol
    costs standing with that faction (-2 a hit, -10 disabling, -25 destroying) and half that gain
    with the rival; killing pirates in their system earns a little. At -25 or worse the patrols
    attack you and the station refuses docking. Standing shows on Ship Status. Pirates also attack
    patrols and merchants. Patrol shots can hit you if you fly through them. AURORA has only 1 pirate now.
18. **Star charts (new).** The galaxy map starts with only AURORA. Press Select on the trading
    page (any place) to buy a chart (500 credits) that reveals systems within 40 map units. Visited
    systems are revealed too. Jump points to unknown systems read "UNCHARTED".
19. **Buttons (new, UI pass 1).** The dock menu, shipyard tabs and rows, and the salvage menu
    are drawn as buttons (highlighted = yellow border, greyed = locked). The interior picture is
    smaller and the menu column is narrower so they no longer touch; the place description now
    sits under the picture. The trading table, status page and galaxy map are unchanged for now.
20. **New controls and the Start menu (M12b).** See the table above. Please try the sticks,
    L3 lock-on, and the Start menu pages.
21. **Escorts and the fleet window (M12b).** Claimed ships now fly with you (the first three not in
    the HANGAR). Start > Fleet sets each one's stance with Left/Right or Cross: HANGAR (stays
    behind), FOLLOW (formation, no fights), GUARD (attacks pirates within 450 px of you), ATTACK
    (hunts your selected target, or the nearest pirate within 900 px). They follow you through jumps
    and are teal, with green shots. A destroyed escort is lost from your fleet. Pirates do not
    hunt escorts on purpose yet; they take stray shots. New claims start on FOLLOW. This works in
    flight, no shipyard needed (the shipyard FLEET tab still swaps and sells).
22. **Radar (new, M12c).** Top right: a square scope centred on you, north up. White square
    = station, coloured disc = planet (both shown on the rim when out of range); red = pirates
    (grey when disabled), green = merchants, blue = UFS patrols, magenta = FSA patrols (blinking
    white when hostile to you), teal = your escorts, yellow corners = your target. Range is 1200 px;
    the new RADAR UPGRADE in the shipyard adds 600 px a level (up to 3000). Is it readable, and does
    it clash with anything?
23. **Hyperspace from open space (new, M12c).** The jump rings are gone. Plot a course on the
    galaxy map (Select), then fly at least 500 px clear of the station and 500 px clear of the
    planet's surface (the HUD says how far to go) and press Triangle. The ship brakes to a stop,
    the drive charges for 2.5 seconds (bar under the ship), then it jumps. You can turn and shoot
    while charging, but thrust, afterburner or Triangle cancels the jump. Fuel is spent only when
    the jump fires. You arrive where the old ring for that route was. Is 2.5 seconds tense but fair?
24. **Drift flight (new, M12c).** Light ships (Wayfarer, Raider) have no retro-thrusters: off the
    throttle they keep drifting, and Down does nothing; to stop, turn round and burn. The Hauler
    has retro-thrusters: it slows by itself and Down is reverse thrust. Flight assist (Start >
    Options) is now OFF by default; switching it on gives every ship gentle drag (easy mode).
    Docking needs under 90 px/s, so practise the flip. NPC pilots still use assist for now.
25. **UFS and FSA (M12c).** The Federation is now the UFS and the Rebels are the FSA in every
    screen; CINDER is Outer Rim Syndicates territory. The target readout says UFS PATROL / FSA PATROL.
26. **Missile types (new, M12d).** L1 cycles ROCKET (straight, 45 damage), SEEKER (homes on
    your combat target if you had one at launch; slower, 900 px reach, 35 damage) and ION (no hull
    damage: drains 50 from every shield arc and 25 from engines and weapons, shields or not; enough
    ion hits leave the engines at zero, which disables the ship intact for boarding). The HUD shows
    the selected type and R/S/I counts. Wayfarer: 6 rockets, 3 seekers, 2 ion. Raider: 10, 4, 2.
    The Hauler has no rack. Missiles restock slowly while docked, like before.
27. **Gun arcs (new, M12d).** With a combat target (R1) inside 30 degrees of your nose and in range,
    your guns fire at where it will be, not straight ahead. Outside that arc they fire straight.
    Does it feel helpful or like cheating? The arc is easy to widen or narrow (GUN_HALF_ARC).
28. **Turrets (new, M12d).** The Wayfarer has one point-defence turret, the Hauler two (grey mounts
    with a barrel). They fire by themselves at the nearest active pirate or hostile patrol within
    320 px, all round, leading the target. They never shoot merchants, friendly patrols or disabled
    ships, so they won't wreck a ship you want to board. They need working weapons and reactor.
29. **UI pass 2 (new).** Please check these three screens for overlaps and readability:
    - **Trading post:** each commodity is a thin button (yellow = selected, cyan text = you hold
      some). A cargo bar sits next to HOLD. The profit note sits in the message line at the bottom
      (a shop message replaces it for a few seconds). The chart price moved into the hint line.
    - **Ship status:** two cards. Left: hull, the four shield arcs and the four subsystems as bars,
      then performance (speed, accel, turn, retro-thrusters, recharge, guns). Right: pilot name
      (Square still renames), UFS / FSA standing bars (centre = neutral), cargo and fuel bars, crew
      and hangar, missile bars and turret count, and module levels as three pips each.
    - **Galaxy map:** the map sits in its own card with a colour legend under it (UFS blue, independent
      green, FSA magenta, Outer Rim red; FSA was orange on the map before). The selected system's info
      is in a card on the right with PLOT / CLEAR / CLOSE buttons that grey out when they don't apply.
30. **Ship sprites (new, graphics stage 1).** The Wayfarer (Chad's ship, after the cover render),
    the Raider (pirates, after the FSA Wasp shape in pirate colours) and the Hauler (after the Iron Hauler)
    are now 64x64, 16-colour sprites, turned by the GPU. Patrols are tinted (UFS blue-grey, FSA purple),
    escorts teal-grey; a hit flashes the whole sprite. Ships are bigger on screen than the old triangles,
    so their hit circles grew too (16, 12, 22 px). Look for: shimmer when turning, readability against
    the stars, and whether the sizes feel right. How they are made: tools/shipgen (see Files).
31. **Turrets and auto-target (hotfix 1).** Turrets fire only while R2 is held, at your combat target, all
    round. Being shot makes the shooter your target unless you already target an active hostile.
32. **Uncharted jumps (hotfix 1).** The galaxy map always shows the systems linked to where you are, as grey
    "?" nodes. Plot to one and jump blind; a star chart (trading post, Select) names them.
33. **The whole fleet (new, graphics stage 2).** 26 ships now exist in the game, each with its own
    sprite: 16 small ones (64 px), 5 corvettes and frigates (128 px), 5 destroyers and cruisers
    (256 px). Stats are the catalog's, scaled down, and all placeholders. Where to see them:
    - UFS patrols (AURORA) are a Sentinel corvette with Talon and Vanguard fighters; FSA patrols
      (KESTREL) are a Renegade with Wasps and a Hammer.
    - Merchants are Haulers, Frontier Runners, Rockbreakers or Reclaimers; some pirates fly
      converted Frontier Runners.
    - The shipyard BUY tab sells the civilian hulls (Wayfarer, Raider, Hauler, Frontier Runner,
      Wayfinder, Rockbreaker, Reclaimer). Military and Veyr ships are not sold.
    - Start > Options > SWITCH SHIP cycles through all 26, capital ships and Veyr included, so you can
      fly a Sovereign or a Leviathan. The big ones are slow to turn and have retro-thrusters.
    Watch for: frame rate with several big ships on screen, and whether 256 px capital ships feel right.
34. **Hyperspace departure (new).** From half charge the ship swings toward the destination and
    speeds up faster and faster until it streaks off the screen; the camera stays put. Only Triangle
    can cancel at that point (thrust no longer does). You come out of the jump flying in that direction
    at top speed; ships with retro-thrusters come out at half speed and brake by themselves.
35. **Weapon mounts (new, M13a).** Every weapon is a module on the hull with its own position,
    facing and firing arc. Holding R2 with a target: each gun swings toward the target (lasers fast,
    cannons slower, railguns slowly) and fires only when the target is inside its arc and range. No
    target: every gun fires straight along its own facing. Turrets cover 270 degrees. You can see
    the barrels turn on the sprites, NPCs included. Lasers draw energy from the capacitors (spare
    reactor power refills them); cannons and gun turrets use rounds from the ammo stores (restocked
    by REPAIR AND REARM). Missiles (L2) leave from missile racks; no rack, no missiles.
36. **The ship editor (new, M13a).** Shipyard > REFIT tab > OPEN THE SHIP EDITOR. Your hull's grid
    is drawn over its sprite: blue dots = edge squares (hardpoints), grey = inside, orange = rear
    (engines only), yellow = the bridge. D-pad moves, L1/R1 picks a module, Cross installs it (costs
    its price), Circle removes the one under the cursor (30% back), Square turns it (long weapons
    can't), L2/R2 swing the firing arc of the weapon under the cursor in 11.25-degree steps (up to
    90 either way); the arc shows as yellow lines. Start restores the default fit (no refund).
    Triangle leaves, once there is a bridge, an engine and enough power. The right column shows power,
    energy, speed, turn, hull, shields, guns and cargo as you change things.
    Every ship comes with a default fit (its catalog numbers). The hull itself keeps 70% of its hull
    points; armor plating adds the rest. Shields come only from shield generators. More engines or
    less mass mean more speed. Buying a new ship gives you its default fit.
    Module list (Tier 1): laser, cannon (1x2), railgun (2x5, fixed), laser turret, gun turret (2x2),
    missile rack (1x2), engine, big engine (2x2), reactor, big reactor (2x2), capacitor, shield gen,
    big shield gen (2x2), armor, ammo store, cargo bay.
37. **Escort stances (changed).** GUARD now goes after whoever shot at you (for ten seconds after the
    last hit), not your target. ATTACK goes only after your current target.
38. **Location damage (new, M13b).** Damage that gets through the shields also hits the module
    nearest the impact. Every module has hit points; armor plates are the toughest (80). A
    destroyed module stops working at once (guns stop firing, engines stop pushing, shields and
    hull shrink) and you get a message. Reactors and fuel cells EXPLODE when destroyed, damaging
    every module within two squares (which can set off more) and the hull. Lose every reactor and
    the core breaches: the ship is destroyed. Lose the cockpit or bridge, or every life support
    module on a big ship, and the ship is disabled (yours gets towed home; NPCs can be boarded).
    Docking with REPAIR rebuilds all modules. The status page counts damaged and lost modules.
39. **New modules (M13b).** Cockpit (small ships: life support built in) or bridge (medium and large
    ships: they also need LIFE SUPPORT modules; spares keep you going if one is lost). Warp drives S
    (1x1), M (2x2), L (3x3): a hull needs one at least its size class to jump (fighters and other 64 px
    hulls: S; corvettes and frigates: M; destroyers and cruisers: L). Extra drives shorten the charge
    but burn more fuel per jump. Fuel cells hold half your fuel (the hull holds the other half) and
    explode if hit. 3x3 modules: huge engine, huge reactor, heavy turret (kinetic). Masses now
    follow the design: support modules lightest, then weapons, then engines, power and shields,
    then armor (6 per plate).
40. **Mass limit (new, M13b).** Every hull has a mass allowance (3 per square, or a quarter more than
    its default fit). The editor shows MASS x OF y; an overweight ship can't leave the editor.
    Heavier fits also accelerate and turn more slowly.
41. **Hull upgrades (new, M13b; the UPGRADES tab, which replaced TUNING).** Mass expansion (+20%
    allowance), Storage expansion (+20% cargo), Maneuvering (+8% turning), each up to level 10 at
    2.2 times the price of the level before (level 10 costs about 2.4 million, 1.8 million and 3
    million). Radar stays at 3 levels. They belong to the pilot and carry over to any ship. The old
    engine, shield, hull and laser percentage upgrades are gone; modules do that job now.
42. **Deeper engine bays (M13b).** Medium hulls get up to 4 rows of rear engine squares and the
    biggest hulls up to 7, so cruisers can stack engines behind engines.
43. **Module tiers and blueprints (new, M13c).** Every module comes in tiers 1-3. In the editor,
    SELECT cycles the tier. Higher tiers are stronger (thrust, power, shields, armor, capacity x1.6 and
    x2.4; weapon damage x1.4 and x1.9; hit points x1.5 and x2.2) but heavier (x1.4 and x2; armor x2 and
    x3.3) and cost more (x4 and x12). You start with tier 1 of everything. A locked tier shows its
    blueprint price; pressing Cross buys the blueprint (15x the module price for tier 2, 60x for tier 3),
    then Cross installs as usual. Better warp drives charge jumps faster without burning extra fuel.
    Placed modules show their tier as small white dots in the corner.
44. **Fleet refits (new, M13c).** Shipyard > FLEET: Triangle opens the editor on that hangar ship. Each
    fleet ship keeps its own fit; swapping into a hangar ship swaps fits too (your old ship goes to the
    hangar with its modules). Escorts fly with their fits; your hull upgrades apply to them as well.
45. **Placement (changed, M13c).** Turrets, missile racks and railguns can go on inside squares (mounted
    on top of or under the hull). Lasers and cannons still need an edge square.
46. **Crash fix (hotfix 2).** A ship whose shield generators were all shot away crashed the game (a divide
    by zero in its health bar). Fixed, and the whole game is now built so a divide by zero can never freeze it.
47. **Patrol scans (M13d).** In UFS space (AURORA) a patrol hails you: "HOLD POSITION FOR A CARGO SCAN".
    Get within about 300 px of it and slow to a near stop for 4 seconds. A clean scan raises UFS standing a
    little. You have about 15 seconds to comply; keep moving, fly off or start a jump charge and that
    system's patrols open fire (they won't let you dock either). Leave the system and it is forgotten: no
    reputation is lost for running. A Veyr ship in your fleet (yours or a flying escort) is attacked on sight.
    In FSA space (KESTREL) patrols just circle you and move on. Scans pause while pirates are near.
48. **Smuggling (M13d).** Two new goods, NARCOTICS and STOLEN TECH (marked "!"), are contraband. CINDER
    (Syndicate) sells them cheap; everywhere else buys them dear, AURORA most of all, since nobody sells them
    there. A UFS scan seizes all contraband and fines you half its base value, with a small UFS standing loss.
    The trade page now scrolls (18 goods). Also fixed: the "PIRATES NEARBY" check when boarding a wreck.
49. **Pirate gangs (M13e).** Pirates now raid as a gang: a leader with wingmen in a V behind it, announced
    on arrival ("PIRATE GANG ON SCANNERS: 4 SHIPS, LED BY A UFS VANGUARD"). The gang heads for you and fights
    whatever its leader fights. Gangs depend on the zone: a lone Raider in the Core (AURORA), three-ship gangs on
    the Mid Belt (VESTA), stolen fighters and corvettes on the Frontier (KESTREL), and on the Rim (CINDER) up to
    six ships, sometimes led by a stolen FSA DEFIANT frigate. Take the leader down (destroyed or disabled) and
    about half the gang runs. Outfly a gang (900 px) and it's gone. The next raid comes after a quiet spell:
    longest in high-security systems. Bounties now scale with the ship: about 195 for a Raider, over 4,600 for
    the frigate. The stress test still uses the old endless single pirates.
50. **Weapon roles (M13f).** Each weapon hits shields and hull differently:
    lasers (and laser turrets) 130% to shields, 70% to hull; cannons and gun turrets 80% / 130%;
    railguns 110% / 150%; heavy turrets 90% / 140%; rockets 60% / 160%; seekers 70% / 140%.
    Capital shields also turn aside part of EVERY hit: corvette 2, frigate 4, destroyer 6, cruiser 8 points.
    So many light hits barely scratch a big ship. Bring railguns, heavy turrets or missiles. When a shield
    breaks, only the part of the hit it didn't stop goes on to the hull. The ship editor shows each weapon's role.
51. **Outer Rim Syndicate ships (M13g).** Four home-made pirate hulls, welded from scrapped civilian, UFS
    and FSA ships (mismatched paint, one wing from each side, bare girders, patch plates):
    ORS SCRAPPER (light fighter), ORS JACKAL (heavy fighter, one long cannon), ORS CHOPSHOP (corvette,
    a hauler spine with a UFS prow, one heavy turret) and ORS MARAUDER (frigate, a container spine, two heavy
    turrets and a lone outrigger engine). Tough hulls and weak shields: they shrug off lasers less well than
    military ships, but take a beating. The gangs now fly mostly these, with the odd stolen military hull. On the Rim,
    the MARAUDER leads a 6-ship gang. You can disable and claim them like any other ship. They're not sold at
    shipyards. Models: tools/shipgen/ships3.py.
52. **Syndicate standing (M13h).** A third standing, shown on the status page. It's separate from UFS and FSA.
    Levels: UNKNOWN, RECOGNIZED (10), TRUSTED (30), ESTABLISHED (55), INNER CIRCLE (80); MARKED at -25 or worse.
    Gain: sell contraband (+1 per 5 units, double in UFS space), buy it in bulk (+1 per 20), slip a UFS scan and
    escape the system (+3), destroy a UFS or FSA patrol ship (+3), fence a ship (+1).
    Lose: destroy a pirate (-2), disable one (-1).
    At TRUSTED and above, Syndicate crews come over to look and leave you alone. Shoot one and its whole gang
    turns on you. Your turrets and auto-targeting ignore friendly crews.
53. **Black market (M13h)** at both CINDER docks. HIDDEN HOLD (TRUSTED): UFS scans miss 10 / 25 / 50 units of
    contraband (6,000 / 18,000 / 45,000). FENCE (RECOGNIZED): sell a hangar ship for 75% of its price
    (LEFT / RIGHT picks it). WIPE RECORDS (ESTABLISHED, 25,000): UFS and FSA standing below zero goes back to
    neutral. VEYR TECHNOLOGY (INNER CIRCLE) is listed for later. MARKED pilots are turned away.
54. **Dev menu (M13i).** START > DEV MENU, on test discs only. Build the release disc with -DFRONTIER_DEV=OFF
    and it's compiled out. God mode, stress test, peace mode, spawn a pirate gang now, spawn any ship as a
    hostile or as a disabled wreck to claim (LEFT / RIGHT picks the ship), fly any ship, warp to any system (no fuel,
    no charge), +100,000 credits, repair / rearm / refuel, UFS / FSA / Syndicate standing (LEFT / RIGHT moves
    them by 10), load 20 contraband. The old test aids moved here from OPTIONS.
55. **Missions (M13j).** The MERCENARY GUILD is open at stations with a shipyard. The board shows up to 5
    contracts from whoever runs the system (UFS, FSA or independents), new each day. Hold up to 4 at once.
    CARGO (goods to a dock), COURIER (a package, tight deadline), EMERGENCY SUPPLY (medical supplies to a colony),
    PASSENGERS: these finish when you dock at the destination. Their goods take hold space.
    BOUNTY: a named wanted pilot (RED VASKO, GRANNY HEX...) shows up with two wingmen when you're in his system.
    Destroy or disable him and you're paid. INTERCEPT: destroy N pirates in a system. SURVEY: find that system's
    anomaly (the HUD shows its bearing and distance). Missed deadlines and abandoned contracts cost standing.
    Losing your ship fails the carrying contracts.
    SYNDICATE JOBS at the black market (more jobs at higher standing): smuggling runs to UFS docks
    (the goods are contraband: a scan beyond your hidden hold fails the job), quiet couriers and passengers.
56. **Discoveries (M13j).** Every system hides one anomaly, shown as a pulsing cyan marker. Fly near it to discover it.
    Sell the survey data at any guild (1,800 each). The dev menu can complete all active contracts.
57. **Title screen and pilots (M14a).** The game opens on a title screen: NEW GAME or LOAD GAME. A new game
    lets you pick one of the four pilots (LEFT / RIGHT), then plays their opening narration:
    CHAD BEEFSTEEL (Wayfarer, 3,000, Aurora, UFS +10); REGINALD FANSHAWE-BUTTERSWORTH III (Hauler,
    25,000, Aurora, UFS +25 / FSA -20); ZSA-ZSA BLASTWELL (Raider, 3,000 and a 1,500 Syndicate debt due on day 30,
    Cinder, UFS -30 / FSA +10); PROFESSOR MARIGOLD QUASAR-PENNYWHISTLE (Wayfarer, 4,000, Kestrel,
    UFS -15 / FSA +30, every system charted). Pay Zsa-Zsa's debt at the black market. If it's late, the Syndicate
    turns on her.
58. **Saving (M14a).** SAVE GAME is in every dock menu. One save on the memory card in slot 1, 2 blocks,
    shown in the console's card manager as FRONTIER with the pilot and day, and a little ship icon. LOAD GAME
    on the title screen, or START > LOAD GAME in flight (press X twice), puts you back docked where you saved.
    START > QUIT GAME (X twice) goes back to the title. Saved: pilot, credits, ship and its fit and damage,
    upgrades, blueprints, hangar ships and their fits, cargo, every market, standings (UFS, FSA, Syndicate,
    hidden hold, debt), the clock, charts, contracts and discoveries.
59. **The lounge (M14b)** at every dock. What's there changes four times a day (and stays the same if you come back at
    the same time). It has up to 3 rumours: trade tips (most are true, some are stale), danger talk, gossip about
    you (or the admiral on the news: "...AND IF BEEFSTEEL IS INVOLVED, GOD HELP US ALL"), and whispers about anomalies.
    It also has an encounter: a passenger who pays double, a retired bounty hunter's tip, a scientist's survey job, or
    nobody. Notices on the wall offer 3 small jobs. The tables: HIGH CARD (wins 46%, pays 2X) and LUCKY SEVENS (wins
    22%, pays 4X), with bets of 100 / 500 / 1,000 / 5,000 (LEFT / RIGHT).
60. **Escort hiring (M14b).** The MERCENARY GUILD's second tab (L1 / R1): up to 3 pilots a day for hire, in a Raider,
    Wasp, Hammer, Vanguard or Renegade, for 3 to 7 days. A hired ship joins your fleet and flies home when its time is
    up. It can't be sold, fenced or flown by you.
61. **Layout fixes (M14b).** Status page: the meters start further right, so labels like SYNDICATE no longer run
    into them (the Syndicate row now reads ORS). Ship editor: the info column starts below the credits and clock,
    the weapon role no longer overlaps the PLACING line, and the role texts are shorter. Ship editor text is now cut at the panel edge, the
    locked-tier line is shorter, and the controls are on two lines. Trading post: the contraband
    note fits the screen, and the scroll marks are now "2 MORE ABOVE" / "2 MORE BELOW" by the column headings.
    Galaxy map: the hint line fits. The opening briefing now types itself out word by word, with a pause after
    each sentence and a blinking cursor (X skips to the end, X again launches).
62. **Module storage (M14c).** Circle in the ship editor now puts a module into storage instead of selling it.
    Fitting a module you have in storage costs only a fee (10% of its price, at least 25). The editor shows
    "IN STORAGE n  FIT FEE x" for the module and tier you've picked. Sell stored modules (30%) on the shipyard's
    SELL tab, if you want to. Start x2 resets to the default fit, asking first ("PRESS START AGAIN"); your old
    modules go to storage, and the default parts come from storage (fee) or are bought new. "Blueprint" is gone
    from the editor: a locked tier just shows its price.
63. **Syndicate shipyard (M14c).** CINDER OUTPOST now has a shipyard and guild. It sells the ORS SCRAPPER, JACKAL,
    CHOPSHOP and MARAUDER, and the only illegal tech in the game:
    RADAR JAMMER (RECOGNIZED, 3 levels, 8,000 / 16,000 / 30,000): patrols, pirates and UFS scans notice you 140 px later
    per level. FALSE TRANSPONDER (TRUSTED, 25,000): half the time a UFS patrol waves you on without scanning.
    Other yards list them as SYNDICATE YARDS ONLY.
64. **3D ships in the shipyard (M14d).** The REFIT tab shows the ship you're about to edit as a real-time 3D model,
    turning on a stand: flat-shaded, lit, a few hundred triangles each (PS1 style). The BUY tab shows the ship
    you've highlighted, and the FLEET tab the hangar ship you've picked. All 30 ships have models, made from the same
    Blender models as the sprites by tools/shipgen/mesh.py (src/meshes_data.c, about 130 KB). Drawn in software by
    src/model.c: turn, tilt, light, cull the back faces, sort by depth.
65. **The graphics pass (M14e).** A new graphics core (src/gfx.c):
    - **Typeface:** the game's own, drawn in tools/artgen/glyphs.py and built in three native sizes (8, 16, 24 px
      cells) with anti-aliasing and a baked drop shadow. Headings are Gouraud-shaded (lighter at the top).
    - **Interface:** glass panels over a drifting starfield, gradient buttons, bars and cards, with dithering on.
    - **Flight HUD:** redrawn. Ship panel (shield arcs, hull, energy, ammo, missiles), a prompt line, a target
      panel under the scope, and a bottom strip. The old debug lines only show in the stress test.
    - **Space:** the planet is a Gouraud-shaded sphere with an atmosphere; the station is a 3D model; star
      colours, halos and nebulae; shots, engines, explosions and shield hits are additive light.
    - **Docks:** the picture is your own ship in 3D, in a hangar or on a spaceport apron.
    - **Title:** a chrome FRONTIER logo (tools/artgen/logo.py: eight strips, each with its own palette, the glow
      added to the scene by the mask bit) over a 3D battle: a UFS corvette, Syndicate raiders, escorts.
    - **3D:** the GTE now transforms the models (src/model.c); faces are lit from stored normals.
    - **Frame pacing:** if a frame runs long it waits for the right interlace field, so the picture stays whole.
    - **Fixes:** illegal tech is only listed at Syndicate yards; the editor's module name was hidden behind its
      box; flight messages were cut at 39 characters.
66. **Preview renderer (tools/preview).** `build.sh` compiles the game's own code for a PC with a software model
    of the GPU and GTE; FR_SCRIPT presses buttons by frame and FR_SHOTS saves screens. It is how screens are
    checked before a disc is made. It is not an emulator (no CPU, timing or sound).
67. **Save slots and autosave (M14f).** Four saves on the card in slot 1, each 2 blocks: an AUTOSAVE, made every
    time you dock, and SAVE 1 to SAVE 3 (SAVE GAME in any dock menu; X twice to save over one). LOAD GAME on the
    title, or START > LOAD GAME in flight, lists them with pilot, day, playing time, system, difficulty and credits.
    Saves from before M14f don't load (the format changed).
68. **Playing time (M14f).** The game counts real playing time (not the in-game clock) and saves it. The status
    page shows it. It will drive the Veyr deadline (10 hours).
69. **Difficulty (M14f).** After choosing a pilot: EASY, NORMAL or HARD. It is about the Veyr only and is saved
    with the game. It does nothing yet: the Veyr come in a later build.
70. **Choosing a side (M14f).** Once a side fully trusts you (ALLY, 60), its enemy turns on you:
    UFS ally: the FSA is hostile and the Syndicates won't deal with you. FSA ally: the UFS is hostile.
    Syndicate ESTABLISHED: the UFS is hostile. Lose that trust and the cap lifts, so switching sides is
    possible but slow. (The dev menu's standing buttons ignore this, for testing.)
71. **Planet conquest (M14g).** Fly within 1200 px of a colony's defence base (AURORA and VESTA: the station;
    KESTREL and CINDER: the planet) and choose START > ATTACK COLONY, then X again to confirm. The owner turns
    hostile at once (UFS or FSA to -60, the Syndicates to Marked; independents only defend their colony). A captured Syndicate port keeps its scrap yard, illegal tech and black market, and in your own port the black market deals with you even while you are Marked, so their
    patrols hunt you everywhere. The base launches waves: 2 + security (CINDER 2, KESTREL 3, VESTA 4, AURORA 5), each
    a ship bigger than the last, and the final wave is led by a flagship (UFS Bastion, FSA Reprisal, Syndicate
    Marauder, militia Watchman). The HUD shows ASSAULT: WAVE x OF y. Defenders never flee; a destroyed or disabled
    defender counts (disabled ones can be boarded). No pirate raids or traffic during the siege, and you can't dock.
    Beat the last wave and the system is YOURS for good: gold on the galaxy map, the dock reads THIS PORT FLIES YOUR
    FLAG, no more patrols of theirs. Jumping away or dying calls it off (they stay hostile).
72. **Tribute (M14g).** Each new game day every system you hold pays 300 + 150 per security level (AURORA 750 a
    day, CINDER 300); the message shows in flight. The galaxy map card shows it; the status page counts SYSTEMS.
    Owners and the tribute day are saved. M14f saves still load (their systems keep their old owners).
73. **Dev menu:** CAPTURE THIS SYSTEM (NO FIGHT) takes the current system at once.
74. **Fleets: where your ships are (M15a).** Every claimed or hired ship is now somewhere. START > FLEET lists
    each one: its group (WING, or SQ A, B, C), its stance, where it is and its fuel. SQUARE moves a ship between
    your WING and squadrons A, B and C (a ship can only join your wing where you are, or a squadron in its own
    system). LEFT/RIGHT sets a wing ship's stance; HANGAR leaves it docked where it is, and the shipyard can only
    swap, refit or sell a ship docked in the system you're in (others show AT <SYSTEM>).
75. **Squadrons and orders (M15a).** The squadron rows at the bottom of the FLEET page: LEFT/RIGHT picks a
    destination (anywhere charted, or an uncharted system next door, shown by its bearing), CROSS sends it. It
    travels jump by jump in game hours, charts what it reaches, and holds there. A squadron holding a system is
    its garrison: when you are there, its ships (up to 3) wait round the station and attack anything hostile
    within 700 px. The galaxy map marks squadrons with A B C (and H for a ship parked in a hangar).
76. **Fuel rules (M15a).** Ships flying with you jump free: only the ship you pilot burns fuel. A squadron on its
    own burns its lead ship's fuel and refills at ports from your credits (5 a unit). If it can't, it is
    STRANDED (! on the map). Every system has a port for now, so stranding needs the empty systems coming with
    the full galaxy (or no credits). The rescue: fly there and SQUARE its ships into your wing; they jump out on
    your fuel. Each ship keeps its own tank when you swap ships in the shipyard.
77. **Antimatter scoop (M15a).** A new upgrade in every shipyard, 5 levels (3000 for the first, then 2.2x each).
    Out in dead space (clear of the planet and station, where you could jump) it gathers 1 fuel unit every
    20 seconds at level 1, down to every 4 seconds at level 5. The HUD shows FUEL x/y SCOOP while it works.
    Dev menu: ADD TO FLEET < ship > and PASS 12 GAME HOURS, for testing squadrons. M14 saves still load.
78. **Squadron missions (M15b).** On a squadron row, TRIANGLE picks the mission, then CROSS sends it:
    SEND (go and hold, guarding it), TRADE (carry the best legal cargo between where it is now and the
    destination, back and forth, for ever, until you give new orders; profit goes straight to your credits, and
    convoys can meet pirates out where security is low: armed ships fight them off, otherwise the cargo is lost and
    maybe a ship), or ATTACK (it fights the colony's whole defence on arrival, off screen: strong enough and the
    system is yours, with losses; too weak and it is beaten, losing ships, and the survivors fall back home).
    The owner turns hostile either way. Judging what is enough is up to you. Rough guide: VESTA's militia is worth
    about 10 Talons; the UFS and FSA cores take a real battle fleet.
79. **Pilots (M15b).** Every ship comes with a named pilot (a call sign). Each ship it destroys or disables is a
    kill: 3 kills make it a PILOT, 8 a VETERAN, 16 an ACE (it says so in flight). Better pilots fire from further
    out, keep a smarter distance, boost into a fight; veterans break off when badly hurt; aces jink. Rank also adds
    20% a step to a ship's worth in squadron battles and convoy fights. Shipyard FLEET tab: the pilot, rank and
    kills; SELECT renames the pilot. Lost ships report who went down with them.
80. **Pirates and enemies now go after your escorts and garrisons (M15b)**, not only you; so do colony defenders
    and hostile patrols. (A Syndicate friend's gangs leave them alone, as they do you.)
81. **The afterburner burns fuel (M16a).** Holding CROSS uses one unit of hyperspace fuel every 3 seconds (the
    autopilot's cruise does not). At 0 fuel the afterburner is dead. The antimatter scoop refills it in dead space.
82. **The full galaxy (M16a).** 64 systems on a 400 x 240 map, generated by `tools/galaxy/gen.py` (same seed,
    same galaxy; it writes src/galaxy_data.c and .h). The Core and the UFS in the west round AURORA; the FSA in
    the south and frontier round KESTREL; the Syndicates on the Outer Rim round CINDER; independent free ports
    scattered between, VESTA the biggest. Routes are twisty: borders are chokepoints, territories have dead ends,
    and a UFS enclave in the south-west is only reachable through FSA space. 14 systems are EMPTY: no planet,
    no station, no fuel, no help; pirates wait there. The Veyr's nebula will lie past the Rim (later).
    Every port has its own name, description and services (some sell ships, most don't). Each system has a
    hidden anomaly. Missions, bounties, surveys and lounge talk now stay within a few jumps of where you are.
    Jumps take half as long in game hours as before (the galaxy is much wider).
83. **Galaxy map zoom (M16a).** L1/R1 zoom. GALAXY: the whole map, just the stars (and where you are).
    REGION: names. CLOSE: names plus owner and security under each (EMPTY, NO FUEL for empty systems).
    The D-pad moves to the next system in that direction; the view glides after it. TRIANGLE: back to you.
    Old saves (before M16a) do not load: the galaxy is new.
84. **The dev menu is hidden behind a cheat code (M16b).** On the title screen (NEW GAME / LOAD GAME), press
    UP UP DOWN DOWN LEFT RIGHT LEFT RIGHT X O START (the classic code, with X O for B A), or the short
    UP DOWN UP DOWN LEFT RIGHT X O START. "CHEAT CODE ACCEPTED" appears, and the next game you start or load has
    DEV MENU at the bottom of the START menu. It is never saved: every visit to the title screen starts with it
    off, so enter it again each time. Without it, START has no DEV MENU and the speed readout stays hidden.
85. **Cargo scans, calmer (M16c).** Not every patrol stops you: the odds go with the system's security (70% in
    high, 55% medium, 35% low, 20% lawless); a patrol that lets you pass means no new roll for 30 seconds. After
    a scan, 3 minutes before the next (was 90 s), and once you scan clean, that system's patrols leave you
    alone until you next dock there (new cargo). "Holding still" now means under 60 px/s (was 30). The patrol
    comes alongside and brakes instead of overshooting, and only YOU can run: if you are holding still, a
    patrol drifting out of range no longer counts as fleeing, and no warnings tick while you wait for it.
86. **Pirate activity by region (M16c).** Each system has a pirate level 0..6 (generated): the UFS heartland
    is quiet (AURORA 1: a lone raider every 8 minutes or so), it grows with every jump out from a capital
    through its own territory, and systems cut off from their own side are the worst (the UFS arm in the
    south-west beyond FSA space: 5, a gang every 90 s or so). Syndicate space 5, empty systems 6 (a gang a
    minute). Raids come at +-25% of the interval, not like clockwork; gangs are no bigger than the level.
    Security follows too: the navy can't hold an enclave.
87. **THE VEYR (M17a).** After 10 hours of real playing time (the clock on the dock status page), whatever you
    are doing, the Veyr come through the nebula past the Rim at RHEA and GALLOWAY. An EMERGENCY BROADCAST
    banner goes up over everything. From then on they spread along the hyperspace links: every 36 game hours
    on EASY (one world), 18 on NORMAL (one or two), 10 on HARD (two or three), worlds before empty space.
    Every human world that falls is broadcast (yours too: its tribute stops). Now and then the UFS, FSA,
    Syndicates or independents win one back. Veyr-held systems are green on the map; their ports are overrun
    (no docking, no fuel); Veyr swarms (Skitters, Mantises, Reavers) fly there, hostile to everyone and
    deaf to Syndicate standing, and raid the human systems on their border (40% of raids). The UFS pays a
    bounty for every one. Lounges talk about it, and grow uneasy as the 10 hours run out.
88. **Fighting back (M17a).** ATTACK COLONY works on Veyr worlds: 3/4/5 waves by difficulty, Broodguards on
    the hardest. Squadrons can attack them too, and a squadron holding a system fights when the Veyr come:
    strong enough and the swarm is thrown back (ships parked in a hangar there flee to a neighbouring port).
89. **The hive (M17a).** The nebula's five systems stay hidden until you find the way in: down 15 Veyr ships
    (their nav cores), take a world back from them, or buy the coordinates at the Syndicate black market
    (VEYR HIVE COORDINATES, 25000). Then the links past RHEA open: THE VEIL, CHITIN DRIFT, BROOD REACH,
    SPAWNING DEEP and VEYR PRIME. Taking VEYR PRIME (5/6/7 waves, a Leviathan last) opens THE HIVE MIND:
    XENOCIDE (burn it: the UFS's order; UFS 100, admiral; the FSA hates you, the
    Syndicates mark you) or SEIZE IT (defy the UFS: UFS -100, the Rim and FSA warm to you; the hive world's
    tech comes in M17b). Either way every Veyr world falls back to whoever held it, the nebula is yours, and
    the game goes on.
90. **Losing (M17a).** If the Veyr take the last human port, that's the end: the credits roll and it is back
    to the title screen. Saves from before M17a do not load (the galaxy grew by the nebula).
    Dev menu: VEYR < INVADE NOW / SPREAD ONE STEP / REVEAL THE HIVE >.
91. **Capturing Veyr ships (M17b).** A disabled Veyr ship can be boarded like any wreck, but everything aboard
    is riskier (+15%), and CLAIM needs a VEYR CONTAINMENT RIG from the Syndicate black market (once the Veyr
    have come; TRUSTED standing): rig 1 (20000) holds Skitters and Mantises, rig 2 (50000) Reavers, rig 3
    (120000) Broodguards and Leviathans. Without the right rig the hull rejects you and blows, every time; with
    it the risk is 45/30/15%. A Veyr hull that goes up takes part of your ship with it (60 damage plus 1/25 of
    its hull). Flying one: UFS patrols open fire on sight.
92. **The seized hive (M17b).** If you SEIZED VEYR PRIME, its spire (THE HIVE MIND) has a shipyard: BUY grows
    Veyr hulls (Skitter to Leviathan, refit them in the editor like any ship) and UPGRADES adds augmented Veyr
    tech, sold nowhere else: VEYR BIO-REGEN HULL (the hull knits itself in flight: 1% every 1.5/0.75/0.5 s)
    and VEYR NERVE LINK (+25% energy recharge a level), 3 levels each. Illegal tech is sold there too.
93. **Joining a navy (M17b).** UFS shipyards sell UFS fighters and corvettes (Talon, Vanguard, Sentinel) to a
    UFS ALLY (60+) (M18b: and the Bastion; the Resolute and Sovereign at 90+, like the FSA's), the ADMIRAL
    is never scanned, and draws 1500 CR a day in admiral's pay. FSA shipyards sell Wasp, Hammer, Renegade and
    Defiant to an FSA ALLY, and the Reprisal and Liberty at 90+. The Syndicate's yards already sell its own.
    The status page shows ADMIRAL OF THE UFS or HIVE HOLDER once the hive is decided. The emergency broadcast
    banner now clears with any button once you've had a moment to read it. M17a saves still load.
94. **Faction story lines (M18a).** Docked, the main menu has a new COMMS page with three contacts:
    COMMANDER IRIS HALE (UFS Naval Intelligence), MAJOR ADA VOSS (FSA Command) and OLD MOTHER RUSK (the
    Syndicate). Each reaches you at any port of their own side. Pick one, read the briefing, press X and the job
    goes on your contract list like any other (THE FOX, LT. GRAYSON, SLIPPERY DEKE and INSPECTOR MOLL are
    story bounties). Finishing one shows a banner with the pay and when the next orders come (14 to 30 game
    hours). Every story job moves your standing: your side up, its rivals down, so following one line for a few
    hours drifts you toward it on its own.
95. **The oath (M18a).** Job 10 of each line is THE OATH / THE PLEDGE / THE BLOOD MARK. Its briefing carries a
    red POINT OF NO RETURN warning and asks for X twice. Swearing makes you an ALLY (UFS/FSA) or ESTABLISHED
    (Syndicate), turns the rivals hostile, and closes the other two stories for good.
96. **The Veyr war in the stories (M18a).** After the oath, the sworn line waits until the Veyr come (hour 10),
    then gives 3 or 4 war jobs: proving they can be killed, finding the way into the nebula (this reveals the
    hive, the same as the other reveal paths), and taking VEYR PRIME. The Syndicate line also hands you a level 1
    containment rig. Neutral players still reach the hive by the old paths.
    Dev menu: COMPLETE CONTRACTS now also clears the story wait. Saves from M17 load (save version 11).
97. **Balance (M18b).** Aurora's last defence wave is led by a RESOLUTE destroyer now, the same class as
    Kestrel's Reprisal. Every FSA ship is built to one rule against its UFS twin: 10% more hull, 15% less
    shield, slower shield recharge, 5% cheaper (the Wasp and Ghost got much tougher; the Defiant, Reprisal and
    Liberty barely moved). XENOCIDE no longer gives a free Sovereign: UFS yards sell the Bastion to an ally and
    the Resolute and Sovereign at 90 standing, the same as the FSA's Reprisal and Liberty.
98. **Three ways to end the Veyr war (M18b).** Taking VEYR PRIME still gives the hive choice, but burning it
    now serves whoever you swore to:
    - Burned for the UFS (sworn UFS, or nobody): ADMIRAL OF THE UFS and THE AURORA STAR.
    - Burned for the FSA (sworn FSA): MARSHAL OF THE FSA and THE LIBERTY STAR.
    - Seized (anyone): THE RIM CROWN. Syndicate standing goes to the top, and every third day the hive grows
      you a Veyr ship (mostly Skitters and Mantises, sometimes a Reaver, rarely a Broodguard) that waits at
      VEYR PRIME.
    Each gets 1500 CR a day, free fuel and 10% better trade and ship prices in your side's ports (and your
    own), and the nebula's five worlds. A burned nebula stands empty: settlers come over ten days, and its
    tribute grows with them. The admiral's and marshal's own patrols salute instead of scanning.
99. **The war after the war (M18b).** The admiral's new orders: bring every FSA world to heel. The marshal's:
    free every world the UFS holds. The crown's: take every navy world, UFS and FSA (the Rim Syndicates are
    yours anyway, and the independents are left alone). A world taken for a navy flies its flag and pays you
    a war prize (4000 to 10000 CR); one taken for the crown is yours and pays tribute. COMMS shows your title
    and how many worlds are left. Take the last one: VICTORY, a broadcast and 100000 CR. The game goes on.
    Dev menu: VEYR < END: BURN FOR UFS / END: BURN FOR FSA / END: SEIZE HIVE > ends the war at once.
    Save version 12 (M17 and M18a saves still load).
100. **Music and sound (M19a).** The game has sound now. Five songs, all synthesized and played by the SPU
    itself (with its hall reverb), and the music follows what you're doing:
    - TITLE ("Frontier", D minor) on the title screen.
    - FLIGHT ("Drift", ambient, the melody comes in on the second time round) in quiet space.
    - COMBAT ("Engage") while anything hostile is within about a screen and a half, and for six seconds after.
    - DOCK ("Port of Call", laid back) whenever you're docked.
    - VEYR ("The Swarm", dark) in a Veyr-held system, near Veyr ships, and over the game-over credits.
    Songs fade into each other. Sound effects: lasers (yours, theirs and the Veyr's all sound different),
    cannons, missiles, shield and hull hits, small and big explosions, ships powering down when disabled,
    docking clamps, launching, the hyperspace jump, the afterburner, a ka-ching for trades and pay, the scan
    ping, the emergency broadcast, a hull alarm under 25%, salvage, and menu clicks. Sounds out in space are
    panned left and right and fade with distance.
101. **Volume (M19a).** START > OPTIONS has MUSIC and EFFECTS, 0 (off) to 10: LEFT and RIGHT. The title
    screen has a SOUND page for the same. The volumes are kept in your save (save version 13; M18 saves
    still load, with the defaults: music 7, effects 8).
    Please tell me: are the levels right on a real TV? Too much bass? Too loud against the effects?
102. **Everything from before** should still work: combat, shields, disabling,
   merchants, targeting, stress test. Please send the stress-test `VBL/FRAME`,
   `CPU` and `PEAK PROJ` again from this Release build.

Questions: does the autopilot feel right (speed, braking, turning)? Is the planet
readable at its new size, is 90 px/s a fair docking speed, and is the menu
text legible at 640x480? Does the station look like a station?

## Files

Sound pipeline (tools/audio, run on a PC): `python3 tools/audio/gen.py` synthesizes every instrument
and effect (`sounds.py`), compiles the music (`songs.py`, notes written as text like `A4:4 C5:2`), encodes it
all to SPU ADPCM and writes `src/audio_data.c/.h`. WAV previews of each sound land in /tmp/frontier_sounds.

Ship art pipeline (tools/shipgen, run on a PC, not on the PS1):
`ships.py` builds each ship as a low-poly 3D model in Blender (the `bpy` Python package) and
renders a 512x512 top-down image and a 3/4 view; `convert.py` shrinks the top-down image to
64x64, picks 15 colours plus transparency, and writes `src/sprites_data.c/.h`. To change a ship,
edit its function in ships.py and run: `python3 ships.py RENDERS wayfarer raider hauler` then
`python3 convert.py RENDERS ../../src wayfarer raider hauler`.


| File | Purpose |
| --- | --- |
| `src/main.c` | Start-up, main loop, camera, HUD |
| `src/gfx.c/.h` | Double-buffered display, drawing layers, primitive pool |
| `src/input.c/.h` | Controller reading |
| `src/ship.c/.h` | Flight model, firing, shields and subsystem damage, ship drawing |
| `src/enemy.c/.h` | Pirate and merchant AI, targeting queries, health bars |
| `src/ships_data.c` | Ship definitions and loadouts (the numbers to tune) |
| `src/weapon.c/.h` | Weapon table and the projectile pool |
| `src/collide.c/.h` | Spatial-grid collision of projectiles against ships |
| `src/drone.c/.h` | Target drones and the stress-test spawner |
| `src/fx.c/.h` | Particles: sparks, fire, trails |
| `src/rng.c/.h` | Seeded random numbers |
| `src/world.c/.h` | Starfield, station, planet, jump point, beacons; the system's fixed objects |
| `src/dock.c/.h` | Docking rules, the station menus and the shipyard |
| `src/place.c/.h` | Per-place table: description, interior picture, services |
| `src/clock.c/.h` | The game calendar (one game hour = 60 real seconds) |
| `src/market.c/.h` | Commodities, per-place stock and prices, the cargo hold |
| `src/outfit.c/.h` | Credits, pilot name, module levels and their effect on ship stats |
| `src/galaxy.c/.h` | Star systems, links, fuel and travel time, route finding, the galaxy map |
| `src/salvage.c/.h` | The boarding and salvage menu, self-destruct risk |
| `src/escort.c/.h` | Escort ships: formation, stances, damage |
| `src/sysmenu.c/.h` | The Start menu: fleet window, options, controls page |
| `src/faction.c/.h` | Reputation with the Federation and Rebels |
| `src/conquest.c/.h` | Planet conquest: assaults, defence waves, captured systems, tribute (M14g) |
| `src/audio.c/.h` | Music sequencer (in the vblank), sound effects, volumes, SPU set-up and reverb (M19a) |
| `src/audio_data.c/.h` | Generated: the sound bank (SPU ADPCM), instruments, effects, songs |
| `tools/audio/` | `gen.py` builds the sound bank: `sounds.py` synthesizes, `songs.py` is the music, `adpcm.py` encodes |
| `tools/preview/host_spu.c` | The preview tool's stand-in SPU: FR_AUDIO=out.wav records what the game plays |
| `DESIGN_NOTES.md` | Owner design notes: factions, radar, stances, controls |
| `src/nav.c/.h` | Navigation target, pointer, autopilot, jump transition |
| `src/common.h` | Number types and fixed-point helpers |
| `iso.xml`, `system.cnf` | Disc layout and boot file |

## Tuning

Shield size and recharge are `shield_arc_max` and `shield_recharge` in
`src/ships_data.c`. NPC behaviour numbers (ranges, flee threshold, respawn
times) are the `#define`s at the top of `src/enemy.c`; the disable threshold
is `DISABLE_PCT` in `src/ship.c`. Weapons are in `src/weapon.c` (`damage`, `range`, `proj_speed`, `fire_interval`);
rocket counts are `ship_loadouts` in `src/ships_data.c`. Ships: `max_speed`, `accel` and `turn_rate` define how each
ship feels; the comment at the top explains the units. Rebuild after changing.

## Known limits of this milestone

- Shapes are flat-coloured polygons. Textured sprites come later.
- NTSC timing only. PAL (50 Hz) needs scaled speeds.
- The lounge and the mercenary guild are locked menu entries until Milestones 13
  and 11. There are only two markets, so profit comes from one route; the
  galaxy brings more in Milestone 9. Credits are not saved anywhere yet.
- Menu text is the small built-in 8x8 font (doubled for headings); a proper
  font comes later.
- The four systems, their positions, owners, security levels and the numbers of pirates
  and merchants are placeholders; territories, fleets and defenders are to be designed later.
- Only the AURORA and VESTA stations sell ships. Lounge and guild are still locked.
- Ship and weapon tables and the system layout are written in C. They move to
  data files when the data pipeline is built.
- The hangar is one global list, not tied to a place. Fleet ships cannot yet fly with you or be hired as escorts. The fleet is not saved (no saving yet).

## Next: Milestone 13 (missions and exploration)

Minimap/radar, missile types, hangar capacity and the graphics overhaul are queued (see DESIGN_NOTES.md).
