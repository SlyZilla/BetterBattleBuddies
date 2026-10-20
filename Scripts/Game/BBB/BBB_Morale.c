// Better Battle Buddies (BBB): squad morale, tactical retrograde break-contact
// withdrawal, and isolated out-of-ammo surrender.
//
// Standalone tactical reaction for autonomous infantry squads under heavy attrition:
// 1. Morale evaluation (per-group, <= 0.5 Hz): monitors casualty ratio (>= 50%),
//    squad leader loss, sustained suppression (> 0.60), and ammunition depletion.
// 2. Retrograde withdrawal: when morale breaks under active contact, the squad
//    conducts a leap-frogging peel-back (retrograde bounding overwatch) directly
//    away from the enemy threat centroid to rear cover. One element covers while
//    the other bounds back, alternating until contact is broken (> 180 m or
//    out of sight) or max retrograde distance (90 m) is reached.
// 3. Isolated surrender: a lone, out-of-ammo survivor cornered by close enemies
//    (<= 15 m) with direct sight halts fire, lowers weapon, disables weapon controls
//    and crouches in place rather than suicide-charging.
// 4. Hard safety constraints: never overrides explicit player/GM orders
//    (BBB_OrderIntent.Blocks), never affects player-controlled squads or vehicles,
//    and respects host hooks (BBB.s_Hooks.AllowMorale). Never invents positions
//    or references ACA classes.

class BBB_GroupMoraleState : Managed
{
	SCR_AIGroup m_Group;
	int m_iInitialCount;
	float m_fMorale;
	float m_fBrokenSince;
	float m_fNextCheck;
	float m_fPhaseSince;
	bool m_bWithdrawing;
	vector m_vThreatCenter;
	vector m_vRetrogradeDir;
	vector m_vWithdrawPos;
	ref array<SCR_AIUtilityComponent> m_TeamCover = {};
	ref array<SCR_AIUtilityComponent> m_TeamMove = {};
	int m_iCoverTeam;
	int m_iPhase;
	float m_fTotalWithdrawn;
	bool m_bContactBroken;
}

class BBB_SurrenderState : Managed
{
	ChimeraCharacter m_Soldier;
	SCR_AIUtilityComponent m_Utility;
	float m_fSurrenderSince;
	bool m_bDisarmed;
}

class BBB_Morale
{
	static bool s_bRead;
	static bool s_bOn;
	static bool s_bSurrenderOn = true;

	// Tunable thresholds
	static float s_fCasualtyThreshold = 0.50;
	static float s_fSuppressionThreshold = 0.60;
	static float s_fRetrogradeBoundDist = 25.0;
	static float s_fMaxWithdrawDist = 90.0;

	// Internal timing constants
	static const float CHECK_INTERVAL_MS = 1500;
	static const float BOUND_TIMEOUT_S = 14.0;
	static const float SURRENDER_DIST_MAX = 15.0;
	static const float SURRENDER_THREAT_MIN = 0.60;
	static const float RECOVERY_CALM_TIME_S = 20.0;
	static const float BREAK_CONTACT_DIST_M = 180.0;

	// State tracking
	protected static ref array<ref BBB_GroupMoraleState> s_States = {};
	protected static ref array<ref BBB_SurrenderState> s_Surrendered = {};

	// Performance counters
	static int s_iWithdrawalsStarted;
	static int s_iWithdrawalBounds;
	static int s_iContactsBroken;
	static int s_iSurrenders;

	static void Read()
	{
		if (s_bRead) return;
		s_bRead = true;

		string value;
		if (System.GetCLIParam("bbbMorale", value))
		{
			s_bOn = value == "1";
		}

		if (System.GetCLIParam("bbbMoraleSurrender", value))
		{
			s_bSurrenderOn = value != "0";
		}

		if (System.GetCLIParam("bbbMoraleCasualties", value))
		{
			float c = value.ToFloat();
			if (c > 0.1 && c <= 0.9) s_fCasualtyThreshold = c;
		}

		if (System.GetCLIParam("bbbMoraleSuppression", value))
		{
			float s = value.ToFloat();
			if (s > 0.1 && s <= 1.0) s_fSuppressionThreshold = s;
		}

		if (System.GetCLIParam("bbbMoraleDistance", value))
		{
			float d = value.ToFloat();
			if (d >= 10.0 && d <= 50.0) s_fRetrogradeBoundDist = d;
		}

		PrintFormat("[BBB] MORALE enabled=%1 surrender=%2 casualtyThreshold=%3", s_bOn, s_bSurrenderOn, s_fCasualtyThreshold);
	}

	static bool IsOn()
	{
		Read();
		return s_bOn;
	}

	// Host or other modules can query if a group is currently breaking contact.
	static bool IsWithdrawing(SCR_AIGroup group)
	{
		if (!group || !s_bOn) return false;
		foreach (BBB_GroupMoraleState state : s_States)
		{
			if (state && state.m_Group == group && state.m_bWithdrawing) return true;
		}
		return false;
	}

	// ---------------------------------------------------------------- threat queries

	// Centroid of currently perceived enemy contacts, using native perception memory only.
	static bool GetThreatCentroid(SCR_AIGroup group, out vector outThreatCenter)
	{
		outThreatCenter = vector.Zero;
		if (!group) return false;

		array<AIAgent> agents = {};
		BBB.s_Hooks.GetMembers(group, agents);
		int count = 0;
		vector sum = vector.Zero;

		foreach (AIAgent agent : agents)
		{
			if (!agent) continue;
			SCR_AIUtilityComponent utility = SCR_AIUtilityComponent.Cast(agent.FindComponent(SCR_AIUtilityComponent));
			if (!utility || !utility.m_PerceptionComponent) continue;

			if (utility.m_CombatComponent)
			{
				BaseTarget current = utility.m_CombatComponent.GetCurrentTarget();
				if (current && current.GetTargetCategory() == ETargetCategory.ENEMY)
				{
					sum = sum + current.GetLastDetectedPosition();
					count++;
				}
			}

			array<BaseTarget> targets = {};
			utility.m_PerceptionComponent.GetTargetsList(targets, ETargetCategory.ENEMY);
			foreach (BaseTarget target : targets)
			{
				if (!target) continue;
				float timeSince = target.GetTimeSinceDetected();
				if (timeSince >= 0 && timeSince <= 15.0)
				{
					sum = sum + target.GetLastDetectedPosition();
					count++;
				}
			}
		}

		if (count == 0) return false;
		outThreatCenter = sum * (1.0 / count);
		return true;
	}

	// ---------------------------------------------------------------- ammunition check

	// Check if soldier is critically low or completely empty of ammunition.
	static bool IsOutOfAmmo(ChimeraCharacter soldier)
	{
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
		if (!weapon) return true;

		array<BaseMuzzleComponent> muzzles = {};
		weapon.GetMuzzlesList(muzzles);
		BaseMuzzleComponent primaryMuzzle;
		foreach (BaseMuzzleComponent m : muzzles)
		{
			if (m && m.GetMagazineWell())
			{
				primaryMuzzle = m;
				break;
			}
		}
		if (!primaryMuzzle) return false;

		int spareMags = inv.GetMagazineCountByMuzzle(soldier, primaryMuzzle);
		int inGun = 0;
		BaseMagazineComponent curMag = primaryMuzzle.GetMagazine();
		if (curMag) inGun = curMag.GetAmmoCount();

		return spareMags == 0 && inGun == 0;
	}

	// ---------------------------------------------------------------- group state helper

	protected static BBB_GroupMoraleState GetOrCreateState(SCR_AIGroup group, int currentCount)
	{
		foreach (BBB_GroupMoraleState state : s_States)
		{
			if (state && state.m_Group == group) return state;
		}

		if (s_States.Count() >= 64) return null;

		BBB_GroupMoraleState newState = new BBB_GroupMoraleState();
		newState.m_Group = group;
		newState.m_iInitialCount = currentCount;
		newState.m_fMorale = 0;
		newState.m_fNextCheck = 0;
		s_States.Insert(newState);
		return newState;
	}

	// ---------------------------------------------------------------- morale evaluation

	static float CalculateGroupMorale(BBB_GroupMoraleState state, array<AIAgent> agents, out int aliveCount, out int outOfAmmoCount)
	{
		aliveCount = 0;
		outOfAmmoCount = 0;
		float totalThreat = 0;
		bool leaderAlive = false;

		IEntity leaderEntity = state.m_Group.GetLeaderEntity();

		foreach (AIAgent agent : agents)
		{
			if (!agent) continue;
			IEntity controlled = agent.GetControlledEntity();
			if (!controlled || !BBB.IsAlive(controlled)) continue;

			ChimeraCharacter character = ChimeraCharacter.Cast(controlled);
			if (!character) continue;

			CharacterControllerComponent controller = character.GetCharacterController();
			if (!controller || controller.IsDead() || controller.IsUnconscious()) continue;

			aliveCount++;
			if (controlled == leaderEntity) leaderAlive = true;

			SCR_AIUtilityComponent utility = SCR_AIUtilityComponent.Cast(agent.FindComponent(SCR_AIUtilityComponent));
			if (utility && utility.m_ThreatSystem)
			{
				totalThreat = totalThreat + utility.m_ThreatSystem.GetThreatMeasure();
			}

			if (IsOutOfAmmo(character)) outOfAmmoCount++;
		}

		if (state.m_iInitialCount < aliveCount) state.m_iInitialCount = aliveCount;
		if (state.m_iInitialCount <= 0) return 0;

		float score = 0;

		// 1. Casualty ratio
		float casualtyRatio = (state.m_iInitialCount - aliveCount) / state.m_iInitialCount;
		if (casualtyRatio >= s_fCasualtyThreshold) score = score + 0.40;
		if (casualtyRatio >= 0.75) score = score + 0.25;

		// 2. Leader loss penalty
		if (!leaderAlive && state.m_iInitialCount > 1) score = score + 0.25;

		// 3. Average threat / suppression
		if (aliveCount > 0)
		{
			float avgThreat = totalThreat / aliveCount;
			if (avgThreat >= s_fSuppressionThreshold) score = score + 0.25;
			if (avgThreat >= 0.85) score = score + 0.15;
		}

		// 4. Ammunition status
		if (aliveCount > 0)
		{
			float ammoEmptyRatio = outOfAmmoCount / aliveCount;
			if (ammoEmptyRatio >= 0.50) score = score + 0.20;
		}

		// 5. Critical lone survivor
		if (aliveCount == 1 && state.m_iInitialCount > 1) score = score + 0.30;

		return score;
	}

	// ---------------------------------------------------------------- retrograde destination

	static vector CalculateRetrogradeDestination(vector squadCenter, vector threatCenter, float distance)
	{
		vector away = squadCenter - threatCenter;
		away[1] = 0;
		if (away.LengthSq() < 0.01)
		{
			away = Vector(0, 0, -1);
		}
		else
		{
			away.Normalize();
		}

		vector dest = squadCenter + away * distance;
		dest = BBB.Ground(dest);

		// If water blocks the direct retreat, check lateral 30 deg detours
		if (BBB_Terrain.IsWaterAt(dest))
		{
			vector lateral = Vector(-away[2], 0, away[0]);
			vector altDest = squadCenter + (away * 0.866 + lateral * 0.5) * distance;
			altDest = BBB.Ground(altDest);
			if (!BBB_Terrain.IsWaterAt(altDest)) return altDest;

			altDest = squadCenter + (away * 0.866 - lateral * 0.5) * distance;
			altDest = BBB.Ground(altDest);
			if (!BBB_Terrain.IsWaterAt(altDest)) return altDest;
		}

		return dest;
	}

	// ---------------------------------------------------------------- withdrawal execution

	protected static void StartWithdrawal(BBB_GroupMoraleState state, vector threatCenter, float now)
	{
		IEntity anchor = BBB.AnchorOf(state.m_Group);
		if (!anchor) return;

		vector squadCenter = anchor.GetOrigin();
		state.m_vThreatCenter = threatCenter;
		state.m_vWithdrawPos = CalculateRetrogradeDestination(squadCenter, threatCenter, s_fRetrogradeBoundDist);
		state.m_bWithdrawing = true;
		state.m_fBrokenSince = now;
		state.m_fPhaseSince = now;
		state.m_fTotalWithdrawn = 0;
		state.m_bContactBroken = false;
		state.m_iPhase = 0;
		state.m_iCoverTeam = 0;

		s_iWithdrawalsStarted++;
		PrintFormat("[BBB] MORALE: Squad %1 broke contact (score=%2, initial=%3), withdrawing away from %4 to %5",
			BBB.s_Hooks.NameOf(state.m_Group), state.m_fMorale, state.m_iInitialCount, threatCenter, state.m_vWithdrawPos);

		ExecuteBoundingPhase(state);
	}

	protected static void ExecuteBoundingPhase(BBB_GroupMoraleState state)
	{
		state.m_TeamCover.Clear();
		state.m_TeamMove.Clear();

		array<AIAgent> agents = {};
		BBB.s_Hooks.GetMembers(state.m_Group, agents);

		array<SCR_AIUtilityComponent> consciousMembers = {};
		foreach (AIAgent agent : agents)
		{
			if (!agent) continue;
			IEntity ent = agent.GetControlledEntity();
			if (!ent || !BBB.IsAlive(ent)) continue;
			ChimeraCharacter character = ChimeraCharacter.Cast(ent);
			if (!character || character.GetCharacterController().IsUnconscious()) continue;

			SCR_AIUtilityComponent utility = SCR_AIUtilityComponent.Cast(agent.FindComponent(SCR_AIUtilityComponent));
			if (!utility || !utility.m_CombatMoveState || BBB_OrderIntent.Blocks(utility)) continue;

			consciousMembers.Insert(utility);
		}

		if (consciousMembers.IsEmpty())
		{
			state.m_bWithdrawing = false;
			return;
		}

		// Split into two logical elements for bounding overwatch
		int half = consciousMembers.Count() / 2;
		if (half == 0) half = 1;

		for (int i = 0; i < consciousMembers.Count(); i++)
		{
			SCR_AIUtilityComponent u = consciousMembers[i];
			if (state.m_iCoverTeam == 0)
			{
				if (i < half) state.m_TeamCover.Insert(u);
				else state.m_TeamMove.Insert(u);
			}
			else
			{
				if (i < half) state.m_TeamMove.Insert(u);
				else state.m_TeamCover.Insert(u);
			}
		}

		// Team Move bounds back to destination
		foreach (SCR_AIUtilityComponent mover : state.m_TeamMove)
		{
			SCR_AICombatMoveRequest_Move req = new SCR_AICombatMoveRequest_Move();
			req.m_vMovePos = state.m_vWithdrawPos;
			req.m_vTargetPos = state.m_vThreatCenter + Vector(0, 1.4, 0);
			req.m_eDirection = SCR_EAICombatMoveDirection.CUSTOM_POS;
			req.m_fCoverSearchDistMin = 4.0;
			req.m_fCoverSearchDistMax = 20.0;
			req.m_bTryFindCover = true;
			req.m_bFailIfNoCover = false;
			req.m_eMovementType = EMovementType.RUN;
			req.m_eStanceMoving = ECharacterStance.CROUCH;
			req.m_eStanceEnd = ECharacterStance.CROUCH;
			req.m_bAimAtTarget = true;
			req.m_bAimAtTargetEnd = true;
			req.m_fMoveDuration_s = BOUND_TIMEOUT_S;

			mover.m_CombatMoveState.ApplyNewRequest(req);
		}

		// Team Cover establishes overwatch and faces threat
		foreach (SCR_AIUtilityComponent coverer : state.m_TeamCover)
		{
			SCR_AICombatMoveRequest_Stop stopReq = new SCR_AICombatMoveRequest_Stop();
			stopReq.m_bAimAtTarget = true;
			stopReq.m_bAimAtTargetEnd = true;
			coverer.m_CombatMoveState.ApplyNewRequest(stopReq);
		}

		s_iWithdrawalBounds++;
	}

	protected static void UpdateWithdrawal(BBB_GroupMoraleState state, float now)
	{
		float phaseElapsed = now - state.m_fPhaseSince;

		// Check if moving team arrived or phase timed out
		bool allArrived = true;
		foreach (SCR_AIUtilityComponent mover : state.m_TeamMove)
		{
			if (!mover || !mover.m_OwnerEntity || !BBB.IsAlive(mover.m_OwnerEntity)) continue;
			float distToDest = vector.DistanceXZ(mover.m_OwnerEntity.GetOrigin(), state.m_vWithdrawPos);
			if (distToDest > 6.0 && mover.m_CombatMoveState.IsMoving())
			{
				allArrived = false;
				break;
			}
		}

		if (allArrived || phaseElapsed >= BOUND_TIMEOUT_S)
		{
			state.m_fTotalWithdrawn = state.m_fTotalWithdrawn + s_fRetrogradeBoundDist;
			state.m_iPhase++;
			state.m_fPhaseSince = now;

			// Check termination criteria
			vector threatCenter;
			bool hasThreat = GetThreatCentroid(state.m_Group, threatCenter);

			IEntity anchor = BBB.AnchorOf(state.m_Group);
			float currentDistToThreat = 999.0;
			if (anchor && hasThreat)
			{
				currentDistToThreat = vector.DistanceXZ(anchor.GetOrigin(), threatCenter);
			}

			// Termination 1: Max withdrawal distance reached
			// Termination 2: Threat distance safely opened
			// Termination 3: No threat detected anymore
			if (state.m_fTotalWithdrawn >= s_fMaxWithdrawDist || currentDistToThreat >= BREAK_CONTACT_DIST_M || !hasThreat)
			{
				state.m_bWithdrawing = false;
				state.m_bContactBroken = true;
				s_iContactsBroken++;
				PrintFormat("[BBB] MORALE: Squad %1 successfully broke contact (total withdrawn=%2 m, threat dist=%3 m)",
					BBB.s_Hooks.NameOf(state.m_Group), state.m_fTotalWithdrawn, currentDistToThreat);
				return;
			}

			// Continue withdrawal: swap roles and bound again
			state.m_iCoverTeam = 1 - state.m_iCoverTeam;
			vector squadCenter = anchor.GetOrigin();
			state.m_vThreatCenter = threatCenter;
			state.m_vWithdrawPos = CalculateRetrogradeDestination(squadCenter, threatCenter, s_fRetrogradeBoundDist);

			ExecuteBoundingPhase(state);
		}
	}

	// ---------------------------------------------------------------- surrender

	protected static void CheckSurrender(ChimeraCharacter soldier, SCR_AIUtilityComponent utility)
	{
		if (!s_bSurrenderOn || !soldier || !utility || !utility.m_CombatComponent || !utility.m_PerceptionComponent) return;
		if (BBB.PlayerOf(soldier) || BBB_OrderIntent.Blocks(utility)) return;

		CharacterControllerComponent controller = soldier.GetCharacterController();
		if (!controller || controller.IsDead() || controller.IsUnconscious() || controller.IsPlayerControlled()) return;

		// Already surrendered
		foreach (BBB_SurrenderState existing : s_Surrendered)
		{
			if (existing && existing.m_Soldier == soldier) return;
		}

		// Condition 1: Must be completely out of ammunition
		if (!IsOutOfAmmo(soldier)) return;

		// Condition 2: High threat/suppression
		if (!utility.m_ThreatSystem || utility.m_ThreatSystem.GetThreatMeasure() < SURRENDER_THREAT_MIN) return;

		// Condition 3: Close enemy with direct exposure
		array<BaseTarget> targets = {};
		utility.m_PerceptionComponent.GetTargetsList(targets, ETargetCategory.ENEMY);
		bool closeEnemyFound = false;
		vector soldierPos = soldier.GetOrigin();

		foreach (BaseTarget target : targets)
		{
			if (!target || target.GetTargetCategory() != ETargetCategory.ENEMY) continue;
			if (target.GetDistance() <= SURRENDER_DIST_MAX && target.GetTraceFraction() >= 0.8)
			{
				closeEnemyFound = true;
				break;
			}
		}

		if (!closeEnemyFound) return;

		// Execute surrender
		ApplySurrender(soldier, utility, controller);
	}

	protected static void ApplySurrender(ChimeraCharacter soldier, SCR_AIUtilityComponent utility, CharacterControllerComponent controller)
	{
		if (!soldier || !utility || !controller) return;

		// 1. Hold fire
		utility.m_CombatComponent.SetCombatMode(EAIGroupCombatMode.HOLD_FIRE);

		// 2. Cancel active combat movement
		if (utility.m_CombatMoveState)
		{
			utility.m_CombatMoveState.CancelRequest();
		}

		// 3. Lower weapon and holster if possible
		controller.SetWeaponRaised(false);
		controller.TryEquipRightHandItem(null, EEquipItemType.EEquipTypeWeapon, false);

		// 4. Stance crouch / surrender posture
		SCR_AIStanceHandling.SetStance(controller, ECharacterStance.CROUCH);

		// 5. Deactivate AI so the surrendered soldier stays in place quietly
		AIControlComponent ai = AIControlComponent.Cast(soldier.FindComponent(AIControlComponent));
		if (ai && ai.IsAIActivated()) ai.DeactivateAI();

		// 5. Store state
		BBB_SurrenderState sState = new BBB_SurrenderState();
		sState.m_Soldier = soldier;
		sState.m_Utility = utility;
		sState.m_fSurrenderSince = GetGame().GetWorld().GetWorldTime();
		sState.m_bDisarmed = true;
		s_Surrendered.Insert(sState);

		s_iSurrenders++;
		PrintFormat("[BBB] MORALE: Soldier %1 surrendered (out of ammo, suppressed, enemy within %2 m)",
			soldier, SURRENDER_DIST_MAX);
	}

	// ---------------------------------------------------------------- main tick

	static void Tick()
	{
		Read();
		if (!s_bOn) return;

		float now = GetGame().GetWorld().GetWorldTime() / 1000.0;

		array<SCR_AIGroup> groups = {};
		BBB.s_Hooks.GetGroups(groups);

		// Clean up invalid states
		for (int i = s_States.Count() - 1; i >= 0; i--)
		{
			BBB_GroupMoraleState gs = s_States[i];
			if (!gs || !gs.m_Group || !groups.Contains(gs.m_Group))
			{
				s_States.RemoveOrdered(i);
			}
		}

		// Clean up surrendered soldiers
		for (int sIdx = s_Surrendered.Count() - 1; sIdx >= 0; sIdx--)
		{
			BBB_SurrenderState ss = s_Surrendered[sIdx];
			if (!ss || !ss.m_Soldier || !BBB.IsAlive(ss.m_Soldier))
			{
				s_Surrendered.RemoveOrdered(sIdx);
			}
		}

		foreach (SCR_AIGroup group : groups)
		{
			if (!group || BBB.HasPlayer(group) || !BBB.s_Hooks.AllowMorale(group)) continue;

			// Don't manage mounted vehicle squads
			IEntity vehicle = BBB_Transport.CrewedVehicle(group);
			if (vehicle) continue;

			array<AIAgent> agents = {};
			BBB.s_Hooks.GetMembers(group, agents);
			if (agents.IsEmpty()) continue;

			BBB_GroupMoraleState state = GetOrCreateState(group, agents.Count());
			if (!state) continue;

			// Active withdrawal update
			if (state.m_bWithdrawing)
			{
				UpdateWithdrawal(state, now);
				continue;
			}

			// Throttled assessment
			if (now < state.m_fNextCheck) continue;
			state.m_fNextCheck = now + (CHECK_INTERVAL_MS / 1000.0);

			int aliveCount;
			int outOfAmmoCount;
			state.m_fMorale = CalculateGroupMorale(state, agents, aliveCount, outOfAmmoCount);

			// Check surrender for lone survivors
			if (aliveCount == 1 && s_bSurrenderOn)
			{
				foreach (AIAgent a : agents)
				{
					if (!a) continue;
					ChimeraCharacter ch = ChimeraCharacter.Cast(a.GetControlledEntity());
					if (ch && BBB.IsAlive(ch))
					{
						SCR_AIUtilityComponent u = SCR_AIUtilityComponent.Cast(a.FindComponent(SCR_AIUtilityComponent));
						if (u) CheckSurrender(ch, u);
					}
				}
			}

			// Check if morale broke and threat exists
			if (state.m_fMorale >= 0.70 && aliveCount > 0)
			{
				vector threatCenter;
				if (GetThreatCentroid(group, threatCenter))
				{
					StartWithdrawal(state, threatCenter, now);
				}
			}
		}
	}

	static void Stop()
	{
		// Restore AI for surrendered soldiers
		foreach (BBB_SurrenderState ss : s_Surrendered)
		{
			if (ss && ss.m_Soldier && BBB.IsAlive(ss.m_Soldier))
			{
				AIControlComponent ai = AIControlComponent.Cast(ss.m_Soldier.FindComponent(AIControlComponent));
				if (ai && !ai.IsAIActivated()) ai.ActivateAI();
			}
		}

		s_Surrendered.Clear();
		s_States.Clear();
	}
}
