// Vanilla compares the requested type before choosing a fallback. For example,
// Auto on an M16 selects Burst again on every evaluation, restarting the selector
// animation. Treat the already selected best fallback as success as well.
class BBB_FireSelector
{
 static int BestIndex(array<EWeaponFiremodeType> available, EWeaponFiremodeType requested)
 {
  array<EWeaponFiremodeType> preference = {};
  switch (requested)
  {
   case EWeaponFiremodeType.Auto:
    preference = {EWeaponFiremodeType.Auto, EWeaponFiremodeType.Burst, EWeaponFiremodeType.Semiauto, EWeaponFiremodeType.Manual};
    break;
   case EWeaponFiremodeType.Burst:
    preference = {EWeaponFiremodeType.Burst, EWeaponFiremodeType.Auto, EWeaponFiremodeType.Semiauto, EWeaponFiremodeType.Manual};
    break;
   case EWeaponFiremodeType.Semiauto:
    preference = {EWeaponFiremodeType.Semiauto, EWeaponFiremodeType.Manual, EWeaponFiremodeType.Burst, EWeaponFiremodeType.Auto};
    break;
   case EWeaponFiremodeType.Safety:
    preference = {EWeaponFiremodeType.Safety};
    break;
  }
  foreach (EWeaponFiremodeType candidate : preference)
  {
   // Vanilla uses the last index when a weapon lists a type more than once.
   for (int i = available.Count() - 1; i >= 0; i--)
    if (available[i] == candidate) return i;
  }
  return -1;
 }

 static bool AlreadySelected(BaseMuzzleComponent muzzle, EWeaponFiremodeType requested)
 {
  if (!muzzle) return false;
  array<BaseFireMode> modes = {};
  muzzle.GetFireModesList(modes);
  array<EWeaponFiremodeType> types = {};
  foreach (BaseFireMode mode : modes)
  {
   if (!mode) return false;
   types.Insert(mode.GetFiremodeType());
  }
  int best = BestIndex(types, requested);
  return best >= 0 && best == muzzle.GetFireModeIndex();
 }
}

modded class SCR_AISelectFireMode
{
 override ENodeResult EOnTaskSimulate(AIAgent owner, float dt)
 {
  // Leave vehicle/turret handling and unavailable-mode failures to vanilla.
  ChimeraCharacter soldier;
  if (owner) soldier = ChimeraCharacter.Cast(owner.GetControlledEntity());
  if (soldier && !soldier.IsInVehicle())
  {
   CharacterControllerComponent controller = soldier.GetCharacterController();
   BaseWeaponManagerComponent manager;
   if (controller) manager = controller.GetWeaponManagerComponent();
   BaseWeaponComponent weapon;
   if (manager) weapon = manager.GetCurrent();
   if (weapon && weapon.GetCurrentFireModeType() != m_FiremodeType && BBB_FireSelector.AlreadySelected(weapon.GetCurrentMuzzle(), m_FiremodeType))
    return ENodeResult.SUCCESS;
  }
  return super.EOnTaskSimulate(owner, dt);
 }
}
