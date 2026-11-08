#pragma once
#include "CharacterBase.h"
#include "Core/Warrior/Flog.hpp"
#include "Core/Utils/UnitEx.hpp"
#include "HPBar.h"
#include "HudLayer.h"

class Tower : public CharacterBase
{
public:
	CREATE_FUNC(Tower);

	bool init()
	{
		RETURN_FALSE_IF(!Sprite::init());

		setAnchorPoint(Vec2(0.5, 0.5));
		scheduleUpdate();

		return true;
	}

	bool isCenterTower()
	{
		return getName() == TowerEnum::KonohaCenter || getName() == TowerEnum::AkatsukiCenter;
	}

	// Base-tower shield (see CharacterBase::isTowerInvincible()): the Center tower is
	// untouchable as long as at least one non-Center tower of ITS OWN group is still
	// alive in _TowerArray (dealloc() erases a tower from that vector the instant it
	// falls, so "present in the array" == "still standing", no extra state needed).
	// Regular (non-Center) towers are never invincible themselves -- only Center reads
	// this override's true branch.
	bool isTowerInvincible() override
	{
		if (!isCenterTower())
			return false;

		for (auto *other : getGameLayer()->_TowerArray)
		{
			if (other == this)
				continue;
			if (other->getGroup() != getGroup())
				continue;
			if (other->isTower() && !static_cast<Tower *>(other)->isCenterTower())
				return true; // a front tower of this group is still up -- Center stays shielded
		}
		return false; // every front tower of this group is down -- Center is now vulnerable
	}

	// Any real (non-blocked) hit lands here -> reset the no-damage idle counter so the
	// Center tower's passive regen (towerRegenTick()) waits out the full idle window again
	// before it starts healing.
	void onTowerDamaged() override
	{
		_towerIdleSeconds = 0;
	}

	// Ticks every second for every tower (cheap: only Center actually does anything).
	// After kTowerRegenIdleSeconds of not taking a real hit, Center heals
	// kTowerRegenPercentPerTick% of its max HP per second until topped up or hit again.
	void towerRegenTick(float dt)
	{
		if (_state == State::DEAD || !isCenterTower())
			return;

		if (_towerIdleSeconds < kTowerRegenIdleSeconds)
		{
			_towerIdleSeconds++;
			return;
		}

		uint32_t maxHp = getMaxHP();
		uint32_t hp = getHP();
		if (hp >= maxHp)
			return;

		uint32_t heal = MAX(maxHp * kTowerRegenPercentPerTick / 100u, 1u);
		setHPValue(MIN(hp + heal, maxHp));
	}

public:
	void setID(const string &name, Role role, Group group)
	{
		clearActionData();
		setName(name);
		setRole(role);
		setGroup(group);

		CCArray *animationArray = CCArray::create();
		const char *filePath;

		if (getName() == TowerEnum::KonohaCenter || getName() == TowerEnum::AkatsukiCenter)
			filePath = "Unit/Tower/CenterData.xml";
		else
			filePath = "Unit/Tower/TowerData.xml";

		KTools::readXMLToArray(filePath, animationArray);

		CCArray *tmpAction = (CCArray *)(animationArray->objectAtIndex(0));
		CCArray *tmpData = (CCArray *)(tmpAction->objectAtIndex(0));
		idleArray = (CCArray *)(tmpAction->objectAtIndex(1));

		string unitName;
		uint32_t maxHP;
		int tmpWidth;
		int tmpHeight;
		uint32_t tmpSpeed;
		int tmpCombatPoint;

		readData(tmpData, unitName, maxHP, tmpWidth, tmpHeight, tmpSpeed, tmpCombatPoint);

		setMaxHPValue(maxHP, false);
		setHPValue(maxHP, false);
		setCKR(0);
		setCKR2(0);
		setHeight(tmpHeight);
		setWalkSpeed(tmpSpeed);

		// init DeadFrame
		tmpAction = (CCArray *)(animationArray->objectAtIndex(6));
		deadArray = (CCArray *)(tmpAction->objectAtIndex(1));

		initAction();
		CCNotificationCenter::sharedNotificationCenter()->addObserver(this, callfuncO_selector(CharacterBase::acceptAttack), "acceptAttack", nullptr);

		schedule(schedule_selector(Tower::towerRegenTick), 1.0f);
	}

	void initAction()
	{
		setIdleAction(createAnimation(idleArray, 5, true, false));
		setDeadAction(createAnimation(deadArray, 10, false, false));
	}

	void setHPbar()
	{
		auto layer = getGameLayer();
		auto player = layer ? layer->currentPlayer : nullptr;
		if (!player || getGroup() != player->getGroup())
			_hpBar = HPBar::create("hp_bar_r.png");
		else
			_hpBar = HPBar::create("hp_bar.png");
		_hpBar->getHPBAR()->setPosition(Vec2(1, 1));
		_hpBar->setPositionY(getHeight());
		_hpBar->setDelegate(this);
		addChild(_hpBar);
	}

	void dealloc()
	{
		unschedule(schedule_selector(CharacterBase::setAI));
		unschedule(schedule_selector(Tower::towerRegenTick));
		setState(State::DEAD);
		stopAllActions();

		getGameLayer()->clearAllFlogsMainTarget(this);

		std::erase(getGameLayer()->_TowerArray, this);
		getGameLayer()->setTowerState(getCharId());
		getGameLayer()->checkTower();
		removeFromParent();
	}

private:
	// Seconds since Center last took a real hit; regen only starts once this reaches
	// kTowerRegenIdleSeconds. Irrelevant for non-Center towers (never read).
	int _towerIdleSeconds = 0;

	static constexpr int kTowerRegenIdleSeconds = 20;      // base must go 20s untouched before it starts healing
	static constexpr uint32_t kTowerRegenPercentPerTick = 1; // heals 1% max HP per second once idle threshold is hit
};
