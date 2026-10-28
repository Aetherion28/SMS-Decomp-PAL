#ifndef ANIMAL_BIRD_HPP
#define ANIMAL_BIRD_HPP

#include <Enemy/Enemy.hpp>
#include <Enemy/EnemyManager.hpp>
#include <Strategic/Nerve.hpp>
#include <System/ParamInst.hpp>

class TMapObjBase;
class TWireBinder;
struct _GXColorS10;

// Member names come from the .prm strings.
class TAnimalBirdParams : public TSpineEnemyParams {
public:
	TAnimalBirdParams(const char* path);

	/* 0xA8 */ TParamRT<f32> mMarchSpeed;
	/* 0xBC */ TParamRT<f32> mTurnSpeed;
	/* 0xD0 */ TParamRT<s32> mReturnTimer;
	/* 0xE4 */ TParamRT<f32> mSearchLength;
	/* 0xF8 */ TParamRT<f32> mSearchHeight;
	/* 0x10C */ TParamRT<f32> mSearchAware;
	/* 0x120 */ TParamRT<f32> mSearchAngle;
	/* 0x134 */ TParamRT<s32> mActionTimer;
	/* 0x148 */ TParamRT<s32> mWaterproofTimerMax;
	/* 0x15C */ TParamRT<s32> mFloatingTimerMax;
	/* 0x170 */ TParamRT<f32> mLandingGravityY;
	/* 0x184 */ TParamRT<f32> mLandingTorqueY;
	/* 0x198 */ TParamRT<f32> mWalkingTorqueY;
	/* 0x1AC */ TParamRT<f32> mWalkingSpeed;
	/* 0x1C0 */ TParamRT<s32> mWalkTimer;
	/* 0x1D4 */ TParamRT<f32> mLandingFric;
	/* 0x1E8 */ TParamRT<s32> mActionTimerAdd;
	/* 0x1FC */ TParamRT<f32> mWaterPowerY;
};

// "(幸せの青い)鳥", the bluebird. Spraying it in flight enough times turns it
// into the coin (or Shine) it was given in the scene file.
class TAnimalBird : public TSpineEnemy {
public:
	TAnimalBird(const char* name = "(幸せの青い)鳥");
	virtual ~TAnimalBird() { }

	virtual void load(JSUMemoryInputStream&);
	virtual void loadAfter();
	virtual BOOL receiveMessage(THitActor*, u32);
	virtual void init(TLiveManager*);
	virtual void calcRootMatrix();
	virtual void bind();
	virtual void moveObject();
	virtual const char** getBasNameTable() const;

	void initTevColor(const _GXColorS10*);
	void initCollision();
	void initParams();
	void behaveHitWater();
	bool isOnGroundNerve() const;
	void checkFalling();
	void checkChangeToItem();
	void checkNotAppear(s32);
	void updateSound();
	bool isWantToFly() const;
	bool isWantToAction() const;
	bool isWantToRest() const;
	bool isFindMario() const;
	bool isChangeToItem() const;
	bool isGroundShaken() const;
	bool isCheckWithWireBinder() const;
	bool isChanged() const;
	bool isFlying() const;
	void doDropCoin();
	void doFlyToCurPathNode();
	void doWalk();
	bool doLanding(bool);
	void doGotoRandomNextGraphNode();
	void setGoalToComeback();
	void setParamsOnFloating();
	void setParamsOnLanding();
	void setBckAnm(int);
	f32 getMyMarchSpeed() const;
	f32 getWaterDamageRate() const;
	f32 getWaterPowerY() const;
	f32 getFootGroundHeight();

	// fabricated: the timer starts on every water hit, and while it runs
	// further hits don't cost hit points.
	bool isWaterproof() const { return mWaterproofTimer > 0; }

	// fabricated
	const TAnimalBirdParams* getParam() const
	{
		return (const TAnimalBirdParams*)getSaveParam();
	}

public:
	/* 0x150 */ TMapObjBase* mItem;
	/* 0x154 */ TWireBinder* mWireBinder;
	/* 0x158 */ JGeometry::TVec3<f32> mHomePos;
	/* 0x164 */ JGeometry::TVec3<f32> mHomeRot;
	/* 0x170 */ f32 mWalkDir;
	/* 0x174 */ f32 mScale;
	/* 0x178 */ s32 mWaterproofTimer;
	/* 0x17C */ s32 mFloatingTimer;
	/* 0x180 */ int mColorIndex;
};

class TAnimalBirdManager : public TEnemyManager {
public:
	TAnimalBirdManager(const char* name = "(幸せの青い)鳥マネージャー");
	virtual ~TAnimalBirdManager() { }

	virtual void load(JSUMemoryInputStream&);
	virtual void loadAfter();
	virtual void createModelData();
};

DECLARE_NERVE(TNerveAnimalBirdWaitOnGround, TLiveActor);
DECLARE_NERVE(TNerveAnimalBirdActionOnGround, TLiveActor);
DECLARE_NERVE(TNerveAnimalBirdWalkOnGround, TLiveActor);
DECLARE_NERVE(TNerveAnimalBirdTakeoff, TLiveActor);
DECLARE_NERVE(TNerveAnimalBirdGraphWander, TLiveActor);
DECLARE_NERVE(TNerveAnimalBirdChangeToCoin, TLiveActor);
DECLARE_NERVE(TNerveAnimalBirdComeback, TLiveActor);
DECLARE_NERVE(TNerveAnimalBirdPreLanding, TLiveActor);
DECLARE_NERVE(TNerveAnimalBirdLanding, TLiveActor);

#endif
