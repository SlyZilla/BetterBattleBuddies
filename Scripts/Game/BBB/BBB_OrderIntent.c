// Native priority levels identify explicit orders independently of their provider.
// A temporary danger/attack behavior must not hide an unfinished player order.
class BBB_OrderIntent
{
 // A BBB feature asks before acting (noted for the BBB_Think panel when it holds back).
 static bool Blocks(SCR_AIUtilityComponent utility)
 {
  if (!Held(utility)) return false;
  BBB_Think.NoteUtility(utility, "Orders", "player/GM order active: BBB stands back");
  return true;
 }

 static bool Held(SCR_AIUtilityComponent utility)
 {
  if (!utility) return false;
  SCR_AIBehaviorBase current = utility.GetCurrentBehavior();
  if (current && current.EvaluatePriorityLevel() >= SCR_AIActionBase.PRIORITY_LEVEL_PLAYER) return true;
  AIAgent agent = utility.GetAIAgent();
  AIGroup group;
  if (agent) group = agent.GetParentGroup();
  SCR_AIWaypoint waypoint;
  if (group) waypoint = SCR_AIWaypoint.Cast(group.GetCurrentWaypoint());
  if (waypoint && waypoint.GetPriorityLevel() >= SCR_AIActionBase.PRIORITY_LEVEL_PLAYER) return true;
  array<ref AIActionBase> actions = {};
  utility.FindActionsOfInheritedType(SCR_AIBehaviorBase, actions);
  foreach (AIActionBase action : actions)
  {
   if (action.GetRemoveAction() || action.GetActionState() == EAIActionState.FAILED || action.GetActionState() == EAIActionState.COMPLETED) continue;
   if (action.EvaluatePriorityLevel() >= SCR_AIActionBase.PRIORITY_LEVEL_PLAYER) return true;
  }
  return false;
 }
}
