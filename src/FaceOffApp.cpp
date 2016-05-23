#include "cinder/app/App.h"
#include "cinder/Capture.h"
#include "cinder/Utilities.h"

#include "cinder/app/RendererGl.h" 
#include "cinder/gl/Texture.h"
#include "cinder/gl/Fbo.h"
#include "cinder/gl/draw.h"
#include "cinder/gl/scoped.h"
#include "cinder/params/Params.h"

#include "CinderOpenCV.h"
#include "ciFaceTracker/ciFacetracker.h"
#include "MiniConfig.h"
#include "Clone.h"

using namespace ci;
using namespace app;
using namespace std;

int	APP_W = 640;
int	APP_H = 480;

int CAM_W = 640;
int CAM_H = 480;

class FaceOff : public App
{
public:
    void prepareSettings(Settings *settings)
    {
        readConfig();

        settings->setWindowSize(APP_W, APP_H);
        settings->setFrameRate(60.0f);
        settings->setFullScreen(false);
        settings->setTitle("FaceOff. CRE Imagination");
    }

    void	keyUp(KeyEvent event)
    {
        switch (event.getChar())
        {
        case 'f':
        {
            setFullScreen(!isFullScreen());
            APP_W = getWindowBounds().getWidth();
            APP_H = getWindowBounds().getHeight();
        }
        break;
        case KeyEvent::KEY_ESCAPE:
            quit();
            break;
        }
    }

    void	fileDrop_DISABLED(FileDropEvent event)
    {
        if (event.getNumFiles() > 0)
        {
            console() << event.getFile(0);
            try
            {
                ImageSourceRef img = loadImage(event.getFile(0));
                gl::Texture2dRef newTex;
                if (img) newTex = gl::Texture::create(img);

                shared_ptr<ciFaceTracker> newTracker = std::make_shared<ciFaceTracker>();
                newTracker->setup();
                newTracker->update(toOcv(img));

                if (newTracker->getFound())
                {
                    mPhotoTracker = newTracker;
                    mPhotoTex = newTex;
                }
            }
            catch (...)
            {

            }
        }
    }

    void	setup();
    void	update();
    void	draw();

    void    shutdown()
    {
        if (mCapture)
        {
            mCapture->stop();
        }
    }

private:

    gl::TextureRef mCaptureTex;				// our camera capture texture
    TriMesh         mFaceMesh;

    ciFaceTracker   mFaceTracker;
    shared_ptr<ciFaceTracker> mPhotoTracker;//to use as background or be replaced
    gl::TextureRef mPhotoTex;

    //
    vector<Capture::DeviceRef>  mDevices;
    vector<string>              mDeviceNames;
    int         mDeviceId;
    CaptureRef		mCapture;

    int         mPeopleId;
    vector<string>              mPeopleNames;

    params::InterfaceGlRef mParam;

    gl::FboRef     mSrcFbo, mMaskFbo;
    Clone       mClone;
};

void FaceOff::setup()
{
    // list out the devices
    vector<Capture::DeviceRef> devices(Capture::getDevices());
    if (devices.empty())
    {
        console() << "No camera device connected." << endl;
        quit();
        return;
    }

    for (vector<Capture::DeviceRef>::const_iterator deviceIt = devices.begin(); deviceIt != devices.end(); ++deviceIt)
    {
        Capture::DeviceRef device = *deviceIt;
        console() << "Found Device " << device->getName() << " ID: " << device->getUniqueId() << endl;

        if (device->checkAvailable())
        {
            mDevices.push_back(device);
            mDeviceNames.push_back(device->getName());
        }
    }

    mFaceTracker.setup();
    mFaceTracker.setRescale(0.5f);

    mParam = params::InterfaceGl::create("param", ivec2(300, getConfigUIHeight()));
    setupConfigUI(mParam.get());

    // TODO: assert
    if (DEVICE_ID > mDeviceNames.size() - 1)
    {
        DEVICE_ID = 0;
    }
    ADD_ENUM_TO_INT(mParam, DEVICE_ID, mDeviceNames);
    mDeviceId = -1;

    // TODO: assert
    fs::directory_iterator kEnd;
    fs::path peopleFolder = getAssetPath("people");
    for (fs::directory_iterator it(peopleFolder); it != kEnd; ++it)
    {
        mPeopleNames.push_back(it->path().filename().string());
    }
    if (PEOPLE_ID > mPeopleNames.size() - 1)
    {
        PEOPLE_ID = 0;
    }
    ADD_ENUM_TO_INT(mParam, PEOPLE_ID, mPeopleNames);
    mPeopleId = -1;

    // Fbo
    gl::Texture::Format texFormat;
    texFormat.setTargetRect();

    gl::Fbo::Format fboFormat;
    fboFormat.setColorTextureFormat(texFormat);
    fboFormat.enableDepthBuffer(false);
    mSrcFbo = gl::Fbo::create(CAM_W, CAM_H, fboFormat);
    mMaskFbo = gl::Fbo::create(CAM_W, CAM_H, fboFormat);

    mClone.setup(CAM_W, CAM_H);
    mClone.setStrength(16);

    gl::disableDepthRead();
    gl::disableDepthWrite();

}

void FaceOff::update()
{
    if (mDeviceId != DEVICE_ID)
    {
        mDeviceId = DEVICE_ID;
        if (mCapture)
        {
            mCapture->stop();
        }
        mCapture = Capture::create(CAM_W, CAM_H, mDevices[DEVICE_ID]);
        mCapture->start();
        CAM_W = mCapture->getWidth();
        CAM_H = mCapture->getHeight();

        // TODO: placeholder text
        //mCaptureTex = gl::Texture();
    }

    if (mPeopleId != PEOPLE_ID)
    {
        mPeopleId = PEOPLE_ID;
        mPhotoTracker = std::make_shared<ciFaceTracker>();
        mPhotoTracker->setup();

        //mPhotoTracker->setRescale(0.5f);
        ImageSourceRef img = loadImage(loadAsset("people/" + mPeopleNames[PEOPLE_ID]));
        if (img) mPhotoTex = gl::Texture::create(img);
        mPhotoTracker->update(toOcv(img));

        mFaceMesh.getBufferTexCoords0().clear();
    }

    if (mCapture && mCapture->checkNewFrame())
    {
        Surface8u surface = *mCapture->getSurface();
        if (mCaptureTex)
        {
            mCaptureTex->update(surface);
        }
        else
        {
            gl::Texture::Format format;
            format.setTargetRect();
            mCaptureTex = gl::Texture::create(surface, format);
        }

        mFaceTracker.update(toOcv(surface));

        if (!mFaceTracker.getFound())
            return;

        int nPoints = mFaceTracker.size();
        if (mFaceMesh.getBufferTexCoords0().empty())
        {
            for (int i = 0; i < nPoints; i++)
            {
                mFaceMesh.appendTexCoord(mPhotoTracker->getUVPoint(i));
            }
            mFaceTracker.addTriangleIndices(mFaceMesh);
        }

        mFaceMesh.getBufferPositions().clear();
        for (int i = 0; i < nPoints; i++)
        {
            mFaceMesh.appendPosition(vec3(mFaceTracker.getImagePoint(i), 0));
        }

        if (FACE_SUB_VISIBLE)
        {
            gl::setMatricesWindow(getWindowSize(), false);

            {
                gl::ScopedFramebuffer fbo(mMaskFbo);
                gl::clear(ColorA::black(), false);
                mPhotoTex->unbind();
                gl::draw(mFaceMesh);
            }

            {
                gl::ScopedFramebuffer fbo(mSrcFbo);
                gl::clear(ColorA::black(), false);
                mPhotoTex->bind();
                gl::draw(mFaceMesh);
                mPhotoTex->unbind();
            }

            mClone.update(mSrcFbo->getTexture2d(GL_COLOR_ATTACHMENT0), mCaptureTex, mMaskFbo->getTexture2d(GL_COLOR_ATTACHMENT0));
        }
    }
}

void FaceOff::draw()
{
    gl::clear(ColorA::black(), false);

    if (!mCaptureTex)
        return;

    gl::setMatricesWindow(getWindowSize());

    gl::enableAlphaBlending();

    if (FACE_SUB_VISIBLE && mFaceTracker.getFound())
    {
        gl::pushModelView();
        gl::scale(APP_W / (float)CAM_W, APP_H / (float)CAM_H);
        mClone.draw();
        gl::popModelView();
    }
    else
    {
        gl::draw(mCaptureTex, getWindowBounds());
    }

    if (PHOTO_VISIBLE)
    {
        gl::draw(mPhotoTex);
    }

    gl::drawStringCentered("fps: " + toString(getAverageFps()), vec2(150, 10));

    gl::disableAlphaBlending();

    gl::enableWireframe();
    if (mFaceTracker.getFound())
    {
        //wire
        if (CAMERA_WIREFRAME)
        {
            gl::color(ColorA(0, 1.0f, 0, 1.0f));
            gl::pushModelView();
            gl::scale(APP_W / (float)CAM_W, APP_H / (float)CAM_H);
            gl::draw(mFaceTracker.getImageMesh());
            gl::popModelView();
            gl::color(ColorA::white());
        }
    }

    if (PHOTO_VISIBLE && mPhotoTracker->getFound())
    {
        gl::color(ColorA(0, 0, 1.0f, 0.5f));
        gl::draw(mPhotoTracker->getImageMesh());
        gl::color(ColorA::white());
    }
    gl::disableWireframe();

    mParam->draw();
}

CINDER_APP(FaceOff, RendererGl)