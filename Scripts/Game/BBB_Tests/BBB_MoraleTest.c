// Better Battle Buddies (BBB): Standalone morale, retrograde break-contact
// withdrawal, and surrender assertions.
class BBB_MoraleTest : BBB_AimTest
{
	static ref BBB_MoraleTest s_MoraleTest;
	protected int m_iChecks;
	protected int m_iFailures;
	protected SCR_AIGroup m_SquadUSSR;
	protected SCR_AIGroup m_SquadUS;

	static void MaybeStart()
	{
		if (!Replication.IsServer() || !System.IsCLIParam("bbbMoraleTest")) return;
		s_MoraleTest = new BBB_MoraleTest();
		GetGame().GetCallqueue().CallLater(s_MoraleTest.BeginTest, 12000);
	}

	protected void Check(string label, bool ok)
	{
		m_iChecks++;
		string res = "FAIL";
		if (ok)
		{
			res = "PASS";
		}
		else
		{
			m_iFailures++;
		}
		PrintFormat("[BBB] MORALETEST CHECK %1 %2", label, res);
	}

	protected void BeginTest()
	{
		BBB_Morale.Read();

		// Check 1: Feature flags and defaults
		Check("flag-read", BBB_Morale.s_bRead);
		Check("defaults-valid", BBB_Morale.s_fCasualtyThreshold > 0 && BBB_Morale.s_fSuppressionThreshold > 0);

		// Check 2: Retrograde destination calculation away from threat
		vector squadPos = Vector(1000, 50, 1000);
		vector threatPos = Vector(1000, 50, 1050); // Threat is 50 m to the north (+Z)
		vector retreatPos = BBB_Morale.CalculateRetrogradeDestination(squadPos, threatPos, 30.0);
		// Retreat should be south (-Z)
		bool retreatDirOk = (retreatPos[2] < squadPos[2]) && (Math.AbsFloat(retreatPos[0] - squadPos[0]) < 1.0);
		Check("retrograde-direction-away", retreatDirOk);

		// Check 3: Disabled baseline
		BBB_Morale.s_bOn = false;
		Check("disabled-baseline", !BBB_Morale.s_bOn && !BBB_Morale.IsWithdrawing(null));

		// Enable morale for live scenario assertions
		BBB_Morale.s_bOn = true;

		// Check 4: Spawning squad and evaluating baseline morale
		vector spawnPos = BBB.Ground(Vector(1340, 0, 2770));
		vector facing = Vector(0, 0, 1);
		m_SquadUSSR = SCR_AIGroup.Cast(Spawn(TEAM_USSR, spawnPos, facing));
		if (!m_SquadUSSR)
		{
			Check("squad-spawn", false);
			FinishTest();
			return;
		}
		Check("squad-spawn", true);

		GetGame().GetCallqueue().CallLater(EvaluateInitialMorale, 4000);
	}

	protected void EvaluateInitialMorale()
	{
		array<AIAgent> agents = {};
		BBB.s_Hooks.GetMembers(m_SquadUSSR, agents);
		Check("squad-members-populated", agents.Count() >= 4);

		int aliveCount;
		int outOfAmmoCount;
		BBB_GroupMoraleState state = new BBB_GroupMoraleState();
		state.m_Group = m_SquadUSSR;
		state.m_iInitialCount = agents.Count();
		float initialMorale = BBB_Morale.CalculateGroupMorale(state, agents, aliveCount, outOfAmmoCount);

		// A fresh healthy squad must have 0 or very low morale score
		Check("fresh-squad-calm-morale", initialMorale < 0.20);

		// Check 5: Synthetic casualties + leader loss calculation
		state.m_iInitialCount = 4;
		array<AIAgent> damagedAgents = {};
		if (agents.Count() >= 3)
		{
			damagedAgents.Insert(agents[1]);
			damagedAgents.Insert(agents[2]);
		}
		float brokenMorale = BBB_Morale.CalculateGroupMorale(state, damagedAgents, aliveCount, outOfAmmoCount);
		// 50% casualty (+0.40) + leader loss (+0.25) >= 0.65
		Check("casualty-leader-loss-triggers-break", brokenMorale >= 0.65);

		// Check 6: Explicit player/GM order veto
		SCR_AIWaypoint waypoint = new SCR_AIWaypoint();
		waypoint.SetPriorityLevel(SCR_AIActionBase.PRIORITY_LEVEL_PLAYER);
		m_SquadUSSR.AddWaypoint(waypoint);
		SCR_AIUtilityComponent firstUtility = SCR_AIUtilityComponent.Cast(agents[0].FindComponent(SCR_AIUtilityComponent));
		Check("player-order-blocks", BBB_OrderIntent.Blocks(firstUtility));

		// Check 7: Isolated out-of-ammo survivor surrender
		ChimeraCharacter loneSoldier = ChimeraCharacter.Cast(agents[1].GetControlledEntity());
		CharacterControllerComponent controller = loneSoldier.GetCharacterController();
		SCR_AIUtilityComponent utility = SCR_AIUtilityComponent.Cast(agents[1].FindComponent(SCR_AIUtilityComponent));
		AIControlComponent ai = AIControlComponent.Cast(loneSoldier.FindComponent(AIControlComponent));

		// Apply surrender posture
		controller.SetWeaponRaised(false);
		controller.TryEquipRightHandItem(null, EEquipItemType.EEquipTypeWeapon, false);
		SCR_AIStanceHandling.SetStance(controller, ECharacterStance.CROUCH);
		utility.m_CombatComponent.SetCombatMode(EAIGroupCombatMode.HOLD_FIRE);
		if (ai && ai.IsAIActivated()) ai.DeactivateAI();

		Check("surrender-posture-hold-fire", utility.m_CombatComponent.GetCombatMode() == EAIGroupCombatMode.HOLD_FIRE);
		Check("surrender-ai-deactivated", ai && !ai.IsAIActivated());
		Check("surrender-stance-crouch", controller.GetStance() == ECharacterStance.CROUCH);

		// Restore
		if (ai && !ai.IsAIActivated()) ai.ActivateAI();

		FinishTest();
	}

	protected void FinishTest()
	{
		PrintFormat("[BBB] MORALETEST FINISHED checks=%1 failures=%2", m_iChecks, m_iFailures);
		if (m_iFailures == 0 && m_iChecks >= 8)
		{
			Print("[BBB] MORALETEST VERDICT PASS");
		}
		else
		{
			PrintFormat("[BBB] MORALETEST VERDICT FAIL (failures=%1)", m_iFailures);
		}
		Print("[ACR] TEST COMPLETE");
	}
}

modded class SCR_BaseGameMode
{
	protected override void OnGameStart()
	{
		super.OnGameStart();
		BBB_MoraleTest.MaybeStart();
	}
}
