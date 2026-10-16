// Standalone adapter acceptance; omitted from player builds.
class BBB_VanillaTest
{
 // -bbbVanillaTest 1 (preset "bbbvanilla"; GM_Arland, which spawns no AI by itself): BBB
 // started itself, frames run, a spawned AI group is found and named.
 static void Test()
 {
  Resource resource = Resource.Load("{657590C1EC9E27D3}Prefabs/Groups/OPFOR/Group_USSR_LightFireTeam.et");
  EntitySpawnParams parameters = new EntitySpawnParams();
  parameters.TransformMode = ETransformMode.WORLD;
  vector spawnAt = Vector(2880, 0, 1580);
  if (GetGame().GetGameMode()) spawnAt = GetGame().GetGameMode().GetOrigin();
  if (GetGame().GetWorld().GetSurfaceY(spawnAt[0], spawnAt[2]) < 2.0)
  {
   vector mins, maxs;
   GetGame().GetWorld().GetBoundBox(mins, maxs);
   spawnAt = (mins + maxs) * 0.5;
  }
  parameters.Transform[3] = BBB.Ground(spawnAt);
  IEntity spawned = GetGame().SpawnEntityPrefab(resource, GetGame().GetWorld(), parameters);
  int frames = BBB.s_iFrames;
  GetGame().GetCallqueue().CallLater(TestEnd, 3000, false, frames, spawned);
 }

 static bool TestSelector(SCR_AIGroup group)
 {
  array<EWeaponFiremodeType> m16 = {EWeaponFiremodeType.Safety, EWeaponFiremodeType.Semiauto, EWeaponFiremodeType.Burst};
  array<EWeaponFiremodeType> ak = {EWeaponFiremodeType.Safety, EWeaponFiremodeType.Auto, EWeaponFiremodeType.Semiauto};
  array<EWeaponFiremodeType> bolt = {EWeaponFiremodeType.Safety, EWeaponFiremodeType.Manual};
  array<EWeaponFiremodeType> semi = {EWeaponFiremodeType.Semiauto};
  array<EWeaponFiremodeType> duplicate = {EWeaponFiremodeType.Auto, EWeaponFiremodeType.Auto};
  bool ok = BBB_FireSelector.BestIndex(m16, EWeaponFiremodeType.Auto) == 2;
  ok = ok && BBB_FireSelector.BestIndex(ak, EWeaponFiremodeType.Burst) == 1;
  ok = ok && BBB_FireSelector.BestIndex(bolt, EWeaponFiremodeType.Semiauto) == 1;
  ok = ok && BBB_FireSelector.BestIndex(semi, EWeaponFiremodeType.Safety) == -1;
  ok = ok && BBB_FireSelector.BestIndex(m16, EWeaponFiremodeType.Safety) == 0;
  ok = ok && BBB_FireSelector.BestIndex(duplicate, EWeaponFiremodeType.Auto) == 1;
  // Also exercise the production guard with actual spawned weapons. Every
  // current fireable mode must be accepted; another available type must not.
  int weaponsChecked;
  array<AIAgent> agents = {};
  if (group) group.GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   ChimeraCharacter soldier = ChimeraCharacter.Cast(agent.GetControlledEntity());
   if (!soldier) continue;
   array<IEntity> items = {};
   soldier.GetCharacterController().GetWeaponManagerComponent().GetWeaponsList(items);
   foreach (IEntity item : items)
   {
    BaseWeaponComponent weapon = BaseWeaponComponent.Cast(item.FindComponent(BaseWeaponComponent));
    if (!weapon || !weapon.GetCurrentMuzzle()) continue;
    BaseMuzzleComponent muzzle = weapon.GetCurrentMuzzle();
    BaseFireMode current = muzzle.GetCurrentFireMode();
    if (!current || current.GetFiremodeType() == EWeaponFiremodeType.Manual) continue;
    ok = ok && BBB_FireSelector.AlreadySelected(muzzle, current.GetFiremodeType());
    array<BaseFireMode> modes = {};
    muzzle.GetFireModesList(modes);
    foreach (BaseFireMode mode : modes)
    {
     if (mode.GetFiremodeType() == current.GetFiremodeType()) continue;
     ok = ok && !BBB_FireSelector.AlreadySelected(muzzle, mode.GetFiremodeType());
    }
    weaponsChecked++;
   }
  }
  PrintFormat("[BBB] SELECTOR regression ok=%1 weapons=%2", ok, weaponsChecked);
  return ok && weaponsChecked > 0;
 }

 static bool TestThreat(SCR_AIGroup group)
 {
  BBB_Threat.Read();
  bool savedOn = BBB_Threat.s_bOn;
  BBB_Threat.s_bOn = true;

  vector origin = Vector(100, 10, 100);
  vector forward = Vector(0, 0, 1);
  vector frontFlat = Vector(100, 10, 150);
  vector frontHigh = Vector(100, 35, 150);
  vector frontLow = Vector(100, 0, 150);
  vector flank90 = Vector(150, 10, 100);
  vector rear180 = Vector(100, 10, 50);

  float multFlat = BBB_Threat.ElevationMultiplier(origin, frontFlat);
  float multHigh = BBB_Threat.ElevationMultiplier(origin, frontHigh);
  float multLow = BBB_Threat.ElevationMultiplier(origin, frontLow);
  bool elevOK = Math.AbsFloat(multFlat - 1.0) < 0.001 && multHigh > 1.35 && multLow < 0.85;

  float flankFront = BBB_Threat.FlankMultiplier(origin, forward, frontFlat);
  float flankSide = BBB_Threat.FlankMultiplier(origin, forward, flank90);
  float flankRear = BBB_Threat.FlankMultiplier(origin, forward, rear180);
  bool flankOK = Math.AbsFloat(flankFront - 1.0) < 0.001 && flankSide > 1.20 && flankRear > flankSide;

  BBB_Threat.s_bGeometry = false;
  float geomOffElev = BBB_Threat.ElevationMultiplier(origin, frontHigh);
  float geomOffFlank = BBB_Threat.FlankMultiplier(origin, forward, flank90);
  BBB_Threat.s_bGeometry = true;
  bool geomSwitchOK = Math.AbsFloat(geomOffElev - 1.0) < 0.001 && Math.AbsFloat(geomOffFlank - 1.0) < 0.001;

  float supNormal = BBB_Threat.ComputeFlybySuppression(0.0, 1, origin, forward, frontFlat, false);
  float supAmbush = BBB_Threat.ComputeFlybySuppression(0.0, 1, origin, forward, frontFlat, true);
  bool ambushOK = supNormal > 0.09 && supAmbush > supNormal + 0.15;

  float decayTuned = BBB_Threat.ComputeDecayedSuppression(0.80, 2000.0);
  BBB_Threat.s_bOn = false;
  float decayBase = BBB_Threat.ComputeDecayedSuppression(0.80, 2000.0);
  BBB_Threat.s_bOn = true;
  bool decayOK = decayTuned > decayBase && decayBase > 0.60;

  float delay0 = BBB_Threat.ComputeExplosionDelayMs(0.0, 0.0);
  float delay34 = BBB_Threat.ComputeExplosionDelayMs(34.3, 0.0);
  float delay68 = BBB_Threat.ComputeExplosionDelayMs(68.6, 0.0);
  bool acousticMathOK = Math.AbsFloat(delay0 - 160.0) < 0.5 && Math.AbsFloat(delay34 - 260.0) < 0.5 && delay68 > delay34;

  bool liveOK = false;
  bool reflexOK = false;
  array<AIAgent> agents = {};
  if (group) group.GetAgents(agents);
  if (!agents.IsEmpty() && agents[0])
  {
   SCR_AIUtilityComponent utility = SCR_AIUtilityComponent.Cast(agents[0].FindComponent(SCR_AIUtilityComponent));
   if (utility && utility.m_ThreatSystem)
   {
    float s0, sh0, in0, en0;
    utility.m_ThreatSystem.GetThreatValues(s0, sh0, in0, en0);

    utility.m_ThreatSystem.SetThreatValues(0, 0, 0, 0);
    BBB_Threat.s_bOn = false;
    utility.m_ThreatSystem.ThreatExplosion(8.0);
    float expBase, sh, inVal, en;
    utility.m_ThreatSystem.GetThreatValues(expBase, sh, inVal, en);

    utility.m_ThreatSystem.SetThreatValues(0, 0, 0, 0);
    BBB_Threat.s_bOn = true;
    utility.m_ThreatSystem.ThreatExplosion(8.0);
    float expTuned;
    utility.m_ThreatSystem.GetThreatValues(expTuned, sh, inVal, en);

    utility.m_ThreatSystem.SetThreatValues(0.80, 0, 0, 0);
    utility.m_ThreatSystem.Update(utility, 1000.0);
    float afterUpdate;
    utility.m_ThreatSystem.GetThreatValues(afterUpdate, sh, inVal, en);
    float expectedAfter = BBB_Threat.ComputeDecayedSuppression(0.80, 1000.0);

    utility.m_ThreatSystem.SetThreatValues(0.0, 0, 0, 0);
    utility.m_ThreatSystem.Update(utility, 1.0);
    utility.m_fBBBAmbushShockReady = 0;
    vector myPos = utility.m_OwnerEntity.GetOrigin();
    vector behindXZ = myPos - utility.m_OwnerEntity.GetTransformAxis(2) * 40.0;
    float behindGroundY = GetGame().GetWorld().GetSurfaceY(behindXZ[0], behindXZ[2]);
    vector behindHigh = Vector(behindXZ[0], Math.Max(myPos[1] + 18.0, behindGroundY + 18.0), behindXZ[2]);
    if (behindHigh[1] - behindGroundY > 30.0) behindHigh[1] = behindGroundY + 30.0;
    utility.m_ThreatSystem.ThreatProjectileFlyby(1);
    utility.m_SectorThreatFilter.OnShotsFired(behindHigh, 1, true);
    float flybySup;
    utility.m_ThreatSystem.GetThreatValues(flybySup, sh, inVal, en);

    utility.m_ThreatSystem.SetThreatValues(0.0, 0, 0, 0);
    utility.m_ThreatSystem.Update(utility, 1.0);
    utility.m_fBBBAmbushShockReady = 0;
    vector airborne = Vector(behindXZ[0], behindGroundY + 60.0, behindXZ[2]);
    utility.m_ThreatSystem.ThreatProjectileFlyby(1);
    utility.m_SectorThreatFilter.OnShotsFired(airborne, 1, true);
    float airborneSup;
    utility.m_ThreatSystem.GetThreatValues(airborneSup, sh, inVal, en);

    // 343 m/s acoustic explosion reflex lifecycle test on live agent
    BBB_Threat.Stop();
    IEntity soldier = utility.m_OwnerEntity;
    vector blastPos = BBB.Ground(myPos + soldier.GetTransformAxis(2) * 15.0);
    float nowMs = GetGame().GetWorld().GetWorldTime();
    utility.m_ThreatSystem.ThreatExplosion(15.0);
    utility.m_SectorThreatFilter.OnExplosion(blastPos);
    bool scheduledBeforeWave = BBB_Threat.s_iExplosionReflexScheduled == 1 && !BBB_Threat.InExplosionReflex(soldier);
    BBB_Threat.StepReflexJobs(nowMs + 600.0);
    bool appliedAfterWave = BBB_Threat.InExplosionReflex(soldier) && BBB_Threat.s_iExplosionReflexApplied == 1;
    BBB_Threat.StepReflexJobs(nowMs + 10000.0);
    bool releasedAfterHold = !BBB_Threat.InExplosionReflex(soldier);
    BBB_Threat.Stop();
    reflexOK = acousticMathOK && scheduledBeforeWave && appliedAfterWave && releasedAfterHold;

    utility.m_ThreatSystem.SetThreatValues(s0, sh0, in0, en0);
    utility.m_fBBBAmbushShockReady = 0;
    liveOK = expTuned > expBase * 2.0 && expTuned > 0.33 && Math.AbsFloat(afterUpdate - expectedAfter) < 0.01 && flybySup > 0.35 && Math.AbsFloat(airborneSup - 0.10) < 0.001;
   }
  }

  BBB_Threat.s_bOn = savedOn;
  bool ok = elevOK && flankOK && ambushOK && decayOK && liveOK && reflexOK && geomSwitchOK;
  PrintFormat("[BBB] THREAT regression ok=%1 elev=%2 flank=%3 ambush=%4 decay=%5 live=%6 reflex=%7 geomSwitch=%8", ok, elevOK, flankOK, ambushOK, decayOK, liveOK, reflexOK, geomSwitchOK);
  return ok;
 }

 static bool TestAntiArmor(SCR_AIGroup group, IEntity vehicle)
 {
  if (!group || !vehicle) return false;
  array<AIAgent> agents = {};
  group.GetAgents(agents);
  if (agents.IsEmpty()) return false;

  ChimeraCharacter soldier0 = ChimeraCharacter.Cast(agents[0].GetControlledEntity());
  if (!soldier0) return false;

  // 1. HasOperationalAT check (rifleman should not have AT)
  bool riflemanHasAT = BBB_Armour.HasOperationalAT(soldier0);

  // 2. IsArmoredThreat check (armed M1025 with M2HB turret should be true)
  bool isThreat = BBB_Armour.IsArmoredThreat(vehicle);

  // 3. Scatter execution: call CheckGroupAntiArmor with forced threat
  BBB_Armour.Stop();
  float nowMs = GetGame().GetWorld().GetWorldTime();
  BBB_Armour.CheckGroupAntiArmor(group, vehicle);
  bool scattered = BBB_Armour.s_iScattered > 0 && BBB_Armour.InAntiArmorJob(soldier0);

  // Check stance setting application
  BBB_AntiArmorJob job = BBB_Armour.JobFor(soldier0);
  bool stanceApplied = job != null && job.m_StanceSetting != null && job.m_eTargetStance != ECharacterStance.STAND;

  // 4. Timeout recovery
  BBB_Armour.StepAntiArmor(nowMs + 30000.0);
  bool recovered = !BBB_Armour.InAntiArmorJob(soldier0) && (job == null || job.m_StanceSetting == null);
  BBB_Armour.Stop();
  foreach (AIAgent a : agents)
  {
   ChimeraCharacter c = ChimeraCharacter.Cast(a.GetControlledEntity());
   if (c)
   {
    SCR_CharacterControllerComponent ctl = SCR_CharacterControllerComponent.Cast(c.GetCharacterController());
    if (ctl) SCR_AIStanceHandling.SetStance(ctl, ECharacterStance.STAND);
   }
  }

  bool ok = !riflemanHasAT && isThreat && scattered && stanceApplied && recovered;
  PrintFormat("[BBB] ANTIARMOR regression ok=%1 noAT=%2 isThreat=%3 lowStance=%4 stance=%5 recovered=%6", ok, !riflemanHasAT, isThreat, scattered, stanceApplied, recovered);
  return ok;
 }

 static bool TestRearm(SCR_AIGroup group)
 {
  if (!group) return false;
  array<AIAgent> agents = {};
  group.GetAgents(agents);
  if (agents.IsEmpty()) return false;
  ChimeraCharacter testMan = null;
  if (agents.Count() > 3)
   testMan = ChimeraCharacter.Cast(agents[3].GetControlledEntity());
  if (!testMan)
   testMan = ChimeraCharacter.Cast(agents[0].GetControlledEntity());
  if (!testMan) return false;

  BaseMuzzleComponent muzzle;
  int magCount;
  BBB_Rearm.NeedsAmmo(testMan, muzzle, magCount);

  // Drop carried magazines so the soldier actually needs ammo
  SCR_InventoryStorageManagerComponent sInv = SCR_InventoryStorageManagerComponent.Cast(testMan.FindComponent(SCR_InventoryStorageManagerComponent));
  if (sInv && muzzle && muzzle.GetMagazineWell())
  {
   array<IEntity> carried = {};
   sInv.GetItems(carried);
   string targetWell = muzzle.GetMagazineWell().Type().ToString();
   int removed = 0;
   foreach (IEntity item : carried)
   {
    if (!item) continue;
    MagazineComponent mag = MagazineComponent.Cast(item.FindComponent(MagazineComponent));
    if (mag && mag.GetMagazineWell() && mag.GetMagazineWell().Type().ToString() == targetWell)
    {
     sInv.TryRemoveItemFromInventory(item);
     SCR_EntityHelper.DeleteEntityAndChildren(item);
     removed++;
     if (removed >= 5) break;
    }
   }
  }

  bool needed = BBB_Rearm.NeedsAmmo(testMan, muzzle, magCount);

  Resource res = Resource.Load("{DCB41B3746FDD1BE}Prefabs/Characters/Factions/OPFOR/USSR_Army/Character_USSR_Rifleman.et");
  IEntity corpse = null;
  if (res.IsValid())
  {
   EntitySpawnParams params = new EntitySpawnParams();
   params.TransformMode = ETransformMode.WORLD;
   params.Transform[3] = BBB.Ground(testMan.GetOrigin() + Vector(1.5, 0, 0));
   corpse = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
  }

  bool matchingMag = false;
  bool looted = false;
  if (corpse)
  {
   SCR_DamageManagerComponent dmg = SCR_DamageManagerComponent.GetDamageManager(corpse);
   if (dmg) dmg.Kill(Instigator.CreateInstigator(null));

   array<IEntity> mags = {};
   matchingMag = BBB_Rearm.CorpseHasMatchingMag(corpse, muzzle, mags);

   int beforeLooted = BBB_Rearm.s_iMagazinesLooted;
   BBB_Rearm.LootFromCorpse(testMan, corpse, true, muzzle, false, group);
   looted = BBB_Rearm.s_iMagazinesLooted > beforeLooted;
   SCR_EntityHelper.DeleteEntityAndChildren(corpse);
  }

  bool needsATBefore = BBB_Rearm.NeedsAntiArmor(testMan, group);
  Resource vRes = Resource.Load("{3EA6F47D95867114}Prefabs/Vehicles/Wheeled/M998/M1025_armed_M2HB.et");
  IEntity testVeh = null;
  bool needsATWithVeh = false;
  if (vRes.IsValid())
  {
   EntitySpawnParams vParams = new EntitySpawnParams();
   vParams.TransformMode = ETransformMode.WORLD;
   vParams.Transform[3] = BBB.Ground(testMan.GetOrigin() + Vector(25, 0, 25));
   testVeh = GetGame().SpawnEntityPrefab(vRes, GetGame().GetWorld(), vParams);
  }
  SCR_AIGroupUtilityComponent gUtil = SCR_AIGroupUtilityComponent.Cast(group.FindComponent(SCR_AIGroupUtilityComponent));
  if (gUtil && gUtil.m_Perception && testVeh)
  {
   gUtil.m_Perception.m_aTargetEntities.Insert(testVeh);
   needsATWithVeh = BBB_Rearm.NeedsAntiArmor(testMan, group);
   gUtil.m_Perception.m_aTargetEntities.RemoveItem(testVeh);
   SCR_EntityHelper.DeleteEntityAndChildren(testVeh);
  }
  bool atTriggerOK = !needsATBefore && needsATWithVeh;

  SCR_AIUtilityComponent testUtil = SCR_AIUtilityComponent.Cast(testMan.FindComponent(SCR_AIUtilityComponent));
  bool cancelOK = true;
  if (testUtil)
  {
   SCR_AIMoveIndividuallyBehavior dummyAct = new SCR_AIMoveIndividuallyBehavior(testUtil, null, testMan.GetOrigin(), priority: 50);
   BBB_Rearm.SetRearmActionForTest(testMan, dummyAct, testMan);
   BBB_Rearm.CancelRearmAction(testMan);
   cancelOK = !BBB_Rearm.InRearmAction(testMan) && dummyAct.GetActionState() == EAIActionState.FAILED;
  }

  bool ok = muzzle != null && needed && matchingMag && looted && atTriggerOK && cancelOK;
  PrintFormat("[BBB] REARM regression ok=%1 needed=%2 matchingMag=%3 looted=%4 atTrigger=%5 cancel=%6", ok, needed, matchingMag, looted, atTriggerOK, cancelOK);
  return ok;
 }

 static bool s_bSelectorOK;
 static bool s_bThreatOK;
 static bool s_bAntiArmorOK;
 static bool s_bRearmOK;
 static int s_iFramesCounted;
 static IEntity s_SpawnedGroup;
 static IEntity s_SpawnedTruck;
 static BaseCompartmentSlot s_TurretSlot;
 static BaseCompartmentSlot s_CargoSlot1;
 static ChimeraCharacter s_Man0;
 static ChimeraCharacter s_Man1;
 static ChimeraCharacter s_Man2;
 static int s_iBeforeReplacements;
 static bool s_bSetupOK;
 static bool s_bAmmoOK;
 static bool s_bPromote1OK;

 static void TestGunnerStep2(SCR_AIGroup mine)
 {
  s_iBeforeReplacements = BBB_VehicleHelpers.s_iGunnerReplacements;
  bool prom1 = BBB_VehicleHelpers.TryReplaceGunner(s_SpawnedTruck, mine);
  GetGame().GetCallqueue().CallLater(TestGunnerStep3, 1200, false, mine, prom1);
 }

 static void TestGunnerStep3(SCR_AIGroup mine, bool prom1)
 {
  IEntity occNow = s_TurretSlot.GetOccupant();
  s_bPromote1OK = prom1 && occNow == s_Man1 && BBB_VehicleHelpers.HasGunner(s_SpawnedTruck) && BBB_VehicleHelpers.s_iGunnerReplacements == s_iBeforeReplacements + 1;
  SCR_CharacterDamageManagerComponent dmg1 = SCR_CharacterDamageManagerComponent.Cast(s_Man1.GetDamageManager());
  if (dmg1) dmg1.Kill(Instigator.CreateInstigator(null));

  GetGame().GetCallqueue().CallLater(TestGunnerStep4, 500, false, mine);
 }

 static void TestGunnerStep4(SCR_AIGroup mine)
 {
  bool recognizedDown = !BBB_VehicleHelpers.HasGunner(s_SpawnedTruck);
  bool prom2 = BBB_VehicleHelpers.TryReplaceGunner(s_SpawnedTruck, mine);
  GetGame().GetCallqueue().CallLater(FinishVanillaTest, 1600, false, mine, recognizedDown, prom2);
 }

 static void FinishVanillaTest(SCR_AIGroup mine, bool recognizedDown, bool prom2)
 {
  IEntity occNow = s_TurretSlot.GetOccupant();
  bool downReplaceOK = recognizedDown && prom2 && occNow == s_Man2 && BBB_VehicleHelpers.HasGunner(s_SpawnedTruck) && BBB_VehicleHelpers.s_iGunnerReplacements == s_iBeforeReplacements + 2;
  bool turretOK = s_bSetupOK && s_bAmmoOK && s_bPromote1OK && downReplaceOK;
  PrintFormat("[BBB] TURRET regression ok=%1 setup=%2 ammo=%3 emptyPromote=%4 downReplace=%5", turretOK, s_bSetupOK, s_bAmmoOK, s_bPromote1OK, downReplaceOK);

  bool found = mine != null;
  string verdict = "FAIL";
  if (BBB.IsStarted() && BBB_VanillaHooks.Cast(BBB.s_Hooks) && found && s_iFramesCounted > 30 && s_bSelectorOK && s_bThreatOK && turretOK && s_bAntiArmorOK && s_bRearmOK) verdict = "PASS";
  PrintFormat("[BBB] VANILLATEST %1", verdict);
  PrintFormat("[ACR] VERDICT %1 test=bbbvanilla started=%2 found=%3 frames=%4", verdict, BBB.IsStarted(), found, s_iFramesCounted);
  if (s_SpawnedTruck) SCR_EntityHelper.DeleteEntityAndChildren(s_SpawnedTruck);
  if (s_SpawnedGroup) SCR_EntityHelper.DeleteEntityAndChildren(s_SpawnedGroup);
  Print("[ACR] TEST COMPLETE");
 }

 static void TestEnd(int framesBefore, IEntity spawned)
 {
  array<SCR_AIGroup> groups = {};
  BBB.s_Hooks.GetGroups(groups);
  string name = "";
  SCR_AIGroup mine = SCR_AIGroup.Cast(spawned);
  if (mine) name = BBB.s_Hooks.NameOf(mine);
  bool found = mine && groups.Contains(mine);
  s_iFramesCounted = BBB.s_iFrames - framesBefore;
  s_SpawnedGroup = spawned;
  PrintFormat("[BBB] VANILLATEST started=%1 hooks=%2 groups=%3 found=%4 name='%5' frames in 3 s=%6", BBB.IsStarted(), BBB.s_Hooks.Type().ToString(), groups.Count(), found, name, s_iFramesCounted);

  s_bSelectorOK = TestSelector(mine);
  s_bThreatOK = TestThreat(mine);
  s_bRearmOK = TestRearm(mine);

  array<AIAgent> agents = {};
  if (mine) mine.GetAgents(agents);
  if (agents.Count() >= 3)
  {
   s_Man0 = ChimeraCharacter.Cast(agents[0].GetControlledEntity());
   s_Man1 = ChimeraCharacter.Cast(agents[1].GetControlledEntity());
   s_Man2 = ChimeraCharacter.Cast(agents[2].GetControlledEntity());
  }

  Resource res = Resource.Load("{3EA6F47D95867114}Prefabs/Vehicles/Wheeled/M998/M1025_armed_M2HB.et");
  if (res.IsValid() && s_Man0 && s_Man1 && s_Man2)
  {
   EntitySpawnParams params = new EntitySpawnParams();
   params.TransformMode = ETransformMode.WORLD;
   params.Transform[3] = BBB.Ground(s_Man0.GetOrigin() + Vector(8, 0, 8));
   s_SpawnedTruck = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
  }

  SCR_BaseCompartmentManagerComponent comps;
  array<BaseCompartmentSlot> pilots = {};
  array<BaseCompartmentSlot> turrets = {};
  array<BaseCompartmentSlot> cargos = {};
  if (s_SpawnedTruck)
  {
   BBB_VehicleHelpers.GetVehicleCompartments(s_SpawnedTruck, pilots, ECompartmentType.PILOT);
   BBB_VehicleHelpers.GetVehicleCompartments(s_SpawnedTruck, turrets, ECompartmentType.TURRET);
   BBB_VehicleHelpers.GetVehicleCompartments(s_SpawnedTruck, cargos, ECompartmentType.CARGO);
  }

  s_bSetupOK = s_SpawnedTruck != null && !pilots.IsEmpty() && !turrets.IsEmpty() && cargos.Count() >= 2;
  s_bAmmoOK = false;
  s_bPromote1OK = false;

  if (s_bSetupOK)
  {
   s_bAntiArmorOK = TestAntiArmor(mine, s_SpawnedTruck);

   s_TurretSlot = turrets[0];
   s_CargoSlot1 = cargos[1];
   s_bAmmoOK = BBB_VehicleHelpers.HasTurret(s_SpawnedTruck) && !BBB_VehicleHelpers.HasGunner(s_SpawnedTruck) && BBB_VehicleHelpers.TurretHasAmmo(s_TurretSlot);

   s_Man0.GetCompartmentAccessComponent().GetInVehicle(pilots[0].GetOwner(), pilots[0], true, -1, ECloseDoorAfterActions.CLOSE_DOOR, false);
   s_Man1.GetCompartmentAccessComponent().GetInVehicle(cargos[0].GetOwner(), cargos[0], true, -1, ECloseDoorAfterActions.CLOSE_DOOR, false);
   s_Man2.GetCompartmentAccessComponent().GetInVehicle(cargos[1].GetOwner(), cargos[1], true, -1, ECloseDoorAfterActions.CLOSE_DOOR, false);

   GetGame().GetCallqueue().CallLater(TestGunnerStep2, 1000, false, mine);
   return;
  }

  FinishVanillaTest(mine, false, false);
 }

}

modded class SCR_BaseGameMode
{
 protected override void OnGameStart()
 {
  super.OnGameStart();
  if (Replication.IsServer() && (System.IsCLIParam("bbbVanillaTest") || System.IsCLIParam("acrBbbVanillaTest")))
   GetGame().GetCallqueue().CallLater(BBB_VanillaTest.Test, 25000);
 }
}
