class BBB_FlareTestHooks : BBB_Hooks
{
 ref array<SCR_AIGroup> m_Groups = {};
 bool m_Veto = true;
 override void GetGroups(notnull array<SCR_AIGroup> groups) { groups.Copy(m_Groups); }
 override bool AllowFlares(SCR_AIGroup group) { return !m_Veto; }
}

// Real sight acquisition, real inventory consumption, and live queue cancellation.
class BBB_FlareTest : BBB_AimTest
{
 static ref BBB_FlareTest s_FlareTest;
 ref BBB_FlareTestHooks m_Hooks;
 ref BBB_Hooks m_SavedHooks;
 SCR_AIGroup m_Team;
 SCR_AIGroup m_Enemy;
 TimeAndWeatherManagerEntity m_Weather;
 BaseMagazineComponent m_Magazine;
 ref BBB_FlareJob m_Job;
 static const int EXPECTED = 31;
 bool m_CheckedHost;
 int m_Checks;
 int m_Failures;
 int m_Phase;
 int m_Wait;
 int m_Ammo;
 bool m_Handheld;
 IEntity m_Specialist;
 ref SCR_AICharacterStanceSetting m_ProneSetting;
 SCR_AIUtilityComponent m_ProneUtility;
 float m_SavedTime;
 bool m_Airborne;
 bool m_Baseline;
 ref array<IEntity> m_PreviousFlares = {};

 static void Start()
 {
  if (!Replication.IsServer() || !System.IsCLIParam("bbbFlareTest")) return;
  s_FlareTest = new BBB_FlareTest();
  GetGame().GetCallqueue().CallLater(s_FlareTest.Setup, 15000);
 }

 void Check(string name, bool ok)
 {
  m_Checks++;
  if (!ok) m_Failures++;
  PrintFormat("[BBB] FLARETEST %1 ok=%2", name, ok);
 }

 void Setup()
 {
  m_Weather = ChimeraWorld.CastFrom(GetGame().GetWorld()).GetTimeAndWeatherManager();
  Check("weather", m_Weather != null);
  if (!m_Weather) { FinishFlare(); return; }
  m_SavedTime = m_Weather.GetTimeOfTheDay();
  m_Weather.SetHoursMinutesSeconds(12, 0, 0);
  BBB_Flares.Read();
  Check("default-on", BBB_Flares.s_bOn);
  BBB_Flares.s_bOn = true;
  m_SavedHooks = BBB.s_Hooks;
  m_Hooks = new BBB_FlareTestHooks();
  BBB.s_Hooks = m_Hooks;
  vector point = BBB.Ground("1340 0 2770");
  ObserversSystem observers = ObserversSystem.Cast(GetGame().GetWorld().FindSystem(ObserversSystem));
  if (observers) observers.InsertObserverSP(22147, point[0], point[2], null);
  m_Team = SCR_AIGroup.Cast(Spawn("{BBB0F1A200000010}Prefabs/BBB_Tests/Group_FlareGrenadier.et", point, "0.5 0 1"));
  m_Enemy = SCR_AIGroup.Cast(Spawn(TEAM_USSR, point + Vector(45, 0, 90), "-0.5 0 -1"));
  Check("groups", m_Team && m_Enemy);
  if (!m_Team || !m_Enemy) { FinishFlare(); return; }
  m_Hooks.m_Groups.Insert(m_Team);
  GetGame().GetCallqueue().CallLater(Prepare, 4000);
 }

 void PrepareGroup(SCR_AIGroup group)
 {
  PinDetail(group);
  group.ActivateAI();
  EAIGroupCombatMode mode = EAIGroupCombatMode.HOLD_FIRE;
  if (group == m_Team) mode = EAIGroupCombatMode.FIRE_AT_WILL;
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
   AIControlComponent bodyControl = AIControlComponent.Cast(body.FindComponent(AIControlComponent));
   if (bodyControl) bodyControl.ActivateAI();
   body.GetDamageManager().EnableDamageHandling(false);
   if (group == m_Enemy)
   {
    body.GetCharacterController().SetDisableWeaponControls(true);
    body.GetCharacterController().SetDisableMovementControls(true);
   }
   SCR_AIUtilityComponent utility = SCR_AIUtilityComponent.Cast(agent.FindComponent(SCR_AIUtilityComponent));
   if (utility && utility.m_PerceptionComponent) utility.m_PerceptionComponent.SetPerceptionFactor(50);
   if (group == m_Enemy && utility && utility.m_SettingsComponent)
    utility.m_SettingsComponent.AddCharacterSetting(SCR_AICharacterStanceSetting.Create(SCR_EAISettingOrigin.BEHAVIOR, SCR_EAIBehaviorCause.ALWAYS, ECharacterStance.STAND), false);
  }
 }

 void Prepare()
 {
  m_Specialist = m_Team.GetLeaderEntity();
  AIControlComponent specialistControl;
  if (m_Specialist) specialistControl = AIControlComponent.Cast(m_Specialist.FindComponent(AIControlComponent));
  bool specialistReady = specialistControl && specialistControl.GetAIAgent() && specialistControl.GetAIAgent().GetParentGroup() == m_Team && m_Team.GetAgentsCount() == 1;
  Check("grenadier-agent-ready", specialistReady);
  if (!specialistReady) { FinishFlare(); return; }
  PrepareGroup(m_Team);
  PrepareGroup(m_Enemy);
  array<AIAgent> agents = {};
  m_Team.GetAgents(agents);
  bool inserted;
  foreach (AIAgent agent : agents)
  {
   IEntity body = agent.GetControlledEntity();
   if (!body) continue;
   SCR_InventoryStorageManagerComponent inventory = SCR_InventoryStorageManagerComponent.Cast(body.FindComponent(SCR_InventoryStorageManagerComponent));
   IEntity ammo = Spawn("{98DB57ECEDC81CC2}Prefabs/Weapons/Ammo/Ammo_Flare_40mm_M583A1_White.et", body.GetOrigin(), "0 0 1");
   if (inventory && ammo && inventory.TryInsertItem(ammo)) inserted = true;
  }
  Check("launcher-ammo-inserted", inserted);
  GetGame().GetCallqueue().CallLater(Poll, 1000, true);
 }

 void Poll()
 {
  m_Wait++;
  if ((m_Phase != 4 && m_Wait > 45) || m_Wait > 65) { PrintFormat("[BBB] FLARETEST timed out phase=%1 handheld=%2", m_Phase, m_Handheld); Check("phase-timeout", false); FinishFlare(); return; }
  if (m_Phase == 0)
  {
   vector target;
   if (!BBB_Flares.KnownPosition(m_Team, target)) return;
   Check("real-perceived-contact", true);
   Check("daylight-veto", !SCR_AIWorldHandling.IsLowLightEnvironment() && !BBB_Flares.TryIlluminate(m_Team));
   m_Weather.SetHoursMinutesSeconds(0, 0, 0);
   m_Phase = 1;
   m_Wait = 0;
   return;
  }
  if (m_Phase == 1)
  {
   if (!SCR_AIWorldHandling.IsLowLightEnvironment()) return;
   if (!m_CheckedHost)
   {
    m_Hooks.m_Veto = true;
    Check("host-veto", !BBB_Flares.TryIlluminate(m_Team));
    m_Hooks.m_Veto = false;
    BBB_Flares.s_bOn = false;
    Check("disabled-baseline", !BBB_Flares.TryIlluminate(m_Team));
    BBB_Flares.s_bOn = true;
    m_Team.GetGroupUtilityComponent().SetCombatMode(EAIGroupCombatMode.HOLD_FIRE);
    Check("hold-fire-veto", !BBB_Flares.TryIlluminate(m_Team));
    m_Team.GetGroupUtilityComponent().SetCombatMode(EAIGroupCombatMode.FIRE_AT_WILL);
    m_Team.GetGroupUtilityComponent().EvaluateCombatMode();
    m_CheckedHost = true;
   }
   m_Hooks.m_Veto = false;
   bool queued = BBB_Flares.TryIlluminate(m_Team);
   if (!queued) return;
   Check("native-action-queued", BBB_Flares.s_Jobs.Count() == 1);
   m_Job = BBB_Flares.s_Jobs[0];
   array<ref AIActionBase> actions = {};
   m_Job.m_Utility.FindActionsOfInheritedType(BBB_FlareBehavior, actions);
   Check("queue-contains-action", actions.Contains(m_Job.m_Action));
   Check("squad-cooldown", !BBB_Flares.GroupReady(m_Team, GetGame().GetWorld().GetWorldTime()));
   vector contact;
   bool known = BBB_Flares.KnownPosition(m_Team, contact);
   Check("shared-area-hold", known && !BBB_Flares.AreaReady(contact + Vector(20, 0, 0), GetGame().GetWorld().GetWorldTime()));
   Check("different-area-ready", known && BBB_Flares.AreaReady(contact + Vector(250, 0, 0), GetGame().GetWorld().GetWorldTime()));
   SCR_AIWaypoint order = SCR_AIWaypoint.Cast(Spawn("{750A8D1695BD6998}Prefabs/AI/Waypoints/AIWaypoint_Move.et", m_Team.GetLeaderEntity().GetOrigin() + Vector(10, 0, 0), "0 0 1"));
   Check("order-fixture", order != null);
   if (!order) { FinishFlare(); return; }
   order.SetPriorityLevel(SCR_AIActionBase.PRIORITY_LEVEL_PLAYER);
   m_Team.AddWaypoint(order);
   BBB_Flares.Maintain();
   Check("order-aborts-owned-action", m_Job.m_Action.GetActionState() == EAIActionState.FAILED && BBB_Flares.s_Jobs.IsEmpty());
   Check("order-preserved", m_Team.GetCurrentWaypoint() == order);
   Check("cancel-keeps-retry-hold", !BBB_Flares.GroupReady(m_Team, GetGame().GetWorld().GetWorldTime()));
   m_Team.RemoveWaypoint(order);
   order = null;
   BBB_Flares.Stop();
   m_Hooks.m_Veto = true;
   StartLauncher();
   return;
  }
  if (m_Phase == 2)
  {
   vector fresh;
   BaseTarget freshContact;
   if (m_Wait < 3 || !BBB_Flares.KnownContact(m_Team, fresh, freshContact) || freshContact.GetTimeSinceSeen() > 1.0) return;
   if (m_Handheld)
   {
    m_ProneSetting = SCR_AICharacterStanceSetting.Create(SCR_EAISettingOrigin.BEHAVIOR, SCR_EAIBehaviorCause.ALWAYS, ECharacterStance.PRONE);
    if (m_ProneUtility.m_SettingsComponent) m_ProneUtility.m_SettingsComponent.AddCharacterSetting(m_ProneSetting, false);
    SCR_AIStanceHandling.SetStance(ChimeraCharacter.Cast(m_Specialist).GetCharacterController(), ECharacterStance.PRONE);
    m_Phase = 5;
    m_Wait = 0;
    return;
   }
   m_Weather.SetHoursMinutesSeconds(0, 0, 0);
   m_Phase = 3;
   m_Wait = 0;
   return;
  }
  if (m_Phase == 5)
  {
   if (ChimeraCharacter.Cast(m_Specialist).GetCharacterController().GetStance() != ECharacterStance.PRONE) return;
   m_Weather.SetHoursMinutesSeconds(0, 0, 0);
   m_Phase = 3;
   m_Wait = 0;
   return;
  }
  if (m_Phase == 3)
  {
   if (!SCR_AIWorldHandling.IsLowLightEnvironment()) return;
   m_Hooks.m_Veto = false;
   if (!BBB_Flares.TryIlluminate(m_Team)) return;
   m_Job = BBB_Flares.s_Jobs[0];
   m_Magazine = m_Job.m_Magazine;
   m_Ammo = m_Magazine.GetAmmoCount();
   Check("live-dispatch", m_Ammo > 0 && m_Job.m_Action != null);
   Check("correct-equipment-kind", m_Job.m_Handheld == m_Handheld);
   if (m_Handheld) Check("handheld-prone", ChimeraCharacter.Cast(m_Job.m_Utility.m_OwnerEntity).GetCharacterController().GetStance() == ECharacterStance.PRONE);
   m_PreviousFlares.Clear();
   m_Airborne = false;
   m_Baseline = true;
   ScanFlares();
   m_Baseline = false;
   m_Phase = 4;
   m_Wait = 0;
   return;
  }
  if (m_Phase == 4)
  {
   int ammo = -1;
   if (m_Magazine) ammo = m_Magazine.GetAmmoCount();
   PrintFormat("[BBB] FLARETEST observe handheld=%1 ammo=%2 initial=%3 state=%4", m_Handheld, ammo, m_Ammo, m_Job.m_Action.GetActionState());
   PrintFormat("[BBB] FLARETEST action allowed=%1 recent=%2 removed=%3 current=%4 using=%5 seen=%6 detected=%7", BBB_Flares.Allowed(m_Job.m_Utility), BBB_Flares.RecentTarget(m_Job.m_Contact), m_Job.m_Action.GetRemoveAction(), m_Job.m_Utility.GetCurrentBehavior(), m_Job.m_Action.m_bUsingHandheld, m_Job.m_Contact.GetTimeSinceSeen(), m_Job.m_Contact.GetTimeSinceDetected());
   ScanFlares();
   bool consumed = !m_Magazine || ammo < m_Ammo;
   // Include the native parachute descent/burn time after the use animation.
   if ((consumed && m_Airborne) || m_Wait >= 60)
   {
    Check("physical-flare-ammo-consumed", consumed);
    Check("new-airborne-flare", m_Airborne);
    if (!consumed || !m_Airborne) { FinishFlare(); return; }
    if (m_Handheld) { FinishFlare(); return; }
    StartHandheld();
   }
  }
 }

 void ScanFlares()
 {
  if (m_Team && m_Team.GetLeaderEntity()) GetGame().GetWorld().QueryEntitiesBySphere(m_Team.GetLeaderEntity().GetOrigin(), 450, CollectFlare);
 }

 bool CollectFlare(IEntity entity)
 {
  if (!entity || !entity.GetPrefabData()) return true;
  string name = entity.GetPrefabData().GetPrefabName();
  if (!name.Contains("Flare")) return true;
  if (m_Baseline) { m_PreviousFlares.Insert(entity); return true; }
  vector point = entity.GetOrigin();
  float height = point[1] - GetGame().GetWorld().GetSurfaceY(point[0], point[2]);
  BaseProjectileComponent projectile = BaseProjectileComponent.Cast(entity.FindComponent(BaseProjectileComponent));
  Instigator instigator;
  if (projectile) instigator = projectile.GetInstigator();
  bool mine = instigator && instigator.GetInstigatorEntity() == m_Job.m_Utility.m_OwnerEntity;
  float contactDistance = vector.Distance(point, m_Job.m_ContactPosition);
  if (m_Handheld && mine && name.Contains("FlareEffect") && !m_PreviousFlares.Contains(entity))
   PrintFormat("[BBB] FLARETEST candidate=%1 mine=%2 height=%3 contactDistance=%4", name, mine, height, contactDistance);
  if (mine && name.Contains("FlareEffect") && height > 10 && contactDistance < 75 && !m_PreviousFlares.Contains(entity))
  {
   m_Airborne = true;
   PrintFormat("[BBB] FLARETEST illumination prefab=%1 height=%2 contactDistance=%3", name, height, contactDistance);
  }
  return true;
 }

 void StartLauncher()
 {
  m_Weather.SetHoursMinutesSeconds(12, 0, 0);
  m_Team = SCR_AIGroup.Cast(Spawn("{BBB0F1A200000010}Prefabs/BBB_Tests/Group_FlareGrenadier.et", BBB.Ground("1330 0 2770"), "0.5 0 1"));
  if (!m_Team) { Check("fresh-launcher-fixture", false); FinishFlare(); return; }
  m_Hooks.m_Groups.Clear();
  m_Hooks.m_Groups.Insert(m_Team);
  GetGame().GetCallqueue().Remove(Poll);
  GetGame().GetCallqueue().CallLater(PrepareLauncher, 4000);
 }

 void PrepareLauncher()
 {
  PrepareGroup(m_Team);
  m_Phase = 2;
  m_Wait = 0;
  GetGame().GetCallqueue().CallLater(Poll, 1000, true);
 }

 void StartHandheld()
 {
  BBB_Flares.Stop();
  m_Hooks.m_Veto = true;
  m_Handheld = true;
  // A rifle-only group prevents the launcher selector masking handheld support.
  SCR_AIGroup replacement = SCR_AIGroup.Cast(Spawn("{BBB0F1A200000011}Prefabs/BBB_Tests/Group_FlareRifleman.et", m_Team.GetLeaderEntity().GetOrigin() + Vector(8, 0, 0), "0.5 0 1"));
  m_Team = replacement;
  m_Hooks.m_Groups.Clear();
  m_Hooks.m_Groups.Insert(m_Team);
  GetGame().GetCallqueue().Remove(Poll);
  GetGame().GetCallqueue().CallLater(PrepareHandheld, 4000);
 }

 void PrepareHandheld()
 {
  if (m_Team) m_Specialist = m_Team.GetLeaderEntity();
  AIControlComponent control;
  if (m_Specialist) control = AIControlComponent.Cast(m_Specialist.FindComponent(AIControlComponent));
  bool ready = m_Team && control && control.GetAIAgent() && control.GetAIAgent().GetParentGroup() == m_Team && m_Team.GetAgentsCount() == 1;
  Check("handheld-rifleman", ready);
  if (!ready) { FinishFlare(); return; }
  PrepareGroup(m_Team);
  SCR_AIUtilityComponent handheldUtility = SCR_AIUtilityComponent.Cast(control.GetAIAgent().FindComponent(SCR_AIUtilityComponent));
  m_ProneUtility = handheldUtility;
  if (!handheldUtility) { Check("handheld-utility", false); FinishFlare(); return; }
  SCR_InventoryStorageManagerComponent inventory = SCR_InventoryStorageManagerComponent.Cast(m_Specialist.FindComponent(SCR_InventoryStorageManagerComponent));
  if (!inventory) { Check("handheld-inventory-fixture", false); FinishFlare(); return; }
  array<IEntity> items = {};
  inventory.GetItems(items);
  foreach (IEntity item : items)
  {
   if (!item || !item.FindComponent(SCR_FlareAnimationComponent)) continue;
   if (!inventory.TryRemoveItemFromInventory(item)) { Check("remove-fixture-flare", false); FinishFlare(); return; }
   SCR_EntityHelper.DeleteEntityAndChildren(item);
  }
  GetGame().GetCallqueue().CallLater(EquipHandheld, 1000);
 }

 void EquipHandheld()
 {
  BaseWeaponComponent absentWeapon;
  BaseMagazineComponent absentMagazine;
  int absentMuzzle;
  Check("no-flare-equipment", !BBB_Flares.FindEquipment(m_ProneUtility, absentWeapon, absentMagazine, absentMuzzle));
  IEntity flare = Spawn("{EC9BDA3D9DDD8795}Prefabs/Weapons/Flares/FlareStarParachute_M127A1_white.et", m_Specialist.GetOrigin(), "0 0 1");
  SCR_InventoryStorageManagerComponent inventory = SCR_InventoryStorageManagerComponent.Cast(m_Specialist.FindComponent(SCR_InventoryStorageManagerComponent));
  Check("handheld-inventory", flare && inventory && inventory.TryInsertItem(flare));
  GetGame().GetCallqueue().CallLater(Poll, 1000, true);
  m_Weather.SetHoursMinutesSeconds(12, 0, 0);
  m_Phase = 2;
  m_Wait = 0;
 }

 void FinishFlare()
 {
  GetGame().GetCallqueue().Remove(Poll);
  BBB_Flares.Stop();
  if (m_ProneUtility && m_ProneUtility.m_SettingsComponent && m_ProneSetting) m_ProneUtility.m_SettingsComponent.RemoveSetting(m_ProneSetting);
  BBB_Flares.s_bOn = false;
  if (m_SavedHooks) BBB.s_Hooks = m_SavedHooks;
  if (m_Weather) m_Weather.SetHoursMinutesSeconds(Math.Floor(m_SavedTime), 0, 0);
  string verdict = "FAIL";
  if (m_Failures == 0 && m_Checks == EXPECTED && m_Handheld && m_Airborne) verdict = "PASS";
  PrintFormat("[ACR] VERDICT %1 test=bbbflares passed=%2 failed=%3 expected=%4", verdict, m_Checks - m_Failures, m_Failures, EXPECTED);
  Print("[ACR] TEST COMPLETE");
 }
}

modded class SCR_BaseGameMode
{
 override protected void OnGameStart()
 {
  super.OnGameStart();
  BBB_FlareTest.Start();
 }
}
