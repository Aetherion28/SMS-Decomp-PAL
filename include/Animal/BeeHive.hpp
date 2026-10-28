#ifndef ANIMAL_BEE_HIVE_HPP
#define ANIMAL_BEE_HIVE_HPP

#include <Animal/fishoid.hpp>
#include <Enemy/EnemyManager.hpp>
#include <Strategic/Spine.hpp>
#include <System/ParamInst.hpp>

class TBeeHive;
class TMapObjBase;

// Member names come from the .prm strings.
class TBeeHiveParams : public TSpineEnemyParams {
public:
	TBeeHiveParams(const char* path);

	/* 0xA8 */ TParamRT<s32> mGiveupTimer;
	/* 0xBC */ TParamRT<f32> mGiveupRange;
	/* 0xD0 */ TParamRT<s32> mDecrimentTimer;
	/* 0xE4 */ TParamRT<f32> mRebound;
	/* 0xF8 */ TParamRT<f32> mDecay;
	/* 0x10C */ TParamRT<f32> mAngleMaxAdd;
	/* 0x120 */ TParamRT<f32> mShakePower;
	/* 0x134 */ TParamRT<f32> mFallAngularVel;
	/* 0x148 */ TParamRT<f32> mSearchRange;
};

// One bee of the swarm. Each one drops a coin when Yoshi eats it.
class TBee : public TRealoidActor {
public:
	TBee(MActor* actor, TBeeHive* hive);

	virtual BOOL receiveMessage(THitActor*, u32);
	virtual void init();

	void behaveToEat();

public:
	/* 0xA8 */ TBeeHive* mHive;
};

// The hanging beehive. Spraying it makes it swing, hitting it knocks it down
// and the swarm then chases Mario until he escapes or dives into water.
class TBeeHive : public TRealoid {
public:
	TBeeHive(const char* name = "ハチの巣とハチ");

	virtual void load(JSUMemoryInputStream&);
	virtual void perform(u32, JDrama::TGraphics*);
	virtual BOOL receiveMessage(THitActor*, u32);
	virtual void init(TLiveManager*);
	virtual void calcRootMatrix();
	virtual void control();
	virtual void bind();
	virtual void reset();
	virtual TRealoidActor* createRealoidActor(MActor*);

	bool isNotBreakOnceYet() const;
	bool isWaiting() const;
	bool isFalling() const;
	JGeometry::TVec3<f32> getCenterOfGravity() const;
	void setShakePower(const JGeometry::TVec3<f32>& dir);
	bool isMissMario() const;
	bool doScaling();
	void prepareReset();
	void prepareVanish();
	void prepareBreak();
	void dropCoin();
	void appearAllBees();
	void decrimentBee();
	void disappearBee(int idx);
	void appearBee(int idx);
	bool doFall();
	void prepareFall();
	bool doWait();
	void prepareWait();
	void controlSound();
	void controlCollision();
	void loadCoin(JSUMemoryInputStream&);
	void receiveMessageFromChild(TBee* bee);
	void setBoidParamOnMarioWaterIn();
	void setBoidParamOnWaiting();
	void setBoidParamOnAttacking();
	void setBoidBaseParams();
	void initBoids();
	void initCollision();

	// fabricated
	const TBeeHiveParams* getParam() const
	{
		return (const TBeeHiveParams*)getSaveParam();
	}

public:
	/* 0x158 */ JGeometry::TQuat4<f32> mSwingDirGoal;
	/* 0x168 */ JGeometry::TQuat4<f32> mSwingDir;
	/* 0x178 */ JGeometry::TQuat4<f32> mBaseRot;
	/* 0x188 */ f32 mSwingAngle;
	/* 0x18C */ f32 mSwingSpeed;
	/* 0x190 */ f32 mSwingLimit;
	/* 0x194 */ JGeometry::TVec3<f32> mInitPos;
	/* 0x1A0 */ JGeometry::TVec3<f32> mSoundPos;
	/* 0x1AC */ TMapObjBase* mItem;
	/* 0x1B0 */ int mCollisionBee;
	/* 0x1B4 */ int mActiveBeeNum;
	/* 0x1B8 */ TMapObjBase** mCoins;
	/* 0x1BC */ int mEatenBeeNum;
};

class TBeeHiveManager : public TEnemyManager {
public:
	TBeeHiveManager(const char* name = "ハチの巣マネージャー");
	virtual ~TBeeHiveManager() { }

	virtual void load(JSUMemoryInputStream&);
	virtual void createModelData();
};

DECLARE_NERVE(TNerveBeeHiveWait, TLiveActor);
DECLARE_NERVE(TNerveBeeHiveFall, TLiveActor);
DECLARE_NERVE(TNerveBeeHiveBreak, TLiveActor);
DECLARE_NERVE(TNerveBeeHiveAttack, TLiveActor);
DECLARE_NERVE(TNerveBeeHiveMarioWaterIn, TLiveActor);
DECLARE_NERVE(TNerveBeeHiveReset, TLiveActor);

#endif
