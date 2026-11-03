// Hot AI for Game Masters: two checkboxes in the game settings (vanilla's Game Settings category), one per
// BBB_HotAI switch (spotting and fire timing; gunners and vehicles). Ticking one switches it for the rest
// of the mission (BBB_HotAI.SetByGameMaster, on the server, where the AI runs); a new mission starts from
// the settings again.
// The attributes come from BBB's own list (Configs/BBB/BBB_EditorAttributes.conf). It is added after
// vanilla's attributes in the attribute manager of the editor mode that has the game settings (the edit
// mode), on every machine at editor init and before vanilla initialises its attributes, so no vanilla
// attribute list is replaced and other mods' lists are untouched. Every machine adds the same list at the
// same place, so attribute numbers match between server and clients.
[BaseContainerProps(), SCR_BaseEditorAttributeCustomTitle()]
class BBB_HotAISpottingEditorAttribute : SCR_BaseEditorAttribute
{
 override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
 {
  if (!IsGameMode(item)) return null;
  BBB_HotAI.Read();
  return SCR_BaseEditorAttributeVar.CreateBool(BBB_HotAI.s_bOn);
 }

 override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var, SCR_AttributesManagerEditorComponent manager, int playerID)
 {
  if (!var || !IsGameMode(item)) return;
  BBB_HotAI.SetByGameMaster(false, var.GetBool());
 }
}

[BaseContainerProps(), SCR_BaseEditorAttributeCustomTitle()]
class BBB_HotAIVehiclesEditorAttribute : SCR_BaseEditorAttribute
{
 override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
 {
  if (!IsGameMode(item)) return null;
  BBB_HotAI.Read();
  return SCR_BaseEditorAttributeVar.CreateBool(BBB_HotAI.s_bVehicles);
 }

 override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var, SCR_AttributesManagerEditorComponent manager, int playerID)
 {
  if (!var || !IsGameMode(item)) return;
  BBB_HotAI.SetByGameMaster(true, var.GetBool());
 }
}

class BBB_HotAIEditor
{
 static const ResourceName ATTRIBUTES = "{BBB0ED17A0000001}Configs/BBB/BBB_EditorAttributes.conf";
 // Declared bare, loaded on the first editor init (the shared static-initializer budget, ACA #185).
 // It owns the attributes: the attribute manager keeps only references to them.
 protected static ref SCR_EditorAttributeList s_List;

 static SCR_EditorAttributeList List()
 {
  if (s_List) return s_List;
  Resource resource = Resource.Load(ATTRIBUTES);
  if (!resource || !resource.IsValid()) return null;
  BaseResourceObject object = resource.GetResource();
  if (!object) return null;
  s_List = SCR_EditorAttributeList.Cast(BaseContainerTools.CreateInstanceFromContainer(object.ToBaseContainer()));
  return s_List;
 }
}

modded class SCR_AttributesManagerEditorComponentClass
{
 protected bool m_bBBBAdded;

 // BBB's attributes after vanilla's, once, in the mode that has the game settings.
 void BBB_AddAttributes()
 {
  if (m_bBBBAdded) return;
  m_bBBBAdded = true;
  bool gameSettings;
  foreach (SCR_BaseEditorAttribute attribute : m_aAttributes)
  {
   if (!SCR_AmbientSoundToggleEditorAttribute.Cast(attribute)) continue;
   gameSettings = true;
   break;
  }
  if (!gameSettings) return;
  SCR_EditorAttributeList list = BBB_HotAIEditor.List();
  if (list) list.InsertAllAttributes(m_aAttributes);
 }
}

modded class SCR_AttributesManagerEditorComponent
{
 override void EOnEditorInit()
 {
  BBB_AddAttributes();
  super.EOnEditorInit();
 }

 override void EOnEditorInitServer()
 {
  BBB_AddAttributes();
  super.EOnEditorInitServer();
 }

 protected void BBB_AddAttributes()
 {
  SCR_AttributesManagerEditorComponentClass data = SCR_AttributesManagerEditorComponentClass.Cast(GetEditorComponentData());
  if (data) data.BBB_AddAttributes();
 }
}
