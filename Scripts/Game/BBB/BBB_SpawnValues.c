// Vanilla AI values from the Settings tab (BBB #8): skill and spotting speed for every AI
// soldier, set once when he spawns. Anything that sets them later (a Game Master, the Scenario
// Framework) still wins; BBB never sets them again. A host mod that sets them itself all the time,
// such as AC: Actual every second, reads them here (BBB_SpawnValues.Skill / Perception) and lets
// them stand in for its own defaults. Default: untouched. Server only (AI runs there).
class BBB_SpawnValues
{
 // The skill the Settings tab sets for every AI soldier; false: the mission's own (nothing set).
 static bool Skill(out int skill)
 {
  string value;
  if (!BBB_Settings.Get("bbbAISkill", value) || value == "0") return false;
  skill = value.ToInt();
  return skill > 0;
 }

 // The spotting speed the Settings tab sets for every AI soldier; false: vanilla's 1 (nothing set).
 static bool Perception(out float factor)
 {
  string value;
  if (!BBB_Settings.Get("bbbAIPerception", value)) return false;
  factor = Math.Clamp(value.ToFloat(), 0.25, 3);
  return Math.AbsFloat(factor - 1) > 0.001;
 }
}

modded class SCR_AICombatComponent
{
 override void EOnInit(IEntity owner)
 {
  super.EOnInit(owner);
  if (!Replication.IsServer()) return;
  string value;
  if (BBB_Settings.Get("bbbAISkill", value) && value != "0")
  {
   m_eAISkillDefault = value.ToInt();
   m_eAISkill = m_eAISkillDefault;
  }
  // The factor alone: vanilla applies it with the threat state's own on the next update.
  if (BBB_Settings.Get("bbbAIPerception", value)) m_fPerceptionFactor = Math.Clamp(value.ToFloat(), 0.25, 3);
 }
}
