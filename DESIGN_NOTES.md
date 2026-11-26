# Design notes from the project owner (to be merged into the GDD)

## Factions and territory (M12 basics built; the rest to come)
- Federation systems: relatively safe; occasional pirates, or other AI space-farers (many ships
  coming and going: freight, passengers, smuggling), swiftly dealt with by large Federation patrols.
  Occasionally rebel assault fleets attack the Fed patrols (M14 dynamic events). Rebels do not
  attack you unless you have lost favour with them by siding with the Feds, and vice versa; you can
  still end up in the line of fire.
- Independent systems: no patrol fleets. You defend yourself. NPCs do not stay passive: if a pirate
  attacks a merchant (you or an NPC) the others fight back.
- Rebel systems: large rebel patrols like the Fed ones, but pirate and Federation attacks are more
  frequent there than in Federation systems.
- Built in M12: Federation (blue) and Rebel (magenta) patrol ships; reputation -100..+100 for each
  faction (siding with one costs half as much with the other); hostile at -25 or worse (patrols attack
  you, stations refuse docking). Not built: Fed raids in rebel systems, rebel assaults, pirate
  frequency tuning, NPC freight/passenger/smuggler traffic.

## Fleet and escorts
- Owned ships and hired escorts always follow you, whatever ship you pilot.
- Stances: FOLLOW, GUARD, ATTACK (so you can flee with them, use them in defence, or send them to attack).
- Built in M11: hangar tab (swap or sell claimed ships). Not built: following, stances, the guild.

## Radar and electronic warfare
- Minimap on the main screen. A ship with no nav upgrades has a very basic view: ships are small dots,
  with static and interference; you cannot tell size or friend from foe.
- Radar upgrades clear the signal, show ship mass, who is hostile and targeting you, and the mass of
  planets and stations.
- Illegal mods in Federation systems: radar jammers, which also disrupt some homing missiles/torpedoes.
- Tech warfare is as important as weapons.

## Weapons
- Missile/torpedo types: line-of-sight javelins (current rockets), heat-seeking homing missiles,
  radio-guided missiles (cheap, can be jammed). Turrets (lasers and cannons) track the target automatically.

## Controls (proposed by the owner, to be built as a dedicated step)
- L1 change missile/torpedo type, L2 fire it, R2 fire all lasers and guns.
- Cross: afterburner. Triangle: dock. Square: cycle stations / systems / jumps. Circle: autopilot.
- Start: system menu (save, load, options, controls, quit). Select: unassigned (suggest: galaxy map).
- DualShock: right stick strafes; click the left stick (L3) to lock on to the target ship (or planet,
  station, jump point; one thing at a time) and automatically follow it, pointing at it.
- Settled: R2 fires all lasers/guns, L1 changes missile/torpedo type, L2 fires the missile, R1 changes target ship; Select = galaxy map.

## Star charts (built in M12)
- A chart can be bought at every trading post (Select on the trading page, 500 credits); it reveals the
  systems within 40 map units. The map shows only visited or charted systems. Final price and radius to
  tune when the full galaxy exists.

## Graphics overhaul (a dedicated step, before the final art pass in M15)
- High-resolution, very detailed art: many ship designs, planets and space stations.
- Federation and Rebel military ships are unique designs, different from the civilian ships you can buy.
  Joining the Federation or Rebel army later lets you buy their ships; you can also try to claim
  disabled ones (salvage already supports claiming).
- Placeholder art stays until then. The owner finds the current placeholders "kinda cute"; we can go
  all out on detail.

## UI overhaul (a dedicated step)
- Switch the dock and shipyard screens from plain lists to buttons.
- Fix spacing problems at the same time. Seen on the dock screen: menu text such as REPAIR AND REARM and
  MERCENARY GUILD runs up against the interior picture. Also check the status page height (it gained
  lines in M11 and M12).

## Hangar capacity (owner idea)
- Big ships with hangar bays can carry small ships; small escorts then dock with the carrier automatically
  (and launch with it). Needs a hangar-capacity stat on ship definitions and a dock/launch behaviour for
  escorts. Not built.

## Galaxy scale and travel (owner, Nov 5)
- 50+ systems in a web of hyperspace links. Some systems can only be reached through others.
- Empty transit systems: no planets or stations, just open space, likely pirates, and nowhere to refuel.
  An antimatter scoop ship addon refuels the engines in such systems (and probably anywhere in flight).
- Large Rebel-held regions, a huge Federation core, and some Independent systems in between.
- The four placeholder systems are only a preliminary test; none of their layout is final.

## Pilots (draft, see the GDD's Chad Beefsteel section for the model)
- Chad Beefsteel: Wayfarer, broke; AURORA; Federation +10.
- Reginald Fanshawe-Buttersworth III: Hauler, about 25,000 credits; AURORA; Federation +25, Rebels -20.
- Zsa-Zsa Blastwell: Raider, about 3,000 credits and a 1,500 debt; CINDER; Federation -30, Rebels +10.
- Professor Marigold Quasar-Pennywhistle: Wayfarer with a big tank, about 4,000 credits, one free chart; KESTREL; Federation -15, Rebels +30.
