#include "camera.hpp"

#include <OgreSceneNode.h>
#include <OgreCamera.h>
#include <OgreSceneManager.h>
#include <OgreTagPoint.h>

#include "../mwbase/environment.hpp"
#include "../mwbase/windowmanager.hpp"
#include "../mwbase/soundmanager.hpp"

#include "../mwworld/ptr.hpp"
#include "../mwworld/refdata.hpp"

#include "npcanimation.hpp"

namespace MWRender
{
    Camera::Camera (Ogre::Camera *camera)
    : mCamera(camera),
      mCameraNode(NULL),
      mFirstPersonView(true),
      mPreviewMode(false),
      mFreeLook(true),
      mHeight(128.f),
      mCameraDistance(300.f),
      mDistanceAdjusted(false),
      mAnimation(NULL),
      mNearest(30.f),
      mFurthest(800.f),
      mIsNearest(false),
      mIsFurthest(false)
    {
        mVanity.enabled = false;
        mVanity.allowed = true;

        mPreviewCam.yaw = 0.f;
        mPreviewCam.offset = 400.f;
        mMainCam.yaw = 0.f;
        mMainCam.offset = 400.f;
    }

    Camera::~Camera()
    {
    }

    void Camera::reset()
    {
        togglePreviewMode(false);
        toggleVanityMode(false);
        if (!mFirstPersonView)
            toggleViewMode();
    }

    void Camera::rotateCamera(const Ogre::Vector3 &rot, bool adjust)
    {
        if (adjust) {
            setYaw(getYaw() + rot.z);
            setPitch(getPitch() + rot.x);
        } else {
            setYaw(rot.z);
            setPitch(rot.x);
        }

        Ogre::Quaternion xr(Ogre::Radian(getPitch() + Ogre::Math::HALF_PI), Ogre::Vector3::UNIT_X);
        if (!mVanity.enabled && !mPreviewMode) {
            mCamera->getParentNode()->setOrientation(xr);
        } else {
            Ogre::Quaternion zr(Ogre::Radian(getYaw()), Ogre::Vector3::NEGATIVE_UNIT_Z);
            mCamera->getParentNode()->setOrientation(zr * xr);
        }
    }

    const std::string &Camera::getHandle() const
    {
        return mTrackingPtr.getRefData().getHandle();
    }

    void Camera::attachTo(const MWWorld::Ptr &ptr)
    {
        mTrackingPtr = ptr;
        Ogre::SceneNode *node = mTrackingPtr.getRefData().getBaseNode()->createChildSceneNode(Ogre::Vector3(0.0f, 0.0f, mHeight));
        if(mCameraNode)
        {
            node->setOrientation(mCameraNode->getOrientation());
            node->setPosition(mCameraNode->getPosition());
            node->setScale(mCameraNode->getScale());
            mCameraNode->getCreator()->destroySceneNode(mCameraNode);
        }
        mCameraNode = node;
        if(!mCamera->isAttached())
            mCameraNode->attachObject(mCamera);
    }

    void Camera::updateListener()
    {
        Ogre::Vector3 pos = mCamera->getRealPosition();
        Ogre::Vector3 dir = mCamera->getRealDirection();
        Ogre::Vector3 up  = mCamera->getRealUp();

        MWBase::Environment::get().getSoundManager()->setListenerPosDir(pos, dir, up);
    }

    void Camera::update(float duration, bool paused)
    {
        updateListener();
        if (paused)
            return;

        // only show the crosshair in game mode and in first person mode.
        MWBase::WindowManager *wm = MWBase::Environment::get().getWindowManager();
        wm->showCrosshair(!wm->isGuiMode() && (mFirstPersonView && !mVanity.enabled && !mPreviewMode));

        if(mVanity.enabled)
        {
            Ogre::Vector3 rot(0.f, 0.f, 0.f);
            rot.z = Ogre::Degree(3.f * duration).valueRadians();
            rotateCamera(rot, true);
        }
    }

    void Camera::toggleViewMode()
    {
        mFirstPersonView = !mFirstPersonView;
        processViewChange();

        if (mFirstPersonView) {
            mCamera->setPosition(0.f, 0.f, 0.f);
        } else {
            mCamera->setPosition(0.f, 0.f, mCameraDistance);
        }
    }
    
    void Camera::allowVanityMode(bool allow)
    {
        if (!allow && mVanity.enabled)
            toggleVanityMode(false);
        mVanity.allowed = allow;
    }

    bool Camera::toggleVanityMode(bool enable)
    {
        if(!mVanity.allowed && enable)
            return false;

        if(mVanity.enabled == enable)
            return true;
        mVanity.enabled = enable;

        processViewChange();

        float offset = mPreviewCam.offset;
        Ogre::Vector3 rot(0.f, 0.f, 0.f);
        if (mVanity.enabled) {
            rot.x = Ogre::Degree(-30.f).valueRadians();
            mMainCam.offset = mCamera->getPosition().z;
        } else {
            rot.x = getPitch();
            offset = mMainCam.offset;
        }
        rot.z = getYaw();

        mCamera->setPosition(0.f, 0.f, offset);
        rotateCamera(rot, false);

        return true;
    }

    void Camera::togglePreviewMode(bool enable)
    {
        if(mPreviewMode == enable)
            return;

        mPreviewMode = enable;
        processViewChange();

        float offset = mCamera->getPosition().z;
        if (mPreviewMode) {
            mMainCam.offset = offset;
            offset = mPreviewCam.offset;
        } else {
            mPreviewCam.offset = offset;
            offset = mMainCam.offset;
        }

        mCamera->setPosition(0.f, 0.f, offset);
        rotateCamera(Ogre::Vector3(getPitch(), 0.f, getYaw()), false);
    }

    void Camera::setSneakOffset()
    {
        if(mAnimation)
            mAnimation->addFirstPersonOffset(Ogre::Vector3(0.f, 0.f, -9.8f));
    }

    float Camera::getYaw()
    {
        if(mVanity.enabled || mPreviewMode)
            return mPreviewCam.yaw;
        return mMainCam.yaw;
    }

    void Camera::setYaw(float angle)
    {
        if (angle > Ogre::Math::PI) {
            angle -= Ogre::Math::TWO_PI;
        } else if (angle < -Ogre::Math::PI) {
            angle += Ogre::Math::TWO_PI;
        }
        if (mVanity.enabled || mPreviewMode) {
            mPreviewCam.yaw = angle;
        } else {
            mMainCam.yaw = angle;
        }
    }

    float Camera::getPitch()
    {
        if (mVanity.enabled || mPreviewMode) {
            return mPreviewCam.pitch;
        }
        return mMainCam.pitch;
    }

    void Camera::setPitch(float angle)
    {
        const float epsilon = 0.000001f;
        float limit = Ogre::Math::HALF_PI - epsilon;
        if(mPreviewMode)
            limit /= 2;

        if(angle > limit)
            angle = limit;
        else if(angle < -limit)
            angle = -limit;

        if (mVanity.enabled || mPreviewMode) {
            mPreviewCam.pitch = angle;
        } else {
            mMainCam.pitch = angle;
        }
    }

    void Camera::setCameraDistance(float dist, bool adjust, bool override)
    {
        if(mFirstPersonView && !mPreviewMode && !mVanity.enabled)
            return;

        mIsFurthest = false;
        mIsNearest = false;

        Ogre::Vector3 v(0.f, 0.f, dist);
        if (adjust) {
            v += mCamera->getPosition();
        }
        if (v.z >= mFurthest) {
            v.z = mFurthest;
            mIsFurthest = true;
        } else if (!override && v.z < 10.f) {
            v.z = 10.f;
        } else if (override && v.z <= mNearest) {
            v.z = mNearest;
            mIsNearest = true;
        }
        mCamera->setPosition(v);

        if (override) {
            if (mVanity.enabled || mPreviewMode) {
                mPreviewCam.offset = v.z;
            } else if (!mFirstPersonView) {
                mCameraDistance = v.z;
            }
        } else {
            mDistanceAdjusted = true;
        }
    }

    void Camera::setCameraDistance()
    {
        if (mDistanceAdjusted) {
            if (mVanity.enabled || mPreviewMode) {
                mCamera->setPosition(0, 0, mPreviewCam.offset);
            } else if (!mFirstPersonView) {
                mCamera->setPosition(0, 0, mCameraDistance);
            }
        }
        mDistanceAdjusted = false;
    }

    void Camera::setAnimation(NpcAnimation *anim)
    {
        // If we're switching to a new NpcAnimation, ensure the old one is
        // using a normal view mode
        if(mAnimation && mAnimation != anim)
        {
            mAnimation->setViewMode(NpcAnimation::VM_Normal);
            mAnimation->setCamera(NULL);
            mAnimation->detachObjectFromBone(mCamera);
        }
        mAnimation = anim;
        mAnimation->setCamera(this);

        processViewChange();
    }

    void Camera::processViewChange()
    {
        mAnimation->detachObjectFromBone(mCamera);
        mCamera->detachFromParent();

        if(isFirstPerson())
        {
            mAnimation->setViewMode(NpcAnimation::VM_FirstPerson);
            Ogre::TagPoint *tag = mAnimation->attachObjectToBone("Head", mCamera);
            tag->setInheritOrientation(false);
        }
        else
        {
            mAnimation->setViewMode(NpcAnimation::VM_Normal);
            mCameraNode->attachObject(mCamera);
        }
    }

    void Camera::getPosition(Ogre::Vector3 &focal, Ogre::Vector3 &camera)
    {
        mCamera->getParentSceneNode()->needUpdate(true);

        camera = mCamera->getRealPosition();
        focal = Ogre::Vector3((mCamera->getParentNode()->_getFullTransform() *
                               Ogre::Vector4(0.0f, 0.0f, 0.0f, 1.0f)).ptr());
    }

    void Camera::togglePlayerLooking(bool enable)
    {
        mFreeLook = enable;
    }

    bool Camera::isVanityOrPreviewModeEnabled()
    {
        return mPreviewMode || mVanity.enabled;
    }

    bool Camera::isNearest()
    {
        return mIsNearest;
    }

    bool Camera::isFurthest()
    {
        return mIsFurthest;
    }
}
