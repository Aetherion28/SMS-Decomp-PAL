#ifndef ENEMY_FRUITS_BOAT_HPP
#define ENEMY_FRUITS_BOAT_HPP

#include <Enemy/Enemy.hpp>
#include <Enemy/EnemyManager.hpp>
#include <Strategic/Spine.hpp>
#include <System/ParamInst.hpp>

class J3DAnmTransform;
class J3DFrameCtrl;

// The fruit boats of Delfino Plaza ("dolpic", as in the ShipDolpic models and
// TBellDolpic). Member names come from the .prm strings.
class TFruitsBoatParams : public TSpineEnemyParams {
public:
	TFruitsBoatParams(const char* path);

	/* 0xA8 */ TParamRT<f32> mSLMoveSpeed;
	/* 0xBC */ TParamRT<f32> mSLRotSpeed;
	/* 0xD0 */ TParamRT<f32> mSLBckMoveSpeed;
};

class TFruitsBoatManager : public TEnemyManager {
public:
	TFruitsBoatManager(int type, const char* name);
	virtual ~TFruitsBoatManager() { }

	virtual void load(JSUMemoryInputStream&);
	virtual void createModelData();
	virtual TSpineEnemy* createEnemyInstance();

public:
	/* 0x54 */ int mBoatType;
};

class TFruitsBoat : public TSpineEnemy {
public:
	TFruitsBoat(const char* name);
	virtual ~TFruitsBoat() { }

	virtual void load(JSUMemoryInputStream&);
	virtual BOOL receiveMessage(THitActor*, u32);
	virtual Mtx* getRootJointMtx() const;
	virtual void init(TLiveManager*);
	virtual void calcRootMatrix();
	virtual void setGroundCollision();
	virtual void moveObject();
	virtual void requestShadow();

	int setBckTrack(const char* name);
	void setJumpReaction();
	void traceBckTrack();
	int getBoatType() const;
	void rowToCurPathNode(f32 speed);

	const TFruitsBoatParams* getParam() const
	{
		return (const TFruitsBoatParams*)getSaveParam();
	}

public:
	/* 0x150 */ s16 unk150;
	/* 0x154 */ f32 mShadowRadiusX;
	/* 0x158 */ f32 mShadowRadiusZ;
	/* 0x15C */ J3DAnmTransform* mBckTrack;
	/* 0x160 */ J3DFrameCtrl* mBckTrackCtrl;
	/* 0x164 */ JGeometry::TVec3<f32> mRockAxis;
	/* 0x170 */ f32 mRockAngle;
	/* 0x174 */ f32 mRockSpeed;
};

DECLARE_NERVE(TNerveFruitsBoatGraphWander, TLiveActor);
DECLARE_NERVE(TNerveFruitsBoatBckTrace, TLiveActor);

#endif
