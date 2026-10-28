#include <Animal/Bird.hpp>
#include <Animal/AnimalBase.hpp>
#include <Enemy/Graph.hpp>
#include <Enemy/PathNode.hpp>
#include <Enemy/WireBinder.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DModel.hpp>
#include <JSystem/JDrama/JDRNameRefGen.hpp>
#include <JSystem/JGeometry.hpp>
#include <JSystem/JUtility/JUTNameTab.hpp>
#include <M3DUtil/MActor.hpp>
#include <MSound/MSound.hpp>
#include <MSound/MSoundSE.hpp>
#include <MSound/SoundEffects.hpp>
#include <Map/Map.hpp>
#include <MarioUtil/EffectUtil.hpp>
#include <MarioUtil/MathUtil.hpp>
#include <MarioUtil/PacketUtil.hpp>
#include <MarioUtil/RandomUtil.hpp>
#include <MoveBG/Item.hpp>
#include <MoveBG/ItemManager.hpp>
#include <MoveBG/MapObjBase.hpp>
#include <MoveBG/MapObjManager.hpp>
#include <Player/MarioAccess.hpp>
#include <Strategic/ObjModel.hpp>
#include <Strategic/Spine.hpp>
#include <Strategic/Strategy.hpp>
#include <System/FlagManager.hpp>
#include <System/MarDirector.hpp>
#include <System/Particles.hpp>

// rogue includes needed for matching sinit & bss
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>
#include <M3DUtil/InfectiousStrings.hpp>

// Indices into the model's BCK list. Only the ones with a .bas file have a
// known name.
enum {
	BIRD_ANM_UNK0  = 0,
	BIRD_ANM_FLY   = 1,
	BIRD_ANM_OPEN  = 2,
	BIRD_ANM_UNK3  = 3,
	BIRD_ANM_UNK4  = 4,
	BIRD_ANM_START = 5,
	BIRD_ANM_STOP  = 6,
	BIRD_ANM_UNK7  = 7,
	BIRD_ANM_UNK8  = 8,
};

static const char* bird_bastable[] = {
	nullptr,
	"/scene/bird/bas/bird_fly.bas",
	"/scene/bird/bas/bird_open.bas",
	nullptr,
	nullptr,
	"/scene/bird/bas/bird_start.bas",
	"/scene/bird/bas/bird_stop.bas",
	nullptr,
	nullptr,
};

namespace {

// The idle animations picked at random while sitting on the ground. Picking
// BIRD_ANM_UNK8 starts a walk instead.
const int cRandomAnims[] = {
	BIRD_ANM_UNK7, BIRD_ANM_UNK4, BIRD_ANM_UNK0, BIRD_ANM_OPEN, BIRD_ANM_UNK8,
};

const char* const cMatName = "_mat_body1";

// Body colour by what the bird turns into: blue coin, coin, Shine, red coin.
const GXColorS10 cColorTable[] = {
	{ 0, 100, 255, 0 },
	{ 0, 200, 0, 0 },
	{ 255, 200, 0, 0 },
	{ 255, 0, 0, 0 },
};

} // namespace

TAnimalBird::TAnimalBird(const char* name)
    : TSpineEnemy(name)
    , mItem(nullptr)
    , mWireBinder(nullptr)
{
}

void TAnimalBird::init(TLiveManager* manager)
{
	mManager = manager;
	mManager->manageActor(this);
	mMActorKeeper = new TMActorKeeper(mManager, 1);
	mMActor       = mMActorKeeper->createMActor("bird_man.bmd", 0);
	mSpine->initWith(&TNerveAnimalBirdWaitOnGround::theNerve());
	initParams();
	initCollision();
	initAnmSound();
}

void TAnimalBird::initTevColor(const _GXColorS10* color)
{
	int index
	    = getModel()->getModelData()->getMaterialName()->getIndex(cMatName);
	SMS_InitPacket_OneTevColor(getModel(), index, GX_TEVREG1, color);
}

void TAnimalBird::initCollision()
{
	initHitActor(ACTOR_TYPE_ANIMAL_BIRD, 0, 0, 50.0f, 50.0f, 70.0f, 80.0f);
	onHitFilter(HIT_FILTER_NO_ATTACK);
	offHitFilter(HIT_FILTER_NO_COLLISION);
	mScaledBodyRadius = 35.0f;
}

void TAnimalBird::initParams()
{
	mHomePos.x = mPosition.x;
	mHomePos.y = mPosition.y;
	mHomePos.z = mPosition.z;
	mHomePos.y += 90.0f;
	mHomeRot.x = mRotation.x;
	mHomeRot.y = mRotation.y;
	mHomeRot.z = mRotation.z;
	resetHitPoints();
	mWaterproofTimer = 0;
	mFloatingTimer   = 0;
	mWalkDir         = 1.0f;
	offLiveFlag(LIVE_FLAG_AIRBORNE);
	mScale = 1.0f - 0.1f * (MsRandF() - 0.5f);
	if (TWireBinder::isOnWire(mPosition)) {
		mWireBinder = new TWireBinder;
		mWireBinder->init(mPosition);
	}
}

// TODO: frame-only, the target's frame is 8 bytes larger.
void TAnimalBird::load(JSUMemoryInputStream& stream)
{
	TSpineEnemy::load(stream);

	s32 eventId;
	stream.read(&eventId, 4);
	if (eventId >= 0)
		mItem = TMapObjBaseManager::newAndRegisterObjByEventID(eventId, "鳥用");
	else
		mItem = TMapObjBaseManager::newAndRegisterObjByEventID(100, "");

	switch (mItem->mActorType) {
	default:
		mColorIndex = 1;
		break;
	case ACTOR_TYPE_SHINE:
		mColorIndex = 2;
		break;
	case ACTOR_TYPE_COIN_RED:
		mColorIndex = 3;
		break;
	case ACTOR_TYPE_COIN_BLUE:
		mColorIndex = 0;
		checkNotAppear(eventId);
		break;
	}

	initTevColor(&cColorTable[mColorIndex]);
}

// TODO: frame-only (0x18 larger in the target), the same as
// TAnimalBase::loadAfter and TMewManager::loadAfter.
void TAnimalBird::loadAfter()
{
	TSpineEnemy::loadAfter();
	MSoundSESystem::MSRandPlay::registerTrans(MSD_SE_OBJ_BIRD_DOL_FLYING1,
	                                          &mPosition);
	MSoundSESystem::MSRandPlay::registerTrans(MSD_SE_OBJ_BIRD_DOL_CHUN,
	                                          &mPosition);
}

// TODO: frame-only, the target's frame is 8 bytes larger.
BOOL TAnimalBird::receiveMessage(THitActor* sender, u32 message)
{
	if (checkLiveFlag(LIVE_FLAG_DEAD))
		return false;

	if (message == HIT_MESSAGE_SPRAYED_BY_WATER) {
		SMS_EasyEmitParticle(PARTICLE_MS_ENM_WATHIT, &sender->mPosition,
		                     nullptr, JGeometry::TVec3<f32>(1.0f, 1.0f, 1.0f));
		gpMSound->startSoundSet(MSD_SE_EN_COMMON_W_HIT_OK, &sender->mPosition,
		                        0, 0.0f, 0, 0, 4);
		behaveHitWater();
		return true;
	}

	if (message == HIT_MESSAGE_TAKE && mHolder == nullptr) {
		onHitFilter(HIT_FILTER_NO_COLLISION);
		mHolder = (TTakeActor*)sender;
		SMS_EasyEmitParticle(PARTICLE_MS_ENM_WATHIT, &sender->mPosition,
		                     nullptr, JGeometry::TVec3<f32>(1.0f, 1.0f, 1.0f));
		return true;
	}

	if ((message == HIT_MESSAGE_PUT || message == HIT_MESSAGE_THROWN)
	    && mHolder == sender) {
		mHolder = nullptr;
		offHitFilter(HIT_FILTER_NO_COLLISION);
		return true;
	}

	if (message == HIT_MESSAGE_UNKB) {
		mHolder = nullptr;
		if (isChangeToItem()) {
			mSpine->reset();
			mSpine->setNext(&TNerveAnimalBirdChangeToCoin::theNerve());
		} else {
			kill();
		}
		return true;
	}

	if (message == HIT_MESSAGE_TRAMPLE
	    && sender->isActorType(ACTOR_TYPE_POPO)) {
		if (isChangeToItem()) {
			mSpine->reset();
			mSpine->setNext(&TNerveAnimalBirdChangeToCoin::theNerve());
		} else {
			receiveMessage(this, HIT_MESSAGE_SPRAYED_BY_WATER);
		}
		return true;
	}

	return TSpineEnemy::receiveMessage(sender, message);
}

void TAnimalBird::calcRootMatrix()
{
	if (mHolder)
		getModel()->setBaseTRMtx(mHolder->getTakingMtx());
	else
		TSpineEnemy::calcRootMatrix();
	getModel()->getBaseTRMtx()[1][3] += 35.0f;
}

// TODO: frame-only, the target's frame is 0x18 larger.
void TAnimalBird::moveObject()
{
	if (isWaterproof())
		mWaterproofTimer--;
	checkFalling();
	checkChangeToItem();
	updateSound();
	TLiveActor::moveObject();
}

// TODO: frame-only, the target's frame is 8 bytes larger.
void TAnimalBird::bind()
{
	if (!isCheckWithWireBinder()) {
		TLiveActor::bind();
		return;
	}
	mWireBinder->bind(this);
}

const char** TAnimalBird::getBasNameTable() const { return bird_bastable; }

void TAnimalBird::behaveHitWater()
{
	if (!isWaterproof()) {
		mWaterproofTimer = getParam()->mWaterproofTimerMax.get();
		if (checkLiveFlag(LIVE_FLAG_AIRBORNE))
			decHitPoints();
	}
}

bool TAnimalBird::isOnGroundNerve() const
{
	TSpineBase<TLiveActor>* spine = mSpine;
	return spine->getLatestNerve() == &TNerveAnimalBirdWaitOnGround::theNerve()
	       || spine->getLatestNerve()
	              == &TNerveAnimalBirdActionOnGround::theNerve()
	       || spine->getLatestNerve()
	              == &TNerveAnimalBirdWalkOnGround::theNerve();
}

// Sitting on the ground while still airborne for too long means whatever the
// bird was standing on is gone, so it takes off again.
void TAnimalBird::checkFalling()
{
	if (isOnGroundNerve()) {
		if (checkLiveFlag(LIVE_FLAG_AIRBORNE)) {
			if (getParam()->mFloatingTimerMax.get() < ++mFloatingTimer) {
				mSpine->reset();
				mSpine->setNext(&TNerveAnimalBirdTakeoff::theNerve());
			}
		} else {
			mFloatingTimer = 0;
		}
	}
}

void TAnimalBird::checkChangeToItem()
{
	if (isChangeToItem() && getHitPoints() == 0) {
		mSpine->reset();
		mSpine->setNext(&TNerveAnimalBirdChangeToCoin::theNerve());
	}
}

// TODO: UNUSED size is 0x58 in the map, ours is 4 bytes smaller.
void TAnimalBird::checkNotAppear(s32 event_id)
{
	if (TFlagManager::smInstance->getBlueCoinFlag(
	        gpMarDirector->getCurrentMap(), event_id))
		onLiveFlag(LIVE_FLAG_DEAD);
}

void TAnimalBird::updateSound()
{
	if (isFlying())
		gpMSound->startSeRandPlay(MSD_SE_OBJ_BIRD_DOL_FLYING1, mInstanceIndex);
	if (isOnGroundNerve())
		gpMSound->startSeRandPlay(MSD_SE_OBJ_BIRD_DOL_CHUN, mInstanceIndex);
}

bool TAnimalBird::isWantToFly() const
{
	return (isWaterproof() || isFindMario()) && MsRandF() < 0.5f;
}

bool TAnimalBird::isWantToAction() const
{
	int t = mSpine->getTime() - getParam()->mActionTimer.get();
	if (t < 0)
		return false;
	f32 chance = (f32)t / (f32)getParam()->mActionTimerAdd.get();
	return MsRandF() < chance;
}

// Not used by TNerveAnimalBirdWalkOnGround, which reads the spine it is given
// instead. The size matches the map.
bool TAnimalBird::isWantToRest() const
{
	return getParam()->mWalkTimer.get() < mSpine->getTime();
}

bool TAnimalBird::isFindMario() const
{
	if (getParam()->mSearchHeight.get()
	    < fabsf(SMS_GetMarioPos().y - mPosition.y))
		return false;
	return isInSight(SMS_GetMarioPos(),
	                 mScale * getParam()->mSearchLength.get(),
	                 mScale * getParam()->mSearchAngle.get(),
	                 mScale * getParam()->mSearchAware.get());
}

bool TAnimalBird::isChangeToItem() const { return !isChanged(); }

bool TAnimalBird::isGroundShaken() const { return false; }

bool TAnimalBird::isCheckWithWireBinder() const
{
	return mWireBinder
	       && (isOnGroundNerve()
	           || mSpine->getLatestNerve()
	                  == &TNerveAnimalBirdPreLanding::theNerve());
}

bool TAnimalBird::isChanged() const
{
	return mSpine->getLatestNerve()
	       == &TNerveAnimalBirdChangeToCoin::theNerve();
}

bool TAnimalBird::isFlying() const
{
	const TNerveBase<TLiveActor>* nerve = mSpine->getLatestNerve();
	return nerve == &TNerveAnimalBirdGraphWander::theNerve()
	       || nerve == &TNerveAnimalBirdComeback::theNerve();
}

void TAnimalBird::doDropCoin()
{
	if (mItem->isActorType(ACTOR_TYPE_SHINE)) {
		mItem->JSGSetTranslation(mPosition);
		((TShine*)mItem)->appearWithDemo("鳥シャインカメラ");
	} else {
		TMapObjBase* coin;
		if (mItem->isActorType(ACTOR_TYPE_COIN))
			coin = gpItemManager->makeObjAppear(ACTOR_TYPE_COIN);
		else
			coin = mItem;

		if (coin) {
			coin->appear();
			coin->JSGSetTranslation(mPosition);
			coin->mVelocity.set(0.0f, -10.0f, 0.0f);
			coin->offLiveFlag(LIVE_FLAG_UNK10);
			coin->onLiveFlag(LIVE_FLAG_AIRBORNE);
		}
	}
}

// TODO: 97.2%, dist and marchSpeed get each other's float registers and a
// stack slot is 4 bytes off.
void TAnimalBird::doFlyToCurPathNode()
{
	JGeometry::TVec3<f32> diff = getUnkF4().getPoint();
	diff -= mPosition;

	f32 dist = diff.length();
	if (dist < 100.0f)
		return;

	f32 marchSpeed = getMyMarchSpeed() * SMSGetAnmFrameRate();
	f32 turnSpeed  = getParam()->mTurnSpeed.get();
	turnSpeed *= SMSGetAnmFrameRate();
	if (dist <= 2.0f * calcMinimumTurnRadius(marchSpeed, turnSpeed))
		turnSpeed = calcTurnSpeedToReach(marchSpeed, 0.5f * dist);

	TAnimalBase::getRotationFlyToDir(&mRotation, diff, marchSpeed, turnSpeed);

	JGeometry::TQuat4<f32> rot = SMS_Eular2Quat(mRotation);
	JGeometry::TVec3<f32> velocity(0.0f, 0.0f, marchSpeed);
	rot.rotate(velocity);

	// Getting sprayed slows the bird down and pushes it towards the ground.
	velocity.scale(1.0f - getWaterDamageRate());
	velocity.y -= getWaterPowerY();
	mPositionDelta = velocity;
}

// TODO: UNUSED size is 0x198 in the map, ours is 4 bytes smaller. Inlined
// into TNerveAnimalBirdWalkOnGround the in-place quaternion rotate is
// scheduled differently (the target reloads the quaternion's x).
void TAnimalBird::doWalk()
{
	mGravity    = 0.15f;
	f32 torqueY = getParam()->mWalkingTorqueY.get();
	mRotation.y
	    = MsWrap(mWalkDir * (torqueY * SMSGetAnmFrameRate()) + mRotation.y,
	             0.0f, 360.0f);

	JGeometry::TQuat4<f32> rot = SMS_Eular2Quat(mRotation);
	JGeometry::TVec3<f32> velocity(0.0f, 0.0f, getParam()->mWalkingSpeed.get());
	rot.rotate(velocity);
	mPositionDelta = velocity;
}

// TODO: 97.4%. The first rotate gets other float registers, and the target
// loads mHomeRot.y before mRotation.y for the MsAngleDiff.
// Glide down onto the perch and turn back to the facing from the scene file.
// Returns true once the bird stands on the ground facing the right way.
bool TAnimalBird::doLanding(bool init)
{
	if (init) {
		f32 speed                  = getMyMarchSpeed() * SMSGetAnmFrameRate();
		JGeometry::TQuat4<f32> rot = SMS_Eular2Quat(mRotation);
		JGeometry::TVec3<f32> dir;
		rot.rotate(JGeometry::TVec3<f32>(0.0f, 0.0f, speed), dir);
		mVelocity = JGeometry::TVec3<f32>(dir.x, 0.0f, dir.z);
	}

	bool landed = false;
	JGeometry::TVec3<f32> delta(0.0f, 0.0f, 0.0f);
	// The height is looked up but never used.
	getFootGroundHeight();
	if (checkLiveFlag(LIVE_FLAG_AIRBORNE))
		delta.y = -getParam()->mLandingGravityY.get();
	else
		landed = true;

	mRotation.x = mHomeRot.x;
	mRotation.z = mHomeRot.z;

	f32 torqueY = getParam()->mLandingTorqueY.get();
	f32 torque  = torqueY * SMSGetAnmFrameRate();
	f32 diff    = MsAngleDiff(mHomeRot.y, mRotation.y);
	f32 turn    = JGeometry::TUtil<f32>::clamp(diff, -torque, torque);
	mRotation.y = MsWrap(mRotation.y + turn, 0.0f, 360.0f);

	mPositionDelta = delta;

	JGeometry::TVec3<f32> velocity(0.0f, 0.0f, getVelocity().length());
	velocity.scale(getParam()->mLandingFric.get());
	SMS_Eular2Quat(mRotation).rotate(velocity);
	mVelocity = velocity;

	return landed && fabsf(turn) < 0.01f;
}

void TAnimalBird::doGotoRandomNextGraphNode()
{
	goToRandomNextGraphNode();

	JGeometry::TVec3<f32> goal = getUnk104().getPoint();
	goal.x += 200.0f * (MsRandF() - 0.5f);
	goal.y += 200.0f * (MsRandF() - 0.5f);
	goal.z += 200.0f * (MsRandF() - 0.5f);
	setGoalPath(TPathNode(goal));
}

void TAnimalBird::setGoalToComeback() { setGoalPath(TPathNode(mHomePos)); }

void TAnimalBird::setParamsOnFloating()
{
	onLiveFlag(LIVE_FLAG_AIRBORNE);
	mGravity       = 0.0f;
	mFloatingTimer = 0;
}

void TAnimalBird::setParamsOnLanding() { }

void TAnimalBird::setBckAnm(int index)
{
	mMActor->setBckFromIndex(index);
	setCurAnmSound();
}

f32 TAnimalBird::getMyMarchSpeed() const
{
	return mScale * getParam()->mMarchSpeed.get();
}

// How soaked the bird still is, from 1 right after a hit down to 0.
f32 TAnimalBird::getWaterDamageRate() const
{
	return (f32)mWaterproofTimer / (f32)getParam()->mWaterproofTimerMax.get();
}

f32 TAnimalBird::getWaterPowerY() const
{
	return getParam()->mWaterPowerY.get() * getWaterDamageRate();
}

f32 TAnimalBird::getFootGroundHeight()
{
	if (mWireBinder) {
		JGeometry::TVec3<f32> point;
		mWireBinder->getPoint(&point, mHomePos);
		return point.y;
	}
	return gpMap->checkGround(mPosition, &mGroundPlane);
}

TAnimalBirdParams::TAnimalBirdParams(const char* path)
    : TSpineEnemyParams(path)
    , PARAM_INIT(mMarchSpeed, 5.0f)
    , PARAM_INIT(mTurnSpeed, 0.1f)
    , PARAM_INIT(mReturnTimer, 1800)
    , PARAM_INIT(mSearchLength, 800.0f)
    , PARAM_INIT(mSearchHeight, 600.0f)
    , PARAM_INIT(mSearchAware, 400.0f)
    , PARAM_INIT(mSearchAngle, 90.0f)
    , PARAM_INIT(mActionTimer, 100)
    , PARAM_INIT(mWaterproofTimerMax, 45)
    , PARAM_INIT(mFloatingTimerMax, 30)
    , PARAM_INIT(mLandingGravityY, 1.0f)
    , PARAM_INIT(mLandingTorqueY, 2.0f)
    , PARAM_INIT(mWalkingTorqueY, 0.5f)
    , PARAM_INIT(mWalkingSpeed, 2.0f)
    , PARAM_INIT(mWalkTimer, 100)
    , PARAM_INIT(mLandingFric, 0.95f)
    , PARAM_INIT(mActionTimerAdd, 300)
    , PARAM_INIT(mWaterPowerY, 15.0f)
{
	TParams::load(mPrmPath);
}

TAnimalBirdManager::TAnimalBirdManager(const char* name)
    : TEnemyManager(name)
{
}

void TAnimalBirdManager::load(JSUMemoryInputStream& stream)
{
	unk38 = new TAnimalBirdParams("/Animal/bird.prm");
	TEnemyManager::load(stream);
}

// TODO: frame-only, the target's frame is 0x20 larger.
void TAnimalBirdManager::loadAfter()
{
	TEnemyManager::loadAfter();
	MSoundSESystem::MSRandPlay::createRandPlayVec(MSD_SE_OBJ_BIRD_DOL_FLYING1,
	                                              mObjNum);
	MSoundSESystem::MSRandPlay::createRandPlayVec(MSD_SE_OBJ_BIRD_DOL_CHUN,
	                                              mObjNum);
}

void TAnimalBirdManager::createModelData()
{
	static const TModelDataLoadEntry entry[]
	    = { { "bird_man.bmd", 0x10210000, 0 }, { nullptr, 0, 0 } };
	createModelDataArray(entry);
}

DEFINE_NERVE(TNerveAnimalBirdWaitOnGround, TLiveActor)
{
	TAnimalBird* self = (TAnimalBird*)spine->getBody();
	if (spine->getTime() == 0)
		self->setBckAnm(BIRD_ANM_UNK7);

	if (self->isWantToFly()) {
		spine->pushAfterCurrent(&TNerveAnimalBirdTakeoff::theNerve());
		return true;
	}

	if (self->checkCurAnmEnd(0) && self->isWantToAction()) {
		spine->pushAfterCurrent(&TNerveAnimalBirdActionOnGround::theNerve());
		return true;
	}

	return false;
}

DEFINE_NERVE(TNerveAnimalBirdActionOnGround, TLiveActor)
{
	TAnimalBird* self = (TAnimalBird*)spine->getBody();
	if (spine->getTime() == 0) {
		int anm = cRandomAnims[(int)(5.0f * MsRandF())];
		if (anm == BIRD_ANM_UNK8) {
			spine->pushAfterCurrent(&TNerveAnimalBirdWalkOnGround::theNerve());
			return true;
		}

		MActor* mactor = self->getMActor();
		if (!mactor->checkCurBckFromIndex(anm))
			mactor->setBckFromIndex(anm);
	}

	if (self->isWantToFly()) {
		spine->pushAfterCurrent(&TNerveAnimalBirdTakeoff::theNerve());
		return true;
	}

	if (self->checkCurAnmEnd(0)) {
		spine->pushAfterCurrent(&TNerveAnimalBirdWaitOnGround::theNerve());
		return true;
	}

	return false;
}

// TODO: 93.1%, see doWalk.
DEFINE_NERVE(TNerveAnimalBirdWalkOnGround, TLiveActor)
{
	TAnimalBird* self = (TAnimalBird*)spine->getBody();
	if (spine->getTime() == 0) {
		self->mWalkDir *= -1.0f;
		self->setBckAnm(BIRD_ANM_UNK8);
	}

	if (self->isWantToFly()) {
		spine->pushAfterCurrent(&TNerveAnimalBirdTakeoff::theNerve());
		return true;
	}

	self->doWalk();

	if (self->getParam()->mWalkTimer.get() < spine->getTime()) {
		spine->pushAfterCurrent(&TNerveAnimalBirdWaitOnGround::theNerve());
		return true;
	}

	return false;
}

// TODO: frame-only, the target's frame is 8 bytes larger.
DEFINE_NERVE(TNerveAnimalBirdTakeoff, TLiveActor)
{
	TAnimalBird* self = (TAnimalBird*)spine->getBody();
	if (spine->getTime() == 0) {
		self->setBckAnm(BIRD_ANM_START);
		self->setParamsOnFloating();
		J3DFrameCtrl* ctrl = self->getMActor()->getFrameCtrl(ANM_TYPE_BCK);
		ctrl->setRate(3.0f * ctrl->getRate());
		if (gpMSound->gateCheck(MSD_SE_OBJ_BIRD_DOL_TO_FLY1))
			MSoundSESystem::MSoundSE::startSoundActor(
			    MSD_SE_OBJ_BIRD_DOL_TO_FLY1, &self->mPosition, 0, nullptr, 0,
			    4);
	}

	if (self->checkCurAnmEnd(0)) {
		spine->pushAfterCurrent(&TNerveAnimalBirdGraphWander::theNerve());
		self->setParamsOnFloating();
		return true;
	}

	return false;
}

// TODO: frame-only, our frame is 0x18 larger.
DEFINE_NERVE(TNerveAnimalBirdGraphWander, TLiveActor)
{
	TAnimalBird* self = (TAnimalBird*)spine->getBody();
	if (spine->getTime() == 0) {
		self->mVelocity = JGeometry::TVec3<f32>(0.0f, 0.0f, 0.0f);
		self->unk124->reset();
		self->goToShortestNextGraphNode();
	}

	if (spine->getTime() == 0 || self->isReachedToGoal()) {
		self->doGotoRandomNextGraphNode();
		// Climb while the next goal is above, glide otherwise.
		if (self->mPosition.y <= self->getUnkF4().getPoint().y)
			self->setBckAnm(BIRD_ANM_FLY);
		else
			self->setBckAnm(BIRD_ANM_UNK3);
	}

	self->checkCurAnmEnd(0);

	if (self->getParam()->mReturnTimer.get() < self->mSpine->getTime()) {
		spine->pushAfterCurrent(&TNerveAnimalBirdComeback::theNerve());
		return true;
	}

	self->doFlyToCurPathNode();
	return false;
}

DEFINE_NERVE(TNerveAnimalBirdChangeToCoin, TLiveActor)
{
	TAnimalBird* self = (TAnimalBird*)spine->getBody();
	if (spine->getTime() == 0) {
		self->onLiveFlag(LIVE_FLAG_DEAD);
		self->doDropCoin();
	}
	return true;
}

DEFINE_NERVE(TNerveAnimalBirdComeback, TLiveActor)
{
	TAnimalBird* self = (TAnimalBird*)spine->getBody();
	if (spine->getTime() == 0) {
		self->setGoalToComeback();
		self->setBckAnm(BIRD_ANM_UNK3);
	}

	self->doFlyToCurPathNode();

	if (self->isFindMario()) {
		spine->pushAfterCurrent(&TNerveAnimalBirdGraphWander::theNerve());
		return true;
	}

	if (self->isReachedToGoal()) {
		spine->pushAfterCurrent(&TNerveAnimalBirdPreLanding::theNerve());
		return true;
	}

	return false;
}

DEFINE_NERVE(TNerveAnimalBirdPreLanding, TLiveActor)
{
	TAnimalBird* self = (TAnimalBird*)spine->getBody();
	if (spine->getTime() == 0) {
		self->setBckAnm(BIRD_ANM_FLY);
		J3DFrameCtrl* ctrl = self->getMActor()->getFrameCtrl(ANM_TYPE_BCK);
		ctrl->setRate(1.5f * ctrl->getRate());
		self->doLanding(true);
	}

	if (self->isFindMario()) {
		spine->pushAfterCurrent(&TNerveAnimalBirdGraphWander::theNerve());
		return true;
	}

	if (self->doLanding(false)) {
		spine->pushAfterCurrent(&TNerveAnimalBirdLanding::theNerve());
		return true;
	}

	return false;
}

// TODO: 98.8%, register allocation only (self, spine and ctrl swap).
DEFINE_NERVE(TNerveAnimalBirdLanding, TLiveActor)
{
	TAnimalBird* self  = (TAnimalBird*)spine->getBody();
	J3DFrameCtrl* ctrl = self->getMActor()->getFrameCtrl(ANM_TYPE_BCK);
	if (spine->getTime() == 0) {
		self->mVelocity = JGeometry::TVec3<f32>(0.0f, 0.0f, 0.0f);
		self->setBckAnm(BIRD_ANM_START);
		ctrl->setAttribute(J3DFrameCtrl::ATTR_ONCE_AND_RESET);
		ctrl->setFrame(ctrl->getEnd());
		ctrl->setRate(-1.0f * ctrl->getRate());
	}

	if (self->isFindMario()) {
		spine->pushAfterCurrent(&TNerveAnimalBirdGraphWander::theNerve());
		return true;
	}

	if (ctrl->checkState(J3DFrameCtrl::STATE_COMPLETED_ONCE)) {
		spine->pushAfterCurrent(&TNerveAnimalBirdWaitOnGround::theNerve());
		return true;
	}

	return false;
}
