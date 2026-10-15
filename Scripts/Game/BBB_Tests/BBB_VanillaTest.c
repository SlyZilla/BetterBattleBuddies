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
  if (BBB.IsStarted() && BBB_VanillaHooks.Cast(BBB.s_Hooks) && found && frames > 30) verdict = "PASS";
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
