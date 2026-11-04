class BBB_WithdrawOrderTestHooks : BBB_Hooks
{
 ref array<SCR_AIGroup> m_Groups = {};
 SCR_AIGroup m_Ordered;
 vector m_vDest;
 bool m_bOrder;
 override void GetGroups(notnull array<SCR_AIGroup> groups) { groups.Copy(m_Groups); }
 // Like ACA: its own squads never break on morale; only the host's order pulls them back.
 override bool AllowMorale(SCR_AIGroup group) { return false; }
 override BBB_EIntent Intent(SCR_AIGroup group, out vector centre, out float radius)
 {
  radius = 0;
  if (m_bOrder && group == m_Ordered)
  {
   centre = m_vDest;
   radius = 12;
   return BBB_EIntent.WITHDRAW;
  }
  return BBB_EIntent.TRANSIT;
 }
}

// Ordered fall-back A/B (-bbbFallbackTest): two identical US fire teams 30 m apart face a USSR
// fire team 90 m north that shoots at them (nobody can be hurt; the enemy can't move). Once both
// teams see it, each gets a native Defend waypoint 130 m south of where it stands: the order a
// host gives a squad it pulls back, at combat level as ACA re-issues it in contact. Team A's host
// intent is WITHDRAW (bbbWithdrawOrder: covered bounds toward the spot); team B's is TRANSIT (the
// waypoint alone, as before). Morale is off and AllowMorale false, so only the ordered fall-back
// can move A. Every 2 s for 60 s: ground gained toward the spot and men lying prone short of it
// (not counting A's covering half, which is meant to fight from the ground).
// A must start the ordered bounds, split into a running and a covering half, and gain 50 m;
// B's numbers are the reference (what a squad does today), printed, not judged.
// FireManeuver, Buildings, StaticWeapons, throws, smoke, rearm runs and relay are off.
class BBB_WithdrawOrderTest : BBB_AimTest
{
 static ref BBB_WithdrawOrderTest s_Fallback;
 static const int EXPECTED = 7;
 static const float BACK_M = 130;
 ref BBB_WithdrawOrderTestHooks m_Hooks;
 ref BBB_Hooks m_SavedHooks;
 ref array<SCR_AIGroup> m_Teams = {};
 SCR_AIGroup m_Enemy;
 ref array<vector> m_vStart = {};
 ref array<vector> m_vDest = {};
 ref array<int> m_aProne = {};   // man-samples lying prone short of the spot, not covering
 ref array<int> m_aSamples = {}; // man-samples short of the spot, not covering (the covering half fights from where it lies)
 int m_Checks;
 int m_Failures;
 int m_Wait;
 bool m_bSawOrdered;
 bool m_bSawSplit;

 static void Start()
 {
  if (!Replication.IsServer() || !System.IsCLIParam("bbbFallbackTest")) return;
  s_Fallback = new BBB_WithdrawOrderTest();
  GetGame().GetCallqueue().CallLater(s_Fallback.Setup, 15000);
 }

 void FallbackCheck(string name, bool ok, string detail = "")
 {
  m_Checks++;
  if (!ok) m_Failures++;
  PrintFormat("[BBB] FALLBACKTEST %1 ok=%2 %3", name, ok, detail);
 }

 void Setup()
 {
  BBB_Morale.Read();
  FallbackCheck("default-on", BBB_Morale.s_bOrderedOn);
  BBB_StaticWeapons.s_bOn = false;
  BBB_FireManeuver.s_bOn = false;
  BBB_Buildings.s_bOn = false;
  BBB_Grenades.s_bOn = false;
  BBB_Smoke.s_bOn = false;
  BBB_Flares.s_bOn = false;
  BBB_ContactRelay.s_bOn = false;
  BBB_Rearm.s_bOn = false;
  BBB_Morale.s_bOn = false;
  BBB_Morale.s_bSurrenderOn = false;
  m_SavedHooks = BBB.s_Hooks;
  m_Hooks = new BBB_WithdrawOrderTestHooks();
  BBB.s_Hooks = m_Hooks;
  vector centre = BBB.Ground("1340 0 2770");
  ObserversSystem observers = ObserversSystem.Cast(GetGame().GetWorld().FindSystem(ObserversSystem));
  if (observers) observers.InsertObserverSP(22149, centre[0], centre[2], null);
  bool made = true;
  for (int i = 0; i < 2; i++)
  {
   vector at = centre + Vector(-15 + i * 30, 0, 0);
   SCR_AIGroup team = SCR_AIGroup.Cast(Spawn(TEAM_US, at, "0 0 1"));
   made = made && team;
   m_Teams.Insert(team);
   m_vStart.Insert(at);
   m_aProne.Insert(0);
   m_aSamples.Insert(0);
  }
  vector enemyAt = centre + Vector(0, 0, 90);
  m_Enemy = SCR_AIGroup.Cast(Spawn(TEAM_USSR, enemyAt, "0 0 -1"));
  made = made && m_Enemy;
  FallbackCheck("groups", made);
  if (!made) { FinishFallback(); return; }
  m_Hooks.m_Groups.Insert(m_Teams[0]);
  m_Hooks.m_Groups.Insert(m_Teams[1]);
  m_Hooks.m_Ordered = m_Teams[0];
  GetGame().GetCallqueue().CallLater(Prepare, 4000);
 }

 void PrepareGroup(SCR_AIGroup group, bool enemy)
 {
  PinDetail(group);
  group.ActivateAI();
  group.GetGroupUtilityComponent().SetCombatMode(EAIGroupCombatMode.FIRE_AT_WILL);
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
   SCR_AIUtilityComponent utility = SCR_AIUtilityComponent.Cast(agent.FindComponent(SCR_AIUtilityComponent));
   // Until the order the US teams fight from where they stand (layout stays as built).
   if (!enemy && utility) utility.m_bBBBHoldGround = true;
   if (enemy) body.GetCharacterController().SetDisableMovementControls(true);
  }
 }

 void Prepare()
 {
  PrepareGroup(m_Teams[0], false);
  PrepareGroup(m_Teams[1], false);
  PrepareGroup(m_Enemy, true);
  GetGame().GetCallqueue().CallLater(WaitContact, 1000, true);
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

 // Does any member of team i perceive (detected <= 15 s) a man of the enemy team?
 bool Sees(int i)
 {
  array<AIAgent> enemies = {};
  m_Enemy.GetAgents(enemies);
  array<IEntity> men = {};
  foreach (AIAgent e : enemies) men.Insert(e.GetControlledEntity());
  array<AIAgent> agents = {};
  m_Teams[i].GetAgents(agents);
  array<BaseTarget> targets = {};
  foreach (AIAgent agent : agents)
  {
   SCR_AIUtilityComponent utility = SCR_AIUtilityComponent.Cast(agent.FindComponent(SCR_AIUtilityComponent));
   if (!utility || !utility.m_PerceptionComponent) continue;
   targets.Clear();
   utility.m_PerceptionComponent.GetTargetsList(targets, ETargetCategory.ENEMY);
   foreach (BaseTarget target : targets)
    if (target && men.Contains(target.GetTargetEntity()) && target.GetTimeSinceDetected() >= 0 && target.GetTimeSinceDetected() <= 15) return true;
  }
  return false;
 }

 void WaitContact()
 {
  m_Wait++;
  if (!(Sees(0) && Sees(1)) && m_Wait < 60) return;
  GetGame().GetCallqueue().Remove(WaitContact);
  FallbackCheck("both-see-the-enemy", Sees(0) && Sees(1), string.Format("after %1 s: A %2, B %3", m_Wait, Sees(0), Sees(1)));
  Order();
 }

 // Both teams get the same order: a Defend waypoint 130 m straight back. A's host also says WITHDRAW.
 void Order()
 {
  for (int i = 0; i < 2; i++)
  {
   array<AIAgent> agents = {};
   m_Teams[i].GetAgents(agents);
   foreach (AIAgent agent : agents)
   {
    SCR_AIUtilityComponent utility = SCR_AIUtilityComponent.Cast(agent.FindComponent(SCR_AIUtilityComponent));
    if (utility) utility.m_bBBBHoldGround = false;
   }
   vector at;
   Centre(m_Teams[i], at);
   m_vStart[i] = at;
   vector dest = BBB.Ground(at - Vector(0, 0, BACK_M));
   m_vDest.Insert(dest);
   PrintFormat("[BBB] FALLBACKTEST team %1 ordered back %2 m to %3 (water there %4)", i, BACK_M, dest, BBB_Terrain.IsWaterAt(dest));
   AIWaypoint wp = AIWaypoint.Cast(Spawn("{93291E72AC23930F}Prefabs/AI/Waypoints/AIWaypoint_Defend.et", dest, "0 0 1"));
   if (wp) m_Teams[i].AddWaypoint(wp);
  }
  m_Hooks.m_vDest = m_vDest[0];
  m_Hooks.m_bOrder = true;
  m_Wait = 0;
  GetGame().GetCallqueue().CallLater(Sample, 2000, true);
 }

 // Ground gained toward the spot (never counts sideways drift).
 float Gained(int i)
 {
  vector now;
  if (!Centre(m_Teams[i], now)) return 0;
  return vector.DistanceXZ(m_vStart[i], m_vDest[i]) - vector.DistanceXZ(now, m_vDest[i]);
 }

 void Sample()
 {
  m_Wait += 2;
  if (BBB_Morale.IsOrdered(m_Teams[0])) m_bSawOrdered = true;
  for (int i = 0; i < 2; i++)
  {
   array<AIAgent> agents = {};
   m_Teams[i].GetAgents(agents);
   int running;
   int covering;
   string line;
   foreach (AIAgent agent : agents)
   {
    ChimeraCharacter man = ChimeraCharacter.Cast(agent.GetControlledEntity());
    if (!man || !BBB.IsAlive(man)) continue;
    SCR_AIUtilityComponent utility = SCR_AIUtilityComponent.Cast(agent.FindComponent(SCR_AIUtilityComponent));
    if (utility && BBB_Morale.Running(utility.m_BBBWithdrawRun)) running++;
    if (utility && utility.m_bBBBHoldGround) covering++;
    bool prone = man.GetCharacterController().GetStance() == ECharacterStance.PRONE;
    bool covers = utility && utility.m_bBBBHoldGround;
    if (!covers && vector.DistanceXZ(man.GetOrigin(), m_vDest[i]) > 15)
    {
     m_aSamples[i] = m_aSamples[i] + 1;
     if (prone) m_aProne[i] = m_aProne[i] + 1;
    }
    string doing = "?";
    if (utility && utility.GetCurrentAction()) doing = utility.GetCurrentAction().ClassName();
    string stance = "up";
    if (prone) stance = "prone";
    line += string.Format(" [%1 m to go, %2, %3]", Math.Round(vector.DistanceXZ(man.GetOrigin(), m_vDest[i])), stance, doing);
   }
   if (i == 0 && running > 0 && covering > 0) m_bSawSplit = true;
   if (m_Wait % 6 == 0) PrintFormat("[BBB] FALLBACKTEST t+%1 team %2 gained %3 m, running %4 covering %5:%6", m_Wait, i, Math.Round(Gained(i)), running, covering, line);
  }
  if (m_Wait < 60) return;
  GetGame().GetCallqueue().Remove(Sample);
  Judge();
 }

 float ProneShare(int i)
 {
  if (m_aSamples[i] == 0) return 0;
  float prone = m_aProne[i];
  return prone / m_aSamples[i];
 }

 void Judge()
 {
  float a = Gained(0);
  float b = Gained(1);
  FallbackCheck("A-bounds-on-orders", m_bSawOrdered, BBB_Morale.s_iOrderedStarted.ToString() + " started");
  FallbackCheck("A-covered-halves", m_bSawSplit, "a sample with one half running and the other covering");
  FallbackCheck("A-got-back", a >= 50, string.Format("%1 m toward the spot in 60 s, prone %2 of the time", Math.Round(a), ProneShare(0).ToString(4, 2)));
  PrintFormat("[BBB] FALLBACKTEST reference B (waypoint only): %1 m toward the spot in 60 s, prone %2 of the time", Math.Round(b), ProneShare(1).ToString(4, 2));
  // Intent withdrawn: the bounds must end at once (the host changed its mind).
  m_Hooks.m_bOrder = false;
  GetGame().GetCallqueue().CallLater(JudgeRelease, 3500);
 }

 void JudgeRelease()
 {
  bool held;
  array<AIAgent> agents = {};
  m_Teams[0].GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   SCR_AIUtilityComponent utility = SCR_AIUtilityComponent.Cast(agent.FindComponent(SCR_AIUtilityComponent));
   if (utility && (utility.m_bBBBHoldGround || BBB_Morale.Running(utility.m_BBBWithdrawRun))) held = true;
  }
  FallbackCheck("released-when-order-ends", !BBB_Morale.IsWithdrawing(m_Teams[0]) && !held);
  FinishFallback();
 }

 void FinishFallback()
 {
  GetGame().GetCallqueue().Remove(Sample);
  if (m_SavedHooks) BBB.s_Hooks = m_SavedHooks;
  string verdict = "FAIL";
  if (m_Failures == 0 && m_Checks == EXPECTED) verdict = "PASS";
  PrintFormat("[ACR] VERDICT %1 test=bbbfallback passed=%2 failed=%3 expected=%4", verdict, m_Checks - m_Failures, m_Failures, EXPECTED);
  Print("[ACR] TEST COMPLETE");
 }
}

modded class SCR_BaseGameMode
{
 override protected void OnGameStart()
 {
  super.OnGameStart();
  BBB_WithdrawOrderTest.Start();
 }
}
