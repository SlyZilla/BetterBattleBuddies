// Better Battle Buddies (BBB): Autonomous static weapon crewing during contact.
//
// Standalone tactical behavior for idle or defending autonomous infantry squads:
// 1. Static weapon discovery: when an autonomous infantry squad detects enemy contact,
//    foot soldiers scan nearby positions (<= 35 m) for operational, loaded, friendly
//    static weapons (or stationary defensible turrets).
// 2. Traverse arc check: the static weapon is ONLY crewed if a known perceived enemy
//    falls within its traverse arc (default <= 60 deg from weapon centerline) and
//    within viable engagement range (5 to 500 m). Weapons facing away from threats
//    are ignored.
// 3. Crew assignment: assigns the closest fit squadmate on foot (preferring non-leaders),
//    boarding via native compartment actions and activating AI gunner control.
// 4. Release when ordered on: the gunner dismounts and rejoins the squad on foot when
//    the squad receives an explicit move order (BBB_OrderIntent.Blocks), moves away
//    (> 45 m tether), the target sector stays quiet for a sustained period (15 s),
//    ammunition is exhausted, or the weapon is destroyed.
// 5. Constraints: never overrides player/GM orders, player control, or host vehicle
//    hooks; never mounts enemy-faction weapons; budget <= 0.1 ms/tick.

class BBB_StaticGunnerJob : Managed
{
	SCR_AIGroup m_Group;
	ChimeraCharacter m_Gunner;
	IEntity m_StaticWeapon;
	BaseCompartmentSlot m_Slot;
	float m_fCrewedSince;
	float m_fLastThreatInArcMs;
	bool m_bMounted;
}

class BBB_StaticWeapons
{
	static bool s_bRead;
	static bool s_bOn;

	// Tunable parameters
	static float s_fSearchRadiusM = 35.0;
	static float s_fMaxArcDeg = 60.0;
	static float s_fQuietTimeoutS = 15.0;
	static const float CHECK_INTERVAL_MS = 2000.0;
	static const float MAX_TETHER_DIST_M = 45.0;
	static const float MAX_ENGAGE_DIST_M = 500.0;
	static const float MIN_ENGAGE_DIST_M = 5.0;

	// Active crewed jobs
	protected static ref array<ref BBB_StaticGunnerJob> s_aJobs = new array<ref BBB_StaticGunnerJob>();

	// Per-group evaluation throttle: Group -> next allowed check time in ms
	protected static ref map<SCR_AIGroup, float> s_mNextCheck = new map<SCR_AIGroup, float>();

	// Scratch query buffer
	protected static ref array<IEntity> s_aQueriedEntities = new array<IEntity>();

	// Performance and test counters
	static int s_iWeaponsCrewed;
	static int s_iWeaponsReleased;
	static int s_iScanTicks;

	// ---------------------------------------------------------------- configuration

	static void Read()
	{
		if (s_bRead) return;
		s_bRead = true;

		string value;
		if (System.GetCLIParam("bbbStaticWeapons", value))
		{
			s_bOn = value == "1";
		}

		if (System.GetCLIParam("bbbStaticReach", value))
		{
			float reach = value.ToFloat();
			if (reach >= 10.0 && reach <= 80.0) s_fSearchRadiusM = reach;
		}

		if (System.GetCLIParam("bbbStaticArc", value))
		{
			float arc = value.ToFloat();
			if (arc >= 15.0 && arc <= 180.0) s_fMaxArcDeg = arc;
		}

		if (System.GetCLIParam("bbbStaticQuiet", value))
		{
			float quiet = value.ToFloat();
			if (quiet >= 5.0 && quiet <= 120.0) s_fQuietTimeoutS = quiet;
		}

		PrintFormat("[BBB] STATIC WEAPONS enabled=%1 reach=%2m arc=%3deg quiet=%4s",
			s_bOn, s_fSearchRadiusM, s_fMaxArcDeg, s_fQuietTimeoutS);
	}

	static void Stop()
	{
		// Dismount all active gunners cleanly upon BBB stop
		for (int i = s_aJobs.Count() - 1; i >= 0; i--)
		{
			BBB_StaticGunnerJob job = s_aJobs[i];
			if (job) DismountGunner(job, "BBB stopped");
		}
		s_aJobs.Clear();
		s_mNextCheck.Clear();
		s_aQueriedEntities.Clear();
		s_iWeaponsCrewed = 0;
		s_iWeaponsReleased = 0;
		s_iScanTicks = 0;
	}

	// ---------------------------------------------------------------- geometry and bearing

	// Signed bearing from forward vector to target direction in degrees [-180, 180]
	static float Bearing(vector forward, vector direction)
	{
		forward[1] = 0;
		direction[1] = 0;
		forward.Normalize();
		direction.Normalize();
		return Math.Atan2(forward[2] * direction[0] - forward[0] * direction[2], vector.Dot(forward, direction)) * Math.RAD2DEG;
	}

	// Check if a target position falls within the weapon forward traverse arc
	static bool IsInTraverseArc(IEntity weapon, vector targetPos, float maxArcDeg)
	{
		if (!weapon) return false;
		vector forward = weapon.GetTransformAxis(2);
		forward[1] = 0;
		if (forward.LengthSq() < 0.001) forward = "0 0 1";
		else forward.Normalize();

		vector toTarget = targetPos - weapon.GetOrigin();
		toTarget[1] = 0;
		float dist = toTarget.Length();
		if (dist < MIN_ENGAGE_DIST_M || dist > MAX_ENGAGE_DIST_M) return false;

		toTarget.Normalize();
		float angle = Math.AbsFloat(Bearing(forward, toTarget));
		return angle <= maxArcDeg;
	}

	// Check if any perceived threat known to the squad is within the weapon traverse arc
	static bool HasThreatInArc(SCR_AIGroup group, IEntity weapon, float maxArcDeg)
	{
		if (!group || !weapon) return false;
		vector forward = weapon.GetTransformAxis(2);
		forward[1] = 0;
		if (forward.LengthSq() < 0.001) forward = "0 0 1";
		else forward.Normalize();

		vector weaponPos = weapon.GetOrigin();
		array<AIAgent> agents = {};
		BBB.s_Hooks.GetMembers(group, agents);

		foreach (AIAgent agent : agents)
		{
			if (!agent) continue;
			SCR_AIUtilityComponent u = SCR_AIUtilityComponent.Cast(agent.FindComponent(SCR_AIUtilityComponent));
			if (!u) continue;

			// 1. Current selected target
			if (u.m_CombatComponent)
			{
				BaseTarget cur = u.m_CombatComponent.GetCurrentTarget();
				if (cur && cur.GetTargetCategory() == ETargetCategory.ENEMY)
				{
					vector curPos = cur.GetLastDetectedPosition();
					vector toCur = curPos - weaponPos;
					toCur[1] = 0;
					float dCur = toCur.Length();
					if (dCur >= MIN_ENGAGE_DIST_M && dCur <= MAX_ENGAGE_DIST_M)
					{
						float bCur = Math.AbsFloat(Bearing(forward, toCur));
						if (bCur <= maxArcDeg) return true;
					}
				}
			}

			// 2. Perceived enemy target list
			if (u.m_PerceptionComponent)
			{
				array<BaseTarget> targets = {};
				u.m_PerceptionComponent.GetTargetsList(targets, ETargetCategory.ENEMY);
				foreach (BaseTarget tgt : targets)
				{
					if (!tgt || tgt.GetTargetCategory() != ETargetCategory.ENEMY) continue;
					if (tgt.GetTimeSinceDetected() > 20.0) continue;

					vector tPos = tgt.GetLastDetectedPosition();
					vector toT = tPos - weaponPos;
					toT[1] = 0;
					float dist = toT.Length();
					if (dist >= MIN_ENGAGE_DIST_M && dist <= MAX_ENGAGE_DIST_M)
					{
						float b = Math.AbsFloat(Bearing(forward, toT));
						if (b <= maxArcDeg) return true;
					}
				}
			}
		}

		return false;
	}

	// ---------------------------------------------------------------- weapon eligibility

	// Identify static weapons and stationary turrets with operational ammo
	static bool IsStaticWeapon(IEntity entity, out BaseCompartmentSlot outTurretSlot)
	{
		outTurretSlot = null;
		if (!entity || BBB.Health(entity) <= 0) return false;

		SCR_BaseCompartmentManagerComponent compMgr = SCR_BaseCompartmentManagerComponent.Cast(entity.FindComponent(SCR_BaseCompartmentManagerComponent));
		if (!compMgr) return false;

		array<BaseCompartmentSlot> slots = {};
		BBB_VehicleHelpers.GetVehicleCompartments(entity, slots);

		BaseCompartmentSlot turretSlot = null;
		bool hasPilot = false;

		foreach (BaseCompartmentSlot s : slots)
		{
			if (!s) continue;
			if (s.GetType() == ECompartmentType.PILOT)
			{
				hasPilot = true;
			}
			else if (s.GetType() == ECompartmentType.TURRET && !turretSlot)
			{
				turretSlot = s;
			}
		}

		if (!turretSlot) return false;

		// Static weapons have a turret seat and no driver seat
		// If it has a pilot seat, it must be completely stationary and unpowered
		if (hasPilot)
		{
			CarControllerComponent car = CarControllerComponent.Cast(entity.FindComponent(CarControllerComponent));
			if (car && car.IsEngineOn()) return false;
			IEntity driver = BBB.DriverOf(entity);
			if (driver) return false;
		}

		// Must have live ammunition
		if (!BBB_VehicleHelpers.TurretHasAmmo(turretSlot)) return false;

		// Seat must be unoccupied or occupant incapacitated
		IEntity occupant = turretSlot.GetOccupant();
		if (occupant)
		{
			if (BBB.PlayerOf(occupant) || BBB_VehicleHelpers.IsAble(occupant)) return false;
		}

		outTurretSlot = turretSlot;
		return true;
	}

	// Check if a weapon's faction is compatible (friendly or neutral; never enemy)
	static bool IsFactionCompatible(SCR_AIGroup group, IEntity weapon)
	{
		if (!group || !weapon) return false;
		Faction squadFac = group.GetFaction();
		if (!squadFac) return true;

		FactionAffiliationComponent aff = FactionAffiliationComponent.Cast(weapon.FindComponent(FactionAffiliationComponent));
		if (!aff) return true;

		Faction weaponFac = aff.GetAffiliatedFaction();
		if (!weaponFac) weaponFac = aff.GetDefaultAffiliatedFaction();
		if (!weaponFac) return true;

		// If weapon belongs to an enemy faction, do not crew it
		if (squadFac.IsFactionEnemy(weaponFac)) return false;

		return true;
	}

	// ---------------------------------------------------------------- candidate gunner selection

	// Check if a soldier is fit to crew a static weapon
	static bool IsFitGunner(ChimeraCharacter soldier, SCR_AIUtilityComponent utility)
	{
		if (!soldier || !utility) return false;
		if (!BBB.IsAlive(soldier)) return false;

		CharacterControllerComponent controller = soldier.GetCharacterController();
		if (!controller || controller.IsDead() || controller.IsUnconscious() || controller.IsPlayerControlled()) return false;
		if (controller.IsReloading() || controller.IsChangingItem() || controller.IsClimbing() || controller.IsSwimming() || controller.GetDisableWeaponControls()) return false;

		if (soldier.IsInVehicle()) return false;
		if (utility.m_AIInfo && (utility.m_AIInfo.HasUnitState(EUnitState.IN_VEHICLE) || utility.m_AIInfo.HasUnitState(EUnitState.UNCONSCIOUS))) return false;

		if (BBB_OrderIntent.Blocks(utility)) return false;
		if (BBB_Drag.Busy(soldier)) return false;

		return true;
	}

	// Find best squadmate on foot to crew the weapon (prefer non-leaders close to weapon)
	static ChimeraCharacter FindBestCandidate(SCR_AIGroup group, IEntity weapon)
	{
		if (!group || !weapon) return null;

		array<AIAgent> agents = {};
		BBB.s_Hooks.GetMembers(group, agents);
		IEntity leaderEnt = group.GetLeaderEntity();

		ChimeraCharacter bestCandidate = null;
		float bestDist = s_fSearchRadiusM;
		ChimeraCharacter leaderCandidate = null;
		float leaderDist = s_fSearchRadiusM;

		vector weaponPos = weapon.GetOrigin();

		foreach (AIAgent agent : agents)
		{
			if (!agent) continue;
			IEntity controlled = agent.GetControlledEntity();
			if (!controlled) continue;

			ChimeraCharacter character = ChimeraCharacter.Cast(controlled);
			if (!character) continue;

			SCR_AIUtilityComponent util = SCR_AIUtilityComponent.Cast(agent.FindComponent(SCR_AIUtilityComponent));
			if (!util || !IsFitGunner(character, util)) continue;

			float d = vector.DistanceXZ(character.GetOrigin(), weaponPos);
			if (d > s_fSearchRadiusM) continue;

			if (controlled == leaderEnt)
			{
				if (d < leaderDist)
				{
					leaderDist = d;
					leaderCandidate = character;
				}
			}
			else
			{
				if (d < bestDist)
				{
					bestDist = d;
					bestCandidate = character;
				}
			}
		}

		if (bestCandidate) return bestCandidate;
		return leaderCandidate;
	}

	// ---------------------------------------------------------------- mounting & dismounting

	// Board static weapon
	static bool CrewWeapon(SCR_AIGroup group, ChimeraCharacter gunner, IEntity weapon, BaseCompartmentSlot slot)
	{
		if (!group || !gunner || !weapon || !slot) return false;

		// Eject incapacitated occupant if present
		IEntity occ = slot.GetOccupant();
		if (occ && occ != gunner)
		{
			ChimeraCharacter occCh = ChimeraCharacter.Cast(occ);
			if (occCh)
			{
				CompartmentAccessComponent occAcc = occCh.GetCompartmentAccessComponent();
				if (occAcc) occAcc.GetOutVehicle(EGetOutType.TELEPORT, -1, ECloseDoorAfterActions.LEAVE_OPEN, false);
			}
		}

		CompartmentAccessComponent acc = gunner.GetCompartmentAccessComponent();
		if (!acc) return false;

		bool ok = acc.GetInVehicle(weapon, slot, true, -1, ECloseDoorAfterActions.LEAVE_OPEN, false);
		if (!ok) return false;

		// Activate AI
		AIControlComponent ai = AIControlComponent.Cast(gunner.FindComponent(AIControlComponent));
		if (ai && !ai.IsAIActivated()) ai.ActivateAI();

		BBB_StaticGunnerJob job = new BBB_StaticGunnerJob();
		job.m_Group = group;
		job.m_Gunner = gunner;
		job.m_StaticWeapon = weapon;
		job.m_Slot = slot;
		float now = GetGame().GetWorld().GetWorldTime();
		job.m_fCrewedSince = now;
		job.m_fLastThreatInArcMs = now;
		job.m_bMounted = true;
		s_aJobs.Insert(job);

		s_iWeaponsCrewed++;
		PrintFormat("[BBB] STATIC: %1 (%2) crewing static weapon %3",
			gunner, BBB.s_Hooks.NameOf(group), weapon);
		return true;
	}

	// Dismount static weapon and release gunner back to squad
	protected static void DismountGunner(BBB_StaticGunnerJob job, string reason)
	{
		if (!job || !job.m_Gunner) return;

		CompartmentAccessComponent acc = job.m_Gunner.GetCompartmentAccessComponent();
		if (acc && job.m_Slot && acc.GetCompartment() == job.m_Slot)
		{
			acc.GetOutVehicle(EGetOutType.TELEPORT, -1, ECloseDoorAfterActions.LEAVE_OPEN, false);
		}

		s_iWeaponsReleased++;
		PrintFormat("[BBB] STATIC: %1 released static weapon %2 (reason=%3)",
			job.m_Gunner, job.m_StaticWeapon, reason);
	}

	// ---------------------------------------------------------------- sphere query filter

	protected static bool CollectStaticWeapon(IEntity entity)
	{
		if (!entity || BBB.Health(entity) <= 0) return true;
		s_aQueriedEntities.Insert(entity);
		return true;
	}

	// ---------------------------------------------------------------- main tick

	static void Tick()
	{
		Read();
		if (!s_bOn && s_aJobs.IsEmpty()) return;

		float nowMs = GetGame().GetWorld().GetWorldTime();
		s_iScanTicks++;

		// 1. Maintain and release active jobs
		for (int j = s_aJobs.Count() - 1; j >= 0; j--)
		{
			BBB_StaticGunnerJob job = s_aJobs[j];
			if (!job)
			{
				s_aJobs.RemoveOrdered(j);
				continue;
			}

			// Condition A: Gunner dead or incapacitated
			if (!job.m_Gunner || !BBB.IsAlive(job.m_Gunner))
			{
				s_aJobs.RemoveOrdered(j);
				continue;
			}

			// Condition B: Static weapon destroyed
			if (!job.m_StaticWeapon || BBB.Health(job.m_StaticWeapon) <= 0)
			{
				DismountGunner(job, "weapon destroyed");
				s_aJobs.RemoveOrdered(j);
				continue;
			}

			// Condition C: Gunner left weapon on their own
			CompartmentAccessComponent gAcc = job.m_Gunner.GetCompartmentAccessComponent();
			if (!gAcc || gAcc.GetCompartment() != job.m_Slot)
			{
				s_aJobs.RemoveOrdered(j);
				continue;
			}

			// Condition D: Weapon out of ammunition
			if (!BBB_VehicleHelpers.TurretHasAmmo(job.m_Slot))
			{
				DismountGunner(job, "out of ammo");
				s_aJobs.RemoveOrdered(j);
				continue;
			}

			// Condition E: Explicit player/GM order on the gunner
			SCR_AIUtilityComponent gUtil = SCR_AIUtilityComponent.Cast(job.m_Gunner.FindComponent(SCR_AIUtilityComponent));
			if (gUtil && BBB_OrderIntent.Blocks(gUtil))
			{
				DismountGunner(job, "player/GM order");
				s_aJobs.RemoveOrdered(j);
				continue;
			}

			// Condition F: Squad tether distance exceeded (squad moved away)
			IEntity anchor = BBB.AnchorOf(job.m_Group);
			if (anchor && vector.DistanceXZ(anchor.GetOrigin(), job.m_StaticWeapon.GetOrigin()) > MAX_TETHER_DIST_M)
			{
				DismountGunner(job, "squad moved on");
				s_aJobs.RemoveOrdered(j);
				continue;
			}

			// Condition G: Threat in arc evaluation & quiet timeout
			if (HasThreatInArc(job.m_Group, job.m_StaticWeapon, s_fMaxArcDeg))
			{
				job.m_fLastThreatInArcMs = nowMs;
			}
			else
			{
				float quietElapsed = nowMs - job.m_fLastThreatInArcMs;
				if (quietElapsed >= (s_fQuietTimeoutS * 1000.0))
				{
					DismountGunner(job, "quiet timeout / target cleared");
					s_aJobs.RemoveOrdered(j);
					continue;
				}
			}
		}

		if (!s_bOn) return;

		// 2. Discover and crew nearby static weapons for autonomous squads in contact
		array<SCR_AIGroup> groups = {};
		BBB.s_Hooks.GetGroups(groups);

		foreach (SCR_AIGroup group : groups)
		{
			if (!group || BBB.HasPlayer(group)) continue;

			// Check if group already has an active gunner job
			bool alreadyCrewing = false;
			foreach (BBB_StaticGunnerJob activeJob : s_aJobs)
			{
				if (activeJob && activeJob.m_Group == group)
				{
					alreadyCrewing = true;
					break;
				}
			}
			if (alreadyCrewing) continue;

			// Throttled per-group check
			if (s_mNextCheck.Contains(group))
			{
				if (nowMs < s_mNextCheck.Get(group)) continue;
			}
			s_mNextCheck.Set(group, nowMs + CHECK_INTERVAL_MS);

			IEntity groupAnchor = BBB.AnchorOf(group);
			if (!groupAnchor) continue;

			// Check if squad is aware of any perceived enemy threats
			vector threatCentroid;
			if (!BBB_Smoke.GetThreatCentroid(group, threatCentroid)) continue;

			// Scan nearby entities for static weapons
			s_aQueriedEntities.Clear();
			GetGame().GetWorld().QueryEntitiesBySphere(groupAnchor.GetOrigin(), s_fSearchRadiusM, CollectStaticWeapon);

			foreach (IEntity candidate : s_aQueriedEntities)
			{
				BaseCompartmentSlot turretSlot;
				if (!IsStaticWeapon(candidate, turretSlot)) continue;
				if (!IsFactionCompatible(group, candidate)) continue;

				// Check if threat is within weapon's traverse arc
				if (!IsInTraverseArc(candidate, threatCentroid, s_fMaxArcDeg)) continue;

				// Find best foot candidate in squad
				ChimeraCharacter gunnerCandidate = FindBestCandidate(group, candidate);
				if (!gunnerCandidate) continue;

				// Crew the weapon
				if (CrewWeapon(group, gunnerCandidate, candidate, turretSlot))
				{
					break; // Group crewed a weapon
				}
			}
		}
	}
}
