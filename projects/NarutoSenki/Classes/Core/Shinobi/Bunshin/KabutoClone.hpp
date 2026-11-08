#pragma once
#include "Hero.hpp"

// Dead Soul Army decoy: nAttack-only, generic target priority, expires via
// CharacterBase::setClone() lifetime (cloneTime passed from <e type="setClone">N</e>).
class KabutoClone : public Hero
{
	void perform() override
	{
		_mainTarget = nullptr;
		findFlogHalf();
		if (notFindFlogHalf())
			findHeroHalf();
		if (!_mainTarget)
			findTowerHalf();

		if (!_mainTarget)
		{
			stepOn();
			return;
		}

		Vec2 sp = getDistanceToTarget();

		if (abs(sp.x) > 32 || abs(sp.y) > 32)
		{
			walk(sp.getNormalized());
			return;
		}

		if (isFreeState())
		{
			changeSide(sp);
			attack(NAttack);
		}
	}
};
