#include <Animal/BeeHive.hpp>
#include <Animal/boid.hpp>
#include <Enemy/PathNode.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DModel.hpp>
#include <JSystem/J3D/J3DGraphLoader/J3DModelLoaderFlags.hpp>
#include <JSystem/JDrama/JDRNameRefGen.hpp>
#include <JSystem/JGeometry.hpp>
#include <JSystem/JMath.hpp>
#include <M3DUtil/MActor.hpp>
#include <MSound/MSound.hpp>
#include <MSound/MSoundSE.hpp>
#include <MSound/SoundEffects.hpp>
#include <Map/Map.hpp>
#include <Map/MapData.hpp>
#include <MarioUtil/MathUtil.hpp>
#include <MoveBG/ItemManager.hpp>
#include <MoveBG/MapObjBase.hpp>
#include <MoveBG/MapObjManager.hpp>
#include <Player/MarioAccess.hpp>
#include <Strategic/ObjManager.hpp>
#include <Strategic/ObjModel.hpp>
#include <Strategic/Spine.hpp>
#include <Strategic/Strategy.hpp>
#include <System/EmitterViewObj.hpp>
#include <System/Particles.hpp>
#include <stdlib.h>

// rogue includes needed for matching sinit & bss
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>
#include <M3DUtil/InfectiousStrings.hpp>

namespace {

// The hive falls once it swings further than this.
f32 cAngleLimit = JGeometry::TUtil<f32>::PI() * 0.375f;

bool SMS_IsMarioInWater()
{
	return SMS_CheckMarioFlag(MARIO_FLAG_VISIBLE)
	       || SMS_CheckMarioFlag(MARIO_FLAG_IN_SHALLOW_WATER)
	       || SMS_GetMarioGroundPlane()->isWaterSurface()
	       || SMS_CheckMarioFlag(MARIO_FLAG_IN_WATER);
}

void SMS_EmitWaterHitParticleAndSound(JGeometry::TVec3<f32>* pos)
{
	TPosition3f mtx;
	mtx.translation(pos->x, pos->y, pos->z);
	gpMarioParticleManager->emitAndBindToMtx(PARTICLE_MS_ENM_WATHIT, mtx, 0,
	                                         nullptr);
	SMSGetMSound()->startSoundSet(MSD_SE_EN_COMMON_W_HIT_OK, pos, 0, 0.0f, 0, 0,
	                              4);
}

} // namespace

TBeeHiveParams::TBeeHiveParams(const char* path)
    : TSpineEnemyParams(path)
    , PARAM_INIT(mGiveupTimer, 600)
    , PARAM_INIT(mGiveupRange, 1750.0f)
    , PARAM_INIT(mDecrimentTimer, 60)
    , PARAM_INIT(mRebound, 0.007f)
    , PARAM_INIT(mDecay, 0.98f)
    , PARAM_INIT(mAngleMaxAdd, 0.08f)
    , PARAM_INIT(mShakePower, 0.003f)
    , PARAM_INIT(mFallAngularVel, 0.01f)
    , PARAM_INIT(mSearchRange, 800.0f)
{
	TParams::load(mPrmPath);
}

TBee::TBee(MActor* actor, TBeeHive* hive)
    : TRealoidActor(actor)
{
	mHive = hive;
}

void TBee::init()
{
	initHitActor(ACTOR_TYPE_BEE, 1, 0x80000000, 20.0f, 20.0f, 50.0f, 50.0f);

	TIdxGroupObj* group
	    = static_cast<TIdxGroupObj*>(JDrama::TNameRefGen::search("敵グループ"));
	group->getChildren().push_back(this);

	onHitFilter(HIT_FILTER_NO_COLLISION);
}

BOOL TBee::receiveMessage(THitActor* sender, u32 message)
{
	switch (message) {
	case HIT_MESSAGE_TAKE:
		if (mHolder == nullptr) {
			mHolder = (TTakeActor*)sender;
			SMS_EasyEmitParticle(PARTICLE_MS_ENM_WATHIT, &mPosition, nullptr,
			                     JGeometry::TVec3<f32>(1.0f, 1.0f, 1.0f));
			return TRUE;
		}
		break;
	case HIT_MESSAGE_DETACH:
		if (mHolder != nullptr) {
			mHolder = nullptr;
			return TRUE;
		}
		break;
	case HIT_MESSAGE_UNKB:
		behaveToEat();
		return TRUE;
	}

	return FALSE;
}

void TBee::behaveToEat() { mHive->receiveMessageFromChild(this); }

TBeeHive::TBeeHive(const char* name)
    : TRealoid(name)
{
	mItem = nullptr;
}

void TBeeHive::init(TLiveManager* manager)
{
	mManager = manager;
	mManager->manageActor(this);
	onLiveFlag(LIVE_FLAG_UNK8 | LIVE_FLAG_UNK10);
	mSpine->initWith(&TNerveBeeHiveWait::theNerve());
	mInitPos      = mPosition;
	mActiveBeeNum = 0;
	mEatenBeeNum  = 0;
	initCollision();
}

void TBeeHive::reset()
{
	mPosition = mInitPos;

	TPosition3f mtx;
	MsMtxSetRotRPH(mtx, mRotation.x, 0.0f, mRotation.z);
	mtx.getQuat(mBaseRot);

	mSwingDir.setEulerY(mRotation.y);
	mSwingDirGoal = mSwingDir;
	mSwingAngle   = 0.0f;
	mSwingSpeed   = 0.0f;
	mSwingLimit   = 0.015707964f;
	mCollisionBee = 0;
	onLiveFlag(LIVE_FLAG_UNK10 | LIVE_FLAG_AIRBORNE);
	mMActor = mMActorKeeper->getMActor("bee_nest.bmd");
	offHitFilter(HIT_FILTER_NO_COLLISION);
}

void TBeeHive::initCollision()
{
	initHitActor(ACTOR_TYPE_BEE_HIVE, 0, 0x80000000, 50.0f, 50.0f, 100.0f,
	             100.0f);
	onHitFilter(HIT_FILTER_NO_ATTACK);
}

void TBeeHive::initBoids()
{
	for (int i = 0; i < unk150->mNumBoids; ++i) {
		TRealoidActor* bee = getRealoid(i);
		bee->init();
		bee->onFlag(TRealoidActor::FLAG_UNK2);
	}
}

void TBeeHive::setBoidBaseParams()
{
	unk150->mBaseSpeed         = 25.0f;
	unk150->mNeighborRadius    = 80.0f;
	unk150->mYawSpeed          = 8.0f;
	unk150->mPitchSpeed        = 8.0f;
	unk150->mMaxPitch          = 85.0f;
	unk150->mAlignmentStrength = 0.001f;
}

void TBeeHive::setBoidParamOnAttacking()
{
	setBoidBaseParams();
	unk150->setGoal(TPathNode((THitActor*)gpMarioAddress));
	unk150->mGoalOffset = JGeometry::TVec3<f32>(0.0f, 200.0f, 0.0f);
}

void TBeeHive::setBoidParamOnWaiting()
{
	setBoidBaseParams();
	unk150->setUnk38(mPosition);
	unk150->mGoalOffset = JGeometry::TVec3<f32>(0.0f, 0.0f, 0.0f);
}

void TBeeHive::setBoidParamOnMarioWaterIn()
{
	setBoidBaseParams();
	unk150->setGoal(TPathNode((THitActor*)gpMarioAddress));
	unk150->mGoalOffset = JGeometry::TVec3<f32>(0.0f, 500.0f, 0.0f);
}

void TBeeHive::receiveMessageFromChild(TBee* bee)
{
	if (bee->checkFlag(TRealoidActor::FLAG_UNK4))
		return;

	bee->onFlag(TRealoidActor::FLAG_UNK4);
	bee->onHitFilter(HIT_FILTER_NO_COLLISION);

	TMapObjBase* coin = mCoins[mEatenBeeNum];
	if (mEatenBeeNum != unk150->getBoidNum() - 1)
		coin = gpItemManager->makeObjAppear(ACTOR_TYPE_COIN);

	if (coin != nullptr) {
		coin->appear();
		coin->mPosition = bee->mPosition;
		coin->setVelocityAndFlag10(0.0f, 15.0f, 0.0f);
	}

	++mEatenBeeNum;
}

void TBeeHive::load(JSUMemoryInputStream& stream)
{
	loadDefault(stream, "bee_body.bmd", 2);
	loadCoin(stream);
	mMActorKeeper->createMActor("bee_nest_break.bmd", 3);
	mMActor = mMActorKeeper->createMActor("bee_nest.bmd", 3);
	initBoids();
	reset();
}

void TBeeHive::loadCoin(JSUMemoryInputStream& stream)
{
	u32 itemEventId;
	u32 lastCoinEventId;
	stream >> itemEventId;
	stream >> lastCoinEventId;

	mItem  = TMapObjBaseManager::newAndRegisterObjByEventID(itemEventId, "");
	mCoins = new TMapObjBase*[unk150->getBoidNum()];

	TMapObjBase** coin = mCoins;
	TMapObjBase** end  = &mCoins[unk150->getBoidNum()];
	for (; coin != end - 1; ++coin)
		*coin = TMapObjBaseManager::newAndRegisterObj("coin");

	*coin = TMapObjBaseManager::newAndRegisterObjByEventID(lastCoinEventId, "");
	if (*coin == nullptr)
		*coin = TMapObjBaseManager::newAndRegisterObj("coin");
}

TRealoidActor* TBeeHive::createRealoidActor(MActor* actor)
{
	return new TBee(actor, this);
}

BOOL TBeeHive::receiveMessage(THitActor* sender, u32 message)
{
	switch (message) {
	case HIT_MESSAGE_SPRAYED_BY_WATER: {
		JGeometry::TVec3<f32> dir = mPosition;
		dir -= *gpMarioPos;
		setShakePower(dir);
		SMS_EmitWaterHitParticleAndSound(&sender->mPosition);
		return TRUE;
	}

	case HIT_MESSAGE_TRAMPLE:
	case HIT_MESSAGE_HIP_DROP:
	case HIT_MESSAGE_PUNCH:
		if (isWaiting()) {
			mSpine->reset();
			mSpine->setNext(&TNerveBeeHiveFall::theNerve());
		}
		return TRUE;
	}

	return FALSE;
}

void TBeeHive::perform(u32 cue, JDrama::TGraphics* graphics)
{
	TRealoid::perform(cue, graphics);
	TSpineEnemy::perform(cue, graphics);
}

void TBeeHive::control()
{
	controlCollision();
	TLiveActor::control();
	controlSound();
}

void TBeeHive::bind()
{
	if (checkLiveFlag(LIVE_FLAG_UNK10))
		return;

	JGeometry::TVec3<f32> next = mPosition;
	next += mPositionDelta;
	next += mVelocity;

	mVelocity.y -= getGravityY();
	if (mVelocity.y < mVelocityMinY)
		mVelocity.y = mVelocityMinY;

	// the hive hangs from its top, so its bottom rises as it tilts
	f32 lift      = -60.0f * (-1.0f + MsCos(57.295776f * mSwingAngle));
	mGroundHeight = gpMap->checkGround(next.x, lift + (next.y + mHeadHeight),
	                                   next.z, &mGroundPlane);
	mGroundHeight += 1.0f;

	if (next.y + lift <= 0.05f + mGroundHeight) {
		if (mGroundPlane->isIllegalData())
			kill();
		offLiveFlag(LIVE_FLAG_AIRBORNE);
		mVelocity.set(0.0f, 0.0f, 0.0f);
		next.y = mGroundHeight - lift;
	} else {
		onLiveFlag(LIVE_FLAG_AIRBORNE);
	}

	gpMap->isTouchedOneWallAndMoveXZ(&next.x, next.y + mHeadHeight, &next.z,
	                                 mBodyRadius);
	mPositionDelta = next - mPosition;
}

// Only one bee per frame takes part in collision.
// TODO: 91%. The target has a dead `num <= idx + 1` compare before it
// reloads mCollisionBee and increments it in place; the idx local and the
// if/else here are a guess that at least keeps control() from inlining it.
void TBeeHive::controlCollision()
{
	int num = mActiveBeeNum;

	int idx            = mCollisionBee;
	TRealoidActor* bee = getRealoid(idx);
	bee->checkHitActors();
	bee->onHitFilter(HIT_FILTER_NO_ATTACK);

	if (num <= idx + 1)
		mCollisionBee = 0;
	else
		mCollisionBee = idx + 1;

	int next = mCollisionBee;
	if (num <= next)
		next = 0;

	TRealoidActor* nextBee = getRealoid(next);
	if (!nextBee->checkFlag(TRealoidActor::FLAG_UNK2_OR_UNK4))
		nextBee->offHitFilter(HIT_FILTER_NO_ATTACK);
}

void TBeeHive::controlSound()
{
	if (mActiveBeeNum == 0)
		return;

	int num = 0;
	JGeometry::TVec3<f32> sum(0.0f, 0.0f, 0.0f);
	for (int i = 0; i < mActiveBeeNum; ++i) {
		TRealoidActor* bee = getRealoid(i);
		if (!bee->checkFlag(TRealoidActor::FLAG_UNK2_OR_UNK4)) {
			++num;
			sum.x += bee->mPosition.x;
			sum.y += bee->mPosition.y;
			sum.z += bee->mPosition.z;
		}
	}

	if (num != 0) {
		sum.scale(1.0f / num);
		mSoundPos.set(sum.x, sum.y, sum.z);
		gpMSound->startBeeSe(&mSoundPos, num);
	}
}

// TODO: 81%. The quaternion products are right but scheduled and allocated
// differently, and the target frame is 0x20 bytes smaller.
void TBeeHive::calcRootMatrix()
{
	JGeometry::TQuat4<f32> rot = mBaseRot;
	JGeometry::TQuat4<f32> swing;
	swing.setEulerX(mSwingAngle);
	rot.mul(rot, mSwingDir);
	rot.mul(rot, swing);

	TPosition3f mtx;
	mtx.setSQ(mScaling, rot);
	mtx.setTrans(mPosition);
	mtx.ref(1, 3) += 120.0f;
	getModel()->setBaseTRMtx(mtx);
}

void TBeeHive::prepareWait()
{
	onLiveFlag(LIVE_FLAG_UNK10 | LIVE_FLAG_AIRBORNE);
	if (mActiveBeeNum < 3) {
		for (int i = 0; i < 3; ++i)
			appearBee(i);
		mActiveBeeNum = 3;
	}
}

// Swing like a pendulum. Returns true once the hive swings so far it falls.
// TODO: 95%. Inside the inlined TQuat4::slerp the target folds both 1 - 0.01
// and 0.01 into constants in the sin() branch, ours keeps t in a register;
// the target frame is also 0x88 bytes bigger.
bool TBeeHive::doWait()
{
	f32 prevSpeed = mSwingSpeed;
	f32 accel     = mSwingAngle * -getParam()->mRebound.get();
	mSwingSpeed += accel;
	mSwingSpeed *= getParam()->mDecay.get();
	mSwingAngle += mSwingSpeed;

	if (mSwingAngle < -mSwingLimit || mSwingLimit < mSwingAngle) {
		mSwingLimit += getParam()->mAngleMaxAdd.get();
		mSwingLimit = JGeometry::TUtil<f32>::clamp(
		    mSwingLimit, 0.0f, JGeometry::TUtil<f32>::halfPI());
		mSwingAngle = JGeometry::TUtil<f32>::clamp(mSwingAngle, -mSwingLimit,
		                                           mSwingLimit);
		mSwingSpeed = 0.0f;
		SMSGetMSound()->startSoundActorWithInfo(
		    MSD_SE_EN_BEENEST_SWING, &mPosition, nullptr, abs(mSwingAngle), 0,
		    0, nullptr, 0, 4);
	}

	if (!JGeometry::TUtil<f32>::epsilonEquals(0.0f, mSwingSpeed,
	                                          JGeometry::TUtil<f32>::epsilon())
	    && prevSpeed * mSwingSpeed <= 0.0f) {
		SMSGetMSound()->startSoundActorWithInfo(
		    MSD_SE_EN_BEENEST_SWING, &mPosition, nullptr, abs(mSwingAngle), 0,
		    0, nullptr, 0, 4);
	}

	mSwingDir.slerp(mSwingDirGoal, 0.01f);
	mSwingDir.normalize();

	JGeometry::TVec3<f32> toMario = *gpMarioPos;
	toMario -= mPosition;
	f32 range = getParam()->mSearchRange.get();
	if (toMario.squared() <= range * range)
		setBoidParamOnAttacking();
	else
		setBoidParamOnWaiting();

	return cAngleLimit <= fabs(mSwingAngle);
}

void TBeeHive::prepareFall()
{
	offLiveFlag(LIVE_FLAG_UNK10);
	onLiveFlag(LIVE_FLAG_AIRBORNE);
	SMSGetMSound()->startSoundActor(MSD_SE_EN_BEENEST_OFF, &mPosition, 0,
	                                nullptr, 0, 4);
	JGeometry::TVec3<f32> dir;
	mSwingDir.getZDir(dir);
	dir.scale(1.0f);
	mSwingSpeed = getParam()->mFallAngularVel.get();
	mVelocity   = dir;
}

bool TBeeHive::doFall()
{
	mSwingAngle += mSwingSpeed;
	return !checkLiveFlag(LIVE_FLAG_AIRBORNE);
}

void TBeeHive::appearBee(int idx)
{
	TRealoidActor* bee = getRealoid(idx);
	if (bee->checkFlag(TRealoidActor::FLAG_UNK4))
		return;
	if (!bee->checkFlag(TRealoidActor::FLAG_UNK2))
		return;

	bee->offFlag(TRealoidActor::FLAG_UNK2);
	bee->offHitFilter(HIT_FILTER_NO_COLLISION);
	unk150->getBoid(idx)->mPosition = mPosition;
}

void TBeeHive::disappearBee(int idx)
{
	TRealoidActor* bee = getRealoid(idx);
	if (bee->checkFlag(TRealoidActor::FLAG_UNK2))
		return;

	bee->onFlag(TRealoidActor::FLAG_UNK2);
	bee->onHitFilter(HIT_FILTER_NO_COLLISION);
}

void TBeeHive::decrimentBee()
{
	if (mActiveBeeNum > 3) {
		--mActiveBeeNum;
		disappearBee(mActiveBeeNum);
	}
}

void TBeeHive::appearAllBees()
{
	for (int i = 0; i < unk150->getBoidNum(); ++i)
		appearBee(i);
	mActiveBeeNum = unk150->getBoidNum();
}

void TBeeHive::dropCoin()
{
	TMapObjBase* item = mItem;
	if (item == nullptr)
		return;

	if (item->isActorType(ACTOR_TYPE_COIN))
		item = gpItemManager->makeObjAppear(ACTOR_TYPE_COIN);

	if (item != nullptr) {
		item->appear();
		item->JSGSetTranslation(mPosition);
		item->setVelocityAndFlag10(0.0f, 0.0f, 0.0f);
		item->onLiveFlag(LIVE_FLAG_AIRBORNE);
		mItem = nullptr;
	}
}

void TBeeHive::prepareBreak()
{
	mMActor = mMActorKeeper->getMActor("bee_nest_break.bmd");
	mMActor->setBck("bee_nest_break");
	onHitFilter(HIT_FILTER_NO_COLLISION);
	SMS_EasyEmitParticle(PARTICLE_MS_ENM_DISAP_A, &mPosition, this,
	                     JGeometry::TVec3<f32>(2.0f, 2.0f, 2.0f));
	SMS_EasyEmitParticle(PARTICLE_MS_ENM_DISAP_B, &mPosition, this,
	                     JGeometry::TVec3<f32>(2.0f, 2.0f, 2.0f));
	SMSGetMSound()->startSoundActor(MSD_SE_EN_BEENEST_LAND, &mPosition, 0,
	                                nullptr, 0, 4);
}

void TBeeHive::prepareVanish() { onLiveFlag(LIVE_FLAG_HIDDEN); }

void TBeeHive::prepareReset()
{
	reset();
	offLiveFlag(LIVE_FLAG_HIDDEN);
	mScaling.set(0.0f, 0.0f, 0.0f);
}

// Grow back in. Returns true once full size.
bool TBeeHive::doScaling()
{
	bool done = false;
	mScaling.x += 0.01f;
	if (1.0f <= mScaling.x) {
		mScaling.x = 1.0f;
		done       = true;
		mScaling.y = 1.0f;
		mScaling.z = 1.0f;
	} else {
		mScaling.y += 0.01f;
		mScaling.z += 0.01f;
	}
	return done;
}

// The swarm gives up once Mario gets far enough from it.
bool TBeeHive::isMissMario() const
{
	if (mActiveBeeNum == 0)
		return false;

	JGeometry::TVec3<f32> diff = *gpMarioPos;
	diff -= getCenterOfGravity();
	f32 range = getParam()->mGiveupRange.get();
	return range * range <= diff.squared();
}

// fabricated: the target reloads mSwingSpeed for the sign, as if it went
// through an inline taking a reference.
static inline int getSign(const f32& x)
{
	return x > 0.0f ? 1 : x < 0.0f ? -1 : 0;
}

void TBeeHive::setShakePower(const JGeometry::TVec3<f32>& dir)
{
	if (isFalling())
		return;

	JGeometry::TVec3<f32> flat = dir;
	flat.y                     = 0.0f;
	flat.normalize();

	f32 sign = abs(mSwingSpeed) < 0.0001f ? 1.0f : getSign(mSwingSpeed);

	mSwingDirGoal.setRotate(JGeometry::TVec3<f32>(0.0f, 0.0f, 1.0f), flat,
	                        1.0f);
	mSwingSpeed += sign * getParam()->mShakePower.get();
}

// TODO: 87%. The target reloads unk150->mBoids for every boid of the
// unrolled loop, ours loads it once.
JGeometry::TVec3<f32> TBeeHive::getCenterOfGravity() const
{
	if (mActiveBeeNum == 0)
		return JGeometry::TVec3<f32>(0.0f, 0.0f, 0.0f);

	JGeometry::TVec3<f32> sum(0.0f, 0.0f, 0.0f);
	for (int i = 0; i < mActiveBeeNum; ++i)
		sum += unk150->getBoid(i)->mPosition;
	sum.scale(1.0f / mActiveBeeNum);
	return sum;
}

bool TBeeHive::isFalling() const
{
	return mSpine->getLatestNerve() == &TNerveBeeHiveFall::theNerve();
}

bool TBeeHive::isWaiting() const
{
	return mSpine->getLatestNerve() == &TNerveBeeHiveWait::theNerve();
}

// TODO: UNUSED, 0x14 bytes, and nothing linked shows its body.
bool TBeeHive::isNotBreakOnceYet() const { return false; }

TBeeHiveManager::TBeeHiveManager(const char* name)
    : TEnemyManager(name)
{
}

void TBeeHiveManager::load(JSUMemoryInputStream& stream)
{
	unk38 = new TBeeHiveParams("/Animal/beehive.prm");
	TEnemyManager::load(stream);
}

void TBeeHiveManager::createModelData()
{
	static const TModelDataLoadEntry entry[] = {
		{ "bee_body.bmd",
		  J3DMLF_MaterialPEFull | J3DMLF_UseUniqueMaterials
		      | (1 << J3DMLF_TevStageNumShift),
		  0 },
		{ "bee_nest.bmd",
		  J3DMLF_MaterialPEFull | J3DMLF_UseUniqueMaterials
		      | (1 << J3DMLF_TevStageNumShift),
		  0 },
		{ "bee_nest_break.bmd",
		  J3DMLF_MaterialPEFull | J3DMLF_UseUniqueMaterials
		      | (1 << J3DMLF_TevStageNumShift),
		  0 },
		{ nullptr, 0, 0 },
	};
	createModelDataArray(entry);
}

DEFINE_NERVE(TNerveBeeHiveWait, TLiveActor)
{
	TBeeHive* hive = (TBeeHive*)spine->getBody();
	if (spine->getTime() == 0) {
		hive->prepareWait();
		hive->setBoidParamOnWaiting();
	}

	if (hive->doWait()) {
		spine->pushAfterCurrent(&TNerveBeeHiveFall::theNerve());
		return true;
	}

	// let one bee go home every now and then
	if (hive->getParam()->mDecrimentTimer.get() < spine->getTime()) {
		hive->decrimentBee();
		spine->pushAfterCurrent(this);
		return true;
	}

	return false;
}

DEFINE_NERVE(TNerveBeeHiveFall, TLiveActor)
{
	TBeeHive* hive = (TBeeHive*)spine->getBody();
	if (spine->getTime() == 0) {
		hive->prepareFall();
		hive->dropCoin();
	}

	if (hive->doFall()) {
		spine->pushAfterCurrent(&TNerveBeeHiveBreak::theNerve());
		return true;
	}

	return false;
}

DEFINE_NERVE(TNerveBeeHiveBreak, TLiveActor)
{
	TBeeHive* hive = (TBeeHive*)spine->getBody();
	if (spine->getTime() == 0) {
		hive->prepareBreak();
		hive->appearAllBees();
		hive->setBoidParamOnAttacking();
	}

	if (hive->checkCurAnmEnd(0)) {
		spine->pushAfterCurrent(&TNerveBeeHiveAttack::theNerve());
		hive->prepareVanish();
		return true;
	}

	return false;
}

DEFINE_NERVE(TNerveBeeHiveAttack, TLiveActor)
{
	TBeeHive* hive = (TBeeHive*)spine->getBody();
	if (spine->getTime() == 0)
		hive->setBoidParamOnAttacking();

	if (SMS_IsMarioInWater()) {
		spine->pushAfterCurrent(&TNerveBeeHiveMarioWaterIn::theNerve());
		return true;
	}

	if (hive->isMissMario()) {
		spine->pushAfterCurrent(&TNerveBeeHiveReset::theNerve());
		return true;
	}

	return false;
}

DEFINE_NERVE(TNerveBeeHiveMarioWaterIn, TLiveActor)
{
	TBeeHive* hive = (TBeeHive*)spine->getBody();
	if (spine->getTime() == 0)
		hive->setBoidParamOnMarioWaterIn();

	if (hive->getParam()->mGiveupTimer.get() < spine->getTime()) {
		spine->pushAfterCurrent(&TNerveBeeHiveReset::theNerve());
		return true;
	}

	if (!SMS_IsMarioInWater()) {
		spine->pushAfterCurrent(&TNerveBeeHiveAttack::theNerve());
		return true;
	}

	return false;
}

DEFINE_NERVE(TNerveBeeHiveReset, TLiveActor)
{
	TBeeHive* hive = (TBeeHive*)spine->getBody();
	if (spine->getTime() == 0)
		hive->prepareReset();

	if (hive->doScaling()) {
		spine->pushAfterCurrent(&TNerveBeeHiveWait::theNerve());
		return true;
	}

	return false;
}
