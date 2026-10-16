// Better Battle Buddies — Opportunistic Combat Rearm / Corpse Looting
// Ports bCombat (fnc_combat_gear / fnc_combat_ammo), LAMBS_Danger (fnc_doCheckBody),
// and Vcom AI (fn_RearmSelf) to Reforger. When AI soldiers run low on ammunition
// (< 2 magazines) or face armored threats without operational AT, they search nearby
// corpses and supply sources within 25 m to scavenge ammunition and AT weapons.

class BBB_Rearm
{
 static bool s_bOn = true;
 static bool s_bRead;
 static int s_iRearmCount;
 static int s_iMagazinesLooted;
 static int s_iLaunchersLooted;

 static float s_fSearchRadius = 25.0;
 static const float LOOT_REACH_M = 3.5;
 static const float REARM_COOLDOWN_MS = 15000.0;
 static const int TARGET_MAGAZINE_COUNT = 4;
 static const int CRITICAL_MAGAZINE_COUNT = 2;

 protected static ref map<IEntity, float> s_mNextRearmCheck = new map<IEntity, float>();
 protected static ref map<IEntity, IEntity> s_mTargetCorpse = new map<IEntity, IEntity>();
 protected static ref map<ChimeraCharacter, ref SCR_AIMoveIndividuallyBehavior> s_mRearmAction = new map<ChimeraCharacter, ref SCR_AIMoveIndividuallyBehavior>();
 protected static ref array<IEntity> s_aQueryCorpseHits = {};

 static void Read()
 {
  if (s_bRead) return;
  s_bRead = true;
  string val;
  if (System.GetCLIParam("bbbRearm", val) && val == "0") s_bOn = false;
  if (System.GetCLIParam("bbbRearmRadius", val)) s_fSearchRadius = Math.Clamp(val.ToFloat(), 5.0, 100.0);
  PrintFormat("[BBB] REARM on=%1 radius=%2", s_bOn, s_fSearchRadius);
 }

 static bool NeedsAmmo(ChimeraCharacter soldier, out BaseMuzzleComponent outMuzzle, out int outCount)
 {
  outMuzzle = null;
  outCount = 0;
  if (!soldier) return false;
  SCR_InventoryStorageManagerComponent inv = SCR_InventoryStorageManagerComponent.Cast(soldier.FindComponent(SCR_InventoryStorageManagerComponent));
  BaseWeaponManagerComponent wm = BaseWeaponManagerComponent.Cast(soldier.FindComponent(BaseWeaponManagerComponent));
  if (!inv || !wm) return false;
  BaseWeaponComponent weapon = wm.GetCurrentWeapon();
  if (!weapon)
  {
   array<IEntity> weapons = {};
   wm.GetWeaponsList(weapons);
   foreach (IEntity wEnt : weapons)
   {
    BaseWeaponComponent w = BaseWeaponComponent.Cast(wEnt.FindComponent(BaseWeaponComponent));
    if (w && (w.GetWeaponType() == EWeaponType.WT_RIFLE || w.GetWeaponType() == EWeaponType.WT_MACHINEGUN || w.GetWeaponType() == EWeaponType.WT_SNIPERRIFLE))
    {
     weapon = w;
     break;
    }
   }
  }
  if (!weapon) return false;
  array<BaseMuzzleComponent> muzzles = {};
  weapon.GetMuzzlesList(muzzles);
  foreach (BaseMuzzleComponent m : muzzles)
  {
   if (m && m.GetMagazineWell())
   {
    outMuzzle = m;
    break;
   }
  }
  if (!outMuzzle) return false;
  outCount = inv.GetMagazineCountByMuzzle(soldier, outMuzzle);
  BaseMagazineComponent curMag = outMuzzle.GetMagazine();
  int inGun = 0;
  if (curMag) inGun = curMag.GetAmmoCount();
  if (outCount == 0 && inGun == 0) return true;
  return outCount < CRITICAL_MAGAZINE_COUNT;
 }

 static bool NeedsAntiArmor(ChimeraCharacter soldier, SCR_AIGroup group)
 {
  if (!soldier || !group) return false;
  if (BBB_Armour.HasOperationalAT(soldier)) return false;
  SCR_AIGroupUtilityComponent groupUtil = SCR_AIGroupUtilityComponent.Cast(group.FindComponent(SCR_AIGroupUtilityComponent));
  if (!groupUtil || !groupUtil.m_Perception) return false;
  vector soldierPos = soldier.GetOrigin();
  Faction soldierFac = group.GetFaction();
  foreach (IEntity target : groupUtil.m_Perception.m_aTargetEntities)
  {
   if (!target) continue;
   if (target.IsInherited(ChimeraCharacter))
   {
    if (!BBB.IsAlive(target)) continue;
   }
   else if (BBB.Health(target) <= 0)
   {
    continue;
   }
   Faction targetFac = null;
   FactionAffiliationComponent aff = FactionAffiliationComponent.Cast(target.FindComponent(FactionAffiliationComponent));
   if (aff) targetFac = aff.GetAffiliatedFaction();
   if (soldierFac && targetFac && !soldierFac.IsFactionEnemy(targetFac)) continue;
   if (vector.Distance(soldierPos, target.GetOrigin()) > 300.0) continue;
   if (BBB_Armour.IsArmoredThreat(target)) return true;
  }
  return false;
 }

 static bool CorpseHasMatchingMag(IEntity corpse, BaseMuzzleComponent muzzle, notnull array<IEntity> outMags)
 {
  outMags.Clear();
  if (!corpse || !muzzle || !muzzle.GetMagazineWell()) return false;
  InventoryStorageManagerComponent corpseInv = InventoryStorageManagerComponent.Cast(corpse.FindComponent(InventoryStorageManagerComponent));
  if (!corpseInv) return false;
  array<IEntity> items = {};
  corpseInv.GetItems(items);
  string targetWell = muzzle.GetMagazineWell().Type().ToString();
  foreach (IEntity item : items)
  {
   if (!item) continue;
   MagazineComponent mag = MagazineComponent.Cast(item.FindComponent(MagazineComponent));
   if (!mag || !mag.GetMagazineWell() || mag.GetAmmoCount() <= 0) continue;
   if (mag.GetMagazineWell().Type().ToString() == targetWell)
    outMags.Insert(item);
  }
  return !outMags.IsEmpty();
 }

 static bool CorpseHasAT(IEntity corpse, out IEntity outLauncher, notnull array<IEntity> outRockets)
 {
  outLauncher = null;
  outRockets.Clear();
  if (!corpse) return false;
  InventoryStorageManagerComponent corpseInv = InventoryStorageManagerComponent.Cast(corpse.FindComponent(InventoryStorageManagerComponent));
  if (!corpseInv) return false;
  array<IEntity> items = {};
  corpseInv.GetItems(items);
  foreach (IEntity item : items)
  {
   if (!item) continue;
   BaseWeaponComponent weapon = BaseWeaponComponent.Cast(item.FindComponent(BaseWeaponComponent));
   if (weapon && weapon.GetWeaponType() == EWeaponType.WT_ROCKETLAUNCHER)
   {
    if (!outLauncher) outLauncher = item;
   }
   MagazineComponent mag = MagazineComponent.Cast(item.FindComponent(MagazineComponent));
   if (mag && mag.GetMagazineWell())
   {
    string wellName = mag.GetMagazineWell().Type().ToString();
    wellName.ToLower();
    if (wellName.Contains("rpg") || wellName.Contains("rocket") || wellName.Contains("law"))
     outRockets.Insert(item);
   }
  }
  return outLauncher != null;
 }

 static bool TransferItem(SCR_InventoryStorageManagerComponent soldierInv, IEntity item, IEntity corpse = null)
 {
  if (!soldierInv || !item) return false;
  InventoryItemComponent itemComp = InventoryItemComponent.Cast(item.FindComponent(InventoryItemComponent));
  InventoryStorageSlot slot = null;
  if (itemComp) slot = itemComp.GetParentSlot();
  if (corpse && slot && slot.GetStorage())
  {
   InventoryStorageManagerComponent corpseInv = InventoryStorageManagerComponent.Cast(corpse.FindComponent(InventoryStorageManagerComponent));
   if (corpseInv) corpseInv.TryRemoveItemFromStorage(item, slot.GetStorage());
  }
  bool ok = soldierInv.TryInsertItem(item, EStoragePurpose.PURPOSE_DEPOSIT);
  if (!ok) ok = soldierInv.TryInsertItem(item, EStoragePurpose.PURPOSE_ANY);
  if (!ok)
  {
   BaseInventoryStorageComponent target = soldierInv.FindStorageForItem(item, EStoragePurpose.PURPOSE_DEPOSIT);
   if (!target) target = soldierInv.FindStorageForItem(item, EStoragePurpose.PURPOSE_ANY);
   if (target) ok = soldierInv.TryMoveItemToStorage(item, target);
  }
  return ok;
 }

 static bool LootFromCorpse(ChimeraCharacter soldier, IEntity corpse, bool needsAmmo, BaseMuzzleComponent muzzle, bool needsAT, SCR_AIGroup group)
 {
  if (!soldier || !corpse) return false;
  SCR_InventoryStorageManagerComponent soldierInv = SCR_InventoryStorageManagerComponent.Cast(soldier.FindComponent(SCR_InventoryStorageManagerComponent));
  if (!soldierInv) return false;
  int magsMoved = 0;
  if (needsAmmo && muzzle)
  {
   array<IEntity> mags = {};
   CorpseHasMatchingMag(corpse, muzzle, mags);
   int currentCount = soldierInv.GetMagazineCountByMuzzle(soldier, muzzle);
   foreach (IEntity mag : mags)
   {
    if (currentCount >= TARGET_MAGAZINE_COUNT) break;
    if (TransferItem(soldierInv, mag, corpse))
    {
     currentCount++;
     magsMoved++;
     s_iMagazinesLooted++;
    }
   }
  }
  int atMoved = 0;
  if (needsAT)
  {
   IEntity launcher;
   array<IEntity> rockets = {};
   if (CorpseHasAT(corpse, launcher, rockets))
   {
    if (soldierInv.EquipWeapon(launcher, null, true))
    {
     atMoved++;
     s_iLaunchersLooted++;
     foreach (IEntity rocket : rockets)
     {
      if (TransferItem(soldierInv, rocket, corpse))
       s_iMagazinesLooted++;
     }
    }
   }
  }
  if (magsMoved > 0 || atMoved > 0)
  {
   s_iRearmCount++;
   BBB_Perf.Count("rearm.looted");
   string gName = "group";
   if (group) gName = BBB.s_Hooks.NameOf(group);
   PrintFormat("[BBB] REARM %1: %2 looted from corpse (mags=%3 at=%4)", gName, soldier, magsMoved, atMoved);
   return true;
  }
  return false;
 }

 protected static bool QueryCorpseHit(IEntity entity)
 {
  if (entity) s_aQueryCorpseHits.Insert(entity);
  return true;
 }

 static IEntity FindLootSource(ChimeraCharacter soldier, bool needsAmmo, BaseMuzzleComponent muzzle, bool needsAT)
 {
  if (!soldier) return null;
  s_aQueryCorpseHits.Clear();
  GetGame().GetWorld().QueryEntitiesBySphere(soldier.GetOrigin(), s_fSearchRadius, QueryCorpseHit, null, EQueryEntitiesFlags.ALL);
  IEntity best = null;
  float bestDist = s_fSearchRadius * s_fSearchRadius;
  vector soldierPos = soldier.GetOrigin();
  foreach (IEntity ent : s_aQueryCorpseHits)
  {
   if (!ent || ent == soldier) continue;
   ChimeraCharacter ch = ChimeraCharacter.Cast(ent);
   if (ch)
   {
    CharacterControllerComponent ctl = ch.GetCharacterController();
    if (!ctl || !ctl.IsDead()) continue;
   }
   else
   {
    if (BBB.IsAlive(ent)) continue;
    if (!ent.FindComponent(InventoryStorageManagerComponent)) continue;
   }
   bool useful = false;
   if (needsAmmo && muzzle)
   {
    array<IEntity> mags = {};
    if (CorpseHasMatchingMag(ent, muzzle, mags)) useful = true;
   }
   if (!useful && needsAT)
   {
    IEntity launcher;
    array<IEntity> rockets = {};
    if (CorpseHasAT(ent, launcher, rockets)) useful = true;
   }
   if (!useful) continue;
   float d = vector.DistanceSq(soldierPos, ent.GetOrigin());
   if (d < bestDist)
   {
    bestDist = d;
    best = ent;
   }
  }
  s_aQueryCorpseHits.Clear();
  return best;
 }

 static void CancelRearmAction(ChimeraCharacter soldier)
 {
  if (!soldier) return;
  if (s_mRearmAction.Contains(soldier))
  {
   SCR_AIMoveIndividuallyBehavior act = s_mRearmAction.Get(soldier);
   if (act && act.GetActionState() != EAIActionState.COMPLETED && act.GetActionState() != EAIActionState.FAILED)
    act.Fail();
   s_mRearmAction.Remove(soldier);
  }
  s_mTargetCorpse.Remove(soldier);
 }

 static bool InRearmAction(ChimeraCharacter soldier)
 {
  if (!soldier) return false;
  return s_mRearmAction.Contains(soldier);
 }

 static void SetRearmActionForTest(ChimeraCharacter soldier, SCR_AIMoveIndividuallyBehavior act, IEntity corpse)
 {
  if (!soldier) return;
  s_mRearmAction.Set(soldier, act);
  s_mTargetCorpse.Set(soldier, corpse);
 }

 static void CheckRearmFor(SCR_AIGroup group)
 {
  Read();
  if (!s_bOn || !group || BBB.HasPlayer(group)) return;
  float nowMs = GetGame().GetWorld().GetWorldTime();
  array<AIAgent> agents = {};
  BBB.s_Hooks.GetMembers(group, agents);
  foreach (AIAgent agent : agents)
  {
   if (!agent) continue;
   ChimeraCharacter soldier = ChimeraCharacter.Cast(agent.GetControlledEntity());
   if (!soldier || !BBB.IsAlive(soldier) || soldier.IsInVehicle() || BBB.PlayerOf(soldier))
   {
    if (soldier) CancelRearmAction(soldier);
    continue;
   }
   CompartmentAccessComponent acc = soldier.GetCompartmentAccessComponent();
   if (acc && (acc.GetCompartment() != null || acc.IsGettingIn() || acc.IsGettingOut()))
   {
    CancelRearmAction(soldier);
    continue;
   }
   if (BBB_Drag.Busy(soldier) || BBB_Medic.s_mGoingTo.Contains(soldier))
   {
    CancelRearmAction(soldier);
    continue;
   }
   CharacterControllerComponent ctl = soldier.GetCharacterController();
   if (!ctl || ctl.IsUnconscious())
   {
    CancelRearmAction(soldier);
    continue;
   }
   if (s_mNextRearmCheck.Contains(soldier) && nowMs < s_mNextRearmCheck.Get(soldier)) continue;

   SCR_AIUtilityComponent utility = SCR_AIUtilityComponent.Cast(agent.FindComponent(SCR_AIUtilityComponent));
   if (utility && utility.m_ThreatSystem)
   {
    float sup, sh, inVal, en;
    utility.m_ThreatSystem.GetThreatValues(sup, sh, inVal, en);
    if (sup >= 0.85)
    {
     CancelRearmAction(soldier);
     continue;
    }
   }

   BaseMuzzleComponent muzzle;
   int currentMags;
   bool needsAmmo = NeedsAmmo(soldier, muzzle, currentMags);
   bool needsAT = NeedsAntiArmor(soldier, group);
   if (!needsAmmo && !needsAT)
   {
    CancelRearmAction(soldier);
    s_mNextRearmCheck.Set(soldier, nowMs + REARM_COOLDOWN_MS);
    continue;
   }

   IEntity corpse = FindLootSource(soldier, needsAmmo, muzzle, needsAT);
   if (!corpse)
   {
    CancelRearmAction(soldier);
    s_mNextRearmCheck.Set(soldier, nowMs + 6000.0);
    continue;
   }

   SCR_AIMoveIndividuallyBehavior curAction = null;
   if (s_mRearmAction.Contains(soldier)) curAction = s_mRearmAction.Get(soldier);
   bool actionActive = curAction && !curAction.GetRemoveAction() && curAction.GetActionState() != EAIActionState.FAILED && curAction.GetActionState() != EAIActionState.COMPLETED;
   if (!actionActive && curAction)
   {
    s_mRearmAction.Remove(soldier);
    s_mTargetCorpse.Remove(soldier);
    curAction = null;
   }

   float dist = vector.DistanceXZ(soldier.GetOrigin(), corpse.GetOrigin());
   if (dist <= LOOT_REACH_M)
   {
    if (curAction && curAction.GetActionState() != EAIActionState.COMPLETED && curAction.GetActionState() != EAIActionState.FAILED)
     curAction.Complete();
    s_mRearmAction.Remove(soldier);
    s_mTargetCorpse.Remove(soldier);
    LootFromCorpse(soldier, corpse, needsAmmo, muzzle, needsAT, group);
    s_mNextRearmCheck.Set(soldier, nowMs + REARM_COOLDOWN_MS);
   }
   else if (utility && dist <= s_fSearchRadius)
   {
    if (!actionActive || !s_mTargetCorpse.Contains(soldier) || s_mTargetCorpse.Get(soldier) != corpse)
    {
     if (curAction && actionActive) curAction.Fail();
     s_mTargetCorpse.Set(soldier, corpse);
     SCR_AIMoveIndividuallyBehavior moveAction = new SCR_AIMoveIndividuallyBehavior(utility, null, corpse.GetOrigin(), priority: SCR_AIActionBase.PRIORITY_BEHAVIOR_MOVE_INDIVIDUALLY + 5, ent: corpse, radius: 2.0);
     s_mRearmAction.Set(soldier, moveAction);
     utility.AddAction(moveAction);
    }
   }
  }
 }

 static void Stop()
 {
  foreach (ChimeraCharacter s, SCR_AIMoveIndividuallyBehavior act : s_mRearmAction)
  {
   if (act && act.GetActionState() != EAIActionState.COMPLETED && act.GetActionState() != EAIActionState.FAILED)
    act.Complete();
  }
  s_mRearmAction.Clear();
  s_mNextRearmCheck.Clear();
  s_mTargetCorpse.Clear();
  s_aQueryCorpseHits.Clear();
  s_iRearmCount = 0;
  s_iMagazinesLooted = 0;
  s_iLaunchersLooted = 0;
 }
}
