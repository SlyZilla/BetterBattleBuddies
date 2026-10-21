class BBB_DoorTestHooks : BBB_Hooks
{
 ref array<SCR_AIGroup> m_Groups = {};
 bool m_Veto;
 override void GetGroups(notnull array<SCR_AIGroup> groups) { groups.Copy(m_Groups); }
 override bool AllowDoorUnstick(SCR_AIGroup group) { return !m_Veto; }
}

// A fireteam ordered through a real Arland house door left half-open across the
// doorway, twice: first with the host veto (BBB swings nothing), then with door
// unstick on. Judged on physical outcomes: soldiers standing jammed next to the
// door (measured by the test, not by the module's counters), soldiers through the
// doorway at the end, and the door leaf's real state after a swing.
class BBB_DoorTest : BBB_AimTest
{
 static ref BBB_DoorTest s_DoorTest;
 static const float PHASE_S = 40;
 static const float BLOCKING = 0.4;
 ref BBB_DoorTestHooks m_Hooks;
 ref BBB_Hooks m_SavedHooks;
 ref array<IEntity> m_Candidates = {};
 IEntity m_Door;
 BaseDoorComponent m_DoorComp;
 vector m_Center;
 vector m_Inward;
 vector m_Outside;
 vector m_Inside;
 SCR_AIGroup m_Team;
 AIWaypoint m_Order;
 int m_Phase;
 float m_PhaseStart;
 ref map<IEntity, vector> m_Last = new map<IEntity, vector>();
 ref map<IEntity, int> m_JamSeconds = new map<IEntity, int>();
 ref array<IEntity> m_EverJammed = {};
 ref array<int> m_Jammed = {};
 ref array<int> m_Through = {};
 ref array<int> m_Swings = {};
 ref array<int> m_Freed = {};
 int m_Checks;
 int m_Failures;
 int m_WalkingSamples; // moved and reported a path to walk
 int m_MovingSamples;  // moved at all

 static void Start()
 {
  if (!Replication.IsServer() || !System.IsCLIParam("bbbDoorTest")) return;
  s_DoorTest = new BBB_DoorTest();
  GetGame().GetCallqueue().CallLater(s_DoorTest.Setup, 15000);
 }

 void DoorCheck(string name, bool ok, string detail = "")
 {
  m_Checks++;
  if (!ok) m_Failures++;
  PrintFormat("[BBB] DOORTEST %1 ok=%2 %3", name, ok, detail);
 }

 // A lived-in house (at least 6 x 6 m), not a shed or a garden outbuilding.
 bool BigHouse(IEntity root)
 {
  if (!BaseBuilding.Cast(root)) return false;
  vector mins, maxs;
  root.GetBounds(mins, maxs);
  return maxs[0] - mins[0] >= 6 && maxs[2] - mins[2] >= 6;
 }

 bool CollectDoor(IEntity entity)
 {
  if (m_Candidates.Count() >= 200 || !entity.FindComponent(BaseDoorComponent)) return true;
  vector mins, maxs;
  entity.GetBounds(mins, maxs);
  vector size = maxs - mins;
  size[1] = 0;
  if (size.Length() <= BBB_DoorUnstick.HOUSE_DOOR_MAX && BigHouse(entity.GetRootParent())) m_Candidates.Insert(entity);
  return true;
 }

 void Setup()
 {
  BBB_DoorUnstick.Read();
  DoorCheck("default-on", BBB_DoorUnstick.s_bOn);
  BBB_DoorUnstick.s_bOn = true;
  m_SavedHooks = BBB.s_Hooks;
  m_Hooks = new BBB_DoorTestHooks();
  BBB.s_Hooks = m_Hooks;
  vector origin = BBB.Ground("3050 0 1700");
  GetGame().GetWorld().QueryEntitiesBySphere(origin, 300, CollectDoor);
  PrintFormat("[BBB] DOORTEST candidates=%1", m_Candidates.Count());
  foreach (IEntity door : m_Candidates)
  {
   BaseDoorComponent comp = BaseDoorComponent.Cast(door.FindComponent(BaseDoorComponent));
   comp.SetControlValue(0, RplId.Invalid());
  }
  GetGame().GetCallqueue().CallLater(ChooseDoor, 2500);
 }

 // A ground-floor door with a roof on one side only: the house wall's own door.
 void ChooseDoor()
 {
  foreach (IEntity door : m_Candidates)
  {
   BaseDoorComponent comp = BaseDoorComponent.Cast(door.FindComponent(BaseDoorComponent));
   vector normal = comp.GetDoorNormal();
   normal[1] = 0;
   if (normal.LengthSq() < 0.5) continue;
   normal.Normalize();
   vector center = SCR_EntityHelper.GetEntityCenterWorld(door);
   if (Math.AbsFloat(center[1] - GetGame().GetWorld().GetSurfaceY(center[0], center[2])) > 2.2) continue;
   IEntity house = door.GetRootParent();
   vector a = BBB.Ground(center + normal * 3);
   vector b = BBB.Ground(center - normal * 3);
   IEntity roofA = BBB_Buildings.RoofAt(a);
   IEntity roofB = BBB_Buildings.RoofAt(b);
   if (roofA == house && !roofB) { m_Inward = normal; m_Inside = a; m_Outside = b; }
   else if (roofB == house && !roofA) { m_Inward = -normal; m_Inside = b; m_Outside = a; }
   else continue;
   m_Door = door;
   m_DoorComp = comp;
   m_Center = center;
   break;
  }
  DoorCheck("door-fixture", m_Door != null, string.Format("door=%1 center=%2 inward=%3", m_Door, m_Center, m_Inward));
  if (!m_Door) { DoorFinish(); return; }
  DoorCheck("finds-house-door", BBB_DoorUnstick.HouseDoorNear(m_Center) == m_DoorComp);
  // Mechanism on its own: a half-open leaf is swung fully open, physically.
  m_DoorComp.SetControlValue(BLOCKING, RplId.Invalid());
  GetGame().GetCallqueue().CallLater(SwingOnce, 2500);
 }

 void SwingOnce()
 {
  PrintFormat("[BBB] DOORTEST half-open state=%1 control=%2", m_DoorComp.GetNormalizedDoorState(), m_DoorComp.GetControlValue());
  BBB_DoorUnstick.Swing(m_DoorComp);
  GetGame().GetCallqueue().CallLater(SwingObserved, 2500);
 }

 void SwingObserved()
 {
  float state = m_DoorComp.GetNormalizedDoorState();
  DoorCheck("swing-opens-leaf", Math.AbsFloat(state) > 0.9, string.Format("state=%1", state));
  BBB_DoorUnstick.Swing(m_DoorComp);
  GetGame().GetCallqueue().CallLater(FlipObserved, 2500);
 }

 void FlipObserved()
 {
  // Not every door swings both ways; report it, the AI phases judge the outcome.
  PrintFormat("[BBB] DOORTEST flip state=%1 control=%2", m_DoorComp.GetNormalizedDoorState(), m_DoorComp.GetControlValue());
  BeginPhase();
 }

 void BeginPhase()
 {
  m_Hooks.m_Veto = m_Phase == 0;
  BBB_DoorUnstick.Stop();
  m_Last.Clear();
  m_JamSeconds.Clear();
  m_DoorComp.SetControlValue(0, RplId.Invalid());
  m_Team = SCR_AIGroup.Cast(Spawn(TEAM_US, m_Outside - m_Inward * 5, m_Inward));
  if (!m_Team) { DoorCheck("team", false); DoorFinish(); return; }
  m_Hooks.m_Groups.Clear();
  m_Hooks.m_Groups.Insert(m_Team);
  GetGame().GetCallqueue().CallLater(Order, 3000);
 }

 void Order()
 {
  PinDetail(m_Team);
  m_Team.ActivateAI();
  array<AIAgent> agents = {};
  m_Team.GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   PinDetail(agent);
   agent.ActivateAI();
  }
  m_Team.GetGroupUtilityComponent().SetCombatMode(EAIGroupCombatMode.HOLD_FIRE);
  m_Order = AIWaypoint.Cast(Spawn("{750A8D1695BD6998}Prefabs/AI/Waypoints/AIWaypoint_Move.et", BBB.Ground(m_Center + m_Inward * 1.5), m_Inward));
  if (m_Order) { m_Order.SetCompletionRadius(1.5); m_Team.AddWaypoint(m_Order); }
  DoorCheck(string.Format("order-%1", m_Phase), m_Order != null);
  m_PhaseStart = Now();
  m_Swings.Insert(BBB_DoorUnstick.s_iSwings);
  m_Freed.Insert(BBB_DoorUnstick.s_iFreed);
  GetGame().GetCallqueue().CallLater(Poll, 1000, true);
 }

 bool Through(IEntity man)
 {
  vector offset = man.GetOrigin() - m_Center;
  offset[1] = 0;
  return vector.Dot(offset, m_Inward) > 0.6 && offset.Length() < 10;
 }

 // Test-side jam measure: next to the door, not through it, and not moving.
 void Poll()
 {
  array<AIAgent> agents = {};
  m_Team.GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   IEntity man = agent.GetControlledEntity();
   if (!man) continue;
   vector pos = man.GetOrigin();
   vector last = pos + "9 0 0";
   if (m_Last.Contains(man)) last = m_Last.Get(man);
   m_Last.Set(man, pos);
   bool still = BBB_DoorUnstick.WantsToMove(agent) && vector.DistanceXZ(pos, last) < 0.2 && vector.DistanceXZ(pos, m_Center) < 2.2 && !Through(man);
   bool walked = vector.DistanceXZ(pos, last) > 0.5 && vector.DistanceXZ(pos, last) < 8;
   if (walked) m_MovingSamples++;
   if (walked && BBB_DoorUnstick.WantsToMove(agent)) m_WalkingSamples++;
   int seconds;
   if (still) seconds = m_JamSeconds.Get(man) + 1;
   m_JamSeconds.Set(man, seconds);
   if (seconds == 3)
   {
    m_EverJammed.Insert(man);
    PrintFormat("[BBB] DOORTEST phase=%1 jammed %2 at %3 door=%4", m_Phase, man, pos, m_DoorComp.GetNormalizedDoorState());
   }
  }
  if (Now() - m_PhaseStart < PHASE_S) return;
  GetGame().GetCallqueue().Remove(Poll);
  EndPhase(agents);
 }

 void EndPhase(array<AIAgent> agents)
 {
  int through;
  foreach (AIAgent agent : agents)
  {
   IEntity man = agent.GetControlledEntity();
   if (man && Through(man)) through++;
  }
  m_Through.Insert(through);
  m_Jammed.Insert(m_EverJammed.Count());
  m_EverJammed.Clear();
  PrintFormat("[BBB] DOORTEST phase=%1 through=%2 jammed=%3 swings=%4 freed=%5", m_Phase, through, m_Jammed[m_Phase], BBB_DoorUnstick.s_iSwings - m_Swings[m_Phase], BBB_DoorUnstick.s_iFreed - m_Freed[m_Phase]);
  if (m_Order) { m_Team.RemoveWaypoint(m_Order); SCR_EntityHelper.DeleteEntityAndChildren(m_Order); }
  foreach (AIAgent agent : agents)
  {
   IEntity man = agent.GetControlledEntity();
   if (man) SCR_EntityHelper.DeleteEntityAndChildren(man);
  }
  SCR_EntityHelper.DeleteEntityAndChildren(m_Team);
  m_Phase++;
  if (m_Phase < 2) { GetGame().GetCallqueue().CallLater(BeginPhase, 2000); return; }
  Judge();
 }

 // Phase 0 is the vetoed baseline, phase 1 runs door unstick.
 void Judge()
 {
  int swingsOff = m_Swings[1] - m_Swings[0];
  int swingsOn = BBB_DoorUnstick.s_iSwings - m_Swings[1];
  int freedOn = BBB_DoorUnstick.s_iFreed - m_Freed[1];
  DoorCheck("sees-soldiers-walking", m_WalkingSamples > 0, string.Format("walking=%1 moving=%2", m_WalkingSamples, m_MovingSamples));
  DoorCheck("veto-swings-nothing", swingsOff == 0, string.Format("swings=%1", swingsOff));
  DoorCheck("baseline-jam-reproduced", m_Jammed[0] > 0, string.Format("jammed=%1 through=%2", m_Jammed[0], m_Through[0]));
  DoorCheck("swings-when-jammed", m_Jammed[1] == 0 || swingsOn > 0, string.Format("jammed=%1 swings=%2", m_Jammed[1], swingsOn));
  DoorCheck("jammed-soldier-moves-after-swing", swingsOn == 0 || freedOn > 0, string.Format("swings=%1 freed=%2", swingsOn, freedOn));
  DoorCheck("more-through-than-baseline", m_Through[1] > m_Through[0], string.Format("off=%1 on=%2", m_Through[0], m_Through[1]));
  DoorFinish();
 }

 void DoorFinish()
 {
  GetGame().GetCallqueue().Remove(Poll);
  BBB_DoorUnstick.Stop();
  if (m_SavedHooks) BBB.s_Hooks = m_SavedHooks;
  string verdict = "FAIL";
  if (m_Failures == 0 && m_Checks >= 8) verdict = "PASS";
  PrintFormat("[ACR] VERDICT %1 test=bbbdoors passed=%2 failed=%3 expected=%4", verdict, m_Checks - m_Failures, m_Failures, m_Checks);
  Print("[ACR] TEST COMPLETE");
 }
}

modded class SCR_BaseGameMode
{
 override protected void OnGameStart()
 {
  super.OnGameStart();
  BBB_DoorTest.Start();
 }
}
