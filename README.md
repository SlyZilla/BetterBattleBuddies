# Better Battle Buddies

[![Game: Arma Reforger](https://img.shields.io/badge/Arma-Reforger-556b2f?style=flat-square)](https://reforger.armaplatform.com/)
[![Workshop release: 0.1.4](https://img.shields.io/badge/Workshop-0.1.4-blue?style=flat-square)](CHANGELOG.md)
[![Standalone addon](https://img.shields.io/badge/Addon-Standalone-teal?style=flat-square)](#install-and-play)
[![License: APL-SA](https://img.shields.io/badge/License-APL--SA-orange?style=flat-square)](LICENSE.md)

AI that keeps its head on a swivel—and knows when to hit the dirt.

Better Battle Buddies improves AI scanning, weapon readiness, combat reactions,
medical assistance and vehicle handling in Arma Reforger. Use it independently
in vanilla scenarios or as AC: Actual's required AI addon. BBB does not require
AC: Actual and does not add a commander game mode.

## Install and play

1. Find **Better Battle Buddies** in Arma Reforger's Workshop, download it and enable it.
2. Start a vanilla scenario such as Conflict or Game Master with AI present.
3. For multiplayer, load BBB on the server and joining clients.

The current Workshop release is **0.1.4**.
See [CHANGELOG.md](CHANGELOG.md) for full version history.

## What your buddies do

- **More alert soldiers.** Horizon scanning, weapon-readiness recovery and engagement fixes help AI notice trouble and bring their weapons back into the fight.
- **Smoke before aid.** Medics and teammates deploy smoke toward a threat before advancing to an exposed casualty, waiting for the cloud to bloom.
- **Tactical dismount under fire.** AI passengers in vehicles slowing under fire dismount to fight on foot while turret crews remain aboard.
- **Less getting stuck.** Standalone AI drivers reverse and steer away from obstacles. Door/gate helpers and spent-launcher handling address everyday snags.
- **A little personality.** Soldiers carry personal bios alongside combat improvements.

## Combat reactions and survival

- **Weapon mode reliability.** Fire-selector fixes accept already-selected fallback modes and prevent mode cycling.
- **Fire that keeps heads down.** Elevated, flanking and rear fire affect suppression; ambushes shock calm squads, and suppression takes longer to wear off.
- **Explosions with a beat of distance.** Foot soldiers hit the dirt as the blast wave reaches them using a 343 m/s acoustic delay: prone close in, crouched farther out.
- **Keep the vehicle fighting.** Available AI squadmates autonomously replace empty or incapacitated turret gunners on operational armed vehicles.
- **Get low against heavy armor.** Soldiers caught without anti-armor weapons adopt low stances (crouch out to 450 m, prone within 120 m) to minimize profile when facing armored vehicles, while rocket specialists hold position to engage.
- **Opportunistic combat rearming.** Soldiers running critically low on ammunition or lacking AT scavenge compatible magazines and rocket launchers from nearby casualties, retrying routes if interrupted.

These behaviors remain subject to the game's AI, navigation, available equipment
and current orders. They do not guarantee that every rescue, engagement or vehicle
recovery succeeds.

## Optional features and compatibility

Medic smoke is enabled by default. The separate safer-healing behavior is off by
default; server startup options include `-bbbMedicSmoke 0` to disable smoke and
`-bbbMedic 1` to enable safer healing. ACE is optional; ACE drag and other
ACE-dependent behavior require ACE to be loaded. Experimental combat options
remain opt-in.

AC: Actual integrates through BBB's host hooks and manages its own commander,
bases, logistics and vehicle-driving decisions. AiO Command Menu is optional.

### Unreleased source options

The source checkout also includes these experimental features. Each switch accepts `0` to disable its feature:

- `-bbbArmorReaction 1`: AI crews react to launcher hits and nearby rocket passes. Supported fitted smoke weapons receive a short firing pulse; slow native wheeled vehicles can reverse with a small steering bias after checking clearance, terrain and water. Movement is bounded to 7 m, 6.5 seconds and 25 degrees. Player control, waypoints and host driving take priority. Tracked driving and custom smoke systems are not supported by this initial option.
- `-bbbBuildings 1`: autonomous infantry near recently detected enemies enter native indoor cover, hold it, and can advance between up to three cover positions when the contact is inside the building. Uses native cover scoring and navigation, releases failed or stalled moves, and never teleports soldiers. Player control, waypoints, retreat and host policy take priority.
- Night illumination (enabled by default; `-bbbFlares 0` disables): infantry use carried launcher or handheld flares in native low-light conditions to illuminate recently sighted enemy sectors. Player/GM orders and host policy take priority. Requests reserve a 120-second squad cooldown and a 90-second, 125 m shared area hold, including failed attempts. No ammunition is spawned.
- `-bbbGrenades 1`: autonomous infantry squads deploy fragmentation hand grenades against perceived targets in cover or defilade within throwing range (10 to 35 m). Verifies friendly safety (>= 14 m clear of blast zone), throwing corridor clearance (2.5 m cylinder), and unobstructed trajectory, with a 25 s squad cooldown. Respects player/GM orders and player control.
- `-bbbStaticWeapons 1`: autonomous infantry squads in contact detect nearby operational, loaded friendly static weapons or defensible turrets (<= 35 m) facing perceived enemy threats (traverse arc <= 60 deg). Assigns a fit foot soldier to crew the weapon, and automatically releases and dismounts when ordered on, the squad moves away, the area clears for 15 s, ammunition is exhausted, or the weapon is destroyed. Respects player/GM orders and player control.
- `-bbbSmoke 1`: autonomous infantry squads deploy smoke grenades to screen casualty drags (`BBB_Drag`), bounding maneuver advances (`BBB_FireManeuver`), and retrograde break-contact withdrawals (`BBB_Morale`) directly toward perceived enemy threat sectors. Features spherical line-of-sight occlusion tracking (`BBB_Smoke.IsOccluded`), squad cooldowns, and strict safety guards respecting player control and orders.
- `-bbbMorale 1`: autonomous infantry squads evaluate morale under heavy attrition (casualty ratio >= 50%, leader loss, sustained suppression). When morale breaks under contact, the squad conducts a retrograde bounding peel-back away from the threat centroid into rear cover until contact is broken. Isolated, out-of-ammo survivors cornered within 15 m enter a surrender posture rather than charging. Use `0` to disable; respects player/GM orders and host hooks.
- `-bbbCrewTurnout 1`: armored AI crews use native turnout positions after 30 quiet seconds. Nearby known threats, incoming fire, suppression and vehicle hits keep them buttoned up. Only supported crew seats are used.
- `-bbbDriverAlign 1`: an idle wheeled-vehicle driver may make a short forward steering adjustment for a stable target the gunner currently sees and can engage. It requires clear, level ground, yields to any waypoint or player/GM order, and leaves host-managed driving alone. The adjustment is limited to 2.5 m, 20 degrees and eight seconds, followed by a cooldown. It does not search for targets or firing lines.

Use `0` in place of `1` to disable either option. These source features are not in
Workshop 0.1.4. Native tracked-vehicle driving, custom vehicle armor layouts and
multiplayer visual acceptance require separate validation.

## Reporting problems

**[Report a bug](https://github.com/propst-design/BetterBattleBuddies/issues/new?template=01-bug-report.yml)**
or **[suggest a feature](https://github.com/propst-design/BetterBattleBuddies/issues/new?template=02-feature-request.yml)**.
Search [existing issues](https://github.com/propst-design/BetterBattleBuddies/issues?q=is%3Aissue)
first; add details to an existing report when it describes the same problem.

The bug report form asks for BBB/game versions, platform, scenario, other mods,
hosting/joining context, steps to reproduce, expected behavior and actual behavior.
Report one problem per issue. Testing with only BBB and the base game helps isolate
conflicts, but is optional.

Attach `console.log` and `script.log` from the affected session when available.
Normal PC game logs are under `Documents\My Games\ArmaReforger\logs`; custom
profiles and dedicated servers may use another location. For multiplayer, include
server and affected client logs when available. Screenshots or video also help,
especially on consoles. Remove passwords, tokens, IP addresses and personal
information before uploading logs. Missing logs do not prevent reporting a bug.

Extracted from AC: Actual on 15 October 2026. Licensed under APL-SA; see
[LICENSE.md](LICENSE.md) for the retained original attribution and project credits.
