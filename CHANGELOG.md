# Changelog

## 0.1.5 — unreleased

- Added optional vehicle defence against launcher fire (`-bbbArmorReaction 1`): supported fitted smoke weapons fire and slow wheeled vehicles can make a short, guarded reverse maneuver. Player driving and movement orders take priority.
- Added optional building garrison and clearing (`-bbbBuildings 1`): infantry use native indoor cover near detected enemies, hold their positions and investigate further indoor cover when contacts are inside. Failed routes are abandoned; player orders take priority.
- Fixed medic smoke: medics and teammates now actually throw the smoke cover added in 0.1.2 (the throw was silently skipped before).
- Added optional autonomous hand grenade employment (`-bbbGrenades 1`): infantry squads engaging perceived enemies in cover or defilade within throwing range (10-35 m) evaluate tactical grenade opportunities. Verifies fragmentation grenade availability, friendly blast safety clearance (>= 14 m), clear throwing corridor (2.5 m), and trajectory clearance without point-blank obstructions, enforcing a 25 s squad cooldown.
- Added optional autonomous static weapon crewing (`-bbbStaticWeapons 1`): idle or defending autonomous squads in contact locate nearby operational, loaded friendly static weapons or defensible turrets (<= 35 m) with threats in their traverse arc (default <= 60 deg). The nearest fit squadmate walks over and climbs in (no teleporting); he gives the gun up for player and Game Master orders and host-mod rules, and automatically releases and dismounts when ordered on, tether distance is exceeded, targets are cleared for 15 s, ammunition is exhausted, or the weapon is destroyed.
- Added optional tactical smoke screening (`-bbbSmoke 1`): AI squads autonomously deploy smoke grenades to screen casualty drags (`BBB_Drag`), bounding maneuver advances (`BBB_FireManeuver`), and retrograde break-contact bounds (`BBB_Morale`) directly toward perceived enemy threat sectors. Features spherical line-of-sight occlusion tracking (`BBB_Smoke.IsOccluded`), squad cooldowns, and strict safety guards respecting player control and orders.
- Added optional squad morale, retrograde break-contact withdrawal and surrender (`-bbbMorale 1`): autonomous infantry squads suffering heavy casualties (>= 50%), squad leader loss or heavy suppression conduct covered peel-back bounds to disengage from active contacts, stopping if they can't actually get away; isolated, out-of-ammo soldiers cornered by close enemies (<= 15 m) surrender instead of charging blindly.

## 0.1.4 — released

- Expanded the Workshop overview with standalone scenario use, squad-command compatibility, server options and public GitHub source/issue links.
- Prevent repeated AI fire-selector requests when the weapon already uses the best available fallback mode.
- Added tactical suppression geometry and recovery tuning (`BBB_Threat`, `-bbbThreatGeometry 0`, `-bbbThreatDecay 0`): incoming fire from elevated high ground or exposed flanks/rear pins AI faster, ambushes against calm or marching squads deal initial suppression shock, nearby explosions carry heavier suppression weight, and suppression decays more gradually so base-of-fire tactics keep heads down longer.
- Added 343 m/s acoustic speed-of-sound explosion reflex (`BBB_Threat`, `-bbbThreatExplosion 0`): foot soldiers near a detonation hit the dirt (prone within 45 m, crouch out to 75 m) as the blast wave physically reaches them rather than reacting on the same frame or remaining standing.
- Added tactical turret gunner seat replacement (`BBB_VehicleHelpers`, `-bbbGunnerReplace 0`): when an armed vehicle's turret gunner is killed, knocked unconscious, or empty with available ammo, AI crew members autonomously replace them. Conscious cargo passengers aboard (preferring non-leaders) are promoted first; if empty and stationary, nearby dismounted squadmates on foot within 8 m mount the turret. Dead or incapacitated gunners are cleanly evicted before boarding.
- Turret logs now describe accepted boarding requests accurately, and development diagnostics track actual occupancy and later dismounts.
- Added anti-armor low-profile posture reaction and AT isolation (`BBB_Armour`, `-bbbArmourScatter 0` / `-bbbAntiArmor 0`): non-AT infantry get low (crouch out to 450 m, prone within 120 m) to minimize profile when confronted by hostile armored vehicles, while dedicated AT specialists hold stance without posture penalties to line up shots.
- Added opportunistic combat rearming and corpse scavenging (`BBB_Rearm`, `-bbbRearm 0`, `-bbbRearmRadius <m>`): soldiers running critically low on ammunition (< 2 magazines) or facing armored threats without AT scavenge nearby friendly/enemy casualties (default 25 m radius) for compatible magazines, launchers, and rockets, checking reach, floor height and obstructions before transfers. Scavenging yields to player/GM orders and control changes, limits failed trips with temporary backoff, restores items after failed inventory insertion, and matches live rocket ammunition to the selected launcher instead of equipment names.

## 0.1.2 — 2026-10-15

- Medics and teammates now deploy smoke cover toward enemy threats by default before advancing to treat an exposed casualty, waiting briefly for the smoke cloud to bloom.
- Added standalone tactical cargo dismount under fire: AI passengers in ground vehicles that slow down or lose their driver under fire automatically dismount to fight on foot while turret crews remain aboard.
- Added standalone speed-aware stalled vehicle unsticking: AI drivers stuck against obstacles reverse and steer clear automatically in Conflict, Game Master, and custom scenarios without requiring Arma Commander: Actual.

## 0.1.1 — 2026-10-15

- Updated the Workshop thumbnail.

## 0.1.0 — 2026-10-15

- First standalone Workshop release, extracted from Arma Commander: Actual.
- Includes existing horizon scanning, weapon readiness, engagement/suppression improvements, vehicle helpers, spent-launcher handling and character bios.
- Preserves the existing defaults for optional combat and medical features. ACE remains optional; ACE-dependent features require ACE.
- Provides the shared AI dependency used by the split ACA release. BBB itself does not require ACA.

Standalone startup and integrated ACA regression checks passed before extraction rollout. Installed gameplay and multiplayer acceptance are tracked separately.
