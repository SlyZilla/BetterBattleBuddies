// Which doors the gate opener swings for a vehicle, and who counts as stuck on a truck bed
// (-bbbGateLaneTest 1, Test-BBB.ps1 -Preset gatelane). Everon soak 23 Oct: the look-ahead
// sphere opened 1080 garage/barn/container doors in 20 min, and the bed check teleported men
// standing beside angled trucks. Uses a real Arland marked gate ("OpenGate") and the nearest
// real wide door without that action, and a parked Ural transport placed around them:
// a door in the lane opens (marked gate or unmarked wide door); 12 m to the side or behind it
// does not, unless it is the driver's navlink; a building door (they carry "OpenGate" too) opens
// only as the navlink. Bed: a point on
// the truck's bed counts; the same height 1.5 m beside the truck, turned 45 degrees (inside its
// world box), does not; 3 m above the roof does not.
// Not covered: the door swing itself (crewstays covers it) and a convoy driving through a town.
class BBB_GateLaneTest : BBB_AimTest
{
 static ref BBB_GateLaneTest s_GateTest;
 static const ResourceName URAL = "{16C1F16C9B053801}Prefabs/Vehicles/Wheeled/Ural4320/Ural4320_transport.et";
 static const int EXPECTED = 13;
 protected int m_iChecks;
 protected int m_iFailures;
 protected vector m_SearchFrom;
 protected IEntity m_Gate;
 protected float m_fGateDistance = float.MAX;
 protected IEntity m_Wide;
 protected float m_fWideDistance = float.MAX;
 protected IEntity m_House;   // a building door that carries the same "OpenGate" action
 protected float m_fHouseDistance = float.MAX;
 protected IEntity m_Truck;

 static void MaybeStartGateLane()
 {
  if (s_GateTest || !Replication.IsServer() || !System.IsCLIParam("bbbGateLaneTest")) return;
  s_GateTest = new BBB_GateLaneTest();
  GetGame().GetCallqueue().CallLater(s_GateTest.Begin, 12000);
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
  PrintFormat("[BBB] GATELANETEST CHECK %1 %2 %3", name, res, detail);
 }

 // A door carrying the "OpenGate" smart action (house doors carry it too, so the gate opener
 // no longer trusts it; here it only picks fixtures).
 protected static bool MarkedGate(IEntity entity)
 {
  if (SCR_AISmartActionHandling.FindSmartAction(entity, "OpenGate", false) != null) return true;
  IEntity parent = entity.GetParent();
  return parent != null && SCR_AISmartActionHandling.FindSmartAction(parent, "OpenGate", false) != null;
 }

 protected bool CollectDoors(IEntity entity)
 {
  if (!entity.FindComponent(BaseDoorComponent)) return true;
  float distance = vector.DistanceXZ(entity.GetOrigin(), m_SearchFrom);
  if (MarkedGate(entity))
  {
   if (BBB_VehicleHelpers.OpeningWidth(entity) < BBB_VehicleHelpers.GATE_MIN_WIDTH)
   {
    if (distance < m_fHouseDistance) { m_fHouseDistance = distance; m_House = entity; }
   }
   else if (distance < m_fGateDistance) { m_fGateDistance = distance; m_Gate = entity; }
   return true;
  }
  vector mins, maxs;
  entity.GetBounds(mins, maxs);
  if (Math.Max(maxs[0] - mins[0], maxs[2] - mins[2]) < BBB_VehicleHelpers.GATE_MIN_WIDTH) return true;
  if (distance < m_fWideDistance) { m_fWideDistance = distance; m_Wide = entity; }
  return true;
 }

 protected static vector Centre(IEntity entity)
 {
  vector mins, maxs;
  entity.GetWorldBounds(mins, maxs);
  return (mins + maxs) * 0.5;
 }

 // The truck parked 'back' metres short of the door's centre, 'side' metres to its right, facing
 // along 'heading' (XZ unit vector).
 protected void Park(IEntity door, vector heading, float back, float side)
 {
  vector right = Vector(heading[2], 0, -heading[0]);
  vector at = Centre(door) - heading * back + right * side;
  at[1] = GetGame().GetWorld().GetSurfaceY(at[0], at[2]);
  Place(at, Math.Atan2(heading[0], heading[2]) * Math.RAD2DEG);
 }

 protected void Place(vector at, float yaw)
 {
  vector transform[4];
  Math3D.AnglesToMatrix(Vector(yaw, 0, 0), transform);
  transform[3] = at;
  m_Truck.SetWorldTransform(transform);
  m_Truck.Update();
 }

 override protected void Begin()
 {
  m_SearchFrom = Vector(1340, 0, 2770);
  GetGame().GetWorld().QueryEntitiesBySphere(m_SearchFrom, 6000, CollectDoors);
  string gateWhere = "none";
  if (m_Gate) gateWhere = string.Format("%1 at %2", m_Gate.GetPrefabData().GetPrefabName(), m_Gate.GetOrigin());
  string wideWhere = "none";
  if (m_Wide) wideWhere = string.Format("%1 at %2", m_Wide.GetPrefabData().GetPrefabName(), m_Wide.GetOrigin());
  string houseWhere = "none";
  if (m_House) houseWhere = m_House.GetOrigin().ToString();
  Check("doors-found", m_Gate != null && m_Wide != null && m_House != null, "gate " + gateWhere + ", wide " + wideWhere + ", building door " + houseWhere);
  m_Truck = Spawn(URAL, BBB.Ground(m_SearchFrom), Vector(0, 0, 1));
  if (!m_Gate || !m_Wide || !m_House || !m_Truck) { Finish(); return; }
  GetGame().GetCallqueue().CallLater(Measure, 2000);
 }

 protected void Measure()
 {
  float reach = BBB_VehicleHelpers.LOOK_AHEAD + BBB_VehicleHelpers.LOOK_RADIUS;
  vector heading = Vector(1, 0, 0);
  Park(m_Wide, heading, 20, 0);
  Check("wide-in-lane-opens", BBB_VehicleHelpers.OpensFor(m_Truck, m_Wide, null, reach), "20 m ahead, centred");
  Park(m_Wide, heading, 20, 12);
  Check("wide-beside-lane-shut", !BBB_VehicleHelpers.OpensFor(m_Truck, m_Wide, null, reach), "20 m ahead, 12 m to the side");
  Park(m_Wide, heading, -10, 0);
  Check("wide-behind-shut", !BBB_VehicleHelpers.OpensFor(m_Truck, m_Wide, null, reach), "10 m behind");
  Park(m_Wide, heading, 20, 12);
  Check("wide-navlink-opens", BBB_VehicleHelpers.OpensFor(m_Truck, m_Wide, m_Wide, reach), "12 m to the side, reported as the driver's navlink");
  Park(m_House, heading, 20, 0);
  Check("building-door-in-lane-shut", !BBB_VehicleHelpers.OpensFor(m_Truck, m_House, null, reach), "building door with OpenGate, 20 m ahead");
  Check("building-door-navlink-opens", BBB_VehicleHelpers.OpensFor(m_Truck, m_House, m_House, reach), "same door reported as the driver's navlink");
  Park(m_Gate, heading, 20, 0);
  Check("marked-gate-in-lane-opens", BBB_VehicleHelpers.OpensFor(m_Truck, m_Gate, null, reach), "marked gate 20 m ahead");
  Park(m_Gate, heading, 20, 12);
  Check("marked-gate-beside-shut", !BBB_VehicleHelpers.OpensFor(m_Truck, m_Gate, null, reach), "marked gate 12 m to the side");

  // Truck beds: the Ural turned 45 degrees in open ground.
  Place(BBB.Ground(m_SearchFrom + Vector(0, 0, 60)), 45);
  vector mins, maxs;
  m_Truck.GetBounds(mins, maxs);
  // Bed: the rear half, at the bed floor (just above the middle of the box height).
  vector bedLocal = Vector(0, mins[1] + (maxs[1] - mins[1]) * 0.45, mins[2] * 0.6);
  vector bed = m_Truck.CoordToParent(bedLocal);
  Check("bed-counts", BBB_VehicleHelpers.OnTopOf(m_Truck, bed), string.Format("local %1", bedLocal));
  // Beside: 1.5 m out from the middle of its side, inside the 45-degree world box.
  vector besideLocal = Vector(maxs[0] + 1.5, bedLocal[1], (mins[2] + maxs[2]) * 0.5);
  vector beside = m_Truck.CoordToParent(besideLocal);
  vector wmins, wmaxs;
  m_Truck.GetWorldBounds(wmins, wmaxs);
  bool inWorldBox = beside[0] >= wmins[0] && beside[0] <= wmaxs[0] && beside[2] >= wmins[2] && beside[2] <= wmaxs[2];
  Check("beside-inside-world-box", inWorldBox, "fixture: the old world-box check would have counted it");
  Check("beside-not-on-bed", !BBB_VehicleHelpers.OnTopOf(m_Truck, beside), string.Format("local %1", besideLocal));
  vector above = m_Truck.CoordToParent(Vector(0, maxs[1] + 3, 0));
  Check("balcony-above-not-on-bed", !BBB_VehicleHelpers.OnTopOf(m_Truck, above), "3 m over the roof");
  Finish();
 }

 override protected void Finish()
 {
  string verdict = "FAIL";
  if (m_iFailures == 0 && m_iChecks == EXPECTED) verdict = "PASS";
  PrintFormat("[ACR] VERDICT %1 test=bbbgatelane passed=%2 failed=%3 expected=%4", verdict, m_iChecks - m_iFailures, m_iFailures, EXPECTED);
  Print("[ACR] TEST COMPLETE");
 }
}

modded class SCR_BaseGameMode
{
 protected override void OnGameStart()
 {
  super.OnGameStart();
  BBB_GateLaneTest.MaybeStartGateLane();
 }
}
