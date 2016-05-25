#include "cinder/app/App.h"
#include "cinder/Capture.h"
#include "cinder/Utilities.h"
#include "cinder/ImageIo.h"

#include "cinder/app/RendererGl.h" 
#include "cinder/gl/Texture.h"
#include "cinder/gl/Fbo.h"
#include "cinder/gl/shader.h"
#include "cinder/gl/draw.h"
#include "cinder/gl/scoped.h"
#include "cinder/params/Params.h"

#include "cinder/qtime/QuickTimeGl.h"
#include "TextureHelper.h"

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
    static void prepareSettings(Settings *settings)
    {
        readConfig();

        settings->setWindowSize(APP_W, APP_H);
        settings->setFrameRate(60.0f);
        settings->setFullScreen(false);
        settings->setTitle("face-switcher");
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
                    mOfflineTracker = newTracker;
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

    ciFaceTracker   mOnlineTracker;
    shared_ptr<ciFaceTracker> mOfflineTracker;
    gl::TextureRef mPhotoTex;

    qtime::MovieSurfaceRef		mMovie;

    //
    vector<Capture::DeviceRef>  mDevices;
    vector<string>              mDeviceNames;
    CaptureRef		mCapture;

    vector<string>              mPeopleNames;

    params::InterfaceGlRef mParam;

    gl::TextureRef mRefTex;
    gl::TextureRef mRenderedRefTex;
    gl::FboRef     mSrcFbo, mMaskFbo;
    Clone       mClone;

    //  param
    int         mDeviceId;
    int         mPeopleId;
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

    mOnlineTracker.setup();
    mOnlineTracker.setRescale(0.5f);

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
    //gl::Texture::Format texFormat;
    //texFormat.setTargetRect();

    gl::Fbo::Format fboFormat;
    //fboFormat.setColorTextureFormat(texFormat);
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
    if (MOVIE_MODE)
    {
        if (!mMovie)
        {
            fs::path moviePath = getAssetPath(MOVIE_PATH);
            try {
                // load up the movie, set it to loop, and begin playing
                mMovie = qtime::MovieSurface::create(moviePath);
                mMovie->setLoop();
                mMovie->play();
                mRefTex.reset();
                mRenderedRefTex.reset();
            }
            catch (ci::Exception &exc) {
                console() << "Exception caught trying to load the movie from path: " << moviePath << ", what: " << exc.what() << std::endl;
                mMovie.reset();
            }
        }
        else
        {
            if (mMovie->checkNewFrame())
            {
                auto surface = mMovie->getSurface();
                if (!mRefTex)
                {
                    mRefTex = gl::Texture2d::create(*surface, gl::Texture::Format().loadTopDown());
                }
                else
                {
                    mRefTex->update(*surface);
                }
            }
        }
    }
    else
    {
        if (mMovie)
        {
            mMovie.reset();
        }
        mRefTex = mPhotoTex;
    }

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
        mOfflineTracker = std::make_shared<ciFaceTracker>();
        mOfflineTracker->setup();

        //mOfflineTracker->setRescale(0.5f);
        ImageSourceRef img = loadImage(loadAsset("people/" + mPeopleNames[PEOPLE_ID]));
        if (img)
        {
            mRefTex = mPhotoTex = gl::Texture::create(img, gl::Texture::Format().loadTopDown());
        }
        mOfflineTracker->update(toOcv(img));

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
            //format.setTargetRect();
            mCaptureTex = gl::Texture::create(surface, format);
        }

        mOnlineTracker.update(toOcv(surface));

        if (!mOnlineTracker.getFound())
            return;

        int nPoints = mOnlineTracker.size();
        if (mFaceMesh.getBufferTexCoords0().empty())
        {
            for (int i = 0; i < nPoints; i++)
            {
                mFaceMesh.appendTexCoord(mOfflineTracker->getUVPoint(i));
            }
            mOnlineTracker.addTriangleIndices(mFaceMesh);
        }

        mFaceMesh.getBufferPositions().clear();
        for (int i = 0; i < nPoints; i++)
        {
            mFaceMesh.appendPosition(vec3(mOnlineTracker.getImagePoint(i), 0));
        }

        if (FACE_SUB_VISIBLE && mRefTex)
        {
            //gl::setMatricesWindow(getWindowSize(), false);
            // TODO: merge these two passes w/ MRTs
            {
                gl::ScopedFramebuffer fbo(mSrcFbo);
                gl::ScopedGlslProg glsl(gl::getStockShader(gl::ShaderDef().texture()));
                gl::ScopedTextureBind t0(mRefTex, 0);
                gl::clear(ColorA::black(), false);
                gl::draw(mFaceMesh);
                mRenderedRefTex = mSrcFbo->getColorTexture();
            }

            if (!MOVIE_MODE)
            {
                {
                    gl::ScopedFramebuffer fbo(mMaskFbo);
                    gl::clear(ColorA::black(), false);
                    mRefTex->unbind();
                    gl::draw(mFaceMesh);
                }

                mClone.update(mRenderedRefTex, mCaptureTex, mMaskFbo->getColorTexture());
            }
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

    if (FACE_SUB_VISIBLE && mOnlineTracker.getFound())
    {
        gl::ScopedModelMatrix modelMatrix;
        gl::scale(APP_W / (float)CAM_W, APP_H / (float)CAM_H);
        if (MOVIE_MODE)
        {
            if (mRenderedRefTex) gl::draw(mRenderedRefTex);
        }
        else
        {
            mClone.draw();
        }
    }
    else
    {
        gl::draw(mCaptureTex, getWindowBounds());
    }

    if (REF_VISIBLE)
    {
        gl::draw(mRefTex);
    }

    gl::drawStringCentered("fps: " + toString(getAverageFps()), vec2(150, 10));

    gl::disableAlphaBlending();

    if (WIREFRAME_MODE && mOnlineTracker.getFound())
    {
        gl::enableWireframe();

        gl::ScopedModelMatrix modelMatrix;
        gl::ScopedColor color(ColorA(0, 1.0f, 0, 1.0f));
        gl::scale(APP_W / (float)CAM_W, APP_H / (float)CAM_H);
        gl::draw(mOnlineTracker.getImageMesh());

        if (REF_VISIBLE)
        {
            gl::draw(mOfflineTracker->getImageMesh());
        }
    
        gl::disableWireframe();
    }

    mParam->draw();
}

CINDER_APP(FaceOff, RendererGl, &FaceOff::prepareSettings)