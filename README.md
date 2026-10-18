# Better Battle Buddies

[![Game: Arma Reforger](https://img.shields.io/badge/Arma-Reforger-556b2f?style=flat-square)](https://reforger.armaplatform.com/)
[![Workshop release: 0.1.2](https://img.shields.io/badge/Workshop-0.1.2-blue?style=flat-square)](CHANGELOG.md)
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

The current public release is **0.1.2**, verified against the Workshop history.
Work toward **0.1.4** is in progress (see upcoming features below).
See [CHANGELOG.md](CHANGELOG.md) for full version history.

## What changes (0.1.2)

- **More alert soldiers.** Horizon scanning, weapon-readiness recovery and engagement fixes help AI notice trouble and bring their weapons back into the fight.
- **Smoke before aid.** Medics and teammates deploy smoke toward a threat before advancing to an exposed casualty, waiting for the cloud to bloom.
- **Tactical dismount under fire.** AI passengers in vehicles slowing under fire dismount to fight on foot while turret crews remain aboard.
- **Less getting stuck.** Standalone AI drivers reverse and steer away from obstacles. Door/gate helpers and spent-launcher handling address everyday snags.
- **A little personality.** Soldiers carry personal bios alongside combat improvements.

## Upcoming in 0.1.4

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
remain opt-in; see [documentation](documentation) for configuration and measured limits.

AC: Actual integrates through BBB's host hooks and manages its own commander,
bases, logistics and vehicle-driving decisions. AiO Command Menu is optional.
Standalone checks and earlier ACA integration checks have passed, but installed
Conflict/GM sessions, lifecycle behavior, ACE combinations and multiplayer still
need broader live acceptance. Publication is separate from that acceptance.

## Reporting problems

Include the BBB and game versions, scenario, other mods, whether you hosted or
joined, and what happened versus what you expected. Attach `console.log` and
`script.log` from the affected session. Normal game logs are under
`Documents\My Games\ArmaReforger\logs`; custom profiles and dedicated servers may
use another location. For multiplayer, include server and affected client logs
when available. Report through the Workshop item's feedback/discussion facilities.

## Working on BBB

BBB's addon ID is `BBB0261015000001`; its only declared base dependency is Arma
Reforger. For the shared development setup, keep this checkout beside
`ArmaCommanderReforger` under Workbench's `addons` directory and open `addon.gproj`.
BBB can also be loaded alone with a vanilla scenario.

Read [documentation](documentation) and [documentation](documentation) before contributing. The
documentation covers architecture, startup options, tests and the shared release
console. Legacy `tools/make_release.py` is not the normal release workflow.

Extracted from AC: Actual on 15 October 2026. Licensed under APL-SA; see
[LICENSE.md](LICENSE.md) for the retained original attribution and project credits.
