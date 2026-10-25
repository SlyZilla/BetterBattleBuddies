class BBB_TacticTestHooks : BBB_Hooks
{
 ref array<SCR_AIGroup> m_Groups = {};
 override void GetGroups(notnull array<SCR_AIGroup> groups) { groups.Copy(m_Groups); }
}

// Squad tactic A/B (-bbbTacticTest). Two pairs of identical US fire teams, A on the committed
// tactic, B left to the features' own rules (BBB_Tactic.s_aVanilla); enemies are passive and
// can't move, nobody can be hurt.
//  Pair 1, a fair fight (one enemy team 100 m north): A must go through the contact pause to
//  ASSAULT and fire and manoeuvre must run for it.
//  Pair 2, outnumbered (four enemy teams 70 m north): A must HOLD, run no fire and manoeuvre,
//  and in the 30 s after it holds advance at most 10 m, while B (old rules) presses on: runs fire
//  and manoeuvre or advances at least 5 m more.
//  Then pair 1's A loses its leader mid-assault: REORG within 4 s, no fire and manoeuvre for 15 s from then,
//  and the acting leader picks a tactic within 30 s.
// Callouts are printed, not judged (the server skips voice nobody can hear).
// Buildings, StaticWeapons, Morale, throws, rearm runs and relay are off.
class BBB_TacticTest : BBB_AimTest
{
 static ref BBB_TacticTest s_Tactic;
 static const int EXPECTED = 12;
 ref BBB_TacticTestHooks m_Hooks;
 ref BBB_Hooks m_SavedHooks;
 ref array<SCR_AIGroup> m_Teams = {};   // pair 1 A, pair 1 B, pair 2 A, pair 2 B
 ref array<ref array<SCR_AIGroup>> m_Foes = {}; // per pair
 int m_Checks;
 int m_Failures;
 int m_Wait;
 bool m_bContact, m_bAssault, m_bAssaultFM, m_bHold, m_bHoldFM;
 float m_fHoldAt = -1;
 vector m_vHoldA, m_vHoldB;
 bool m_bMeasured;
 bool m_bReorgFM;
 bool m_bWindowFMB; // B ran fire and manoeuvre in the 30 s after A held
 int m_iReorgSeen = -1; // tick REORG was first seen

 static void Start()
 {
  if (!Replication.IsServer() || !System.IsCLIParam("bbbTacticTest")) return;
  s_Tactic = new BBB_TacticTest();
  GetGame().GetCallqueue().CallLater(s_Tactic.Setup, 15000);
 }

 void TacticCheck(string name, bool ok, string detail = "")
 {
  m_Checks++;
  if (!ok) m_Failures++;
  PrintFormat("[BBB] TACTICTEST %1 ok=%2 %3", name, ok, detail);
 }

 void Setup()
 {
  BBB_Tactic.Read();
  TacticCheck("default-on", BBB_Tactic.s_bOn);
  BBB_StaticWeapons.s_bOn = false;
  BBB_Buildings.s_bOn = false;
  BBB_Morale.s_bOn = false;
  BBB_Grenades.s_bOn = false;
  BBB_Smoke.s_bOn = false;
  BBB_Flares.s_bOn = false;
  BBB_ContactRelay.s_bOn = false;
  BBB_Rearm.s_bOn = false;
  m_SavedHooks = BBB.s_Hooks;
  m_Hooks = new BBB_TacticTestHooks();
  BBB.s_Hooks = m_Hooks;
  ObserversSystem observers = ObserversSystem.Cast(GetGame().GetWorld().FindSystem(ObserversSystem));
  bool made = true;
  for (int p = 0; p < 2; p++)
  {
   vector centre = BBB.Ground(Vector(1340 + p * 500, 0, 2770));
   if (observers) observers.InsertObserverSP(22149 + p, centre[0], centre[2], null);
   for (int t = 0; t < 2; t++)
   {
    SCR_AIGroup team = SCR_AIGroup.Cast(Spawn(TEAM_US, centre + Vector(-20 + t * 40, 0, 0), "0 0 1"));
    made = made && team;
    m_Teams.Insert(team);
   }
   ref array<SCR_AIGroup> foes = {};
   int count = 1;
   if (p == 1) count = 4;
   for (int f = 0; f < count; f++)
   {
    // Pair 2's four teams stand closer and tighter (70 m, 12 m apart) so the squad sees enough of them.
    float ahead = 100; // inside 120 m: assault range (farther, a still enemy gets flanked: stage 4)
    float spacing = 25;
    if (p == 1) { ahead = 70; spacing = 12; }
    SCR_AIGroup foe = SCR_AIGroup.Cast(Spawn(TEAM_USSR, centre + Vector((f - (count - 1) * 0.5) * spacing, 0, ahead), "0 0 -1"));
    made = made && foe;
    foes.Insert(foe);
   }
   m_Foes.Insert(foes);
  }
  TacticCheck("groups", made);
  if (!made) { FinishTactic(); return; }
  BBB_Tactic.s_aVanilla.Insert(m_Teams[1]);
  BBB_Tactic.s_aVanilla.Insert(m_Teams[3]);
  foreach (SCR_AIGroup team : m_Teams) m_Hooks.m_Groups.Insert(team);
  GetGame().GetCallqueue().CallLater(Prepare, 4000);
 }

 void PrepareGroup(SCR_AIGroup group, bool enemy)
 {
  PinDetail(group);
  group.ActivateAI();
  EAIGroupCombatMode mode = EAIGroupCombatMode.FIRE_AT_WILL;
  if (enemy) mode = EAIGroupCombatMode.HOLD_FIRE;
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
   if (!enemy) continue;
   body.GetCharacterController().SetDisableWeaponControls(true);
   body.GetCharacterController().SetDisableMovementControls(true);
  }
 }

 void Prepare()
 {
  foreach (SCR_AIGroup team : m_Teams) PrepareGroup(team, false);
  foreach (array<SCR_AIGroup> foes : m_Foes)
   foreach (SCR_AIGroup foe : foes) PrepareGroup(foe, true);
  GetGame().GetCallqueue().CallLater(Watch, 1000, true);
 }

 bool Centre(SCR_AIGroup group, out vector centre)
 {
  centre = vector.Zero;
  if (!group) return false;
  array<AIAgent> agents = {};
  group.GetAgents(agents);
  int count;
  foreach (AIAgent agent : agents)
  {
   IEntity man = agent.GetControlledEntity();
   if (!man || !BBB.IsAlive(man)) continue;
   centre = centre + man.GetOrigin();
   count++;
  }
  if (count == 0) return false;
  centre = centre * (1.0 / count);
  return true;
 }

 // Distance from a team's centre to its pair's enemies (centre of all of them).
 float ToEnemy(int team, vector from)
 {
  vector sum;
  int count;
  foreach (SCR_AIGroup foe : m_Foes[team / 2])
  {
   vector at;
   if (Centre(foe, at)) { sum = sum + at; count++; }
  }
  if (count == 0) return 0;
  return vector.DistanceXZ(from, sum * (1.0 / count));
 }

 void Watch()
 {
  m_Wait++;
  int a1 = BBB_Tactic.TacticOf(m_Teams[0]);
  int a2 = BBB_Tactic.TacticOf(m_Teams[2]);
  if (a1 != BBB_Tactic.NONE) m_bContact = true;
  if (a1 == BBB_Tactic.ASSAULT) m_bAssault = true;
  if (a1 == BBB_Tactic.ASSAULT && BBB_FireManeuver.Job(m_Teams[0])) m_bAssaultFM = true;
  if (a2 == BBB_Tactic.HOLD && !m_bHold)
  {
   m_bHold = true;
   m_fHoldAt = m_Wait;
   Centre(m_Teams[2], m_vHoldA);
   Centre(m_Teams[3], m_vHoldB);
  }
  if (a2 == BBB_Tactic.HOLD && BBB_FireManeuver.Job(m_Teams[2])) m_bHoldFM = true;
  if (m_bHold && !m_bMeasured && BBB_FireManeuver.Job(m_Teams[3])) m_bWindowFMB = true;
  if (m_Wait % 5 == 0) PrintFormat("[BBB] TACTICTEST t=%1 pair1 A %2 (fm %3) B fm %4 | pair2 A %5 (fm %6) B fm %7 | callouts %8", m_Wait, BBB_Tactic.Name(a1), BBB_FireManeuver.Job(m_Teams[0]) != null, BBB_FireManeuver.Job(m_Teams[1]) != null, BBB_Tactic.Name(a2), BBB_FireManeuver.Job(m_Teams[2]) != null, BBB_FireManeuver.Job(m_Teams[3]) != null, BBB_Tactic.s_iCallouts);
  if (m_bHold && !m_bMeasured && m_Wait - m_fHoldAt >= 30) MeasureHold();
  bool done = m_bAssaultFM && (m_bMeasured || (!m_bHold && m_Wait >= 80));
  if (!done && m_Wait < 110) return;
  GetGame().GetCallqueue().Remove(Watch);
  if (m_bHold && !m_bMeasured) MeasureHold();
  TacticCheck("pair1-A-in-contact", m_bContact);
  TacticCheck("pair1-A-assaults", m_bAssault, BBB_Tactic.Describe(m_Teams[0]));
  TacticCheck("pair1-A-fire-and-manoeuvre", m_bAssaultFM, string.Format("fire manoeuvre jobs started %1", BBB_FireManeuver.s_iStarted));
  TacticCheck("pair2-A-holds", m_bHold, BBB_Tactic.Describe(m_Teams[2]));
  TacticCheck("pair2-A-no-fire-and-manoeuvre", m_bHold && !m_bHoldFM);
  Reorganise();
 }

 void MeasureHold()
 {
  m_bMeasured = true;
  vector nowA;
  vector nowB;
  Centre(m_Teams[2], nowA);
  Centre(m_Teams[3], nowB);
  float advA = ToEnemy(2, m_vHoldA) - ToEnemy(2, nowA);
  float advB = ToEnemy(3, m_vHoldB) - ToEnemy(3, nowB);
  PrintFormat("[BBB] TACTICTEST pair2 in the 30 s after A held: A advanced %1 m, B %2 m (B fire manoeuvre %3)", Math.Round(advA), Math.Round(advB), m_bWindowFMB);
  TacticCheck("pair2-A-stayed", advA <= 10, string.Format("%1 m", Math.Round(advA)));
  // The old rules press on into four times their number: B bounds or walks forward, A doesn't.
  TacticCheck("pair2-B-pressed-on-A-held", m_bWindowFMB || advA + 5 <= advB, string.Format("A %1 m, B %2 m, B fire manoeuvre %3", Math.Round(advA), Math.Round(advB), m_bWindowFMB));
 }

 void Reorganise()
 {
  IEntity leader = m_Teams[0].GetLeaderEntity();
  SCR_DamageManagerComponent damage = SCR_DamageManagerComponent.GetDamageManager(leader);
  if (damage)
  {
   damage.EnableDamageHandling(true);
   damage.Kill(Instigator.CreateInstigator(null));
  }
  m_Wait = 0;
  GetGame().GetCallqueue().CallLater(WatchReorg, 1000, true);
 }

 void WatchReorg()
 {
  m_Wait++;
  int a1 = BBB_Tactic.TacticOf(m_Teams[0]);
  if (a1 == BBB_Tactic.REORG && m_iReorgSeen < 0) m_iReorgSeen = m_Wait;
  // Judged from the first tick REORG is in force (the tactic looks every 2 s).
  if (m_iReorgSeen >= 0 && m_Wait <= m_iReorgSeen + 15 && BBB_FireManeuver.Job(m_Teams[0])) m_bReorgFM = true;
  if (m_Wait == 4) TacticCheck("leader-down-reorganises", a1 == BBB_Tactic.REORG, BBB_Tactic.Describe(m_Teams[0]));
  if (m_Wait % 5 == 0) PrintFormat("[BBB] TACTICTEST reorg t=%1 A %2 (fm %3)", m_Wait, BBB_Tactic.Name(a1), BBB_FireManeuver.Job(m_Teams[0]) != null);
  if (m_Wait < 30 && (m_Wait < 16 || a1 == BBB_Tactic.REORG)) return;
  GetGame().GetCallqueue().Remove(WatchReorg);
  TacticCheck("reorg-no-fire-and-manoeuvre", m_iReorgSeen >= 0 && !m_bReorgFM, string.Format("REORG from t=%1", m_iReorgSeen));
  TacticCheck("acting-leader-picks", a1 != BBB_Tactic.REORG && a1 != BBB_Tactic.NONE, BBB_Tactic.Describe(m_Teams[0]));
  FinishTactic();
 }

 void FinishTactic()
 {
  PrintFormat("[BBB] TACTICTEST changes %1, callouts queued %2", BBB_Tactic.s_iChanges, BBB_Tactic.s_iCallouts);
  if (m_SavedHooks) BBB.s_Hooks = m_SavedHooks;
  string verdict = "FAIL";
  if (m_Failures == 0 && m_Checks == EXPECTED) verdict = "PASS";
  PrintFormat("[ACR] VERDICT %1 test=bbbtactic passed=%2 failed=%3 expected=%4", verdict, m_Checks - m_Failures, m_Failures, EXPECTED);
  Print("[ACR] TEST COMPLETE");
 }
}

modded class SCR_BaseGameMode
{
 override protected void OnGameStart()
 {
  super.OnGameStart();
  BBB_TacticTest.Start();
 }
}
