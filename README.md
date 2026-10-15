# Better Battle Buddies

General AI improvements for Arma Reforger, extracted from AC: Actual.
Runs independently in vanilla scenarios through its own startup adapter. AC: Actual
integrates through BBB_Hooks and depends on this addon.

Includes AI horizon scanning, weapon readiness, engagement and suppression fixes,
vehicle door/gate helpers, spent-launcher handling, personal bios, and existing
optional combat/medical features. Existing feature defaults are preserved; safer
medic healing is off by default and ACE drag requires ACE to be loaded.
BBB does not supply ACA's commander, bases, logistics, orders or vehicle stall decisions.

This is a development extraction, not a published Workshop release. Installed
Conflict/GM and multiplayer acceptance remain outstanding.

Place this checkout beside ArmaCommanderReforger in Workbench/addons, then open
addon.gproj in Workbench. For BBB alone, load addon BBB0261015000001 and a vanilla
scenario. For ACA, load its addon; the project declares BBB as a dependency.

Run `powershell -File tools/Test-BBB.ps1 -Preset vanilla` for independent startup,
or `python tools/make_release.py` for a stripped candidate outside Workbench/addons. See documentation for developer
instructions and LICENSE.md for APL-SA and the retained project credits.
