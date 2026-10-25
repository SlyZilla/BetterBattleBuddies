class BBB_SquadTestHooks : BBB_Hooks
{
 ref array<SCR_AIGroup> m_Groups = {};
 override void GetGroups(notnull array<SCR_AIGroup> groups) { groups.Copy(m_Groups); }
}

// Squad picture A/B (-bbbSquadTest): two identical US fire teams 40 m apart, facing 70 deg, sit
// between two USSR fire teams 110 m north and 60 m south-east (contact on two sides).
// Nobody can be hurt; the enemies can't move, and only the south-east team shoots: a squad
// faces whoever it fights, so without the shots it rarely notices the second group. Team A uses the squad picture, team B
// keeps the old averaged centroid (BBB_Squad.s_aVanilla). Once team A has seen both groups:
// (1) on team A's own perception, the picture's threat (smoke aim, static gun arc, withdrawal
// aim) must sit on a real enemy group and the old centroid of the same reports must sit well
// off both, out in the empty ground between them or on the squad itself; (2) two men of each team
// are killed and morale breaks: A must be sent clear of both groups, no worse than the old rule
// would send it on the same reports (here the two agree; with groups on opposite sides they don't,
// checked on the way-out rule directly), and to a spot farther from the nearer group. B's live order
// is printed as a reference only (its own perception may differ). The break needs the fallen
// leader to count (break score 0.60). After 20 s A's survivors must really be 10 m farther from the
// nearer group and B's must have run 12 m (withdrawal runs, not combat-move requests that
// stalled here). Until the break the US teams hold ground (no advancing on the passive enemies).
// Smoke is off; FireManeuver, Buildings, StaticWeapons, throws, rearm runs and relay are off so
// only the withdrawal moves anyone.
class BBB_SquadTest : BBB_AimTest
{
 static ref BBB_SquadTest s_Squad;
 static const int EXPECTED = 15;
 ref BBB_SquadTestHooks m_Hooks;
 ref BBB_Hooks m_SavedHooks;
 ref array<SCR_AIGroup> m_Teams = {};
 ref array<SCR_AIGroup> m_Enemies = {}; // north, south
 ref array<vector> m_vStart = {};
 int m_Checks;
 int m_Failures;
 int m_Wait;
 vector m_vOld; // team A's old centroid when both groups were seen

 static void Start()
 {
  if (!Replication.IsServer() || !System.IsCLIParam("bbbSquadTest")) return;
  s_Squad = new BBB_SquadTest();
  GetGame().GetCallqueue().CallLater(s_Squad.Setup, 15000);
 }

 void SquadCheck(string name, bool ok, string detail = "")
 {
  m_Checks++;
  if (!ok) m_Failures++;
  PrintFormat("[BBB] SQUADTEST %1 ok=%2 %3", name, ok, detail);
 }

 void Setup()
 {
  BBB_Squad.Read();
  SquadCheck("default-on", BBB_Squad.s_bOn);
  BBB_StaticWeapons.s_bOn = false;
  BBB_FireManeuver.s_bOn = false;
  BBB_Buildings.s_bOn = false;
  BBB_Grenades.s_bOn = false;
  BBB_Smoke.s_bOn = false;
  BBB_Flares.s_bOn = false;
  BBB_ContactRelay.s_bOn = false;
  BBB_Rearm.s_bOn = false;
  BBB_Morale.s_bOn = true;
  BBB_Morale.s_bSurrenderOn = false;
  m_SavedHooks = BBB.s_Hooks;
  m_Hooks = new BBB_SquadTestHooks();
  BBB.s_Hooks = m_Hooks;
  vector centre = BBB.Ground("1340 0 2770");
  ObserversSystem observers = ObserversSystem.Cast(GetGame().GetWorld().FindSystem(ObserversSystem));
  if (observers) observers.InsertObserverSP(22149, centre[0], centre[2], null);
  bool made = true;
  for (int i = 0; i < 2; i++)
  {
   vector at = centre + Vector(-20 + i * 40, 0, 0); // A west (sees the north group), B east
   SCR_AIGroup team = SCR_AIGroup.Cast(Spawn(TEAM_US, at, "0.94 0 0.34"));
   // North 110 m out (the ground north is open only at that range), south-east 60 m at 140 deg.
   vector enemyAt = centre + Vector(0, 0, 110);
   if (i == 1) enemyAt = centre + Vector(38.6, 0, -46);
   SCR_AIGroup enemy = SCR_AIGroup.Cast(Spawn(TEAM_USSR, enemyAt, centre - enemyAt));
   made = made && team && enemy;
   m_Teams.Insert(team);
   m_Enemies.Insert(enemy);
   m_vStart.Insert(at);
  }
  SquadCheck("groups", made);
  if (!made) { FinishSquad(); return; }
  BBB_Squad.s_aVanilla.Insert(m_Teams[1]);
  m_Hooks.m_Groups.Insert(m_Teams[0]);
  m_Hooks.m_Groups.Insert(m_Teams[1]);
  GetGame().GetCallqueue().CallLater(Prepare, 4000);
 }

 void PrepareGroup(SCR_AIGroup group, bool enemy, bool shoots = false)
 {
  PinDetail(group);
  group.ActivateAI();
  EAIGroupCombatMode mode = EAIGroupCombatMode.FIRE_AT_WILL;
  if (enemy && !shoots) mode = EAIGroupCombatMode.HOLD_FIRE;
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
   // Until the break the US teams turn, look and shoot but don't advance (layout stays as built).
   SCR_AIUtilityComponent utility = SCR_AIUtilityComponent.Cast(agent.FindComponent(SCR_AIUtilityComponent));
   if (!enemy && utility) utility.m_bBBBHoldGround = true;
   if (!enemy) continue;
   // Standing and unable to move; the north group can't shoot (seen reliably, nobody
   // suppressed), the south-east one fires like a real flank so the squad hears it.
   if (!shoots) body.GetCharacterController().SetDisableWeaponControls(true);
   body.GetCharacterController().SetDisableMovementControls(true);
  }
 }

 void Prepare()
 {
  for (int i = 0; i < 2; i++)
  {
   PrepareGroup(m_Teams[i], false);
   PrepareGroup(m_Enemies[i], true, i == 1);
  }
  GetGame().GetCallqueue().CallLater(WaitContact, 1000, true);
 }

 // Centre of a group's living men (ground truth, not perception).
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

 vector EnemyAt(int e)
 {
  vector at;
  Centre(m_Enemies[e], at);
  return at;
 }

 // Distance from a point to the nearer enemy group's real centre.
 float NearestEnemy(vector point)
 {
  return Math.Min(vector.DistanceXZ(point, EnemyAt(0)), vector.DistanceXZ(point, EnemyAt(1)));
 }

 float Bearing(vector from, vector to)
 {
  vector d = to - from;
  return Math.Atan2(d[0], d[2]) * Math.RAD2DEG;
 }

 // Does any member of team i perceive (detected <= 15 s) a man of this enemy group?
 bool Sees(int i, SCR_AIGroup enemy)
 {
  array<AIAgent> enemies = {};
  enemy.GetAgents(enemies);
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

 bool SeesBoth(int i) { return Sees(i, m_Enemies[0]) && Sees(i, m_Enemies[1]); }

 void WaitContact()
 {
  m_Wait++;
  if (m_Wait % 5 == 0) PrintFormat("[BBB] SQUADTEST t=%1 A sees north %2 south %3, B sees north %4 south %5", m_Wait, Sees(0, m_Enemies[0]), Sees(0, m_Enemies[1]), Sees(1, m_Enemies[0]), Sees(1, m_Enemies[1]));
  if (!(SeesBoth(0) && (SeesBoth(1) || m_Wait >= 30)) && m_Wait < 90) return;
  GetGame().GetCallqueue().Remove(WaitContact);
  SquadCheck("A-sees-both", SeesBoth(0), string.Format("after %1 s", m_Wait));
  PrintFormat("[BBB] SQUADTEST B (old way, live reference) sees both %1", SeesBoth(1));
  ComparePictures();
  Break();
 }

 // Same perception, both ways: the picture's threat vs the old average.
 void ComparePictures()
 {
  vector team;
  Centre(m_Teams[0], team);
  vector picture;
  bool flanked;
  bool has = BBB_Squad.MainThreat(m_Teams[0], 15.0, picture, flanked);
  BBB_Squad.s_aVanilla.Insert(m_Teams[0]);
  vector old;
  bool hasOld = BBB_Morale.GetThreatCentroid(m_Teams[0], old);
  m_vOld = old;
  BBB_Squad.s_aVanilla.RemoveItem(m_Teams[0]);
  float newOff = NearestEnemy(picture);
  float oldOff = NearestEnemy(old);
  PrintFormat("[BBB] SQUADTEST north at %1 deg %2 m, south at %3 deg %4 m", Math.Round(Bearing(team, EnemyAt(0))), Math.Round(vector.DistanceXZ(team, EnemyAt(0))), Math.Round(Bearing(team, EnemyAt(1))), Math.Round(vector.DistanceXZ(team, EnemyAt(1))));
  PrintFormat("[BBB] SQUADTEST picture %1 deg %2 m (%3 m from a group); old centroid %4 deg %5 m (%6 m from a group)", Math.Round(Bearing(team, picture)), Math.Round(vector.DistanceXZ(team, picture)), Math.Round(newOff), Math.Round(Bearing(team, old)), Math.Round(vector.DistanceXZ(team, old)), Math.Round(oldOff));
  PrintFormat("[BBB] SQUADTEST %1", BBB_Squad.Describe(m_Teams[0]));
  BBB_SquadPicture seen = BBB_Squad.Get(m_Teams[0]);
  if (seen)
  {
   foreach (BBB_SquadContact contact : seen.m_Contacts)
    PrintFormat("[BBB] SQUADTEST contact %1 deg %2 m, %3 s old, weight %4", Math.Round(contact.m_fBearing), Math.Round(vector.DistanceXZ(contact.m_vPos, seen.m_vCentre)), contact.m_fAge.ToString(4, 1), contact.m_fWeight.ToString(4, 2));
  }
  SquadCheck("picture-on-a-group", has && newOff <= 20, string.Format("%1 m from the nearer group", Math.Round(newOff)));
  SquadCheck("picture-flanked", flanked);
  SquadCheck("old-centroid-off-both", hasOld && oldOff >= newOff + 15, string.Format("old %1 m from the nearer group, picture %2", Math.Round(oldOff), Math.Round(newOff)));
 }

 // Kill the leader and one other man of team i.
 void KillTwo(int i)
 {
  array<AIAgent> agents = {};
  m_Teams[i].GetAgents(agents);
  IEntity leader = m_Teams[i].GetLeaderEntity();
  array<IEntity> victims = {};
  if (leader && BBB.IsAlive(leader)) victims.Insert(leader);
  foreach (AIAgent agent : agents)
  {
   IEntity man = agent.GetControlledEntity();
   if (victims.Count() < 2 && man && man != leader && BBB.IsAlive(man)) victims.Insert(man);
  }
  foreach (IEntity victim : victims)
  {
   SCR_DamageManagerComponent damage = SCR_DamageManagerComponent.GetDamageManager(victim);
   if (!damage) continue;
   damage.EnableDamageHandling(true);
   damage.Kill(Instigator.CreateInstigator(null));
  }
 }

 void Break()
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
   KillTwo(i);
  }
  // Half the team lost scores 0.40; only the fallen leader (0.25, counted through the
  // reorganisation window although a new one is named at once) takes it to 0.65.
  BBB_Morale.s_fBreakScore = 0.60;
  m_Wait = 0;
  GetGame().GetCallqueue().CallLater(WaitBreak, 1000, true);
 }

 // Morale checks every 1.5 s; give both teams up to 12 s to break.
 void WaitBreak()
 {
  m_Wait++;
  if (m_Wait < 12 && !(BBB_Morale.IsWithdrawing(m_Teams[0]) && BBB_Morale.IsWithdrawing(m_Teams[1]))) return;
  GetGame().GetCallqueue().Remove(WaitBreak);
  JudgeOrders();
 }

 void JudgeOrders()
 {
  array<float> clearance = {};
  array<bool> farther = {};
  for (int t = 0; t < 2; t++)
  {
   vector at;
   Centre(m_Teams[t], at);
   m_vStart[t] = at;
   vector threat;
   vector destination;
   bool ordered = BBB_Morale.WithdrawOrder(m_Teams[t], threat, destination);
   float heading = Bearing(at, destination);
   // How far the heading stays from the bearing to either enemy group.
   float clear = Math.Min(BBB_Squad.AngleBetween(heading, Bearing(at, EnemyAt(0))), BBB_Squad.AngleBetween(heading, Bearing(at, EnemyAt(1))));
   if (!ordered) clear = 0;
   clearance.Insert(clear);
   farther.Insert(ordered && NearestEnemy(destination) > NearestEnemy(at) + 3);
   PrintFormat("[BBB] SQUADTEST team %1 withdrawal order %2: aims at %3 deg %4 m, sent toward %5 deg (%6 deg clear of both groups), nearer group %7 m now, %8 m there", t, ordered, Math.Round(Bearing(at, threat)), Math.Round(vector.DistanceXZ(at, threat)), Math.Round(heading), Math.Round(clear), Math.Round(NearestEnemy(at)), Math.Round(NearestEnemy(destination)));
   SquadCheck(string.Format("team-%1-withdraws", t), ordered, "");
  }
  SquadCheck("A-sent-clear-of-both", clearance[0] >= 60, string.Format("%1 deg clear (B %2)", Math.Round(clearance[0]), Math.Round(clearance[1])));
  // Same reports the old way: straight away from team A's averaged centroid.
  vector oldDestination = BBB_Morale.CalculateRetrogradeDestination(m_vStart[0], m_vOld, BBB_Morale.s_fRetrogradeBoundDist);
  float oldHeading = Bearing(m_vStart[0], oldDestination);
  float oldClear = Math.Min(BBB_Squad.AngleBetween(oldHeading, Bearing(m_vStart[0], EnemyAt(0))), BBB_Squad.AngleBetween(oldHeading, Bearing(m_vStart[0], EnemyAt(1))));
  SquadCheck("A-not-worse-than-old-way", clearance[0] >= oldClear - 10, string.Format("%1 deg clear, the old way %2 deg toward %3 deg (nearer group %4 m there)", Math.Round(clearance[0]), Math.Round(oldClear), Math.Round(oldHeading), Math.Round(NearestEnemy(oldDestination))));
  // Where the rules really differ: groups on opposite sides (north 110 m, south 60 m). The old rule
  // backs away from their average (just north of the squad), straight at the south group.
  array<float> opposite = {-4, 0, 3, 6, 176, 180, 183, 187};
  vector sideways;
  BBB_Squad.EscapeFrom(opposite, sideways);
  float sidewaysBearing = Bearing(vector.Zero, sideways);
  float sidewaysClear = Math.Min(BBB_Squad.AngleBetween(sidewaysBearing, 0), BBB_Squad.AngleBetween(sidewaysBearing, 180));
  // The old rule: straight away from the average position, (0, 25) for four men each side.
  float oldOppositeBearing = Bearing(Vector(0, 0, 25), vector.Zero);
  float oldOppositeClear = Math.Min(BBB_Squad.AngleBetween(oldOppositeBearing, 0), BBB_Squad.AngleBetween(oldOppositeBearing, 180));
  SquadCheck("opposite-sides-goes-sideways", sidewaysClear >= 80 && oldOppositeClear <= 10, string.Format("way out %1 deg clear, the old rule %2", Math.Round(sidewaysClear), Math.Round(oldOppositeClear)));
  SquadCheck("A-sent-farther-from-both", farther[0], string.Format("B farther %1", farther[1]));
  m_Wait = 0;
  GetGame().GetCallqueue().CallLater(Trace, 5000, true);
  GetGame().GetCallqueue().CallLater(Measure, 20000);
 }

 // Every 5 s: where each survivor is and what he is doing.
 void Trace()
 {
  m_Wait += 5;
  for (int i = 0; i < 2; i++)
  {
   array<AIAgent> agents = {};
   m_Teams[i].GetAgents(agents);
   string line;
   foreach (AIAgent agent : agents)
   {
    IEntity man = agent.GetControlledEntity();
    if (!man || !BBB.IsAlive(man)) continue;
    SCR_AIUtilityComponent utility = SCR_AIUtilityComponent.Cast(agent.FindComponent(SCR_AIUtilityComponent));
    string doing = "?";
    if (utility && utility.GetCurrentAction()) doing = utility.GetCurrentAction().ClassName();
    line += string.Format(" [%1 m at %2 deg, %3]", Math.Round(vector.DistanceXZ(man.GetOrigin(), m_vStart[i])), Math.Round(Bearing(m_vStart[i], man.GetOrigin())), doing);
   }
   PrintFormat("[BBB] SQUADTEST t+%1 team %2 withdrawing %3:%4", m_Wait, i, BBB_Morale.IsWithdrawing(m_Teams[i]), line);
  }
 }

 void Measure()
 {
  GetGame().GetCallqueue().Remove(Trace);
  for (int i = 0; i < 2; i++)
  {
   vector now;
   Centre(m_Teams[i], now);
   float gained = NearestEnemy(now) - NearestEnemy(m_vStart[i]);
   PrintFormat("[BBB] SQUADTEST team %1 after 20 s: moved %2 m at %3 deg, nearer group %4 m (was %5)", i, Math.Round(vector.DistanceXZ(now, m_vStart[i])), Math.Round(Bearing(m_vStart[i], now)), Math.Round(NearestEnemy(now)), Math.Round(NearestEnemy(m_vStart[i])));
   // A must get clear; B (old direction rule) must at least really run.
   if (i == 0) SquadCheck("A-got-away", gained >= 10, string.Format("%1 m farther from the nearer group", Math.Round(gained)));
   else SquadCheck("B-ran", vector.DistanceXZ(now, m_vStart[i]) >= 12, string.Format("%1 m, %2 m farther from the nearer group", Math.Round(vector.DistanceXZ(now, m_vStart[i])), Math.Round(gained)));
  }
  SquadCheck("pictures-built", BBB_Squad.s_iBuilt > 0, string.Format("built %1, held %2", BBB_Squad.s_iBuilt, BBB_Squad.Count()));
  FinishSquad();
 }

 void FinishSquad()
 {
  BBB_Morale.s_fBreakScore = 0.70;
  if (m_SavedHooks) BBB.s_Hooks = m_SavedHooks;
  string verdict = "FAIL";
  if (m_Failures == 0 && m_Checks == EXPECTED) verdict = "PASS";
  PrintFormat("[ACR] VERDICT %1 test=bbbsquad passed=%2 failed=%3 expected=%4", verdict, m_Checks - m_Failures, m_Failures, EXPECTED);
  Print("[ACR] TEST COMPLETE");
 }
}

modded class SCR_BaseGameMode
{
 override protected void OnGameStart()
 {
  super.OnGameStart();
  BBB_SquadTest.Start();
 }
}
