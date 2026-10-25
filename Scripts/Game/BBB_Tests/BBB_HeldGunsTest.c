class BBB_HeldGunsTestHooks : BBB_Hooks
{
 ref array<SCR_AIGroup> m_Groups = {};
 ref array<IEntity> m_Held = {};
 override void GetGroups(notnull array<SCR_AIGroup> groups) { groups.Copy(m_Groups); }
 override void GetHeldGuns(SCR_AIGroup group, notnull array<IEntity> guns)
 {
  guns.Clear();
  if (m_Groups.Contains(group)) guns.Copy(m_Held);
 }
}

// Held guns (-bbbHeldGunsTest 1, Test-BBB.ps1 -Preset heldguns, about 1.5 min): one US team with
// two tripod MGs 14 m in front of it, gun A held (the test hooks list it), gun B not. BBB's
// static crewing walks a man to each. With the sector quiet past the quiet timeout (5 s here),
// B's gunner is released and A's stays seated. Vanilla's dismount check for a threat behind A
// (outside its arc) is overruled with no enemy close, and stands with the option off or with A
// unlisted. A's gunner is taken off: someone mans A again. An armed Humvee in the list is never
// held. An enemy team 35 m in front of A (holding its fire): a fresh close enemy lets the gunner
// get out. Fire and manoeuvre, buildings, morale, grenades, smoke, flares, radio relay and squad
// tactics are off so only the gun jobs act.
// Not covered: a host's own list (ACA's checkpoint and Fortify guns), multiplayer.
class BBB_HeldGunsTest : BBB_AimTest
{
 static ref BBB_HeldGunsTest s_HeldTest;
 static const ResourceName MG = "{887DB60486A18575}Prefabs/Weapons/Tripods/Tripod_M122_MG_M60.et";
 static const ResourceName ARMED_M2HB = "{3EA6F47D95867114}Prefabs/Vehicles/Wheeled/M998/M1025_armed_M2HB.et";
 static const int EXPECTED = 10;
 protected ref BBB_HeldGunsTestHooks m_Hooks;
 protected ref BBB_Hooks m_SavedHooks;
 protected int m_iChecks;
 protected int m_iFailures;
 protected int m_iPhase;
 protected int m_iWait;
 protected vector m_vBase;
 protected SCR_AIGroup m_Team;
 protected SCR_AIGroup m_Enemy;
 protected IEntity m_GunA;
 protected IEntity m_GunB;
 protected IEntity m_Truck;
 protected ChimeraCharacter m_ManA;
 protected ChimeraCharacter m_ManB;
 protected bool m_bALeft;
 protected int m_iReleasedBefore;

 static void MaybeStartHeld()
 {
  if (s_HeldTest || !Replication.IsServer() || !System.IsCLIParam("bbbHeldGunsTest")) return;
  s_HeldTest = new BBB_HeldGunsTest();
  GetGame().GetCallqueue().CallLater(s_HeldTest.Setup, 15000);
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
  PrintFormat("[BBB] HELDGUNSTEST CHECK %1 %2 %3", name, res, detail);
 }

 protected void Setup()
 {
  BBB_HeldGuns.Read();
  BBB_StaticWeapons.Read();
  Check("held-guns-on", BBB_HeldGuns.s_bOn && BBB_StaticWeapons.s_bOn, string.Format("held guns %1, static crewing %2, quiet %3 s", BBB_HeldGuns.s_bOn, BBB_StaticWeapons.s_bOn, BBB_StaticWeapons.s_fQuietTimeoutS));
  BBB_FireManeuver.s_bOn = false;
  BBB_Buildings.s_bOn = false;
  BBB_Morale.s_bOn = false;
  BBB_Grenades.s_bOn = false;
  BBB_Smoke.s_bOn = false;
  BBB_Flares.s_bOn = false;
  BBB_ContactRelay.s_bOn = false;
  BBB_Tactic.s_bOn = false;
  m_SavedHooks = BBB.s_Hooks;
  m_Hooks = new BBB_HeldGunsTestHooks();
  BBB.s_Hooks = m_Hooks;
  m_vBase = BBB.Ground("1340 0 2770");
  ObserversSystem observers = ObserversSystem.Cast(GetGame().GetWorld().FindSystem(ObserversSystem));
  if (observers) observers.InsertObserverSP(22150, m_vBase[0], m_vBase[2], null);
  m_Team = SCR_AIGroup.Cast(Spawn(TEAM_US, m_vBase, "0 0 1"));
  m_GunA = Spawn(MG, m_vBase + Vector(-8, 0, 14), "0 0 1");
  m_GunB = Spawn(MG, m_vBase + Vector(8, 0, 14), "0 0 1");
  m_Truck = Spawn(ARMED_M2HB, m_vBase + Vector(0, 0, -25), "0 0 1");
  bool made = m_Team && m_GunA && m_GunB && m_Truck;
  Check("spawned", made, string.Format("team %1, guns %2/%3, Humvee %4", m_Team != null, m_GunA != null, m_GunB != null, m_Truck != null));
  if (!made)
  {
   Finish();
   return;
  }
  m_Hooks.m_Groups.Insert(m_Team);
  m_Hooks.m_Held.Insert(m_GunA);
  GetGame().GetCallqueue().CallLater(Prepare, 4000);
 }

 protected void Prepare()
 {
  PinDetail(m_Team);
  m_Team.ActivateAI();
  array<AIAgent> agents = {};
  m_Team.GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   PinDetail(agent);
   ChimeraCharacter man = ChimeraCharacter.Cast(agent.GetControlledEntity());
   if (!man || man == m_Team.GetLeaderEntity()) continue;
   if (!m_ManA) m_ManA = man;
   else if (!m_ManB) m_ManB = man;
  }
  BaseCompartmentSlot seatA = BBB_HeldGuns.GunSeat(m_GunA);
  BaseCompartmentSlot seatB = BBB_HeldGuns.GunSeat(m_GunB);
  bool sentA = m_ManA && seatA && BBB_StaticWeapons.CrewWeapon(m_Team, m_ManA, m_GunA, seatA);
  bool sentB = m_ManB && seatB && BBB_StaticWeapons.CrewWeapon(m_Team, m_ManB, m_GunB, seatB);
  PrintFormat("[BBB] HELDGUNSTEST crewing sent A=%1 B=%2", sentA, sentB);
  m_iPhase = 0;
  m_iWait = 0;
  GetGame().GetCallqueue().CallLater(Poll, 1000, true);
 }

 protected ChimeraCharacter Occupant(IEntity gun)
 {
  BaseCompartmentSlot seat = BBB_HeldGuns.GunSeat(gun);
  if (!seat) return null;
  return ChimeraCharacter.Cast(seat.GetOccupant());
 }

 protected bool SeatedIn(ChimeraCharacter man, IEntity gun)
 {
  if (!man || Occupant(gun) != man) return false;
  CompartmentAccessComponent access = man.GetCompartmentAccessComponent();
  return access && !access.IsGettingIn();
 }

 protected SCR_AICombatComponent CombatOf(IEntity soldier)
 {
  SCR_AIUtilityComponent utility = BBB_StaticWeapons.UtilityOf(soldier);
  if (!utility) return null;
  return utility.m_CombatComponent;
 }

 protected void Poll()
 {
  m_iWait++;
  if (m_Enemy) m_Enemy.GetGroupUtilityComponent().SetCombatMode(EAIGroupCombatMode.HOLD_FIRE);

  if (m_iPhase == 0)
  {
   bool both = SeatedIn(m_ManA, m_GunA) && SeatedIn(m_ManB, m_GunB);
   if (!both && m_iWait < 30) return;
   Check("boarded", both, string.Format("A %1, B %2 after %3 s", SeatedIn(m_ManA, m_GunA), SeatedIn(m_ManB, m_GunB), m_iWait));
   if (!both)
   {
    Finish();
    return;
   }
   m_iReleasedBefore = BBB_StaticWeapons.s_iWeaponsReleased;
   NextPhase();
   return;
  }

  if (m_iPhase == 1)
  {
   // The sector stays quiet: BBB lets B's gunner go after the quiet timeout, A's stays.
   if (!SeatedIn(m_ManA, m_GunA)) m_bALeft = true;
   bool bEmpty = Occupant(m_GunB) == null;
   if (m_iWait < BBB_StaticWeapons.s_fQuietTimeoutS + 3 || (!bEmpty && m_iWait < BBB_StaticWeapons.s_fQuietTimeoutS + 15)) return;
   Check("quiet-held-stays", !m_bALeft && SeatedIn(m_ManA, m_GunA), string.Format("A's gunner left %1 in %2 s", m_bALeft, m_iWait));
   Check("quiet-unheld-released", bEmpty && BBB_StaticWeapons.s_iWeaponsReleased > m_iReleasedBefore, string.Format("B empty %1, released %2 -> %3", bEmpty, m_iReleasedBefore, BBB_StaticWeapons.s_iWeaponsReleased));
   DismountChecks();
   // A's gunner is taken off his gun (as vanilla would): someone mans it again.
   CompartmentAccessComponent access = m_ManA.GetCompartmentAccessComponent();
   if (access) access.GetOutVehicle(EGetOutType.TELEPORT, -1, ECloseDoorAfterActions.LEAVE_OPEN, false);
   NextPhase();
   return;
  }

  if (m_iPhase == 2)
  {
   ChimeraCharacter gunner = Occupant(m_GunA);
   bool manned = gunner && SeatedIn(gunner, m_GunA);
   if (m_iWait < 3 || (!manned && m_iWait < 30)) return;
   Check("remanned", manned && BBB_HeldGuns.s_iRemounted >= 1, string.Format("manned %1 after %2 s, re-man orders %3, same man %4", manned, m_iWait, BBB_HeldGuns.s_iRemounted, gunner == m_ManA));
   // An enemy team 35 m in front of A, holding its fire.
   vector front = m_GunA.GetOrigin() + m_GunA.GetTransformAxis(2) * 35;
   m_Enemy = SCR_AIGroup.Cast(Spawn(TEAM_USSR, front, "0 0 -1"));
   if (m_Enemy)
   {
    PinDetail(m_Enemy);
    m_Enemy.ActivateAI();
    m_Enemy.GetGroupUtilityComponent().SetCombatMode(EAIGroupCombatMode.HOLD_FIRE);
   }
   NextPhase();
   return;
  }

  if (m_iPhase == 3)
  {
   ChimeraCharacter gunner = Occupant(m_GunA);
   bool fresh = gunner && BBB_HeldGuns.FreshClose(gunner);
   if (!fresh && m_iWait < 30) return;
   Check("fresh-close-lets-go", fresh && !BBB_HeldGuns.Keep(gunner, m_GunA), string.Format("gunner %1, enemy %2, fresh close %3 after %4 s", gunner != null, m_Enemy != null, fresh, m_iWait));
   Finish();
  }
 }

 // Vanilla's own dismount check for a point 80 m behind gun A (outside its arc), no enemy near.
 protected void DismountChecks()
 {
  vector behind = m_GunA.GetOrigin() - m_GunA.GetTransformAxis(2) * 80;
  SCR_AICombatComponent combat = CombatOf(m_ManA);
  float priority;
  int keptBefore = BBB_HeldGuns.s_iKept;
  bool stays = combat && !combat.DismountTurretCondition(behind, true, priority);
  BBB_HeldGuns.s_bOn = false;
  bool offOut = combat && combat.DismountTurretCondition(behind, true, priority);
  BBB_HeldGuns.s_bOn = true;
  Check("dismount-overruled", stays && offOut && BBB_HeldGuns.s_iKept > keptBefore, string.Format("stays %1, with -bbbHoldGuns 0 vanilla dismounts %2, kept %3", stays, offOut, BBB_HeldGuns.s_iKept));
  m_Hooks.m_Held.Clear();
  bool unlistedOut = combat && combat.DismountTurretCondition(behind, true, priority);
  m_Hooks.m_Held.Insert(m_Truck);
  bool truckHeld = BBB_HeldGuns.Holds(m_ManA, m_Truck) || BBB_HeldGuns.GunSeat(m_Truck) != null;
  m_Hooks.m_Held.Clear();
  m_Hooks.m_Held.Insert(m_GunA);
  Check("unlisted-vanilla", unlistedOut && !BBB_HeldGuns.Holds(m_ManB, m_GunB), string.Format("A unlisted: vanilla dismounts %1", unlistedOut));
  Check("vehicle-never-held", !truckHeld, "an armed Humvee in the list");
 }

 protected void NextPhase()
 {
  m_iPhase++;
  m_iWait = 0;
 }

 override protected void Finish()
 {
  GetGame().GetCallqueue().Remove(Poll);
  PrintFormat("[BBB] HELDGUNSTEST kept=%1 remounted=%2 static crewed=%3 released=%4", BBB_HeldGuns.s_iKept, BBB_HeldGuns.s_iRemounted, BBB_StaticWeapons.s_iWeaponsCrewed, BBB_StaticWeapons.s_iWeaponsReleased);
  if (m_SavedHooks) BBB.s_Hooks = m_SavedHooks;
  string verdict = "FAIL";
  if (m_iFailures == 0 && m_iChecks == EXPECTED) verdict = "PASS";
  PrintFormat("[ACR] VERDICT %1 test=bbbheldguns passed=%2 failed=%3 expected=%4", verdict, m_iChecks - m_iFailures, m_iFailures, EXPECTED);
  Print("[ACR] TEST COMPLETE");
 }
}

modded class SCR_BaseGameMode
{
 override protected void OnGameStart()
 {
  super.OnGameStart();
  BBB_HeldGunsTest.MaybeStartHeld();
 }
}
