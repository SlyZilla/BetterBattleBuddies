// A/B for turret-linked cargo seats (LAV-25 commander, BBB_Transport.CommanderSeat).
// -bbbCommanderSeatTest 1 (Test-BBB.ps1 -Preset commanderseat). Two LAV-25s, each with a
// commander in his seat and a passenger in a hull cargo seat from the same US team, with
// driver and gunner aboard. A cargo-only dismount is requested on lane A with
// -bbbCommanderCrew off and on lane B with it on; 500 ms seat samples over 8 s show
// whether the commander actually left (off) or stayed in his seat (on).
// Controlled input (a direct DismountGroupFromVehicle call), not an autonomous firefight.
class BBB_CommanderSeatLane
{
 SCR_AIGroup m_Group;
 IEntity m_Lav;
 BaseCompartmentSlot m_CommanderSeat;
 ChimeraCharacter m_Commander;
 ChimeraCharacter m_Passenger;
 bool m_bCommanderOut;
 bool m_bPassengerOut;
}

class BBB_CommanderSeatTest : BBB_AimTest
{
 static ref BBB_CommanderSeatTest s_CommanderTest;
 static const ResourceName LAV = "{0FBF8F010F81A4E5}Prefabs/Vehicles/Wheeled/LAV25/LAV25.et";
 static const int EXPECTED = 11;
 protected int m_iChecks;
 protected int m_iFailures;
 protected ref BBB_CommanderSeatLane m_A = new BBB_CommanderSeatLane();
 protected ref BBB_CommanderSeatLane m_B = new BBB_CommanderSeatLane();
 protected int m_iSamples;

 static void MaybeStartCommander()
 {
  if (s_CommanderTest || !Replication.IsServer() || !System.IsCLIParam("bbbCommanderSeatTest")) return;
  s_CommanderTest = new BBB_CommanderSeatTest();
  GetGame().GetCallqueue().CallLater(s_CommanderTest.Begin, 12000);
 }

 override protected void Check(string name, bool ok, string detail)
 {
  m_iChecks++;
  string res = "PASS";
  if (!ok)
  {
   res = "FAIL";
   m_iFailures++;
  }
  PrintFormat("[BBB] COMMANDERTEST CHECK %1 %2 %3", name, res, detail);
 }

 override protected void Begin()
 {
  vector at = BBB.Ground(Vector(1340, 0, 2770));
  SpawnLane(m_A, at);
  SpawnLane(m_B, at + Vector(20, 0, 0));
  GetGame().GetCallqueue().CallLater(SeatLanes, 4000);
 }

 protected void SpawnLane(BBB_CommanderSeatLane lane, vector at)
 {
  lane.m_Group = SCR_AIGroup.Cast(Spawn(TEAM_US, at + Vector(0, 0, -10), Vector(0, 0, 1)));
  lane.m_Lav = Spawn(LAV, at, Vector(0, 0, 1));
 }

 protected bool SeatLane(BBB_CommanderSeatLane lane)
 {
  if (!lane.m_Group || !lane.m_Lav) return false;
  array<BaseCompartmentSlot> cargo = {};
  BBB_VehicleHelpers.GetVehicleCompartments(lane.m_Lav, cargo, ECompartmentType.CARGO);
  BaseCompartmentSlot plain = null;
  foreach (BaseCompartmentSlot slot : cargo)
  {
   if (BBB_Transport.CommanderSeat(slot))
   {
    if (!lane.m_CommanderSeat) lane.m_CommanderSeat = slot;
   }
   else if (!plain) plain = slot;
  }
  array<BaseCompartmentSlot> pilots = {};
  BBB_VehicleHelpers.GetVehicleCompartments(lane.m_Lav, pilots, ECompartmentType.PILOT);
  array<BaseCompartmentSlot> turrets = {};
  BBB_VehicleHelpers.GetVehicleCompartments(lane.m_Lav, turrets, ECompartmentType.TURRET);
  array<AIAgent> agents = {};
  BBB.s_Hooks.GetMembers(lane.m_Group, agents);
  if (!lane.m_CommanderSeat || !plain || pilots.IsEmpty() || turrets.IsEmpty() || agents.Count() < 4) return false;
  // Driver and gunner seated too: a half-empty LAV gets re-seated by native AI and by
  // BBB's gunner replacement, which would move the men under test.
  array<BaseCompartmentSlot> seats = {pilots[0], turrets[0], lane.m_CommanderSeat, plain};
  array<ChimeraCharacter> men = {};
  bool accepted = true;
  for (int i = 0; i < 4; i++)
  {
   ChimeraCharacter man = ChimeraCharacter.Cast(agents[i].GetControlledEntity());
   if (!man) return false;
   men.Insert(man);
   if (!man.GetCompartmentAccessComponent().GetInVehicle(lane.m_Lav, seats[i], true, -1, ECloseDoorAfterActions.CLOSE_DOOR, false)) accepted = false;
  }
  lane.m_Commander = men[2];
  lane.m_Passenger = men[3];
  // Register the LAV with the group as native boarding does.
  SCR_AIGroupUtilityComponent utility = SCR_AIGroupUtilityComponent.Cast(lane.m_Group.FindComponent(SCR_AIGroupUtilityComponent));
  SCR_AIVehicleUsageComponent usage = SCR_AIVehicleUsageComponent.Cast(lane.m_Lav.FindComponent(SCR_AIVehicleUsageComponent));
  if (utility && usage) utility.AddUsableVehicle(usage);
  return accepted;
 }

 protected void SeatLanes()
 {
  bool a = SeatLane(m_A);
  bool b = SeatLane(m_B);
  Check("commander-seat-found", m_A.m_CommanderSeat != null && m_B.m_CommanderSeat != null, "");
  Check("seating-accepted", a && b, string.Format("A=%1 B=%2", a, b));
  if (!a || !b)
  {
   Finish();
   return;
  }
  GetGame().GetCallqueue().CallLater(Dismount, 4000);
 }

 protected bool Seated(ChimeraCharacter man, IEntity vehicle)
 {
  return man && man.IsInVehicle() && BBB_Transport.VehicleOf(man) == vehicle;
 }

 protected void Dismount()
 {
  // Both lanes must still be aboard before the request, so any exit below is caused by it.
  bool settled = m_B.m_CommanderSeat.GetOccupant() == m_B.m_Commander && Seated(m_B.m_Passenger, m_B.m_Lav)
   && m_A.m_CommanderSeat.GetOccupant() == m_A.m_Commander && Seated(m_A.m_Passenger, m_A.m_Lav);
  Check("aboard-before-request", settled, "");

  BBB_Transport.s_bCommanderCrew = false;
  bool oldCargo = BBB_Transport.SeatOf(m_A.m_Commander) == ECompartmentType.CARGO;
  int droppedA = BBB_VehicleHelpers.DismountGroupFromVehicle(m_A.m_Group, m_A.m_Lav, true);

  BBB_Transport.s_bCommanderCrew = true;
  bool newCrew = BBB_Transport.SeatOf(m_B.m_Commander) == ECompartmentType.TURRET;
  bool passengerCargo = BBB_Transport.SeatOf(m_B.m_Passenger) == ECompartmentType.CARGO;
  int droppedB = BBB_VehicleHelpers.DismountGroupFromVehicle(m_B.m_Group, m_B.m_Lav, true);

  Check("off-commander-is-cargo", oldCargo, "");
  Check("on-commander-is-crew", newCrew && passengerCargo, string.Format("turret=%1 passengerCargo=%2", newCrew, passengerCargo));
  Check("off-requests-two", droppedA == 2, droppedA.ToString());
  Check("on-requests-one", droppedB == 1, droppedB.ToString());
  GetGame().GetCallqueue().CallLater(Sample, 500, true);
 }

 protected void Sample()
 {
  m_iSamples++;
  array<ref BBB_CommanderSeatLane> lanes = {m_A, m_B};
  foreach (BBB_CommanderSeatLane lane : lanes)
  {
   if (lane.m_CommanderSeat.GetOccupant() != lane.m_Commander) lane.m_bCommanderOut = true;
   if (!Seated(lane.m_Passenger, lane.m_Lav)) lane.m_bPassengerOut = true;
  }
  if (m_iSamples < 16) return;
  GetGame().GetCallqueue().Remove(Sample);
  // Physical outcome over 8 s: actual seat occupancy, not the request counts above.
  // Not asserted: in this fixture the hull passengers stay aboard even under the native
  // SCR_AIGetOutVehicle behaviour (20 Oct probes), so their exit is not what this tests.
  PrintFormat("[BBB] COMMANDERTEST INFO passenger out: off=%1 on=%2", m_A.m_bPassengerOut, m_B.m_bPassengerOut);
  Check("off-commander-out", m_A.m_bCommanderOut, "");
  Check("on-commander-stayed", !m_B.m_bCommanderOut, "");
  bool still = m_B.m_CommanderSeat.GetOccupant() == m_B.m_Commander && BBB.IsAlive(m_B.m_Commander);
  Check("on-commander-seated-at-end", still, "");
  float walked = vector.DistanceXZ(m_A.m_Commander.GetOrigin(), m_A.m_Lav.GetOrigin());
  Check("off-commander-on-ground", !m_A.m_Commander.IsInVehicle() && walked > 1, string.Format("%1 m", walked));
  Finish();
 }

 override protected void Finish()
 {
  string verdict = "FAIL";
  if (m_iFailures == 0 && m_iChecks == EXPECTED) verdict = "PASS";
  PrintFormat("[ACR] VERDICT %1 test=bbbcommanderseat passed=%2 failed=%3 expected=%4", verdict, m_iChecks - m_iFailures, m_iFailures, EXPECTED);
  Print("[ACR] TEST COMPLETE");
 }
}

modded class SCR_BaseGameMode
{
 protected override void OnGameStart()
 {
  super.OnGameStart();
  BBB_CommanderSeatTest.MaybeStartCommander();
 }
}
