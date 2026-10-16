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
  vector mins, maxs;
  GetGame().GetWorld().GetBoundBox(mins, maxs);
  parameters.Transform[3] = BBB.Ground((mins + maxs) * 0.5);
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

 static void TestEnd(int framesBefore, IEntity spawned)
 {
  array<SCR_AIGroup> groups = {};
  BBB.s_Hooks.GetGroups(groups);
  string name = "";
  SCR_AIGroup mine = SCR_AIGroup.Cast(spawned);
  if (mine) name = BBB.s_Hooks.NameOf(mine);
  bool found = mine && groups.Contains(mine);
  int frames = BBB.s_iFrames - framesBefore;
  PrintFormat("[BBB] VANILLATEST started=%1 hooks=%2 groups=%3 found=%4 name='%5' frames in 3 s=%6", BBB.IsStarted(), BBB.s_Hooks.Type().ToString(), groups.Count(), found, name, frames);
  string verdict = "FAIL";
  bool selectorOK = TestSelector(mine);
  if (BBB.IsStarted() && BBB_VanillaHooks.Cast(BBB.s_Hooks) && found && frames > 30 && selectorOK) verdict = "PASS";
  PrintFormat("[BBB] VANILLATEST %1", verdict);
  PrintFormat("[ACR] VERDICT %1 test=bbbvanilla started=%2 found=%3 frames=%4", verdict, BBB.IsStarted(), found, frames);
  if (spawned) SCR_EntityHelper.DeleteEntityAndChildren(spawned);
  Print("[ACR] TEST COMPLETE");
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
