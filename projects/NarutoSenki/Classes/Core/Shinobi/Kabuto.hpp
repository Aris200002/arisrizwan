#pragma once
#include "Hero.hpp"
#include "Bunshin/KabutoClone.hpp"
#include "Core/Projectile/Bullet.hpp"

// Kabuto — full skill-set redesign (non-slash-spam kit).
// Engine only exposes 3 skill buttons + 2 ougi slots (SKILL_SYSTEM.md §1: ABType has no
// SKILL4/SKILL5) — the 5 abilities below map 1:1 onto SKILL1/SKILL2/SKILL3/OUGIS1/OUGIS2.
//
// Passive:  Yin Healing Focus         -> CharacterBase::setDamage() victim branch (getName()==Kabuto).
//           LATE-GAME BUFF: dmg window naik dari +8% -> +12% (_kbYinDmgBuffActive) supaya proc-nya
//           tetap terasa berdampak lawan hero ber-DEF tinggi di late game.
// SKILL1:   Chakra Scalpel Activation -> "cBuff" event -> changeAction()/resumeAction(). Pure
//           stat/attackType buff (extra magic dmg + self-heal per hit) for 6s -- NAttack
//           animation stays normal the whole time (fixed; used to wrongly swap to skill1Array).
//           Also refunds a % of CKR/CKR2 so OUGIS1/OUGIS2 come off cooldown sooner.
//           LATE-GAME BUFF: window ini sekarang JUGA menaikkan damage SEMUA serangan Kabuto
//           (bukan cuma NAttack ber-tag kb_scalpel_hit) sebesar +12% selama 6 detik
//           (_kbScalpelDmgBuffActive, lihat CharacterBase::setDamage()) -- dulu SKILL1 cuma
//           berguna sebagai sustain/CKR refund dan kurang masif di late game, sekarang jadi
//           damage-amp window yang layak dipencet sebelum combo/trade.
// SKILL2:   Nerve Strike Dash         -> dash + AoE slash trail, attackType "ns_hit" (slow hook)
// SKILL3:   Dead Soul Vault           -> jump + AoE slam, existing setMove push on hit frame
// OUGIS1:   Dead Soul Jutsu (Possession) -> REDESIGN (dari "spawn clone baru" jadi "hidupkan +
//           kendalikan mayat asli", sesuai anime -- Kabuto tidak menciptakan bunshin, ia
//           menghidupkan kembali jasad musuh/sekutu yang baru tumbang di titik ia mati).
//           Hanya bisa cast kalau ada minimal 1 unit BERTIPE HERO (isPlayerOrCom() -- bukan
//           Mon/Summon/Kugutsu/Guardian/Clone) berstatus ELIMINATED (State::DEAD) dan BELUM
//           dikendalikan puppet lain, dalam radius 96px dari Kabuto (lihat
//           trySpawnDeadSoulPuppet() di bawah). Mayat TERDEKAT yang ketemu langsung dihidupkan
//           DI TEMPAT ia tumbang (reviveCorpseAsPuppet()) -- bukan teleport ke Kabuto/spawn
//           point. Kalau mayatnya MUSUH, di-changeGroup() jadi rekan sementara (persis pola
//           possession Ino/Shintenshin, minus swap kontrol player); kalau mayatnya REKAN SETIM
//           Kabuto sendiri, TIDAK di-changeGroup() sama sekali -- cukup dihidupkan apa adanya
//           (fix bug: dulu changeGroup() unconditional, jadi rekan setim yang dihidupkan malah
//           ke-flip jadi musuh). Status swap-atau-tidak dicatat di _kbPuppetGroupSwapped supaya
//           revert tahu perlu changeGroup() balik atau tidak. Window kontrol berlangsung
//           kKbPuppetControlDuration (20 detik, lihat kabutoPuppetTimeout()) + 3 detik reborn
//           setelahnya (_isSuicide di dead()) = ~23 detik total hidup (di-rebalance dari versi
//           lama 8+3=11 detik yang kerasa kependekan). Selama window itu Kabuto WAJIB tetap
//           dalam radius kKbPuppetLeashRadius dari puppet-nya (kabutoPuppetRangeTick(), tick tiap
//           0.2 detik) -- kalau Kabuto menjauh atau ikut tereliminasi, puppet LANGSUNG balik
//           status dead saat itu juga, tidak nunggu 8 detik habis. Begitu window habis (atau
//           leash putus), puppet direvert TANPA lewat damage (revertPuppetNoCredit()) -> balik
//           grup asal, _isSuicide=true supaya dead() kasih reborn 3 detik (bukan _rebornTime
//           penuh) sebelum unit itu spawn normal lagi sebagai musuh. Kalau puppet malah
//           dihabisi lawan SAAT masih dikendalikan (HP kena damage sampai 0), HPBar::loseHP()
//           yang menangani lewat flag _isKabutoPuppet (bypass generic, lihat patch di
//           HPBar.cpp) -- kedua jalur mati ini SENGAJA TIDAK PERNAH mencatat kill ke siapa pun
//           (baik ke Kabuto maupun ke attacker), karena secara lore ini cuma mayat yang
//           dikendalikan, bukan eliminasi sungguhan. SELAMA puppet masih aktif, OUGIS1 TERKUNCI
//           baik secara logic (attack() override menolak cast lewat isDeadSoulPuppetActive(),
//           berlaku utk AI perform() MAUPUN tombol manual) MAUPUN secara UI (tombol
//           skill4Button->setLock(), pola sama seperti Kiba::changeAction() mengunci
//           skill1Button). Keduanya di-unlock lagi tepat saat puppet direvert, lewat
//           onPuppetReverted() hook (dipanggil dari kedua jalur revert di atas, pola sama
//           seperti onCloneDealloc()). NOTE: hanya SATU mayat dikendalikan per cast (yang
//           TERDEKAT), walau ada 2+ mayat Hero dalam radius -- SENGAJA (anti-spam), bukan bug.
// OUGIS2:   Nehan Shojo: Final Slash  -> skill05 xml, attackType "kb_ult_hit" (true dmg + 4s stun).
//           LATE-GAME BUFF: attackValue per proc dinaikkan 700 -> 850 (~+21%, lihat Kabuto.xml)
//           supaya burst true-damage finisher ini tetap relevan lawan HP/DEF hero late game,
//           masih jauh di bawah attackValue 2600 lama yang bikin one-shot.
//           REVISI mekanik CC: TIDAK lagi 1 bulu di titik target lock saja -> saat OUGIS2
//           di-trigger, Kabuto men-scan SEMUA musuh (getGroup() beda, bukan sekutu) dalam
//           radius 160px dan spawn 1 objek "bulu" (Bullet custom, KabutoFeather.xml) TEPAT di
//           posisi tiap musuh yang kena scan -- termasuk _mainTarget kalau dia masuk radius.
//           Tiap bulu proc instan (attackType "kb_feather_hit") begitu muncul: Stun 1.5s penuh,
//           lanjut Slow 60% speed 2s setelah stun lepas (lihat kabutoFeatherReleaseStun() di
//           patch §3). Dash+circular-slash true-damage (kb_ult_hit, tetap 4s stun) Kabuto
//           SENDIRI tidak berubah, tetap cuma kena _mainTarget sebagai finisher -- jadi kit
//           akhir: AoE CC ringan (bulu) + single-target burst berat (slash Kabuto). Lihat
//           spawnNehanShoujoFeathers() di bawah.
class Kabuto : public Hero
{
	// NOTE: setScale() koreksi visual (dulu 0.82f, lihat riwayat) SUDAH DIHAPUS -- sprite sheet
	// resprite kedua (Kabuto_frames__7_/__8_.json + 1000234057.png) sudah di-export pada kanvas
	// yang PAS dengan proporsi roster lain (Idle 38x75px vs median roster ~75px, lihat komentar
	// "RESPRITE #2" di Kabuto.xml), jadi transform tambahan tidak dibutuhkan lagi. Kalau Kabuto
	// suatu saat resprite ulang dan keliatan salah ukuran lagi, cek dulu sourceSize di
	// Kabuto.plist vs karakter lain di Resources/Unit/Ninja/ sebelum menambah setScale() lagi --
	// setScale cuma transform render, TIDAK ikut mengecilkan getContentSize() yang dipakai untuk
	// hit-box (CharacterBase::acceptAttack), jadi lebih baik dihindari kalau bisa export ulang
	// asset dengan kanvas yang sudah benar dari awal.

	// Start the passive idle-timer tick right after standard XML load, same style as
	// doAI() scheduling setAI() every 0.1s (AI_BEHAVIOR_SYSTEM.md §2).
	void setID(const string &name, Role role, Group group) override
	{
		Hero::setID(name, role, group);
		schedule(schedule_selector(CharacterBase::kabutoPassiveTick), 0.1f);
	}

	void perform() override
	{
		_mainTarget = nullptr;
		findHeroHalf();

		tryBuyGear(GearType::Gear03, GearType::Gear01, GearType::Gear02);

		if (needBackToTowerToRestoreHP() ||
			needBackToDefendTower())
			return;

		if (_mainTarget && _mainTarget->isNotFlog())
		{
			Vec2 moveDirection;
			Vec2 sp = getDistanceToTarget();

			if (isFreeState())
			{
				// OUGIS2: finisher — lock+burst once CKR2 ready and target is close enough to lock
				if (_isCanOugis2 && !_isControlled && getGameLayer()->_isOugis2Game && !_isArmored)
				{
					if (abs(sp.x) > 96 || abs(sp.y) > 32)
					{
						moveDirection = sp.getNormalized();
						walk(moveDirection);
						return;
					}
					// Corpse-scan... err, feather-scan+spawn sekarang di attack() override (lihat
					// centralized guard di bawah) supaya jalur manual (tombol skill5 ->
					// GameLayer::attackButtonClick() -> currentPlayer->attack(OUGIS2) langsung,
					// TIDAK pernah lewat perform() ini) juga kena spawn yang sama -- pola identik
					// dengan fix OUGIS1 di atas.
					changeSide(sp);
					attack(OUGIS2);
					return;
				}

				// OUGIS1: Dead Soul Jutsu — hanya bisa cast kalau (a) tidak ada puppet yang masih
				// dikendalikan (anti-spam lock) DAN (b) ada mayat unit ber-tipe Hero dalam radius.
				// trySpawnDeadSoulPuppet() scan+revive sekaligus, return false kalau syarat
				// tidak terpenuhi supaya attack(OUGIS1) tidak pernah jalan tanpa mayat ke-revive.
				if (_isCanOugis1 && !_isControlled && !_isArmored &&
					enemyCombatPoint > friendCombatPoint &&
					!isDeadSoulPuppetActive())
				{
					// Corpse-scan + spawn sekarang di attack() override (lihat di bawah) supaya
					// jalur manual (tombol skill4 -> GameLayer::attackButtonClick() ->
					// currentPlayer->attack(OUGIS1) langsung, TIDAK pernah lewat perform() ini)
					// juga kena guard yang sama -- root cause bug "clone tidak muncul saat main
					// manual" adalah spawn-nya dulu 100% terkunci di perform(), yang tidak pernah
					// jalan kalau Kabuto dikontrol manual oleh player.
					changeSide(sp);
					attack(OUGIS1);
					return;
				}

				// SKILL3: burst dive once in mid range
				if (_isCanSkill3 && !_isArmored && abs(sp.x) < 192 && abs(sp.x) > 64)
				{
					changeSide(sp);
					attack(SKILL3);
					return;
				}

				if (abs(sp.x) < 160)
				{
					if (abs(sp.x) > 96 || abs(sp.y) > 32)
					{
						moveDirection = sp.getNormalized();
						walk(moveDirection);
						return;
					}

					// SKILL2: gap-closer / poke+slow before committing to melee
					if (_isCanSkill2 && !_isArmored && abs(sp.x) > 48)
					{
						changeSide(sp);
						attack(SKILL2);
						return;
					}

					// SKILL1: pop the scalpel buff before trading basic attacks
					if (_isCanSkill1 && !_isArmored && !_skillChangeBuffValue)
					{
						changeSide(sp);
						attack(SKILL1);
						return;
					}

					changeSide(sp);
					attack(NAttack);
					return;
				}

				moveDirection = sp.getNormalized();
				walk(moveDirection);
				return;
			}
		}

		_mainTarget = nullptr;
		if (notFindFlogHalf())
			findTowerHalf();

		if (_mainTarget)
		{
			Vec2 moveDirection;
			Vec2 sp = getDistanceToTarget();

			if (abs(sp.x) > 32 || abs(sp.y) > 32)
			{
				moveDirection = sp.getNormalized();
				walk(moveDirection);
				return;
			}

			if (isFreeState())
			{
				changeSide(sp);
				attack(NAttack);
			}
			return;
		}

		checkHealingState();
	}

	// SKILL1 "cBuff" -> pure stat/attackType buff, 6s window. This is NOT a Tsunade-style
	// transformation: NAttack keeps its normal animation (nattackArray) the whole time, only
	// _nAttackType is retagged to "kb_scalpel_hit" so CharacterBase::setDamage()'s attacker
	// branch applies the extra magic dmg (4% target maxHP) + self-heal (1.5% own maxHP) per hit
	// (see patches/CharacterBase.cpp.patch.txt §4) — separate from Yin Healing Focus's own
	// +12% dmg window, which uses its own flag (_kbYinDmgBuffActive) and stacks on top of this.
	// Each activation also refunds ougi gauge (CKR) so OUGIS1/OUGIS2 come off cooldown faster.
	void changeAction() override
	{
		_originNAttackType = _nAttackType;
		_nAttackType = "kb_scalpel_hit";

		// Chakra refund: +CHAKRA_REFUND_PERCENT% of each ougi gauge's max per SKILL1 use, so
		// OUGIS1 (Dead Soul Army) / OUGIS2 (Nehan Shojo) come off cooldown sooner.
		refundOugiChakra(kChakraRefundPercent);

		// Late-game buff: SKILL1 window sekarang juga menaikkan damage SEMUA serangan Kabuto
		// (lihat CharacterBase::setDamage() _kbScalpelDmgBuffActive branch), bukan cuma extra
		// magic dmg 4% maxHP per hit NAttack -- supaya SKILL1 tetap worth dipencet sebelum
		// combo/trade di late game, bukan cuma sumber sustain.
		_kbScalpelDmgBuffActive = true;

		unschedule(schedule_selector(CharacterBase::resumeAction));
		scheduleOnce(schedule_selector(CharacterBase::resumeAction), 6.0f);
	}

	void resumeAction(float dt) override
	{
		_nAttackType = _originNAttackType;
		_kbScalpelDmgBuffActive = false;

		if (_state != State::DEAD)
		{
			_state = State::WALK;
			idle();
		}
		CharacterBase::resumeAction(dt);
	}

	// Centralized OUGIS1 guard: intercepts BOTH call sites --
	//   (a) Kabuto::perform() (AI loop), and
	//   (b) GameLayer::attackButtonClick(OUGIS1) -> currentPlayer->attack(OUGIS1) direct,
	//       fired the instant the player taps the manual Skill4 button, which never touches
	//       perform() at all.
	// Sebelumnya trySpawnDeadSoulArmyClone() cuma dipanggil dari perform(), jadi jalur (b)
	// selalu lolos ke oAttack(OUGIS1) tanpa scan mayat sama sekali -> animasi/SFX main tapi
	// clone tidak pernah muncul walau ada mayat Hero di sekitar (bug report ini). Sekarang
	// scan+spawn dipindah ke sini supaya berlaku sama persis di kedua jalur.
	void attack(ABType type) override
	{
		if (type == OUGIS1)
		{
			if (isDeadSoulPuppetActive() || !trySpawnDeadSoulPuppet(96.0f))
				return; // no valid Hero corpse in range, or a puppet is still under control: block cast, keep CKR gauge intact
		}
		else if (type == OUGIS2)
		{
			// BUG FIX ("efek feathers tidak muncul saat di-cast manual oleh pemain"): sama pola
			// dengan fix OUGIS1 di atas -- spawnNehanShoujoFeathers() sebelumnya CUMA dipanggil
			// dari Kabuto::perform() (loop AI), sedangkan tombol skill manual (Skill5/OUGIS2)
			// langsung memanggil currentPlayer->attack(OUGIS2) TANPA pernah lewat perform() sama
			// sekali -- jadi versi player-controlled/manual selalu skip AoE bulu-nya (makanya
			// versi COM/AI Kabuto normal, versi manual pemain tidak). Dipindah ke sini supaya
			// berlaku sama persis di kedua jalur, identik dengan guard OUGIS1 di atas.
			spawnNehanShoujoFeathers(160.0f);

			// BUG FIX ("Kabuto tidak bisa kena damage"): skill05.xml membungkus SELURUH durasi
			// animasi finisher dengan <e type="setCommand">setInvincible</e> di frame 02 ...
			// <e type="setCommand">reInvincible</e> di frame TERAKHIR (10). Kalau action-nya
			// kepotong sebelum sampai frame 10 -- misal ke-pause lewat GameLayer::setOugis()
			// (freeze cutscene GLOBAL yang dipicu <e type="setOugis">10</e> di frame 01 skill05
			// ini SENDIRI, atau dipicu hero lain yang ougis bersamaan/berdekatan) dan tidak
			// pernah ter-resume bersih sampai ke frame terakhir -- reInvincible tidak pernah
			// kepanggil, _isInvincible NYANGKUT true selamanya (satu-satunya reset lain cuma di
			// CharacterBase::dead()). Safety-net: paksa clear beberapa detik setelah cast,
			// idempotent/no-op kalau reInvincible normal sudah jalan duluan.
			unschedule(schedule_selector(Kabuto::kabutoForceClearInvincible));
			scheduleOnce(schedule_selector(Kabuto::kabutoForceClearInvincible), 3.0f);
		}
		Hero::attack(type);
	}

	// Generic single-clone fallback (base Hero/CharacterBase contract). Tidak lagi dipanggil
	// lewat XML (skill04 sudah tidak punya <e type="setClone"> sama sekali -- lihat
	// trySpawnDeadSoulArmyClone() di bawah, yang memanggil finalizeClone() langsung), tapi tetap
	// disediakan untuk kompatibilitas kalau ada jalur lain yang manggil CharacterBase::setClone().
	Hero *createClone(int cloneTime) override
	{
		auto clone = createCloneHero<KabutoClone>(getName());
		clone->setMaster(this);
		// NOTE: HP/ATK/DEF nerf (50% dari Kabuto asli) sekarang dilakukan secara konsisten di
		// CharacterBase::finalizeClone() untuk SEMUA clone Kabuto, supaya tidak ada lagi
		// override race seperti sebelumnya (di sini sempat di-set 30% tapi lalu ditimpa balik ke
		// 100% oleh finalizeClone() -- lihat fix bug laporan #4).
		return clone;
	}

	// Lihat komentar di attack()/OUGIS2 di atas -- dt tidak dipakai, cuma perlu match signature
	// schedule_selector.
	void kabutoForceClearInvincible(float dt)
	{
		_isInvincible = false;
	}

	bool isDeadSoulPuppetActive()
	{
		return _kbControlledPuppet != nullptr;
	}

	// Hook dari HPBar.cpp (loseHP() Kabuto-puppet bypass): dipanggil TEPAT SEBELUM puppet->dead()
	// dijalankan, saat puppet dihabisi lawan lewat damage sementara masih dikendalikan. Null-kan
	// referensi + bersihkan schedule di sini (pola sama seperti onCloneDealloc()) supaya
	// kabutoPuppetTimeout()/kabutoPuppetRangeTick() tidak pernah jalan lagi atas puppet yang
	// sudah kembali jadi mayat biasa.
	void onPuppetReverted(CharacterBase *puppet) override
	{
		if (_kbControlledPuppet == puppet)
		{
			_kbControlledPuppet = nullptr;
			unschedule(schedule_selector(Kabuto::kabutoPuppetTimeout));
			unschedule(schedule_selector(Kabuto::kabutoPuppetRangeTick));
			if (isPlayer())
				getGameLayer()->getHudLayer()->skill4Button->unLock();
		}
	}

	// Revert TERJADWAL (timeout 8 detik ATAU keluar leash-range) -- BUKAN lewat damage, jadi
	// TIDAK PERNAH lewat HPBar::loseHP()/kredit kill siapa pun sama sekali (beda dari puppet
	// yang dihabisi lawan, yang ditangani generic lewat flag _isKabutoPuppet di HPBar.cpp).
	// Tetap set _isSuicide=true supaya dead() kasih reborn 3 detik (bukan _rebornTime penuh)
	// & tidak nambah _deadNum korban -- persis efek "kembali mati kembali dalam 3 detik lalu
	// spawn normal sebagai unit lawan" di spek.
	void revertPuppetNoCredit(CharacterBase *puppet)
	{
		if (!puppet)
			return;

		unschedule(schedule_selector(Kabuto::kabutoPuppetTimeout));
		unschedule(schedule_selector(Kabuto::kabutoPuppetRangeTick));

		if (puppet->_kbPuppetGroupSwapped)
			puppet->changeGroup(); // cuma balik grup kalau tadi memang di-swap (mayat musuh)
		puppet->_isKabutoPuppet = false;
		puppet->_kbPuppetGroupSwapped = false;
		puppet->_kbPuppetMaster = nullptr;
		puppet->_isSuicide = true;

		_kbControlledPuppet = nullptr;
		if (isPlayer())
			getGameLayer()->getHudLayer()->skill4Button->unLock();

		puppet->dead();
	}

	// scheduleOnce, kKbPuppetControlDuration detik setelah cast: window kontrol habis.
	void kabutoPuppetTimeout(float dt)
	{
		revertPuppetNoCredit(_kbControlledPuppet);
	}

	// schedule tiap 0.2 detik selama window kontrol aktif: Kabuto WAJIB tetap berada dalam
	// radius kKbPuppetLeashRadius dari mayat yang ia kendalikan (lore: dead soul jutsu butuh
	// Kabuto tetap di sekitar mayat). Keluar radius (atau Kabuto sendiri sudah DEAD) -> puppet
	// LANGSUNG balik status dead saat itu juga, tidak nunggu timeout 8 detik habis.
	void kabutoPuppetRangeTick(float dt)
	{
		if (!_kbControlledPuppet)
		{
			unschedule(schedule_selector(Kabuto::kabutoPuppetRangeTick));
			return;
		}

		if (_state == State::DEAD)
		{
			revertPuppetNoCredit(_kbControlledPuppet);
			return;
		}

		float dx = _kbControlledPuppet->getPositionX() - getPositionX();
		float dy = _kbControlledPuppet->getPositionY() - getPositionY();
		if (fabs(dx) > kKbPuppetLeashRadius || fabs(dy) > kKbPuppetLeashRadius)
		{
			revertPuppetNoCredit(_kbControlledPuppet);
		}
	}

	// Hidupkan mayat DI TEMPAT ia tumbang (bukan di spawn point -- beda dari Hero::reborn()
	// normal) dan jadikan rekan sementara Kabuto. corpse harus Hero* (bukan CharacterBase*)
	// karena reborn()/countDown()/rebornSprite adalah anggota Hero, dan _CharacterArray sendiri
	// memang vector<Hero*> (lihat GameLayer.h).
	void reviveCorpseAsPuppet(Hero *corpse)
	{
		// Batalkan alur respawn normal yang sudah berjalan sejak korban ini dieliminasi
		// (Hero::dealloc() men-schedule Hero::reborn + Hero::countDown, lihat Hero.hpp) --
		// corpse "dicuri" dari antrian respawn normalnya, bukan disalin/di-clone.
		corpse->unschedule(schedule_selector(Hero::reborn));
		corpse->unschedule(schedule_selector(Hero::countDown));
		if (corpse->rebornSprite)
		{
			corpse->rebornSprite->removeFromParent();
			corpse->rebornSprite = nullptr;
		}
		corpse->rebornLabelTime = 0;

		// CharacterBase::dead() melepas observer ini -- tanpa di-pasang ulang, puppet tidak
		// akan pernah bisa menerima damage sama sekali (lihat CharacterBase::acceptAttack()).
		CCNotificationCenter::sharedNotificationCenter()->addObserver(
			corpse, callfuncO_selector(CharacterBase::acceptAttack), "acceptAttack", nullptr);

		corpse->setOpacity(255);
		corpse->setVisible(true);
		corpse->_isVisable = true;
		corpse->setHPValue(corpse->getMaxHP(), false);
		corpse->setState(State::IDLE); // _state itu `protected` (lihat VPROP di CharacterBase.h)
										// -- tidak bisa di-assign langsung lewat Hero* ke instance
										// SIBLING class (bukan Kabuto sendiri), harus lewat setter.

		// BUG FIX: changeGroup() dulu UNCONDITIONAL di sini -> mayat REKAN SETIM Kabuto sendiri
		// yang dihidupkan ikut ke-flip jadi musuh (padahal mestinya cukup dihidupkan apa adanya).
		// Sekarang hanya di-swap kalau mayatnya memang tadinya MUSUH -- dicek SEBELUM setHPbar()
		// supaya warna bar tetap konsisten dgn grup final. _kbPuppetGroupSwapped dicatat supaya
		// revertPuppetNoCredit()/HPBar.cpp tahu perlu changeGroup() balik atau tidak saat puppet
		// ini mati lagi.
		bool wasEnemy = (corpse->getGroup() != getGroup());
		if (wasEnemy)
			corpse->changeGroup();
		corpse->_kbPuppetGroupSwapped = wasEnemy;
		corpse->setHPbar();

		corpse->idle();
		corpse->scheduleUpdate();

		// Paksa AI supaya langsung ikut bertarung untuk Kabuto, terlepas dulunya unit ini
		// dikontrol player manusia atau bot -- pemilik aslinya sudah tumbang/menunggu respawn.
		corpse->_isAI = true;
		corpse->doAI();

		corpse->_isKabutoPuppet = true;
		corpse->_kbPuppetMaster = this;
	}

	// Called from perform()/attack() the instant OUGIS1 is considered. Scan radius untuk mayat
	// unit BERTIPE HERO (isPlayerOrCom() -- exclude Mon/Summon/Kugutsu/Guardian/Clone corpses)
	// yang BELUM dikendalikan puppet lain, TIDAK difilter getGroup() (ally atau musuh sama-sama
	// valid trigger). Hidupkan+kendalikan PERSIS SATU mayat TERDEKAT yang ketemu -- bukan
	// semuanya. Return false (tanpa revive apa pun) kalau tidak ada mayat Hero yang cocok,
	// supaya caller tahu untuk TIDAK jalankan attack(OUGIS1).
	bool trySpawnDeadSoulPuppet(float radius)
	{
		Hero *nearestCorpse = nullptr;
		float nearestDistSq = radius * radius + 1.0f;

		for (auto unit : getGameLayer()->_CharacterArray)
		{
			if (!unit || unit == this)
				continue;
			if (!unit->isPlayerOrCom())
				continue; // hanya unit ber-tipe Hero, bukan Mon/Summon/Kugutsu/Guardian/Clone
			if (unit->getState() != State::DEAD)
				continue;
			if (unit->_isKabutoPuppet)
				continue; // sudah dikendalikan (Kabuto ini atau Kabuto lain), skip

			float dx = unit->getPositionX() - getPositionX();
			float dy = unit->getPositionY() - getPositionY();
			if (fabs(dx) > radius || fabs(dy) > radius)
				continue;

			float distSq = dx * dx + dy * dy;
			if (distSq < nearestDistSq)
			{
				nearestDistSq = distSq;
				nearestCorpse = unit;
			}
		}

		if (!nearestCorpse)
			return false;

		reviveCorpseAsPuppet(nearestCorpse);
		_kbControlledPuppet = nearestCorpse;

		unschedule(schedule_selector(Kabuto::kabutoPuppetTimeout));
		scheduleOnce(schedule_selector(Kabuto::kabutoPuppetTimeout), kKbPuppetControlDuration);
		schedule(schedule_selector(Kabuto::kabutoPuppetRangeTick), 0.2f);

		// UI lock (pola sama seperti Kiba::changeAction()/setActionResume() mengunci skill1Button
		// selama transformasi): supaya tombol skill4 (OUGIS1) tidak kelihatan bisa ditekan padahal
		// isDeadSoulPuppetActive() bakal langsung menolak cast-nya di attack(). Di-unlock lagi di
		// onPuppetReverted() atau revertPuppetNoCredit() begitu puppet-nya balik jadi mayat biasa.
		if (isPlayer())
			getGameLayer()->getHudLayer()->skill4Button->setLock();

		return true;
	}

private:
	string _originNAttackType;

	// Pointer ke satu-satunya mayat yang sedang dikendalikan (Dead Soul Jutsu), atau nullptr
	// kalau belum pernah/sudah direvert. Di-set di trySpawnDeadSoulPuppet(), di-null-kan lewat
	// onPuppetReverted()/revertPuppetNoCredit() -- JANGAN pernah dereference pointer ini
	// langsung tanpa lewat isDeadSoulPuppetActive() dulu.
	CharacterBase *_kbControlledPuppet = nullptr;

	static constexpr float kKbPuppetControlDuration = 20.0f; // detik rekan sementara aktif;
															   // + 3 detik reborn (_isSuicide di
															   // dead()) = ~23 detik total hidup,
															   // di-rebalance dari 8+3=11 detik.
	static constexpr float kKbPuppetLeashRadius = 260.0f;   // REBALANCE: dinaikkan dari 160 -> 260
	                                                          // supaya puppet lebih fleksibel bergerak
	                                                          // lebih jauh dari Kabuto; mekanisme
	                                                          // "keluar radius = mati" di
	                                                          // kabutoPuppetRangeTick() TETAP dipertahankan
	                                                          // apa adanya, cuma radiusnya yang diperlebar.
	                                                          // Kabuto harus tetap sedekat ini

	// SKILL1 chakra refund percent. Tweak this one constant to retune how much OUGIS1/OUGIS2
	// cooldown SKILL1 shaves off per use.
	static const int kChakraRefundPercent = 10; // REBALANCE: diturunkan dari 15% -> 10% (infact gameplay kurang seimbang)

	// Menambahkan X% dari kapasitas maksimum CKR (utk OUGIS1) dan CKR2 (utk OUGIS2), dipanggil
	// dari changeAction() SKILL1. Mengikuti pola cap + auto-unlock flag yang sama seperti
	// CharacterBase::increaseAllCkrs(), hanya saja persen dihitung terpisah per gauge supaya
	// hasilnya proporsional walau CKR (max 45000) dan CKR2 (max 50000) beda kapasitas.
	void refundOugiChakra(int percent)
	{
		if (_level >= 2)
		{
			uint32_t gain = 45000u * percent / 100u;
			uint32_t ckr = MIN(getCKR() + gain, 45000u);
			setCKR(ckr);
			if (ckr >= 15000)
				_isCanOugis1 = true;
		}

		if (_level >= 4)
		{
			uint32_t gain2 = 50000u * percent / 100u;
			uint32_t ckr2 = MIN(getCKR2() + gain2, 50000u);
			setCKR2(ckr2);
			if (ckr2 >= 25000)
				_isCanOugis2 = true;
		}
	}

	// OUGIS2 "Nehan Shojo": spawn 1 objek bulu (Bullet, XML terpisah di
	// Resources/Unit/Projectile/KabutoFeather.xml) di posisi tiap musuh (getGroup() beda
	// dari Kabuto, bukan sekutu -- beda dari findNearbyEliminatedUnit() yang inklusif sekutu)
	// yang masih hidup dalam radius. Pola spawn (Bullet::create + setID + setPosition + idle()
	// + attack(NAttack) + addChild + scheduleOnce(removeSelf)) MENGIKUTI pola
	// ImmortalSasuke::Amaterasu di CharacterBase::setTrap() (dikonfirmasi dari source asli).
	// attack(NAttack) di sini langsung proc karena KabutoFeather.xml taruh
	// <e type="setAttackBox"> di frame pertama nAttack-nya.
	//
	// FIX (visual invisible bug, versi lama): dulu feather di-spawn TEPAT di titik anchor musuh
	// tanpa anchor point/offset/z-boost, jadi ke-render DI BELAKANG/DI DALAM sprite musuh sendiri
	// -- efek mekanis (stun/slow) tetap jalan tapi tidak pernah kelihatan. Anchor point + offset
	// + z-order di bawah masih dipertahankan untuk proc AWAL (frame munculnya bulu), TAPI
	// visual yang bertahan SELAMA durasi stun sekarang bukan tanggung jawab Bullet transient ini
	// lagi -- itu di-handle terpisah oleh _kbFeatherStunEffect yang di-attach sebagai CHILD
	// langsung ke unit korban di CharacterBase::setDamage() (kb_feather_hit branch), pola yang
	// sama seperti heal-effect Karin (_healBuffEffect / "hBuff") tapi nempel ke unit MUSUH,
	// bukan sekutu. Bullet feather ini sendiri cukup hidup 0.5s (sekadar buat proc attackBox-nya
	// sekali), TIDAK lagi dipakai sebagai referensi jangka panjang oleh siapa pun (_controller
	// korban sekarang nunjuk ke attacker->_master / Kabuto yang persisten, bukan ke Bullet ini --
	// lihat fix use-after-free di CharacterBase.cpp), jadi lifetime pendeknya sudah aman.
	void spawnNehanShoujoFeathers(float radius)
	{
		static const float kFeatherYOffset = 48.0f; // muncul di atas kepala musuh, bukan menimpa

		for (auto hero : getGameLayer()->_CharacterArray)
		{
			if (!hero || hero == this)
				continue;
			if (getGroup() == hero->getGroup())
				continue; // hanya musuh, bukan sekutu -- beda spek dari Dead Soul Army
			if (hero->getState() == State::DEAD)
				continue;
			if (fabs(hero->getPositionX() - getPositionX()) > radius ||
				fabs(hero->getPositionY() - getPositionY()) > radius)
				continue;

			auto feather = Bullet::create();
			feather->setMaster(this);
			feather->setID(ProjectileEnum::KabutoFeather, Role::Mon, getGroup());
			feather->setAnchorPoint(Vec2(0.5f, 0));
			feather->setPosition(Vec2(hero->getPositionX(), hero->getPositionY() + kFeatherYOffset));
			feather->idle();
			feather->attack(NAttack);
			auto removeCall = CallFunc::create(std::bind(&Bullet::removeFromParent, feather));
			feather->runAction(newSequence(DelayTime::create(0.5f), removeCall));
			// +1 z-order supaya digambar SETELAH (di depan) musuh yang berada di ketinggian
			// (Y) yang sama, bukan ketimpa oleh sprite musuh.
			getGameLayer()->addChild(feather, -feather->getPositionY() + 1);
		}
	}
};
