# Contributing

Bug reports, reproducible test cases, documentation, scenarios and focused code
changes are welcome. Before starting a feature, check the pinned **Roadmap** issue
and the open issues labelled `in progress`: those are already being worked on. To take
something on, comment on its issue (or open one) so it can be assigned to you, and for a
substantial feature discuss it there or in [Discord](https://discord.gg/WK8xGSztB) first.
Push work in progress early as a draft pull request so others can see it.

## Development and releases

`main` is the shared development branch. It can include unfinished or unverified
changes ahead of Workshop. The README identifies the published Workshop version;
[CHANGELOG.md](CHANGELOG.md) separates unreleased work from released changes.
A source push does not publish a Workshop update. Official Workshop publication
is handled by the maintainer.

## Get your own working copy

Install Git, Arma Reforger and **Arma Reforger Tools** (Workbench) through Steam.
Use the regular game/tools versions together. Python 3 is used for source checks.
Fork [this repository](https://github.com/propst-design/BetterBattleBuddies) on GitHub, then run
these commands from your Workbench `addons` directory (replace `YOUR-USERNAME`):

```powershell
git clone https://github.com/YOUR-USERNAME/BetterBattleBuddies.git BetterBattleBuddies
cd BetterBattleBuddies
git remote add upstream https://github.com/propst-design/BetterBattleBuddies.git
git switch -c fix-my-change
```

Keep the addon folder name shown above. Open Workbench Launcher, add the base-game
project if it is missing, then use **Add Existing Project** to register the cloned
`addon.gproj`. See Bohemia's
[project setup guide](https://community.bistudio.com/wiki/Arma_Reforger:Mod_Project_Setup).
Avoid having multiple copies with the same addon GUID in an active addon search
path. Confirm your test loads your source copy rather than an installed Workshop
copy. Preserve existing GUIDs and commit metadata for any new resources.

BBB is standalone and requires only the base game. Add its `addon.gproj` to
Workbench, open it and let Workbench build the local resource database.
For a controlled fixture, open `Worlds/BBB_TestArland.ent`; also check a normal
Game Master or Conflict session with AI. You do not need ACA to develop BBB.

Relevant source: `Scripts/Game/BBB`, tests: `Scripts/Game/BBB_Tests`, resources:
`Configs/BBB` and `Prefabs`. Host integration uses `BBB_Hooks`; never add references
to ACA classes. To test ACA integration, clone
[ACA](https://github.com/propst-design/ArmaCommanderReforger) next to BBB and load
both local addons. State the exact source commits used in paired tests.

## Keep your work independent

Your branch and local files stay unchanged when the maintainer pushes to `main`.
Bring newer upstream changes in deliberately, after committing your own work:

```powershell
git fetch upstream
git merge upstream/main
```

Resolve any conflicts and retest. Do not force-push the shared `main`. For each
new task, start a fresh branch from an updated main:

```powershell
git switch main
git pull --ff-only upstream main
git switch -c another-fix
```

## Implement and test

Explicit player/GM orders and host policy take priority over autonomous reactions.
Use real perception, native movement/boarding and owned-state cleanup. Keep scans
bounded and measure cost. Tests should establish physical outcomes such as actual
movement, seat occupancy or ammunition use, rather than counters alone.

Preserve resource GUIDs, license notices and existing line endings. Keep changes
focused and avoid unrelated formatting. Enforce Script must compile in Workbench;
be careful with engine API signatures and avoid unsupported ternary expressions.

Install **Arma Reforger Server** through Steam. From the repository root:

```powershell
powershell -NoProfile -File tools/Test-BBB.ps1 -Preset vanilla
```

Use the relevant feature preset too (see the `ValidateSet` in
`tools/Test-BBB.ps1`), for example `orderleash`, `reactions`, `featureaudit` or
`firemaneuverperf`. Use `-ServerPath` if automatic server discovery fails.
The runner uses port 2001; do not stop an existing game/server to free it.
For changes that affect ACA, run its `srv:regress` and `srv:perf` presets with
the paired source as described in ACA's contribution guide. Report standalone
and integration results separately, including measurements and limitations.

For multiplayer changes, include a joined-client check when possible and state
what remains untested. Distinguish compilation, test completion, physical gameplay
results and multiplayer acceptance. A blocked test is not a passing test.

Before submitting, stage only the intended files and check them:

```powershell
git diff --cached --stat
python tools/check_public_source.py
git diff --cached --check
```

The source check also runs on GitHub pull requests and pushes. It catches common
private/generated files and recognizable sensitive text; it cannot replace a
manual review or compile/test the game.

## Submit a pull request

Commit your focused change, push your work branch to your fork, then open a pull
request with **base: `propst-design/BetterBattleBuddies`, branch: `main`**. Include the problem,
resulting behavior, reproduction steps, source commits, game/mod versions, tests
and remaining limitations. Draft pull requests are welcome for work in progress.
For paired changes in another mod, link the other pull request. The maintainer
reviews and merges accepted changes; no direct write access is required.
See [GitHub's contribution guide](https://docs.github.com/en/get-started/exploring-projects-on-github/contributing-to-a-project).

## Public files and licensing

Submit source, resource metadata, tests and useful contributor/player docs.
Keep generated caches, logs, local settings, credentials, personal paths and private
project records out of commits. Review staged file names and content before pushing;
sanitize any diagnostic excerpts you share. Do not commit extracted base-game or
third-party mod assets. By submitting a contribution, you agree to distribute it
under this project's existing [license](LICENSE.md); retain required attribution.
