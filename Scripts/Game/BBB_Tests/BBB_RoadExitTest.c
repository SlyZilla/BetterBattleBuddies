class BBB_RoadExitHooks : BBB_Hooks
{
 ref array<SCR_AIGroup> m_Groups = {};
 override void GetGroups(notnull array<SCR_AIGroup> groups)
 {
  groups.Clear();
  foreach (SCR_AIGroup group : m_Groups) if (group) groups.Insert(group);
 }
}

// Road exit A/B (-bbbRoadExitTest). A US and a USSR fire team are placed on the same straight
// Arland road 140 m apart, facing each other (damage off, every other BBB feature at its
// default), each with a move order down the road past the other. Phase OFF runs with the road exit disabled, phase ON re-places both teams and
// enables it. Measured from where the men physically are, once a second after the first
// SETTLE_S: the share of man-seconds spent on the road, and rounds fired by both sides.
// -bbbRoadExitIsolate 1 turns off fire and manoeuvre, tactics and morale to see vanilla alone.
class BBB_RoadExitTest : BBB_AimTest
{
 static ref BBB_RoadExitTest s_Road;
 static const float PHASE_S = 50;
 static const float SETTLE_S = 10;
 static const float GAP_M = 100;
 static const ResourceName WP_MOVE = "{750A8D1695BD6998}Prefabs/AI/Waypoints/AIWaypoint_Move.et";
 ref BBB_RoadExitHooks m_Hooks;
 ref BBB_Hooks m_SavedHooks;
 SCR_AIGroup m_Us;
 SCR_AIGroup m_Them;
 vector m_A, m_B;
 ref map<IEntity, int> m_Ammo = new map<IEntity, int>();
 int m_Fired, m_OnRoad, m_Samples;
 bool m_bOnPhase;
 float m_At;
 int m_Checks, m_Failures;
 float m_OffShare = -1;
 float m_OnShare = -1;
 int m_OffFired, m_OnFired, m_OnStarted;

 static void Start()
 {
  if (!Replication.IsServer() || !System.IsCLIParam("bbbRoadExitTest")) return;
  s_Road = new BBB_RoadExitTest();
  GetGame().GetCallqueue().CallLater(s_Road.Setup, 15000);
 }

 void Assert(string label, bool passed, string detail = "")
 {
  m_Checks++;
  if (!passed) m_Failures++;
  PrintFormat("[BBB] ROADEXIT CHECK %1 ok=%2 %3", label, passed, detail);
 }

 // A straight stretch of road at least GAP_M long near 'centre'.
 bool FindStretch(vector centre)
 {
  ChimeraAIWorld aiWorld = ChimeraAIWorld.Cast(GetGame().GetAIWorld());
  if (!aiWorld || !aiWorld.GetRoadNetworkManager()) return false;
  array<BaseRoad> roads = {};
  aiWorld.GetRoadNetworkManager().GetRoadsInAABB(centre - "900 500 900", centre + "900 500 900", roads);
  float bestFrom = float.MAX;
  foreach (BaseRoad road : roads)
  {
   if (road.GetWidth() < 4) continue;
   array<vector> points = {};
   road.GetPoints(points);
   for (int i = 0; i < points.Count(); i++)
   {
    float along = 0;
    for (int j = i + 1; j < points.Count(); j++)
    {
     along += vector.DistanceXZ(points[j - 1], points[j]);
     if (along < GAP_M) continue;
     float chord = vector.DistanceXZ(points[i], points[j]);
     if (chord > along * 0.98 && chord < GAP_M + 40 && Math.AbsFloat(points[i][1] - points[j][1]) < 3 && Clear(points[i], points[j]) && !BBB_Terrain.IsWaterAt(points[i]) && !BBB_Terrain.IsWaterAt(points[j]))
     {
      vector a = points[i];
      vector dir = (points[j] - a).Normalized();
      vector b = a + dir * GAP_M;
      // Open ground: fewest things (trees, bushes, walls, houses) beside the stretch.
      // -bbbRoadExitNear 1: just the stretch nearest the centre.
      float from = vector.DistanceXZ(a, centre);
      if (!System.IsCLIParam("bbbRoadExitNear") && from < 700) from = Clutter(a, b) * 1000 + from;
      if (from < bestFrom) { bestFrom = from; m_A = BBB.Ground(a); m_B = BBB.Ground(b); }
     }
     break;
    }
   }
  }
  return bestFrom < float.MAX;
 }

 int m_iClutter;
 bool CountClutter(IEntity entity)
 {
  if (!ChimeraCharacter.Cast(entity) && !SCR_AIGroup.Cast(entity) && entity.GetVObject()) m_iClutter++;
  return true;
 }
 int Clutter(vector a, vector b)
 {
  m_iClutter = 0;
  for (int k = 0; k <= 4; k++) GetGame().GetWorld().QueryEntitiesBySphere(a + (b - a) * (k / 4.0), 20, CountClutter);
  return m_iClutter;
 }

 // Eye to eye along the road: no terrain in between.
 bool Clear(vector a, vector b)
 {
  for (int i = 1; i < 20; i++)
  {
   vector p = a + (b - a) * (i / 20.0);
   if (GetGame().GetWorld().GetSurfaceY(p[0], p[2]) > p[1] + 1.2) return false;
  }
  return true;
 }

 // What the road query reads at p (diagnostic).
 string Reading(vector p)
 {
  ChimeraAIWorld aiWorld = ChimeraAIWorld.Cast(GetGame().GetAIWorld());
  BaseRoad road;
  float distance;
  aiWorld.GetRoadNetworkManager().GetClosestRoad(p, road, distance, true);
  if (!road) return "no road";
  BaseRoad any;
  float anyDistance;
  aiWorld.GetRoadNetworkManager().GetClosestRoad(p, any, anyDistance, false);
  return string.Format("distance=%1 width=%2 (with navlinks %3)", distance, road.GetWidth(), anyDistance);
 }

 void Setup()
 {
  Assert("default-on", BBB_RoadExit.s_bOn);
  m_SavedHooks = BBB.s_Hooks;
  m_Hooks = new BBB_RoadExitHooks();
  BBB.s_Hooks = m_Hooks;
  if (System.IsCLIParam("bbbRoadExitIsolate"))
  {
   BBB_Tactic.s_bOn = false;
   BBB_FireManeuver.s_bOn = false;
   BBB_Morale.s_bOn = false;
  }
  vector centre = BBB.Ground("3030 0 1700");
  bool found = FindStretch(centre);
  Assert("road-fixture", found, string.Format("a=%1 b=%2", m_A, m_B));
  if (!found) { FinishRoad(); return; }
  // The measure itself: a point on the centre line reads as on the road, 20 m off it as off.
  vector dir = (m_B - m_A).Normalized();
  vector side = Vector(-dir[2], 0, dir[0]);
  vector spot;
  Assert("measure-on-road", BBB_RoadExit.OffRoadSpot(m_A, m_B, spot) != 0, "at a: " + Reading(m_A));
  Assert("measure-off-road", BBB_RoadExit.OffRoadSpot(BBB.Ground(m_A + side * 20), m_B, spot) == 0, "20 m off: " + Reading(BBB.Ground(m_A + side * 20)));
  NavmeshWorldComponent nav = Navmesh();
  if (nav)
  {
   for (float f = -0.2; f <= 1.2; f += 0.1)
    for (int s = -2; s <= 2; s++) nav.LoadTileIn(m_A + (m_B - m_A) * f + side * (s * 50));
  }
  ObserversSystem observers = ObserversSystem.Cast(GetGame().GetWorld().FindSystem(ObserversSystem));
  if (observers) observers.InsertObserverSP(22138, (m_A[0] + m_B[0]) * 0.5, (m_A[2] + m_B[2]) * 0.5, null);
  m_Us = SCR_AIGroup.Cast(Spawn(TEAM_US, m_A, dir));
  m_Them = SCR_AIGroup.Cast(Spawn(TEAM_USSR, m_B, -dir));
  Assert("fixtures", m_Us && m_Them);
  if (!m_Us || !m_Them) { FinishRoad(); return; }
  m_Hooks.m_Groups.Insert(m_Us);
  m_Hooks.m_Groups.Insert(m_Them);
  // Both on the march: a move order 150 m past the other team, down the same road, as two
  // squads travelling the road meet (-bbbRoadExitFree 1: no orders, vanilla's own attack).
  if (!System.IsCLIParam("bbbRoadExitFree"))
  {
   AIWaypoint usOrder = AIWaypoint.Cast(Spawn(WP_MOVE, BBB.Ground(m_B + dir * 150), dir));
   AIWaypoint themOrder = AIWaypoint.Cast(Spawn(WP_MOVE, BBB.Ground(m_A - dir * 150), -dir));
   Assert("orders", usOrder && themOrder);
   if (usOrder) m_Us.AddWaypoint(usOrder);
   if (themOrder) m_Them.AddWaypoint(themOrder);
  }
  PrintFormat("[BBB] ROADEXIT road a=%1 b=%2 gap=%3 clutter=%4", m_A, m_B, vector.DistanceXZ(m_A, m_B), Clutter(m_A, m_B));
  GetGame().GetCallqueue().CallLater(BeginPhase, 5000, false, System.IsCLIParam("bbbRoadExitOnFirst"));
 }

 void Prepare(SCR_AIGroup group, vector at, vector face)
 {
  PinDetail(group);
  group.ActivateAI();
  array<AIAgent> agents = {};
  group.GetAgents(agents);
  vector side = Vector(-face[2], 0, face[0]);
  foreach (int i, AIAgent agent : agents)
  {
   PinDetail(agent);
   agent.ActivateAI();
   ChimeraCharacter body = ChimeraCharacter.Cast(agent.GetControlledEntity());
   if (!body) continue;
   body.GetDamageManager().EnableDamageHandling(false);
   // In file down the road, alternating a metre either side of the centre line.
   float lateral = 1;
   if (i % 2 == 1) lateral = -1;
   body.SetOrigin(BBB.Ground(at - face * (i * 3) + side * lateral));
   body.SetAngles(face.VectorToAngles());
  }
 }

 void BeginPhase(bool on)
 {
  m_bOnPhase = on;
  BBB_RoadExit.s_bOn = on;
  vector dir = (m_B - m_A).Normalized();
  Prepare(m_Us, m_A, dir);
  Prepare(m_Them, m_B, -dir);
  m_Ammo.Clear();
  m_Fired = 0; m_OnRoad = 0; m_Samples = 0;
  BBB_RoadExit.s_iRequests = 0; BBB_RoadExit.s_iStarted = 0; BBB_RoadExit.s_iCover = 0; BBB_RoadExit.s_iArrived = 0; BBB_RoadExit.s_iNoSpot = 0;
  m_At = Now();
  PrintFormat("[BBB] ROADEXIT phase on=%1 start", on);
  GetGame().GetCallqueue().CallLater(Observe, 1000, true);
 }

 void Sample(SCR_AIGroup group, bool settled)
 {
  array<AIAgent> agents = {};
  group.GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   ChimeraCharacter body = ChimeraCharacter.Cast(agent.GetControlledEntity());
   if (!body) continue;
   if (settled)
   {
    m_Samples++;
    vector spot;
    if (BBB_RoadExit.OffRoadSpot(body.GetOrigin(), m_B, spot) != 0) m_OnRoad++;
   }
   SCR_AICombatComponent combat = SCR_AICombatComponent.Cast(body.FindComponent(SCR_AICombatComponent));
   if (combat && combat.GetCurrentWeapon() && combat.GetCurrentWeapon().GetCurrentMagazine())
   {
    int ammo = combat.GetCurrentWeapon().GetCurrentMagazine().GetAmmoCount();
    if (m_Ammo.Contains(body) && ammo < m_Ammo.Get(body)) m_Fired += m_Ammo.Get(body) - ammo;
    m_Ammo.Set(body, ammo);
   }
  }
 }

 void Describe(SCR_AIGroup group)
 {
  array<AIAgent> agents = {};
  group.GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   ChimeraCharacter body = ChimeraCharacter.Cast(agent.GetControlledEntity());
   SCR_AIUtilityComponent u = SCR_AIUtilityComponent.Cast(agent.FindComponent(SCR_AIUtilityComponent));
   if (!body || !u) continue;
   string behavior = "none";
   if (u.GetCurrentBehavior()) behavior = u.GetCurrentBehavior().Type().ToString();
   string target = "none";
   SCR_AICombatComponent combat = SCR_AICombatComponent.Cast(body.FindComponent(SCR_AICombatComponent));
   if (combat && combat.GetCurrentTarget()) target = string.Format("%1 m seen %2 s ago", Math.Round(combat.GetCurrentTarget().GetDistance()), combat.GetCurrentTarget().GetTimeSinceSeen());
   PrintFormat("[BBB] ROADEXIT man %1 behavior=%2 target=%3 threat=%4 stance=%5 road: %6", body, behavior, target, typename.EnumToString(EAIThreatState, u.m_ThreatSystem.GetState()), typename.EnumToString(ECharacterStance, body.GetCharacterController().GetStance()), Reading(body.GetOrigin()));
  }
 }

 void Observe()
 {
  bool settled = Now() - m_At >= SETTLE_S;
  Sample(m_Us, settled);
  Sample(m_Them, settled);
  if (Now() - m_At < PHASE_S) return;
  GetGame().GetCallqueue().Remove(Observe);
  Describe(m_Us);
  Describe(m_Them);
  float share = 0;
  if (m_Samples > 0) share = m_OnRoad / (float)m_Samples;
  PrintFormat("[BBB] ROADEXIT phase on=%1 onRoad=%2/%3 (%4) fired=%5 requests=%6 started=%7 cover=%8 arrived=%9", m_bOnPhase, m_OnRoad, m_Samples, share, m_Fired, BBB_RoadExit.s_iRequests, BBB_RoadExit.s_iStarted, BBB_RoadExit.s_iCover, BBB_RoadExit.s_iArrived);
  if (m_bOnPhase && m_OffShare < 0)
  {
   m_OnShare = share;
   m_OnFired = m_Fired;
   m_OnStarted = BBB_RoadExit.s_iStarted;
   BeginPhase(false);
   return;
  }
  if (!m_bOnPhase)
  {
   m_OffShare = share;
   m_OffFired = m_Fired;
   if (m_OnShare >= 0) { Judge(); return; }
   BeginPhase(true);
   return;
  }
  m_OnShare = share;
  m_OnFired = m_Fired;
  m_OnStarted = BBB_RoadExit.s_iStarted;
  Judge();
 }

 void Judge()
 {
  float share = m_OnShare;
  Assert("road-exit-no-worse", share <= m_OffShare + 0.05, string.Format("off share=%1 on share=%2", m_OffShare, share));
  if (m_OffShare >= 0.3) Assert("road-exit-gets-men-off", share <= m_OffShare * 0.5, string.Format("off share=%1 on share=%2", m_OffShare, share));
  else PrintFormat("[BBB] ROADEXIT vanilla already left this road (off share %1): fixture can't judge the gain", m_OffShare);
  Assert("both-phases-fight", m_OffFired > 0 && m_OnFired > 0, string.Format("off fired=%1 on fired=%2", m_OffFired, m_OnFired));
  Assert("road-exit-moves-started", m_OnStarted > 0, string.Format("started=%1 (coverage only)", m_OnStarted));
  FinishRoad();
 }

 void FinishRoad()
 {
  GetGame().GetCallqueue().Remove(Observe);
  BBB_RoadExit.s_bOn = true;
  if (m_SavedHooks) BBB.s_Hooks = m_SavedHooks;
  string verdict = "PASS";
  if (m_Failures > 0) verdict = "FAIL";
  PrintFormat("[ACR] VERDICT %1 test=bbb-roadexit checks=%2 failed=%3", verdict, m_Checks, m_Failures);
  Print("[ACR] TEST COMPLETE");
 }
}

modded class SCR_BaseGameMode
{
 protected override void OnGameStart()
 {
  super.OnGameStart();
  BBB_RoadExitTest.Start();
 }
}
