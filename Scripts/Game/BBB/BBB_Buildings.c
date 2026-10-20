// Contact-driven indoor cover. Native cover scoring uses this soldier's pathfinding;
// native MoveIndividually handles doors and traversal. No authored room coordinates.
class BBB_BuildingJob : Managed
{
 SCR_AIGroup m_Group;
 SCR_AIUtilityComponent m_Utility;
 IEntity m_Building;
 vector m_Position;
 vector m_Threat;
 vector m_LastPosition;
 ref SCR_AICoverLock m_Cover;
 ref SCR_AIBehaviorBase m_Action;
 float m_Started;
 float m_Progress;
 float m_Arrived;
 int m_Steps;
}

// Re-evaluated by the native utility before selection, including between BBB ticks.
class BBB_IndoorMove : SCR_AIMoveIndividuallyBehavior
{
 override float CustomEvaluate()
 {
  if (!BBB_Buildings.Allowed(m_Utility, false)) return 0;
  return GetPriority();
 }
}

class BBB_Buildings
{
 static bool s_bRead;
 static bool s_bOn;
 static int s_iStarted;
 static int s_iArrived;
 static int s_iFailed;
 static int s_iQueries;
 static int s_iClearingSteps;
 protected static int s_iQueryBudget;
 static ref array<ref BBB_BuildingJob> s_Jobs = {};
 protected static ref map<SCR_AIUtilityComponent, float> s_Retry = new map<SCR_AIUtilityComponent, float>();
 protected static int s_iCursor;
 protected static bool s_bInTick;
 protected static ref array<IEntity> s_Buildings = {};

 protected static bool CollectBuilding(IEntity entity)
 {
  if (BaseBuilding.Cast(entity) && s_Buildings.Count() < 24) s_Buildings.Insert(entity);
  return true;
 }

 static void Read()
 {
  if (s_bRead) return;
  s_bRead = true;
  string value;
  s_bOn = System.GetCLIParam("bbbBuildings", value) && value == "1";
  PrintFormat("[BBB] BUILDINGS enabled=%1", s_bOn);
 }

 static SCR_AIGroup GroupOf(SCR_AIUtilityComponent utility)
 {
  if (!utility || !utility.GetAIAgent()) return null;
  return SCR_AIGroup.Cast(utility.GetAIAgent().GetParentGroup());
 }

 static bool Allowed(SCR_AIUtilityComponent utility, bool inspectGroup = true)
 {
  if (!s_bOn || !BBB.IsStarted() || !utility || !utility.m_OwnerEntity) return false;
  SCR_AIGroup group = GroupOf(utility);
  if (!group || group.GetCurrentWaypoint() || (inspectGroup && BBB.HasPlayer(group)) || !BBB.s_Hooks.AllowBuildings(group)) return false;
  if (BBB_OrderIntent.Blocks(utility) || !utility.IsMilitary()) return false;
  ChimeraCharacter body = ChimeraCharacter.Cast(utility.m_OwnerEntity);
  if (!body || !BBB.IsAlive(body) || body.IsInVehicle() || BBB.PlayerOf(body) || BBB_Drag.Busy(body)) return false;
  CharacterControllerComponent cc = body.GetCharacterController();
  if (!cc || cc.IsUnconscious() || cc.IsPlayerControlled() || cc.IsClimbing() || cc.IsSwimming()) return false;
  AIControlComponent control = AIControlComponent.Cast(body.FindComponent(AIControlComponent));
  if (!control || !control.IsAIActivated()) return false;
  if (BBB_Morale.IsWithdrawing(group) || utility.m_BBBFireJob) return false;
  return true;
 }

 static bool RecentContact(SCR_AIUtilityComponent utility, out vector known)
 {
  if (!utility || !utility.m_PerceptionComponent) return false;
  array<BaseTarget> targets = {};
  utility.m_PerceptionComponent.GetTargetsList(targets, ETargetCategory.ENEMY);
  foreach (BaseTarget target : targets)
  {
   if (!target || target.GetUnitType() != EAIUnitType.UnitType_Infantry || target.IsDisarmed()) continue;
   float age = target.GetTimeSinceDetected();
   if (age < 0 || age > 20) continue;
   vector point = target.GetLastDetectedPosition();
   float distance = vector.Distance(utility.m_OwnerEntity.GetOrigin(), point);
   if (distance < 8 || distance > 100) continue;
   known = point;
   return true;
  }
  return false;
 }

 // A roof belonging to a real building, rather than trees or outdoor cover.
 static IEntity RoofAt(vector position)
 {
  TraceParam trace = new TraceParam();
  trace.Start = position + Vector(0, 1.8, 0);
  trace.End = position + Vector(0, 8, 0);
  trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
  if (GetGame().GetWorld().TraceMove(trace, null) >= 1) return null;
  IEntity hit = trace.TraceEnt;
  if (!hit) return null;
  if (BaseBuilding.Cast(hit)) return hit;
  if (BaseBuilding.Cast(hit.GetRootParent())) return hit.GetRootParent();
  return null;
 }

 static bool FindIndoorCover(SCR_AIUtilityComponent utility, vector threat, out vector position, out IEntity building, out SCR_AICoverLock cover)
 {
  if (!utility || !utility.m_OwnerEntity || !GetGame().GetAIWorld()) return false;
  if (!s_bInTick) s_iQueryBudget = 4;
  AIPathfindingComponent path = AIPathfindingComponent.Cast(utility.m_OwnerEntity.FindComponent(AIPathfindingComponent));
  ChimeraCoverManagerComponent manager = ChimeraCoverManagerComponent.Cast(GetGame().GetAIWorld().FindComponent(CoverManagerComponent));
  if (!path || !manager || !path.GetNavmeshComponent()) return false;
  vector origin = utility.m_OwnerEntity.GetOrigin();
  if (!path.GetNavmeshComponent().IsTileLoaded(origin)) return false;
  s_Buildings.Clear();
  GetGame().GetWorld().QueryEntitiesBySphere(origin, 32, CollectBuilding);
  IEntity selected;
  float nearest = float.MAX;
  foreach (IEntity candidate : s_Buildings)
  {
   vector boundsMin, boundsMax;
   candidate.GetBounds(boundsMin, boundsMax);
   vector size = boundsMax - boundsMin;
   // Tiny closed sheds and other non-room structures are poor garrison targets.
   if (size[0] < 5 || size[2] < 5 || size[1] < 2.5) continue;
   float dist = vector.Distance(origin, candidate.GetOrigin());
   if (dist < nearest) { selected = candidate; nearest = dist; }
  }
  if (!selected) return false;
  CoverQueryProperties props = new CoverQueryProperties();
  vector towards = threat - origin;
  towards[1] = 0;
  towards.Normalize();
  props.m_vSectorPos = selected.GetOrigin();
  props.m_vSectorDir = towards;
  props.m_vThreatPos = threat + Vector(0, 1, 0);
  props.m_vNearestPolyHalfExtend = "1 2 1";
  vector mins, maxs;
  selected.GetBounds(mins, maxs);
  props.m_fSectorDistMax = Math.Clamp((maxs - mins).Length() * 0.5, 5, 18);
  props.m_fCoverHeightMin = 0;
  props.m_fCoverToThreatAngleCosMin = -1;
  props.m_fNmAreaCostScale = 1.0 / 3.0;
  props.m_iMaxCoversToCheck = 12;
  props.m_bCheckVisibility = false;
  props.m_bSelectHighestScore = false;
  vector tallest;
  int tileX, tileY, coverId;
  // Temporarily exclude rejected outdoor covers so an exterior wall cannot
  // permanently hide the usable indoor positions of this building.
  array<ref SCR_AICoverLock> rejected = {};
  bool found = false;
  for (int attempt = 0; attempt < 4; attempt++)
  {
   if (s_iQueryBudget <= 0) break;
   s_iQueryBudget--;
   s_iQueries++;
   bool coverFound = manager.GetBestCover("Soldiers", path, props, position, tallest, tileX, tileY, coverId);
   if (!coverFound) break;
   SCR_AICoverLock candidateCover = new SCR_AICoverLock(tileX, tileY, coverId, position, tallest);
   building = RoofAt(position);
   vector snapped;
   if (building == selected && vector.Distance(origin, position) <= 32 && vector.Distance(origin, position) >= 2 && path.GetClosestPositionOnNavmesh(position, "0.3 0.3 0.3", snapped) && vector.Distance(position, snapped) <= 0.4)
   {
    cover = candidateCover;
    found = true;
    break;
   }
   rejected.Insert(candidateCover);
  }
  foreach (SCR_AICoverLock rejectedCover : rejected) rejectedCover.Release();
  return found;
 }

 static bool Begin(SCR_AIUtilityComponent utility, vector threat)
 {
  if (!Allowed(utility)) return false;
  foreach (BBB_BuildingJob existing : s_Jobs)
   if (existing.m_Utility == utility) return false;
  vector position;
  IEntity building;
  SCR_AICoverLock cover;
  if (!FindIndoorCover(utility, threat, position, building, cover)) return false;
  BBB_BuildingJob job = new BBB_BuildingJob();
  job.m_Group = GroupOf(utility);
  job.m_Utility = utility;
  job.m_Position = position;
  job.m_Threat = threat;
  job.m_Building = building;
  job.m_Cover = cover;
  job.m_Started = GetGame().GetWorld().GetWorldTime();
  job.m_Progress = job.m_Started;
  job.m_LastPosition = utility.m_OwnerEntity.GetOrigin();
  // Above ordinary attack selection, below native aid/grenade/retreat/danger.
  job.m_Action = new BBB_IndoorMove(utility, null, position, 91, SCR_AIActionBase.PRIORITY_LEVEL_NORMAL, null, 0.3);
  utility.AddAction(job.m_Action);
  s_Jobs.Insert(job);
  s_iStarted++;
  return true;
 }

 static void Release(BBB_BuildingJob job)
 {
  if (job.m_Action && job.m_Action.GetActionState() != EAIActionState.COMPLETED && job.m_Action.GetActionState() != EAIActionState.FAILED) job.m_Action.Fail();
  if (job.m_Utility && job.m_Utility.m_CombatMoveState && job.m_Utility.m_CombatMoveState.GetAssignedCover() == job.m_Cover) job.m_Utility.m_CombatMoveState.ReleaseCover();
  else if (job.m_Cover) job.m_Cover.Release();
  if (job.m_Utility) s_Retry.Set(job.m_Utility, GetGame().GetWorld().GetWorldTime() + 30000);
 }

 static void Stop()
 {
  foreach (BBB_BuildingJob job : s_Jobs) Release(job);
  s_Jobs.Clear();
  s_Retry.Clear();
 }

 static void Tick()
 {
  s_bInTick = true;
  TickJobs();
  s_bInTick = false;
 }

 protected static void TickJobs()
 {
  Read();
  if (!s_bOn) { Stop(); return; }
  s_iQueryBudget = 4;
  float now = GetGame().GetWorld().GetWorldTime();
  array<SCR_AIGroup> groups = {};
  BBB.s_Hooks.GetGroups(groups);
  for (int i = s_Jobs.Count() - 1; i >= 0; i--)
  {
   BBB_BuildingJob job = s_Jobs[i];
   vector known;
   bool valid = groups.Contains(job.m_Group) && Allowed(job.m_Utility) && job.m_Building && job.m_Cover && job.m_Cover.IsValid();
   if (valid) valid = RecentContact(job.m_Utility, known) && vector.Distance(known, job.m_Threat) < 25;
   if (valid)
   {
    vector here = job.m_Utility.m_OwnerEntity.GetOrigin();
    if (vector.Distance(here, job.m_LastPosition) > 0.75) { job.m_LastPosition = here; job.m_Progress = now; }
    if (job.m_Arrived == 0 && vector.Distance(here, job.m_Position) <= 1.3 && RoofAt(here) == job.m_Building)
    {
     job.m_Arrived = now;
     s_iArrived++;
     if (job.m_Action.GetActionState() != EAIActionState.COMPLETED) job.m_Action.Fail();
     if (job.m_Utility.m_CombatMoveState)
     {
      job.m_Utility.m_CombatMoveState.AssignCover(job.m_Cover);
      job.m_Utility.m_CombatMoveState.m_bInCover = true;
      job.m_Utility.m_CombatMoveState.m_bExposedInCover = true;
     }
     // Native investigation scans this occupied cover position. Native attacks,
     // aid, danger and orders outrank its normal 64 priority while holding.
     job.m_Action = new SCR_AIMoveAndInvestigateBehavior(job.m_Utility, null, job.m_Position, 64, 0, 0.7, true, EAIUnitType.UnitType_Infantry, 12);
     job.m_Utility.AddAction(job.m_Action);
    }
    if (job.m_Arrived > 0 && now - job.m_Arrived >= 6000 && job.m_Steps < 2 && RoofAt(known) == job.m_Building && s_iQueryBudget > 0)
    {
     vector nextPosition;
     IEntity nextBuilding;
     SCR_AICoverLock nextCover;
     if (FindIndoorCover(job.m_Utility, known, nextPosition, nextBuilding, nextCover))
     {
      if (nextBuilding == job.m_Building && vector.Distance(nextPosition, job.m_Position) >= 3)
      {
       job.m_Action.Fail();
       if (job.m_Utility.m_CombatMoveState.GetAssignedCover() == job.m_Cover) job.m_Utility.m_CombatMoveState.ReleaseCover();
       else job.m_Cover.Release();
       job.m_Cover = nextCover;
       job.m_Position = nextPosition;
       job.m_Arrived = 0;
       job.m_Progress = now;
       job.m_Started = now;
       job.m_Steps++;
       s_iClearingSteps++;
       job.m_Action = new BBB_IndoorMove(job.m_Utility, null, nextPosition, 91, 0, null, 0.3);
       job.m_Utility.AddAction(job.m_Action);
      }
      else nextCover.Release();
     }
    }
    if (job.m_Arrived > 0) valid = now - job.m_Arrived < 12000 && vector.Distance(here, job.m_Position) < 4;
    else valid = (job.m_Action.GetActionState() != EAIActionState.RUNNING || now - job.m_Progress < 8000) && now - job.m_Started < 30000 && job.m_Action.GetActionState() != EAIActionState.FAILED;
   }
   if (!valid)
   {
    if (job.m_Arrived == 0) s_iFailed++;
    Release(job);
    s_Jobs.RemoveOrdered(i);
   }
  }
  for (int r = s_Retry.Count() - 1; r >= 0; r--)
   if (!s_Retry.GetKey(r) || now >= s_Retry.GetElement(r)) s_Retry.RemoveElement(r);
  if (groups.IsEmpty()) return;
  // One group's discovery per second, at most four bounded cover queries. No frame scans.
  s_iCursor = (s_iCursor + 1) % groups.Count();
  SCR_AIGroup group = groups[s_iCursor];
  int working;
  foreach (BBB_BuildingJob occupied : s_Jobs)
   if (occupied.m_Group == group) working++;
  if (working >= 2 || s_Jobs.Count() >= 16 || s_iQueryBudget <= 0) return;
  array<AIAgent> agents = {};
  BBB.s_Hooks.GetMembers(group, agents);
  foreach (AIAgent agent : agents)
  {
   if (!agent) continue;
   SCR_AIUtilityComponent utility = SCR_AIUtilityComponent.Cast(agent.FindComponent(SCR_AIUtilityComponent));
   if (!Allowed(utility) || now < s_Retry.Get(utility)) continue;
   bool busy;
   foreach (BBB_BuildingJob activeJob : s_Jobs)
    if (activeJob.m_Utility == utility) busy = true;
   if (busy) continue;
   vector threat;
   if (!RecentContact(utility, threat)) continue;
   s_Retry.Set(utility, now + 10000);
   Begin(utility, threat);
   break;
  }
 }
}
