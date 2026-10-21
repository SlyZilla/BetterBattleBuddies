Better Battle Buddies

Keep your head on a swivel. Keep the gun in the fight. And when the blast comes, hit the dirt.

Better Battle Buddies brings general AI improvements to Arma Reforger: sharper scanning, weapon-readiness fixes, heavier suppression reactions, medical smoke and vehicle helpers. Run it in vanilla scenarios on its own, or alongside AC: Actual. No commander game mode is included, and AC: Actual is not required.

WHAT YOUR BUDDIES DO

- Watch the horizon and recover weapon readiness when trouble starts.
- Lower weapons during quiet ordinary travel and recover readiness when danger returns; combat and explicit tactical orders take priority.
- Throw smoke toward a threat before advancing to an exposed casualty, waiting briefly for the cloud to bloom.
- Passengers bail out of a slowing vehicle under fire while turret crews stay aboard.
- Reverse and steer out of vehicle snags, handle doors and gates, and discard spent launchers. Personal bios give the soldiers a little character too.

COMBAT REACTIONS AND SURVIVAL

- Prevent mode cycling with reliable fire-selector fallback handling.
- Feel the pressure of elevated and flanking fire. Ambushes shock calm squads, and sustained suppression keeps heads down longer.
- Hit the dirt as a nearby blast reaches them, with a 343 m/s acoustic reaction delay: prone close in, crouched farther out.
- Keep a turret manned when a gunner goes down, autonomously promoting available AI squadmates into the gun.
- Get low against armor: non-AT infantry adopt low stances to minimize profile when facing armored vehicles, while rocket specialists hold position to engage.
- Scavenge compatible ammunition, rocket launchers and rockets from nearby casualties when critically low in combat. Searches check reach and obstructions, retry interrupted trips and yield to explicit player/GM orders.

WHERE IT FITS

Use BBB for the AI already in your scenario: infantry patrols, firefights, casualty aid and ground-vehicle crews. It does not recruit a squad or issue your battle plan. AC: Actual adds map-level command and logistics; the optional AiO Command Menu gives orders to your own linked squad. BBB can run without either.

Mission makers can add BBB as a dependency to bring these behaviors to their scenarios. There is no ACA manager or commander setup to place for standalone use; BBB starts its vanilla integration automatically.

GET STARTED

Download and enable BBB, then start a scenario with AI, such as Conflict or Game Master. Multiplayer requires the addon on the server and joining clients. No other mod is required. ACE is optional; ACE-dependent features need ACE loaded.

OPTIONS AND LIMITS

Medic smoke is on by default. The separate safer-healing option and experimental combat features remain opt-in. Server startup options include -bbbMedicSmoke 0 to disable smoke and -bbbMedic 1 to enable safer healing. Individual opt-outs include -bbbThreatGeometry 0, -bbbThreatDecay 0, -bbbThreatExplosion 0, -bbbGunnerReplace 0, -bbbArmourScatter 0 and -bbbRearm 0. Use -bbbRearmRadius <metres> to change the 25 m scavenging radius (5–100 m).

BBB works through Reforger's AI: terrain, pathfinding, equipment and current orders still matter. AC: Actual supplies its own commander, logistics and vehicle-driving decisions through BBB's host hooks.

SOURCE, GUIDE AND FEEDBACK

Public GitHub: https://github.com/propst-design/BetterBattleBuddies
Read the README for setup and compatibility.
Discord reports and ideas: https://discord.gg/WK8xGSztB
Use #issues for bugs (choose BBB), #suggestions for ideas. No GitHub account needed.
GitHub alternative: https://github.com/propst-design/BetterBattleBuddies/issues

When reporting a problem, include versions, scenario, other mods, host/client role, steps and expected behavior. Attach the affected console.log and script.log; normal game logs are under Documents\My Games\ArmaReforger\logs, with custom profiles/server setups using their configured location. Multiplayer reports benefit from both server and affected client logs.

Directed and tested by Jacob; developed with Claude, Codex and Antigravity. Extracted from AC: Actual. Arma Public License Share Alike (APL-SA); full original attribution and project credits are retained in the included LICENSE.md.
