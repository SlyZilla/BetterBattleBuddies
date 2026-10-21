// BBB_Think (the "what is he thinking" panel). -bbbThinkTest 1 -bbbThink 2
// (Test-BBB.ps1 -Preset think). A US and a USSR fire team spawn 120 m apart facing each
// other. Checks the note ring on real soldiers (repeats folded, capped, off switch writes
// nothing, deleted soldiers pruned), that a squad note shows in a member's panel, that the
// panel reads a live soldier's vanilla behaviour and threat, and, once the teams fight,
// that it names a real perceived target. The camera pick and the on-screen drawing need a
// player's view: a dedicated server has none, so they are Jacob's to see in game.
class BBB_ThinkTest : BBB_AimTest
{
 static ref BBB_ThinkTest s_ThinkTest;
 static const int EXPECTED = 17;
 protected int m_iChecks;
 protected int m_iFailures;
 protected SCR_AIGroup m_Us;
 protected SCR_AIGroup m_Ussr;
 protected int m_iPolls;

 static void MaybeStartThink()
 {
  if (s_ThinkTest || !Replication.IsServer() || !System.IsCLIParam("bbbThinkTest")) return;
  s_ThinkTest = new BBB_ThinkTest();
  GetGame().GetCallqueue().CallLater(s_ThinkTest.Begin, 12000);
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
  PrintFormat("[BBB] THINKTEST CHECK %1 %2 %3", name, res, detail);
 }

 override protected void Begin()
 {
  BBB_Think.Read();
  Check("flag-on", BBB_Think.s_bOn && BBB_Think.s_bPrint, "");
  vector at = BBB.Ground(Vector(1340, 0, 2770));
  m_Us = SCR_AIGroup.Cast(Spawn(TEAM_US, at, Vector(0, 0, 1)));
  m_Ussr = SCR_AIGroup.Cast(Spawn(TEAM_USSR, at + Vector(0, 0, 120), Vector(0, 0, -1)));
  GetGame().GetCallqueue().CallLater(Notes, 5000);
 }

 protected IEntity Member(SCR_AIGroup group, int index)
 {
  if (!group) return null;
  array<AIAgent> agents = {};
  group.GetAgents(agents);
  if (index >= agents.Count()) return null;
  return agents[index].GetControlledEntity();
 }

 protected void Notes()
 {
  IEntity a = Member(m_Us, 0);
  IEntity b = Member(m_Us, 1);
  IEntity c = Member(m_Us, 2);
  if (!a || !b || !c)
  {
   Check("spawned", false, "US team has fewer than 3 soldiers");
   Finish();
   return;
  }

  BBB_Think.Note(a, "Test", "same");
  BBB_Think.Note(a, "Test", "same");
  BBB_ThinkLog notes = BBB_Think.LogOf(a);
  Check("repeat-folded", notes && notes.m_Lines.Count() == 1 && notes.m_Lines[0].m_iRepeats == 2, "");
  BBB_Think.Note(a, "Test", "other");
  Check("new-line", notes && notes.m_Lines.Count() == 2 && notes.m_Lines[1].m_sText == "other", "");

  for (int i = 0; i < 20; i++) BBB_Think.Note(b, "Test", "n" + i);
  BBB_ThinkLog ring = BBB_Think.LogOf(b);
  bool capped = ring && ring.m_Lines.Count() == BBB_Think.LINES && ring.m_Lines[0].m_sText == "n8" && ring.m_Lines[BBB_Think.LINES - 1].m_sText == "n19";
  Check("ring-capped", capped, "");

  BBB_Think.s_bOn = false;
  BBB_Think.Note(c, "Test", "should not land");
  BBB_Think.s_bOn = true;
  Check("off-writes-nothing", BBB_Think.LogOf(c) == null, "");

  BBB_Think.NoteGroup(m_Us, "Test", "squad line");
  array<string> lines = {};
  vector targetAt;
  BBB_Think.Describe(a, GetGame().GetWorld().GetWorldTime(), lines, targetAt);
  string panel = "";
  foreach (string line : lines) panel += line + " | ";
  Check("panel-has-squad-note", panel.Contains("Test: squad line") && panel.Contains("Test: other"), panel);
  Check("panel-reads-brain", panel.Contains("doing: ") && !panel.Contains("doing: none") && panel.Contains("threat: "), panel);

  vector eye = a.GetOrigin() + "100 1.2 0";
  Check("camera-picks-soldier", BBB_Think.PickFrom(eye, "-1 0 0", null) == a, "look directly at a real soldier from 100 m");
  Check("camera-excludes-self", BBB_Think.PickFrom(eye, "-1 0 0", a) != a, "controlled body excluded");
  Check("camera-range-and-direction", BBB_Think.PickFrom(a.GetOrigin() + "600 1.2 0", "-1 0 0", null) == null && BBB_Think.PickFrom(eye, "1 0 0", null) == null, "beyond 500 m and behind camera excluded");

  // A deleted soldier's log goes on the next prune.
  SCR_EntityHelper.DeleteEntityAndChildren(b);
  GetGame().GetCallqueue().CallLater(Pruned, 500);
 }

 protected void Pruned()
 {
  int before = BBB_Think.NullLogs();
  BBB_Think.Prune(GetGame().GetWorld().GetWorldTime(), false);
  int after = BBB_Think.NullLogs();
  Check("deleted-pruned", before >= 1 && after == 0, string.Format("logs of deleted soldiers %1 -> %2", before, after));

  IEntity a = Member(m_Us, 0);
  // Benchmark the overlay's note storage (mode 1), separately from synchronous
  // console I/O in mode 2. Real printed notes are exercised before/after this.
  bool wasPrint = BBB_Think.s_bPrint;
  BBB_Think.s_bPrint = false;
  int started = System.GetTickCount();
  for (int i = 0; i < 2000; i++) BBB_Think.Note(a, "Perf", "n" + (i % 3));
  int ms = System.GetTickCount() - started;
  BBB_Think.s_bPrint = wasPrint;
  Check("note-cost", ms <= 40, string.Format("%1 ms for 2000 notes", ms));

  int offStarted = System.GetTickCount();
  BBB_Think.s_bOn = false;
  for (int j = 0; j < 20000; j++) BBB_Think.Note(a, "Perf", "off");
  BBB_Think.s_bOn = true;
  int offMs = System.GetTickCount() - offStarted;
  Check("off-cost", offMs <= 50, string.Format("%1 ms for 20000 notes while off", offMs));
  Bounds();
  GetGame().GetCallqueue().CallLater(PollTarget, 2000, true);
 }

 protected void Bounds()
 {
  bool wasPrint = BBB_Think.s_bPrint;
  BBB_Think.s_bPrint = false;
  BBB_Think.Stop();
  array<IEntity> subjects = {};
  EntitySpawnParams parameters = new EntitySpawnParams();
  for (int i = 0; i <= BBB_Think.MAX_LOGS; i++)
  {
   IEntity subject = GetGame().SpawnEntity(GenericEntity, GetGame().GetWorld(), parameters);
   subjects.Insert(subject);
   BBB_Think.Note(subject, "Bounds", "fresh");
   // These entities are created in one simulation frame. Give the first entry
   // a distinct age so this asserts oldest-entry eviction, not map tie order.
   BBB_ThinkLog first = BBB_Think.LogOf(subject);
   if (i == 0 && first) first.m_fLast -= 1;
  }
  Check("fresh-logs-capped", subjects.Count() == 401 && subjects[0] && subjects[400] && BBB_Think.LogCount() == BBB_Think.MAX_LOGS && BBB_Think.LogOf(subjects[0]) == null && BBB_Think.LogOf(subjects[400]) != null, string.Format("%1 logs after 401 live subjects", BBB_Think.LogCount()));
  BBB_Think.Prune(GetGame().GetWorld().GetWorldTime() + BBB_Think.FORGET_MS + 1, false);
  Check("idle-logs-expire", BBB_Think.LogCount() == 0, "all entries expire after five minutes without notes");
  foreach (IEntity spawned : subjects) if (spawned) SCR_EntityHelper.DeleteEntityAndChildren(spawned);
  BBB_Think.s_bPrint = wasPrint;
 }

 // Once the teams see each other, some soldier's panel names a real target and a spot.
 protected void PollTarget()
 {
  m_iPolls++;
  array<AIAgent> agents = {};
  array<string> lines = {};
  vector targetAt;
  array<SCR_AIGroup> groups = {m_Us, m_Ussr};
  foreach (SCR_AIGroup group : groups)
  {
   if (!group) continue;
   group.GetAgents(agents);
   foreach (AIAgent agent : agents)
   {
    IEntity soldier = agent.GetControlledEntity();
    if (!BBB.IsAlive(soldier)) continue;
    BBB_Think.Describe(soldier, GetGame().GetWorld().GetWorldTime(), lines, targetAt);
    if (targetAt == vector.Zero) continue;
    string panel = "";
    foreach (string line : lines) panel += line + " | ";
    GetGame().GetCallqueue().Remove(PollTarget);
    Check("panel-names-target", panel.Contains("target: ") && !panel.Contains("target: none"), panel);
    float off = vector.DistanceXZ(targetAt, soldier.GetOrigin());
    Check("target-spot-real", off > 60 && off < 200, string.Format("%1 m from him", off));
    Finish();
    return;
   }
  }
  if (m_iPolls < 30) return;
  GetGame().GetCallqueue().Remove(PollTarget);
  Check("panel-names-target", false, "no soldier perceived an enemy in 60 s");
  Check("target-spot-real", false, "");
  Finish();
 }

 override protected void Finish()
 {
  string verdict = "FAIL";
  if (m_iFailures == 0 && m_iChecks == EXPECTED) verdict = "PASS";
  PrintFormat("[BBB] THINKTEST INFO logs=%1", BBB_Think.LogCount());
  PrintFormat("[ACR] VERDICT %1 test=bbbthink passed=%2 failed=%3 expected=%4", verdict, m_iChecks - m_iFailures, m_iFailures, EXPECTED);
  Print("[ACR] TEST COMPLETE");
 }
}

modded class SCR_BaseGameMode
{
 protected override void OnGameStart()
 {
  super.OnGameStart();
  BBB_ThinkTest.MaybeStartThink();
 }
}
