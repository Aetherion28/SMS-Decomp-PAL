#include <Enemy/FruitsBoat.hpp>
#include <Enemy/Conductor.hpp>
#include <Enemy/Graph.hpp>
#include <JSystem/JDrama/JDRNameRefGen.hpp>
#include <JSystem/JSupport/JSUInputStream.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DAnimation.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DModel.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DTransform.hpp>
#include <M3DUtil/MActor.hpp>
#include <M3DUtil/MActorData.hpp>
#include <MSound/MSound.hpp>
#include <MSound/SoundEffects.hpp>
#include <Map/Map.hpp>
#include <Map/MapCollisionManager.hpp>
#include <Map/MapData.hpp>
#include <MarioUtil/MathUtil.hpp>
#include <MarioUtil/ShadowUtil.hpp>
#include <MoveBG/MapObjWave.hpp>
#include <Player/MarioAccess.hpp>
#include <Player/Yoshi.hpp>
#include <Strategic/ObjModel.hpp>
#include <Strategic/question.hpp>
#include <System/MarDirector.hpp>

// rogue includes needed for matching sinit & bss
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>
#include <M3DUtil/InfectiousStrings.hpp>

TFruitsBoatParams::TFruitsBoatParams(const char* path)
    : TSpineEnemyParams(path)
    , PARAM_INIT(mSLMoveSpeed, 4.0f)
    , PARAM_INIT(mSLRotSpeed, 0.1f)
    , PARAM_INIT(mSLBckMoveSpeed, 0.2f)
{
	TParams::load(mPrmPath);
}

TFruitsBoat::TFruitsBoat(const char* name)
    : TSpineEnemy(name)
    , unk150(0)
    , mShadowRadiusX(800.0f)
    , mShadowRadiusZ(800.0f)
    , mBckTrack(nullptr)
    , mBckTrackCtrl(nullptr)
    , mRockAxis(1.0f, 0.0f, 0.0f)
    , mRockAngle(0.0f)
    , mRockSpeed(0.0f)
{
	onLiveFlag(LIVE_FLAG_UNK10);
}

// Without a graph, the boat follows the joint animation named like the graph
// instead.
// TODO: frame-only, ours is 8 bytes smaller. The `rate` local is what keeps
// this function out of line in load(), like in the target.
int TFruitsBoat::setBckTrack(const char* name)
{
	MActorAnmDataEach<J3DAnmTransformKey>* bcks
	    = mManager->getMActorAnmData()->getUnk2C();
	for (int i = 0; i < bcks->getAnmNum(); ++i) {
		if (strcmp(name, bcks->getName(i)) == 0) {
			mBckTrack     = bcks->getAnmPtr(i);
			mBckTrackCtrl = new J3DFrameCtrl(0);
			mBckTrackCtrl->init(mBckTrack->getFrameMax());
			mBckTrackCtrl->setAttribute(mBckTrack->getAttribute());
			f32 rate = getParam()->mSLBckMoveSpeed.get();
			mBckTrackCtrl->setRate(rate * SMSGetAnmFrameRate());
			return 0;
		}
	}
	return -1;
}

// The sail bounces when Mario lands on it.
void TFruitsBoat::setJumpReaction()
{
	const char* name;
	switch (getBoatType()) {
	case 0:
		name = "shipdolpic";
		break;
	case 1:
		name = "shipdolpic2";
		break;
	case 2:
		name = "shipdolpic3";
		break;
	case 3:
	default:
		return;
	}

	if (!mMActor->checkCurAnm(name, ANM_TYPE_BCK)
	    || mMActor->curAnmEndsNext(ANM_TYPE_BCK, nullptr))
		mMActor->setBck(name);
}

void TFruitsBoat::traceBckTrack()
{
	mBckTrackCtrl->update();
	mBckTrack->setFrame(mBckTrackCtrl->getFrame());

	J3DTransformInfo info0;
	mBckTrack->getTransform(0, &info0);
	J3DTransformInfo info1;
	mBckTrack->getTransform(1, &info1);

	mPosition.x = info0.mTranslate.x + info1.mTranslate.x;
	mPosition.y = info0.mTranslate.y + info1.mTranslate.y;
	mPosition.z = info0.mTranslate.z + info1.mTranslate.z;
	mRotation.x
	    = (360.0f / 65536.0f) * (info0.mRotation.x + info1.mRotation.x);
	mRotation.y
	    = (360.0f / 65536.0f) * (info0.mRotation.y + info1.mRotation.y);
	mRotation.z
	    = (360.0f / 65536.0f) * (info0.mRotation.z + info1.mRotation.z);
	mScaling.x = info0.mScale.x * info1.mScale.x;
	mScaling.y = info0.mScale.y * info1.mScale.y;
	mScaling.z = info0.mScale.z * info1.mScale.z;
}

int TFruitsBoat::getBoatType() const
{
	return ((TFruitsBoatManager*)mManager)->mBoatType;
}

Mtx* TFruitsBoat::getRootJointMtx() const
{
	return (Mtx*)mMActor->getModel()->getAnmMtx(0);
}

void TFruitsBoat::rowToCurPathNode(f32 speed)
{
	if (unk124->getGraph()->getSplineRail() ? TRUE : FALSE) {
		f32 splineSpeed = unk124->calcSplineSpeed(speed);
		unk124->traceSpline(splineSpeed);

		JGeometry::TVec3<f32> pos;
		JGeometry::TVec3<f32> rot;
		unk124->getGraph()->getSplineRail()->getPosAndRot(unk124->unk14, &pos,
		                                                  &rot);
		pos.sub(mPosition);
		mPositionDelta.add(pos);
		mRotation.y = rot.y;
		if (splineSpeed < 0.0f)
			mRotation.y = MsWrap(mRotation.y + 180.0f, 0.0f, 360.0f);
	} else {
		walkToCurPathNode(speed, mTurnSpeed, 0.0f);
	}

	SMSGetMSound()->startSoundActor(MSD_SE_OBJ_PONPONSEN, &mPosition, 0,
	                                nullptr, 0, 4);
}

void TFruitsBoat::load(JSUMemoryInputStream& stream)
{
	JDrama::TActor::load(stream);

	char managerName[256];
	stream.readString(managerName, 256);
	TLiveManager* manager
	    = (TLiveManager*)JDrama::TNameRefGen::search(managerName);

	char graphName[256];
	stream.readString(graphName, 256);
	TGraphWeb* graph = gpConductor->getGraphByName(graphName);
	unk124->init(graph);

	mGroundPlane = TMap::getIllegalCheckData();

	init(manager);

	if ((graph == nullptr || graph->isDummy()) && setBckTrack(graphName) == 0)
		mSpine->initWith(&TNerveFruitsBoatBckTrace::theNerve());
}

// TODO: nonmatching, the target's frame is 0x30 bigger and it gives `this` and
// the string pool base the opposite registers (r31/r30).
void TFruitsBoat::init(TLiveManager* manager)
{
	mManager = manager;
	mManager->manageActor(this);
	mMActorKeeper = new TMActorKeeper(mManager, 1);
	initHitActor(ACTOR_TYPE_FRUITS_BOAT, 1, 0xC0000000, 0.0f, 0.0f, 0.0f,
	             0.0f);
	offHitFilter(HIT_FILTER_NO_COLLISION);

	unk124->reset();
	goToShortestNextGraphNode();
	if (unk124->getGraph()->getSplineRail()) {
		mPosition = unk124->getCurrentPos();
		unk124->moveToShortestNext();
	}

	switch (getBoatType()) {
	case 0:
		mMapCollisionManager
		    = new TMapCollisionManager(1, "/scene/fruitsboat", this);
		mMActor = mMActorKeeper->createMActor("ShipDolpic.bmd", 0);
		mMapCollisionManager->init("ShipDolpic.col", 1, nullptr);
		setAttackRadius(850.0f);
		setAttackHeight(600.0f);
		break;
	case 1:
		mMapCollisionManager
		    = new TMapCollisionManager(1, "/scene/fruitsboatb", this);
		mMActor = mMActorKeeper->createMActor("ShipDolpic2.bmd", 0);
		mMapCollisionManager->init("ShipDolpic2.col", 1, nullptr);
		setAttackRadius(750.0f);
		setAttackHeight(480.0f);
		break;
	case 2:
		mMapCollisionManager
		    = new TMapCollisionManager(1, "/scene/fruitsboatc", this);
		mMActor = mMActorKeeper->createMActor("ShipDolpic3.bmd", 0);
		mMapCollisionManager->init("ShipDolpic3.col", 1, nullptr);
		setAttackRadius(1000.0f);
		setAttackHeight(300.0f);
		break;
	case 3:
	default:
		mMapCollisionManager
		    = new TMapCollisionManager(1, "/scene/fruitsboatd", this);
		mMActor = mMActorKeeper->createMActor("ShipDolpic4.bmd", 0);
		mMapCollisionManager->init("ShipDolpic4.col", 1, nullptr);
		setAttackRadius(760.0f);
		setAttackHeight(270.0f);
		break;
	}

	mMapCollisionManager->setUpActiveCollisionTRS(mPosition, mRotation,
	                                              mScaling);

	mSpine->initWith(&TNerveFruitsBoatGraphWander::theNerve());

	// A boat starting on a node flagged 0x80 stays anchored there.
	if (unk124->getGraph() == nullptr)
		onLiveFlag(LIVE_FLAG_UNK10000);
	else if (unk124->getCurrent().checkFlag(0x80))
		onLiveFlag(LIVE_FLAG_UNK10000);
	else
		offLiveFlag(LIVE_FLAG_UNK10000);

	mMarchSpeed = getParam()->mSLMoveSpeed.get();
	mTurnSpeed  = getParam()->mSLRotSpeed.get();
	offLiveFlag(LIVE_FLAG_CLIPPED_OUT);
	onLiveFlag(LIVE_FLAG_UNK20);
	offLiveFlag(LIVE_FLAG_ENABLE_CLIPPING);
	mMActor->setLightType(2);
	calcRootMatrix();
	mMActor->calc();
}

BOOL TFruitsBoat::receiveMessage(THitActor* sender, u32 message)
{
	return false;
}

// Only keep the collision moving with the boat while someone can stand on it.
// TODO: frame-only, ours is 8 bytes smaller.
void TFruitsBoat::setGroundCollision()
{
	JGeometry::TVec3<f32> diff = mPosition;
	diff -= *gpMarioPos;
	if (mColCount != 0 || diff.length() < 1000.0f
	    || (SMS_GetYoshi()->isHatched()
	        && mPosition.x - 1000.0f < SMS_GetYoshi()->getTranslation().x
	        && mPosition.x + 1000.0f > SMS_GetYoshi()->getTranslation().x
	        && mPosition.z - 1000.0f < SMS_GetYoshi()->getTranslation().z
	        && mPosition.z + 1000.0f > SMS_GetYoshi()->getTranslation().z))
		mMapCollisionManager->moveActiveCollisionMtx(
		    getModel()->getAnmMtx(0));
}

void TFruitsBoat::calcRootMatrix()
{
	J3DModel* model = getModel();
	MtxPtr mtx      = model->getBaseTRMtx();
	MsMtxSetRotRPH(mtx, mRotation.x, mRotation.y, mRotation.z);
	Mtx rock;
	MTXRotAxisRad(rock, &mRockAxis, 0.017453292f * mRockAngle);
	MTXConcat(rock, mtx, mtx);
	MTXTransApply(mtx, mtx, mPosition.x, mPosition.y, mPosition.z);
	model->setBaseScale(mScaling);
}

// TODO: fake inline, the same one as in enemy.cpp and riccohook.cpp
static inline JGeometry::TVec3<f32> polarXZ(f32 theta, f32 radius)
{
	f32 c = radius * MsCos(theta);
	f32 s = radius * MsSin(theta);
	return JGeometry::TVec3<f32>(s, 0.0f, c);
}

// TODO: nonmatching, see the note on the rocking axis below; the frame is also
// 0x10 smaller than the target's.
void TFruitsBoat::moveObject()
{
	// Pitch the boat along the waves under its bow and stern.
	JGeometry::TVec3<f32> offset = polarXZ(mRotation.y, 300.0f);
	JGeometry::TVec3<f32> bow    = mPosition;
	bow += offset;
	JGeometry::TVec3<f32> stern = mPosition;
	stern -= offset;
	bow.y   = gpMapObjWave->getWaveHeight(bow.x, bow.z);
	stern.y = gpMapObjWave->getWaveHeight(stern.x, stern.z);

	JGeometry::TVec3<f32> dir = bow - stern;
	JGeometry::TVec3<f32> rot = MsGetRotFromZaxis(dir);
	rot.x *= 0.5f;
	f32 pitchStep = MsAngleDiff(rot.x, mRotation.x);
	if (pitchStep >= 0.0f)
		pitchStep = MsMin(pitchStep, 1.0f);
	else
		pitchStep = MsMax(pitchStep, -1.0f);
	mRotation.x += pitchStep;

	// Rock sideways when Mario lands on the boat.
	const TBGCheckData* plane = SMS_GetMarioGrPlane();
	if (!checkLiveFlag(LIVE_FLAG_UNK20000)) {
		if (plane != nullptr && plane->getActor() == this
		    && SMS_IsMarioTouchGround4cm()) {
			JGeometry::TVec3<f32> dir = *gpMarioPos;
			dir -= mPosition;
			dir.y    = 0.0f;
			f32 dist = dir.length();
			if (dist != 0.0f) {
				static const JGeometry::TVec3<f32> up(0.0f, 1.0f, 0.0f);
				dir.normalize();
				mRockAxis.cross2(up, dir);
				mRockAxis.normalize();
				mRockSpeed += 0.0003f * dist;
			}
			onLiveFlag(LIVE_FLAG_UNK20000);
			offLiveFlag(LIVE_FLAG_UNK10000);
			if (plane->isBounceOnLanding()) {
				setJumpReaction();
				offLiveFlag(LIVE_FLAG_UNK20000);
			}
		}
	} else if (plane == nullptr || plane->getActor() != this
	           || !SMS_IsMarioTouchGround4cm()) {
		offLiveFlag(LIVE_FLAG_UNK20000);
	}

	// Keep turning the rocking axis towards Mario while he stands on it.
	if (checkLiveFlag(LIVE_FLAG_UNK20000)) {
		JGeometry::TVec3<f32> dir = *gpMarioPos;
		dir -= mPosition;
		dir.y = 0.0f;
		if (dir.length() != 0.0f) {
			dir.normalize();
			static const JGeometry::TVec3<f32> up(0.0f, 1.0f, 0.0f);
			JGeometry::TVec3<f32> axis;
			axis.cross(up, dir);
			axis.normalize();
			// TODO: the target does not fuse these into multiply-adds; writing
			// each step into its own local (dx, dy, dz) reproduces that, but
			// nothing else hints at such locals, so maybe a lost helper.
			mRockAxis.x += 0.1f * (axis.x - mRockAxis.x);
			mRockAxis.y += 0.1f * (axis.y - mRockAxis.y);
			mRockAxis.z += 0.1f * (axis.z - mRockAxis.z);
		}
	}

	mRockSpeed += 0.01f * -MsSin(mRockAngle);
	mRockAngle += mRockSpeed;
	if (mRockAngle < -10.0f || mRockAngle > 10.0f)
		mRockSpeed = -mRockSpeed;
	mRockSpeed *= 0.99f;

	TLiveActor::moveObject();
}

void TFruitsBoat::requestShadow()
{
	if (mLiveFlag & (LIVE_FLAG_DEAD | LIVE_FLAG_HIDDEN | LIVE_FLAG_UNK8))
		return;

	if (!(mLiveFlag & (LIVE_FLAG_UNK200 | LIVE_FLAG_CLIPPED_OUT))
	    || (mLiveFlag & LIVE_FLAG_FORCE_SHADOW)) {
		TCircleShadowRequest shadow;
		shadow.mPosition   = mPosition;
		shadow.mRadiusX    = mShadowRadiusX;
		shadow.mRadiusZ    = mShadowRadiusZ;
		shadow.mShadowType = SHADOW_TYPE_SHIP;
		shadow.mRotationY  = (s16)mRotation.y;

		if (mLiveFlag & LIVE_FLAG_FORCE_SHADOW)
			gpBindShadowManager->forceRequest(shadow, getActorType());
		else
			gpBindShadowManager->request(shadow, getActorType());
	}

	if (!(mLiveFlag & (LIVE_FLAG_UNK200 | LIVE_FLAG_CLIPPED_OUT))
	    && !isHitCategory(HIT_CATEGORY_MAP_OBJECT)) {
		gpQuestionManager->request(mPosition, mScaledBodyRadius);
	}
}

TFruitsBoatManager::TFruitsBoatManager(int type, const char* name)
    : TEnemyManager(name)
    , mBoatType(type)
{
}

void TFruitsBoatManager::createModelData()
{
	switch (mBoatType) {
	case 0: {
		static const TModelDataLoadEntry entry[]
		    = { { "ShipDolpic.bmd", 0x10210000, 0 }, { nullptr, 0, 0 } };
		createModelDataArray(entry);
		break;
	}
	case 1: {
		static const TModelDataLoadEntry entry[]
		    = { { "ShipDolpic2.bmd", 0x10210000, 0 }, { nullptr, 0, 0 } };
		createModelDataArray(entry);
		break;
	}
	case 2: {
		static const TModelDataLoadEntry entry[]
		    = { { "ShipDolpic3.bmd", 0x10210000, 0 }, { nullptr, 0, 0 } };
		createModelDataArray(entry);
		break;
	}
	case 3:
	default: {
		static const TModelDataLoadEntry entry[]
		    = { { "ShipDolpic4.bmd", 0x10210000, 0 }, { nullptr, 0, 0 } };
		createModelDataArray(entry);
		break;
	}
	}
}

void TFruitsBoatManager::load(JSUMemoryInputStream& stream)
{
	unk38 = new TFruitsBoatParams("/enemy/fruitsBoat.prm");
	TEnemyManager::load(stream);
}

TSpineEnemy* TFruitsBoatManager::createEnemyInstance() { return nullptr; }

// TODO: frame-only, ours is 0x10 bytes smaller.
DEFINE_NERVE(TNerveFruitsBoatGraphWander, TLiveActor)
{
	TFruitsBoat* self = (TFruitsBoat*)spine->getBody();
	if (self->unk124->getGraph() == nullptr
	    || self->unk124->getGraph()->isDummy())
		return false;

	if (self->isReachedToGoal()) {
		TGraphNode& node = self->unk124->getCurrent();
		if (node.checkFlag(0x100))
			self->onLiveFlag(LIVE_FLAG_UNK10000);
		if (node.checkFlag(0x400))
			self->unk150 ^= 1;

		self->goToDirectedNextGraphNode(polarXZ(self->mRotation.y, 1.0f));
		if (!self->checkLiveFlag(LIVE_FLAG_UNK10000))
			self->rowToCurPathNode(self->mMarchSpeed);
		spine->pushAfterCurrent(&TNerveFruitsBoatGraphWander::theNerve());
		return true;
	}

	if (self->checkLiveFlag(LIVE_FLAG_UNK10000))
		return false;

	self->rowToCurPathNode(self->mMarchSpeed);
	return false;
}

// TODO: frame-only, ours is 8 bytes smaller. Reading the frame into a local
// in traceBckTrack ("f32 frame = mBckTrackCtrl->getFrame();") closes the gap,
// but nothing else points at that local, so it stays out.
DEFINE_NERVE(TNerveFruitsBoatBckTrace, TLiveActor)
{
	TFruitsBoat* self = (TFruitsBoat*)spine->getBody();
	self->traceBckTrack();
	return false;
}
