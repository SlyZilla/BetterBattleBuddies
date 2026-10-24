class BBB_ClaimsTestHooks : BBB_Hooks
{
 ref array<SCR_AIGroup> m_Groups = {};
 override void GetGroups(notnull array<SCR_AIGroup> groups) { groups.Copy(m_Groups); }
}

// Role claims A/B (-bbbClaimsTest): two identical US fire teams see a passive enemy team
// 140 m away. Once both teams have contact, one rifleman in each loses his spare magazines
// and FireManeuver (held off until then) is switched on in the same second: Rearm sends him
// to dead squadmates 24 m back just as FireManeuver looks for four riflemen to bound.
// Team A uses role claims; team B is on the vanilla list (the old per-feature guards).
// B (old): FireManeuver enlists the man on his live ammo run; Rearm then sees the fire job
// and drops the run. A: while Rearm holds him, FireManeuver doesn't count him.
// What this does NOT show (measured 24 Oct): a better outcome for the player. Under attack
// the rearm move (move-individually +5) never runs ahead of the attack behaviour, and Rearm
// drops the run itself when he swaps weapons, so FireManeuver gets him a few seconds later
// with claims on as well. The end state is printed, not asserted.
// Buildings, StaticWeapons, Morale, throws and relay are off so only this pair acts.
class BBB_ClaimsTest : BBB_AimTest
{
 static ref BBB_ClaimsTest s_Claims;
 static const int EXPECTED = 11;
 ref BBB_ClaimsTestHooks m_Hooks;
 ref BBB_Hooks m_SavedHooks;
 ref array<SCR_AIGroup> m_Teams = {};
 ref array<SCR_AIGroup> m_Enemies = {};
 ref array<SCR_AIGroup> m_Donors = {};
 ref array<ref array<IEntity>> m_Bodies = {};
 ref array<ChimeraCharacter> m_Runner = {};
 ref array<int> m_Overlap = {};      // seconds on a live ammo run while in a fire job
 ref array<int> m_FireWhileHeld = {}; // seconds in a fire job while his claim said rearm
 ref array<int> m_InFire = {};       // seconds in a fire job while out of spare magazines
 ref array<int> m_RearmClaimed = {};  // seconds his claim said rearm
 ref array<float> m_Closest = {};    // nearest he got to a body
 ref array<bool> m_bTripSeen = {};
 vector m_vBase;
 int m_Checks;
 int m_Failures;
 int m_Phase;
 int m_Wait;
 IEntity m_LapseProbe;

 static void Start()
 {
  if (!Replication.IsServer() || !System.IsCLIParam("bbbClaimsTest")) return;
  s_Claims = new BBB_ClaimsTest();
  GetGame().GetCallqueue().CallLater(s_Claims.Setup, 15000);
 }

 void ClaimCheck(string name, bool ok, string detail = "")
 {
  m_Checks++;
  if (!ok) m_Failures++;
  PrintFormat("[BBB] CLAIMSTEST %1 ok=%2 %3", name, ok, detail);
 }

 void Setup()
 {
  BBB_Claims.Read();
  ClaimCheck("default-on", BBB_Claims.s_bOn);
  BBB_StaticWeapons.s_bOn = false;
  BBB_FireManeuver.s_bOn = false;
  BBB_Buildings.s_bOn = false;
  BBB_Morale.s_bOn = false;
  BBB_Grenades.s_bOn = false;
  BBB_Smoke.s_bOn = false;
  BBB_Flares.s_bOn = false;
  BBB_ContactRelay.s_bOn = false;
  m_SavedHooks = BBB.s_Hooks;
  m_Hooks = new BBB_ClaimsTestHooks();
  BBB.s_Hooks = m_Hooks;
  m_vBase = BBB.Ground("1340 0 2770");
  ObserversSystem observers = ObserversSystem.Cast(GetGame().GetWorld().FindSystem(ObserversSystem));
  if (observers) observers.InsertObserverSP(22149, m_vBase[0], m_vBase[2], null);
  bool made = true;
  for (int i = 0; i < 2; i++)
  {
   vector at = m_vBase + Vector(i * 120, 0, 0);
   SCR_AIGroup team = SCR_AIGroup.Cast(Spawn(TEAM_US, at, "0 0 1"));
   SCR_AIGroup enemy = SCR_AIGroup.Cast(Spawn(TEAM_USSR, at + Vector(0, 0, 140), "0 0 -1"));
   SCR_AIGroup donor = SCR_AIGroup.Cast(Spawn(TEAM_US, at + Vector(0, 0, -24), "0 0 1"));
   made = made && team && enemy && donor;
   m_Teams.Insert(team);
   m_Enemies.Insert(enemy);
   m_Donors.Insert(donor);
   m_Runner.Insert(null);
   m_Overlap.Insert(0);
   m_FireWhileHeld.Insert(0);
   m_InFire.Insert(0);
   m_RearmClaimed.Insert(0);
   m_Closest.Insert(1000);
   m_bTripSeen.Insert(false);
  }
  ClaimCheck("groups", made);
  if (!made) { FinishClaims(); return; }
  m_Hooks.m_Groups.Insert(m_Teams[0]);
  m_Hooks.m_Groups.Insert(m_Teams[1]);
  GetGame().GetCallqueue().CallLater(Prepare, 4000);
 }

 void PrepareGroup(SCR_AIGroup group, bool passive)
 {
  PinDetail(group);
  group.ActivateAI();
  EAIGroupCombatMode mode = EAIGroupCombatMode.FIRE_AT_WILL;
  if (passive) mode = EAIGroupCombatMode.HOLD_FIRE;
  group.GetGroupUtilityComponent().SetCombatMode(mode);
  group.GetGroupUtilityComponent().EvaluateCombatMode();
  array<AIAgent> agents = {};
  group.GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   PinDetail(agent);
   agent.ActivateAI();
   ChimeraCharacter body = ChimeraCharacter.Cast(agent.GetControlledEntity());
   if (!body) continue;
   body.GetDamageManager().EnableDamageHandling(false);
   if (passive)
   {
    body.GetCharacterController().SetDisableWeaponControls(true);
    body.GetCharacterController().SetDisableMovementControls(true);
   }
  }
 }

 void Prepare()
 {
  for (int i = 0; i < 2; i++)
  {
   PrepareGroup(m_Teams[i], false);
   PrepareGroup(m_Enemies[i], true);
   // The donor team dies where it stands: its bodies carry matching magazines.
   array<AIAgent> donors = {};
   m_Donors[i].GetAgents(donors);
   ref array<IEntity> bodies = {};
   m_Bodies.Insert(bodies);
   foreach (AIAgent donor : donors)
   {
    bodies.Insert(donor.GetControlledEntity());
    SCR_DamageManagerComponent damage = SCR_DamageManagerComponent.GetDamageManager(donor.GetControlledEntity());
    if (damage) damage.Kill(Instigator.CreateInstigator(null));
   }
   array<AIAgent> agents = {};
   m_Teams[i].GetAgents(agents);
   ChimeraCharacter runner;
   if (agents.Count() > 3) runner = ChimeraCharacter.Cast(agents[3].GetControlledEntity());
   m_Runner[i] = runner;
  }
  // Team B keeps the old arbitration.
  array<AIAgent> oldWay = {};
  m_Teams[1].GetAgents(oldWay);
  foreach (AIAgent agent : oldWay) BBB_Claims.s_aVanilla.Insert(agent.GetControlledEntity());
  // Service semantics on a bystander of team A: rank, release, lapse.
  array<AIAgent> teamA = {};
  m_Teams[0].GetAgents(teamA);
  IEntity probe = teamA[1].GetControlledEntity();
  bool ranked = BBB_Claims.Take(probe, BBB_Claims.BUILDINGS) && BBB_Claims.Take(probe, BBB_Claims.RESCUE) && !BBB_Claims.Take(probe, BBB_Claims.FIRE) && !BBB_Claims.Can(probe, BBB_Claims.REARM) && BBB_Claims.Committed(probe);
  BBB_Claims.Release(probe, BBB_Claims.FIRE);
  bool wrongRelease = BBB_Claims.Role(probe) == BBB_Claims.RESCUE;
  BBB_Claims.Release(probe, BBB_Claims.RESCUE);
  ClaimCheck("rank-and-release", ranked && wrongRelease && BBB_Claims.Role(probe) == BBB_Claims.NONE, string.Format("ranked %1, other role's release kept it %2", ranked, wrongRelease));
  BBB_Claims.Take(probe, BBB_Claims.BUILDINGS, 500);
  m_LapseProbe = probe;
  GetGame().GetCallqueue().CallLater(CheckLapse, 1500);
  GetGame().GetCallqueue().CallLater(Poll, 1000, true);
 }

 void CheckLapse()
 {
  ClaimCheck("unrenewed-claim-lapses", m_LapseProbe && BBB_Claims.Role(m_LapseProbe) == BBB_Claims.NONE);
 }

 void StripMagazines(ChimeraCharacter soldier)
 {
  if (!soldier) return;
  BaseMuzzleComponent muzzle;
  int count;
  BBB_Rearm.NeedsAmmo(soldier, muzzle, count);
  SCR_InventoryStorageManagerComponent inventory = SCR_InventoryStorageManagerComponent.Cast(soldier.FindComponent(SCR_InventoryStorageManagerComponent));
  if (!inventory || !muzzle || !muzzle.GetMagazineWell()) return;
  string well = muzzle.GetMagazineWell().Type().ToString();
  array<IEntity> carried = {};
  inventory.GetItems(carried);
  foreach (IEntity item : carried)
  {
   if (!item) continue;
   MagazineComponent magazine = MagazineComponent.Cast(item.FindComponent(MagazineComponent));
   if (!magazine || !magazine.GetMagazineWell() || magazine.GetMagazineWell().Type().ToString() != well) continue;
   inventory.TryRemoveItemFromInventory(item);
   SCR_EntityHelper.DeleteEntityAndChildren(item);
  }
 }

 void Next(int phase)
 {
  m_Phase = phase;
  m_Wait = 0;
 }

 // Members with a report FireManeuver would count (45-300 m, at most 5 s old).
 int Reporting(SCR_AIGroup group)
 {
  int count;
  array<AIAgent> agents = {};
  group.GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   vector known;
   if (BBB_FireManeuver.Detected(SCR_AIUtilityComponent.Cast(agent.FindComponent(SCR_AIUtilityComponent)), known)) count++;
  }
  return count;
 }

 int SpareMags(ChimeraCharacter soldier)
 {
  BaseMuzzleComponent muzzle;
  int count;
  if (!soldier) return 0;
  BBB_Rearm.NeedsAmmo(soldier, muzzle, count);
  return count;
 }

 float NearestBody(int i)
 {
  float best = 1000;
  foreach (IEntity body : m_Bodies[i])
   if (body) best = Math.Min(best, vector.DistanceXZ(body.GetOrigin(), m_Runner[i].GetOrigin()));
  return best;
 }

 void Poll()
 {
  m_Wait++;
  if (m_Wait > 70) { ClaimCheck("phase-timeout", false, string.Format("phase=%1", m_Phase)); FinishClaims(); return; }
  if (m_Phase == 0)
  {
   bool contact = Reporting(m_Teams[0]) >= 2 && Reporting(m_Teams[1]) >= 2;
   if (!contact && m_Wait < 30) return;
   ClaimCheck("both-teams-in-contact", contact, string.Format("reporting A %1, B %2", Reporting(m_Teams[0]), Reporting(m_Teams[1])));
   if (!contact) { FinishClaims(); return; }
   for (int r = 0; r < 2; r++) StripMagazines(m_Runner[r]);
   BaseMuzzleComponent muzzle;
   int mags0, mags1;
   bool need0 = m_Runner[0] && BBB_Rearm.NeedsAmmo(m_Runner[0], muzzle, mags0);
   bool need1 = m_Runner[1] && BBB_Rearm.NeedsAmmo(m_Runner[1], muzzle, mags1);
   ClaimCheck("runners-need-ammo", need0 && need1, string.Format("spare mags A %1, B %2", mags0, mags1));
   // Rearm checked them a moment ago (nothing needed, 15 s cooldown): clear that and let
   // its real per-group pass run now, so the run has started when FireManeuver looks.
   BBB_Rearm.Stop();
   BBB_Rearm.CheckRearmFor(m_Teams[0]);
   BBB_Rearm.CheckRearmFor(m_Teams[1]);
   BBB_FireManeuver.s_bOn = true;
   Next(1);
   return;
  }
  for (int i = 0; i < 2; i++)
  {
   ChimeraCharacter runner = m_Runner[i];
   SCR_AIUtilityComponent utility = BBB_StaticWeapons.UtilityOf(runner);
   if (m_Wait <= 3 || m_Wait % 5 == 0) PrintFormat("[BBB] CLAIMSTEST t=%1 team %2: claim %3, trip %4, fire job %5, spare %6, body %7 m", m_Wait, i, BBB_Claims.Name(BBB_Claims.Role(runner)), BBB_Rearm.InRearmAction(runner), utility && utility.m_BBBFireJob, SpareMags(runner), (int)NearestBody(i));
   bool inFire = utility && utility.m_BBBFireJob;
   bool onTrip = BBB_Rearm.InRearmAction(runner);
   if (inFire && onTrip) m_Overlap[i] = m_Overlap[i] + 1;
   if (inFire && SpareMags(runner) == 0) m_InFire[i] = m_InFire[i] + 1;
   if (BBB_Claims.Role(runner) == BBB_Claims.REARM) m_RearmClaimed[i] = m_RearmClaimed[i] + 1;
   if (inFire && BBB_Claims.Role(runner) == BBB_Claims.REARM) m_FireWhileHeld[i] = m_FireWhileHeld[i] + 1;
   if (onTrip) m_bTripSeen[i] = true;
   m_Closest[i] = Math.Min(m_Closest[i], NearestBody(i));
  }
  // Long enough for FireManeuver's 4 s admissions and a 22 m walk.
  if (m_Wait < 35) return;
  ClaimCheck("rearm-trip-on", m_bTripSeen[0], string.Format("vanilla trip seen %1", m_bTripSeen[1]));
  ClaimCheck("vanilla-fire-job-enlists-runner-on-run", m_Overlap[1] >= 1, string.Format("%1 s on a live run in a fire job", m_Overlap[1]));
  ClaimCheck("on-fire-job-leaves-runner-on-run", m_Overlap[0] == 0 && m_FireWhileHeld[0] == 0, string.Format("%1 s on a live run in a fire job, %2 s while held for rearm", m_Overlap[0], m_FireWhileHeld[0]));
  ClaimCheck("on-run-claimed", m_RearmClaimed[0] >= 1, string.Format("%1 s", m_RearmClaimed[0]));
  PrintFormat("[BBB] CLAIMSTEST end state (not asserted): spare mags A %1 / B %2, closest to a body A %3 m / B %4 m, empty in a fire job A %5 s / B %6 s", SpareMags(m_Runner[0]), SpareMags(m_Runner[1]), m_Closest[0].ToString(4, 1), m_Closest[1].ToString(4, 1), m_InFire[0], m_InFire[1]);
  BBB_Claims.s_bOn = false;
  bool offTake = BBB_Claims.Take(m_Runner[0], BBB_Claims.BUILDINGS) && BBB_Claims.Can(m_Runner[0], BBB_Claims.BUILDINGS) && BBB_Claims.Role(m_Runner[0]) == BBB_Claims.NONE && !BBB_Claims.Committed(m_Runner[0]);
  BBB_Claims.s_bOn = true;
  ClaimCheck("off-switch-arbitrates-nothing", offTake);
  FinishClaims();
 }

 void FinishClaims()
 {
  GetGame().GetCallqueue().Remove(Poll);
  for (int i = 0; i < m_Runner.Count(); i++)
  {
   if (!m_Runner[i]) continue;
   SCR_AIUtilityComponent utility = BBB_StaticWeapons.UtilityOf(m_Runner[i]);
   PrintFormat("[BBB] CLAIMSTEST team %1: spare mags %2, closest to a body %3 m, fire job now %4, %5 s empty in a fire job, %6 s both jobs, fire jobs started %7", i, SpareMags(m_Runner[i]), m_Closest[i], utility && utility.m_BBBFireJob, m_InFire[i], m_Overlap[i], BBB_FireManeuver.s_iStarted);
  }
  PrintFormat("[BBB] CLAIMSTEST taken=%1 preempted=%2 refused=%3", BBB_Claims.s_iTaken, BBB_Claims.s_iPreempted, BBB_Claims.s_iRefused);
  if (m_SavedHooks) BBB.s_Hooks = m_SavedHooks;
  string verdict = "FAIL";
  if (m_Failures == 0 && m_Checks == EXPECTED) verdict = "PASS";
  PrintFormat("[ACR] VERDICT %1 test=bbbclaims passed=%2 failed=%3 expected=%4", verdict, m_Checks - m_Failures, m_Failures, EXPECTED);
  Print("[ACR] TEST COMPLETE");
 }
}

modded class SCR_BaseGameMode
{
 override protected void OnGameStart()
 {
  super.OnGameStart();
  BBB_ClaimsTest.Start();
 }
}
