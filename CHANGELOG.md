# Changelog

## 0.1.4 — unreleased

- Expanded the Workshop overview with standalone scenario use, squad-command compatibility and server options.
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
