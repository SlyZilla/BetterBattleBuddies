// Better Battle Buddies (BBB): general fixes for the game's own AI that don't
// depend on AC: Actual. Nothing under Scripts/Game/BBB may use an ACR_ class;
// Host mods plug in through BBB_Hooks; this standalone addon can
// also start itself through the vanilla adapter for Conflict and Game Master.
//
// What's here: gates opened ahead of AI vehicles, soldiers set down from truck
// beds, and AI vehicle doors closed (BBB_VehicleHelpers), spent disposable launchers
// dropped (BBB_Launchers), road routes and land routes (BBB_Roads, BBB_Terrain),
// the "vehicle settled" check for dismounts (BBB_Transport) and backing a stuck
// vehicle out (BBB_Unstick).

// What the host mod tells BBB. The defaults suit plain vanilla groups.
class BBB_Hooks : Managed
{
 // The AI groups BBB looks after (in a fixed order).
 void GetGroups(notnull array<SCR_AIGroup> groups) { groups.Clear(); }
 // A group's soldiers (the host may count members held elsewhere, e.g. a leader
 // a player commands through).
 void GetMembers(SCR_AIGroup group, notnull array<AIAgent> agents) { agents.Clear(); if (group) group.GetAgents(agents); }
 // Name for log lines.
 string NameOf(SCR_AIGroup group) { return "group"; }
 // Where a group is (its leader).
 IEntity AnchorOf(SCR_AIGroup group) { return BBB.AnchorOf(group); }
 // The vehicle a crew came with, even while they are out of it (or null).
 IEntity OwnVehicleOf(SCR_AIGroup group) { return null; }
 // Vehicles BBB must leave alone (e.g. one a player drives from the gunner seat).
 bool LeaveAlone(IEntity vehicle) { return false; }
 // Host orders may reserve a squad for holding, defence or another job.
 bool AllowFireManeuver(SCR_AIGroup group) { return true; }
 // Quiet ordinary travel may lower weapons; host assault/clear orders may veto.
 bool AllowRelaxedMarch(SCR_AIGroup group) { return true; }
 // Host orders or commander retreats may handle morale for this group.
 bool AllowMorale(SCR_AIGroup group) { return true; }
 // Host tactical doctrines may veto autonomous tactical smoke.
 bool AllowSmoke(SCR_AIGroup group) { return true; }
 // Host tactical doctrines may veto static weapon crewing.
 bool AllowStaticWeapons(SCR_AIGroup group) { return true; }
 // Host tactical doctrines may veto offensive hand grenade employment.
 bool AllowGrenades(SCR_AIGroup group) { return true; }
 bool AllowBuildings(SCR_AIGroup group) { return true; }
 bool AllowArmorReaction(SCR_AIGroup group) { return true; }
 // Host mod manages its own vehicle stall/unstick and tactical cargo dismount for this group.
 bool ManagesVehicleDriving(SCR_AIGroup group) { return false; }
 // Something dropped on the ground that the host may clean up later.
 void OnLitter(IEntity item) {}
}

class BBB
{
	static ref BBB_Hooks s_Hooks = new BBB_Hooks();
	static ref BBB_VehicleHelpers s_VehicleHelpers;
	static ref BBB_Launchers s_Launchers;
	static ref BBB_Combat s_Combat;
 protected static bool s_bStarted;

 // Server only. Ticks once a second on its own; Frame() must be called every frame
 // by the host (the unstick manoeuvre writes vehicle inputs each frame).
 static void Start(BBB_Hooks hooks)
 {
  if (hooks) s_Hooks = hooks;
  if (s_bStarted || !Replication.IsServer()) return;
		s_bStarted = true;
		BBB_Perf.Read();
		BBB_Rearm.Read();
		BBB_Morale.Read();
		BBB_Smoke.Read();
		BBB_StaticWeapons.Read();
		BBB_Grenades.Read();
  BBB_Buildings.Read();
  BBB_ArmorReaction.Read();
		s_VehicleHelpers = new BBB_VehicleHelpers();
		s_Launchers = new BBB_Launchers();
		s_Combat = new BBB_Combat();
		BBB_Combat.s_Combat = s_Combat;
  GetGame().GetCallqueue().CallLater(Tick, 1000, true);
 }

 static bool IsStarted() { return s_bStarted; }

 static void Stop()
 {
  if (GetGame()) GetGame().GetCallqueue().Remove(Tick);
  BBB_DriverAlignment.Stop();
  BBB_CrewTurnout.Stop();
  BBB_Unstick.StopAll();
  BBB_FireManeuver.Stop();
  BBB_Drag.Stop();
  BBB_Medic.Stop();
  BBB_RelaxedMarch.Stop();
  BBB_HorizonScan.Stop();
  BBB_Threat.Stop();
  BBB_Armour.Stop();
  BBB_Rearm.Stop();
  BBB_Morale.Stop();
  BBB_Smoke.Stop();
  BBB_StaticWeapons.Stop();
  BBB_Grenades.Stop();
  BBB_Buildings.Stop();
  BBB_ArmorReaction.Stop();
  BBB_Transport.ClearHits();
  s_bStarted = false;
		s_VehicleHelpers = null;
		s_Launchers = null;
		s_Combat = null;
		BBB_Combat.s_Combat = null;
  s_Hooks = new BBB_Hooks();
 }

 static void Tick()
 {
  int t = System.GetTickCount();
  BBB_CrewTurnout.Tick();
  t = BBB_Perf.Lap("tick.crewTurnout", t);
  BBB_DriverAlignment.Tick();
  t = BBB_Perf.Lap("tick.driverAlign", t);
  if (s_VehicleHelpers) s_VehicleHelpers.Tick();
  t = BBB_Perf.Lap("tick.vehicleHelpers", t);
		if (s_Launchers) s_Launchers.Tick();
		t = BBB_Perf.Lap("tick.launchers", t);
		if (s_Combat) s_Combat.Tick();
		t = BBB_Perf.Lap("tick.combat", t);
		array<SCR_AIGroup> rearmGroups = {};
		s_Hooks.GetGroups(rearmGroups);
		BBB_Rearm.WatchTrips(rearmGroups);
		foreach (SCR_AIGroup rg : rearmGroups)
			BBB_Rearm.CheckRearmFor(rg);
		t = BBB_Perf.Lap("tick.rearm", t);
		BBB_FireManeuver.Tick();
		t = BBB_Perf.Lap("tick.fireManeuver", t);
		BBB_WeaponReady.Tick();
		t = BBB_Perf.Lap("tick.weaponReady", t);
		BBB_RelaxedMarch.Tick();
		t = BBB_Perf.Lap("tick.relaxedMarch", t);
		BBB_HorizonScan.Tick();
		t = BBB_Perf.Lap("tick.horizonScan", t);
		BBB_Morale.Tick();
		t = BBB_Perf.Lap("tick.morale", t);
		BBB_Smoke.Tick();
		t = BBB_Perf.Lap("tick.smoke", t);
		BBB_StaticWeapons.Tick();
		t = BBB_Perf.Lap("tick.staticWeapons", t);
		BBB_Grenades.Tick();
		t = BBB_Perf.Lap("tick.grenades", t);
  BBB_Buildings.Tick();
  t = BBB_Perf.Lap("tick.buildings", t);
  BBB_ArmorReaction.Tick();
  t = BBB_Perf.Lap("tick.armorReaction", t);
		BBB_Perf.OnTick();
 }

 // Every frame, once (a host and BBB's own frame source may both call it).
 protected static int s_iLastFrame = -1;
 static int s_iFrames; // frames run (tests read it)
 static void Frame()
 {
  int frame = GetGame().GetWorld().GetFrameNumber();
  if (frame == s_iLastFrame) return;
  s_iLastFrame = frame;
  s_iFrames++;
  BBB_Perf.OnFrame();
  BBB_Unstick.FrameAll();
  int armorStarted = System.GetTickCount();
  BBB_ArmorReaction.Frame();
  BBB_Perf.Add("frame.armorReaction", System.GetTickCount() - armorStarted);
  BBB_DriverAlignment.Frame();
 }

 // ---------------------------------------------------------------- helpers

 // The point on the ground (just above it).
 static vector Ground(vector point)
 {
  point[1] = GetGame().GetWorld().GetSurfaceY(point[0], point[2]) + 0.15;
  return point;
 }

 static bool IsAlive(IEntity entity)
 {
  ChimeraCharacter character = ChimeraCharacter.Cast(entity);
  return character && character.GetCharacterController() && !character.GetCharacterController().IsDead();
 }

 // The player controlling this entity (0: nobody).
 static int PlayerOf(IEntity entity)
 {
  if (!entity) return 0;
  array<int> players = {};
  GetGame().GetPlayerManager().GetPlayers(players);
  foreach (int id : players)
  {
   PlayerController pc = GetGame().GetPlayerManager().GetPlayerController(id);
   if (pc && pc.GetControlledEntity() == entity) return id;
  }
  return 0;
 }

 static bool HasPlayer(SCR_AIGroup group)
 {
  if (!group) return false;
  array<AIAgent> agents = {};
  group.GetAgents(agents);
  foreach (AIAgent agent : agents)
   if (PlayerOf(agent.GetControlledEntity())) return true;
  return false;
 }

 static ChimeraCharacter DriverOf(IEntity vehicle)
 {
  if (!vehicle) return null;
  CarControllerComponent car = CarControllerComponent.Cast(vehicle.FindComponent(CarControllerComponent));
  if (!car || !car.GetPilotCompartmentSlot()) return null;
  return ChimeraCharacter.Cast(car.GetPilotCompartmentSlot().GetOccupant());
 }

 static float Health(IEntity entity)
 {
  if (!entity) return 0;
  DamageManagerComponent damage = DamageManagerComponent.Cast(entity.FindComponent(DamageManagerComponent));
  if (!damage) return 1;
  if (damage.IsDestroyed()) return 0;
  return damage.GetHealthScaled();
 }

 // Leader (else any living member) of a group: where the group is.
 static IEntity AnchorOf(SCR_AIGroup group)
 {
  if (!group) return null;
  AIAgent leader = group.GetLeaderAgent();
  if (leader && IsAlive(leader.GetControlledEntity())) return leader.GetControlledEntity();
  array<AIAgent> agents = {};
  group.GetAgents(agents);
  foreach (AIAgent agent : agents)
   if (IsAlive(agent.GetControlledEntity())) return agent.GetControlledEntity();
  return null;
 }
}
