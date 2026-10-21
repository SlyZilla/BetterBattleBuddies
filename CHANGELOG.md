# Changelog

## 0.1.5 — unreleased

- Every BBB feature is now on by default, including fire and manoeuvre, safer medic healing, grenades, smoke, morale, static weapons, building garrison, vehicle defence, crew turnout, driver alignment and flares. Each one still has its own server option to switch it off (`-bbbFireManeuver 0`, `-bbbMedic 0`, `-bbbCombatManeuver 0` and the options listed below).
- Added doorway unsticking (`-bbbDoorUnstick 0` disables): a soldier who stands stuck next to a house door while trying to move gets that door swung fully open, or flipped to its other side. Doors are never closed on anyone.
- Added autonomous night illumination (`-bbbFlares 0` disables): soldiers carrying flare ammunition illuminate recently sighted enemy sectors, with player/GM order vetoes and shared squad/area cooldowns to limit repeated launches.
- AI pinned down in the open now fight back instead of freezing: a suppressed soldier with no cover keeps spotting enemies, shoots without the extra hesitation and fires faster and looser. In cover they still keep their heads down (`-bbbExposedFire 0` restores the old behaviour).
- Medics no longer walk past an enemy they can see on their way to a casualty: with a visible enemy within 50 m they fight first, then go. Soldiers dragging a casualty let go and shoot back when they see an enemy within 20 m.
- Fixed riflemen ignoring jeeps and Humvees: once rifle hits had bounced off a UAZ or Humvee hull, AI stopped shooting at it for the rest of the game, even at its exposed machine gunner. Only real armour (APCs, IFVs, tanks) is now treated as immune to rifle fire.
- Infantry no longer drop flat whenever an enemy vehicle with a turret is anywhere within 450 m. The get-low reaction now only answers real armour seen in the last 10 seconds and is on by default (`-bbbArmourScatter 0` turns it off).
- Fixed AI holding a very fast-firing weapon mode who could aim but never fire.
- Added armored-crew turnout (`-bbbCrewTurnout 0` disables): supported AI crew positions turn out after a quiet period, then button up when threats, incoming fire or vehicle hits return.
- Added limited-turret driver alignment (`-bbbDriverAlign 0` disables): idle AI drivers can make a small adjustment on clear, level ground for their gunner's visible, viable target. Movement orders, player control and unsafe positions prevent the adjustment.
- Added vehicle defence against launcher fire (`-bbbArmorReaction 0` disables): supported fitted smoke weapons fire and slow wheeled vehicles can make a short, guarded reverse maneuver. Player driving and movement orders take priority.
- Added building garrison and clearing (`-bbbBuildings 0` disables): infantry use native indoor cover near detected enemies, hold their positions and investigate further indoor cover when contacts are inside. Failed routes are abandoned; player orders take priority.
- Vehicle commanders (such as the LAV-25 commander's seat) now count as crew, not passengers: they stay aboard when passengers dismount under fire, and a commander left on his own is treated as the vehicle's crew (`-bbbCommanderCrew 0` restores the old behaviour).
- Fixed medic smoke: medics and teammates now actually throw the smoke cover added in 0.1.2 (the throw was silently skipped before).
- Added autonomous hand grenade employment (`-bbbGrenades 0` disables): infantry squads engaging perceived enemies in cover or defilade within throwing range (10-35 m) evaluate tactical grenade opportunities. Verifies fragmentation grenade availability, friendly blast safety clearance (>= 14 m), clear throwing corridor (2.5 m), and trajectory clearance without point-blank obstructions, enforcing a 25 s squad cooldown.
- Added autonomous static weapon crewing (`-bbbStaticWeapons 0` disables): idle or defending autonomous squads in contact locate nearby operational, loaded friendly static weapons or defensible turrets (<= 35 m) with threats in their traverse arc (default <= 60 deg). The nearest fit squadmate walks over and climbs in (no teleporting); he gives the gun up for player and Game Master orders and host-mod rules, and automatically releases and dismounts when ordered on, tether distance is exceeded, targets are cleared for 15 s, ammunition is exhausted, or the weapon is destroyed.
- Added tactical smoke screening (`-bbbSmoke 0` disables): AI squads autonomously deploy smoke grenades to screen casualty drags (`BBB_Drag`), bounding maneuver advances (`BBB_FireManeuver`), and retrograde break-contact bounds (`BBB_Morale`) directly toward perceived enemy threat sectors. Features spherical line-of-sight occlusion tracking (`BBB_Smoke.IsOccluded`), squad cooldowns, and strict safety guards respecting player control and orders.
- Added squad morale, retrograde break-contact withdrawal and surrender (`-bbbMorale 0` disables): autonomous infantry squads suffering heavy casualties (>= 50%), squad leader loss or heavy suppression conduct covered peel-back bounds to disengage from active contacts, stopping if they can't actually get away; isolated, out-of-ammo soldiers cornered by close enemies (<= 15 m) surrender instead of charging blindly.

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
