#include "cinder/app/App.h"
#include "cinder/Capture.h"
#include "cinder/Utilities.h"
#include "cinder/ImageIo.h"
#include "cinder/Thread.h"

#include "cinder/app/RendererGl.h" 
#include "cinder/gl/Texture.h"
#include "cinder/gl/Fbo.h"
#include "cinder/gl/shader.h"
#include "cinder/gl/draw.h"
#include "cinder/gl/scoped.h"
#include "cinder/params/Params.h"

#include "TextureHelper.h"
#include "CaptureHelper.h"

#include "IFaceTracker/IFaceTracker.h"
#include "MiniConfig.h"
#include "Clone.h"

#if 0 // defined (CINDER_MSW)
#include "ciWMFVideoPlayer.h"
typedef ciWMFVideoPlayer CiMovieType;
#else
#include "cinder/qtime/QuickTimeGl.h"
typedef qtime::MovieSurfaceRef CiMovieType;
#endif

using namespace ci;
using namespace app;
using namespace std;

#if defined( CINDER_GL_ES )
namespace cinder {
    namespace gl {
        void enableWireframe() {}
        void disableWireframe() {}
    }
}
#endif

class FaceOff : public App
{
public:
    static void prepareSettings(Settings *settings)
    {
        readConfig();

        settings->setWindowSize(APP_W, APP_H);
        settings->setFrameRate(60.0f);
        settings->setFullScreen(false);
        settings->setTitle("FaceVFX");
    }

    void keyUp(KeyEvent event)
    {
        switch (event.getChar())
        {
        case 'f':
        {
            setFullScreen(!isFullScreen());
            resize();
        }
            break;
        case KeyEvent::KEY_ESCAPE:
            quit();
            break;
        }
    }

    void fileDrop_DISABLED(FileDropEvent event)
    {
        if (event.getNumFiles() > 0)
        {
            console() << event.getFile(0);
            try
            {
                ImageSourceRef img = loadImage(event.getFile(0));
                gl::Texture2dRef newTex;
                if (img) newTex = gl::Texture::create(img);

                mOfflineTracker->reset();
                mOfflineTracker->update(img);
                mPhotoTex = newTex;
            }
            catch (...)
            {

            }
        }
    }

    void setup();
    void shutdown()
    {
        mTrackerThread->join();
    }

    void update();
    void draw();
    void resize();

private:

    void trackerThreadFn();

    TriMesh         mFaceMesh;

    ft::FaceTrackerRef   mOnlineTracker;
    ft::FaceTrackerRef   mOfflineTracker;
    gl::TextureRef mPhotoTex;

    CiMovieType   mMovie;

    //
    vector<Capture::DeviceRef>  mDevices;
    vector<string>              mDeviceNames;
    CaptureHelper   mCapture;
    shared_ptr<thread> mTrackerThread;

    vector<string>              mPeopleNames;

    params::InterfaceGlRef mParam;

    gl::TextureRef mOfflineFaceTex;
    gl::FboRef     mRenderedOfflineFaceFbo, mFaceMaskFbo;
    Clone       mClone;

    //  param
    int         mDeviceId;
    int         mPeopleId;
    bool        mDoesCaptureNeedsInit;
};

void FaceOff::resize()
{
    APP_W = getWindowWidth();
    APP_H = getWindowHeight();
}

void FaceOff::trackerThreadFn()
{
}

void FaceOff::setup()
{
    resize();

    // list out the devices
    vector<Capture::DeviceRef> devices(Capture::getDevices());
    if (devices.empty())
    {
        console() << "No camera device connected." << endl;
        quit();
        return;
    }

    for (auto device : devices)
    {
        console() << "Found Device " << device->getName() << " ID: " << device->getUniqueId() << endl;

        if (device->checkAvailable())
        {
            mDevices.push_back(device);
            mDeviceNames.push_back(device->getName());
        }
    }

    ft::Option option;
    option.scale = 0.5f;
    mOnlineTracker = ft::IFaceTracker::create(option);
    mOfflineTracker = ft::IFaceTracker::create();

    // TODO: assert
    fs::directory_iterator kEnd;
    fs::path peopleFolder = getAssetPath("people");
    for (fs::directory_iterator it(peopleFolder); it != kEnd; ++it)
    {
        mPeopleNames.push_back(it->path().filename().string());
    }

#if !defined( CINDER_GL_ES )
    mParam = params::InterfaceGl::create("param", ivec2(300, getConfigUIHeight()));
    setupConfigUI(mParam.get());

    // TODO: assert
    if (DEVICE_ID > mDeviceNames.size() - 1)
    {
        DEVICE_ID = 0;
    }
    ADD_ENUM_TO_INT(mParam, DEVICE_ID, mDeviceNames);
    mDeviceId = -1;

    if (PEOPLE_ID > mPeopleNames.size() - 1)
    {
        PEOPLE_ID = 0;
    }
    ADD_ENUM_TO_INT(mParam, PEOPLE_ID, mPeopleNames);
#else
    DEVICE_ID = 1; // pick front camera for mobile devices
#endif

    mPeopleId = -1;

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
            try
            {
                // load up the movie, set it to loop, and begin playing
                mMovie = qtime::MovieSurface::create(moviePath);
                mMovie->setLoop();
                mMovie->play();
                mOfflineFaceTex.reset();
            }
            catch (ci::Exception &exc)
            {
                console() << "Exception caught trying to load the movie from path: " << MOVIE_PATH << ", what: " << exc.what() << std::endl;
                mMovie.reset();
            }
        }
        else
        {
            if (mMovie->checkNewFrame())
            {
                auto surface = mMovie->getSurface();
                if (!mOfflineFaceTex)
                {
                    mOfflineFaceTex = gl::Texture2d::create(*surface, gl::Texture::Format().loadTopDown());
                }
                else
                {
                    mOfflineFaceTex->update(*surface);
                }
            }
        }
    }
    else
    {
        mMovie.reset();
        mOfflineFaceTex = mPhotoTex;
    }

    if (mDeviceId != DEVICE_ID)
    {
        mDeviceId = DEVICE_ID;
        mDoesCaptureNeedsInit = true;
        mCapture.setup(CAM_W, CAM_H, mDevices[DEVICE_ID]);
    }

    mCapture.flip = CAM_FLIP;

    if (mPeopleId != PEOPLE_ID)
    {
        mPeopleId = PEOPLE_ID;
        mOfflineTracker->reset();

        ImageSourceRef img = loadImage(loadAsset("people/" + mPeopleNames[PEOPLE_ID]));
        if (img)
        {
            mOfflineFaceTex = mPhotoTex = gl::Texture::create(img, gl::Texture::Format().loadTopDown());
            mOfflineTracker->update(img);
        }

        mFaceMesh.getBufferTexCoords0().clear();
    }

    // TODO: more robust with update_signal
    if (mCapture.checkNewFrame())
    {
        if (mDoesCaptureNeedsInit)
        {
            // TODO: more robust with setup_signal
            mDoesCaptureNeedsInit = false;

            CAM_W = mCapture.size.x;
            CAM_H = mCapture.size.y;

            gl::Fbo::Format fboFormat;
            fboFormat.enableDepthBuffer(false);
            mRenderedOfflineFaceFbo = gl::Fbo::create(CAM_W, CAM_H, fboFormat);
            mFaceMaskFbo = gl::Fbo::create(CAM_W, CAM_H, fboFormat);

            mClone.setup(CAM_W, CAM_H);
            mClone.setStrength(16);
        }

        mOnlineTracker->update(mCapture.surface);

        if (!mOnlineTracker->getFound())
            return;

        int nPoints = mOnlineTracker->size();
        if (mFaceMesh.getBufferTexCoords0().empty())
        {
            auto imgSize = mOfflineTracker->getImageSize();
            for (int i = 0; i < nPoints; i++)
            {
                vec3 point = mOfflineTracker->getImagePoint(i);
                mFaceMesh.appendTexCoord({ point.x / imgSize.x, point.y / imgSize.y });
            }
            mOnlineTracker->addTriangleIndices(mFaceMesh);
        }

        mFaceMesh.getBufferPositions().clear();
        for (int i = 0; i < nPoints; i++)
        {
            mFaceMesh.appendPosition(mOnlineTracker->getImagePoint(i));
        }

        if (VFX_VISIBLE && mOfflineFaceTex)
        {
            gl::ScopedMatrices mvp;
            gl::setMatricesWindow(mRenderedOfflineFaceFbo->getSize());
            gl::ScopedViewport viewport(0, 0, mRenderedOfflineFaceFbo->getWidth(), mRenderedOfflineFaceFbo->getHeight());
            
            // TODO: merge these two passes w/ MRTs
            {
                gl::ScopedFramebuffer fbo(mRenderedOfflineFaceFbo);
                gl::ScopedGlslProg glsl(gl::getStockShader(gl::ShaderDef().texture()));
                gl::ScopedTextureBind t0(mOfflineFaceTex, 0);
                gl::clear(ColorA::black(), false);
                gl::draw(mFaceMesh);
            }

            if (!MOVIE_MODE)
            {
                {
                    gl::ScopedFramebuffer fbo(mFaceMaskFbo);
                    gl::clear(ColorA::black(), false);
                    gl::draw(mFaceMesh);
                }

                // TODO: add gl::ScopedMatrices in mClone.update()
                mClone.update(mRenderedOfflineFaceFbo->getColorTexture(), mCapture.texture, mFaceMaskFbo->getColorTexture());
            }
        }
    }
}

void FaceOff::draw()
{
    gl::clear(ColorA::black(), false);

    if (!mCapture.isReady())
        return;

    gl::setMatricesWindow(getWindowSize());

    gl::enableAlphaBlending();

    float camAspect = CAM_W / (float)CAM_H;
    float winAspect = getWindowAspectRatio();
    float adaptiveCamW = 0;
    float adaptiveCamH = 0;
    if (camAspect > winAspect)
    {
        adaptiveCamW = APP_W;
        adaptiveCamH = APP_W / camAspect;
    }
    else
    {
        adaptiveCamH = APP_H;
        adaptiveCamW = APP_H * camAspect;
    }
    Area srcArea = { 0, 0, CAM_W, CAM_H };
    Rectf dstRect =
    {
        APP_W * 0.5f - adaptiveCamW * 0.5f,
        APP_H * 0.5f - adaptiveCamH * 0.5f,
        APP_W * 0.5f + adaptiveCamW * 0.5f,
        APP_H * 0.5f + adaptiveCamH * 0.5f
    };

    gl::Texture2dRef fullscreenTex;
    if (VFX_VISIBLE && mOnlineTracker->getFound())
    {
        fullscreenTex = mClone.getResultTexture();
    }
    else if (VFX_VISIBLE && MOVIE_MODE)
    {
        fullscreenTex = mRenderedOfflineFaceFbo->getColorTexture();
    }
    else
    {
        fullscreenTex = mCapture.texture;
    }
    gl::draw(fullscreenTex, srcArea, dstRect);

    if (OFFLINE_VISIBLE)
    {
        gl::draw(mOfflineFaceTex);
    }

    FPS = getAverageFps();

    gl::disableAlphaBlending();

    if (WIREFRAME_MODE && (MOVIE_MODE || mOnlineTracker->getFound()))
    {
        gl::enableWireframe();

        gl::ScopedModelMatrix modelMatrix;
        gl::ScopedColor color(ColorA(0, 1.0f, 0, 1.0f));
        gl::translate(dstRect.x1, dstRect.y1);
        gl::scale(adaptiveCamW / CAM_W, adaptiveCamH / CAM_H);
        gl::draw(mFaceMesh);

        if (OFFLINE_VISIBLE)
        {
            gl::draw(mOfflineTracker->getImageMesh());
        }

        gl::disableWireframe();
    }
}

CINDER_APP(FaceOff, RendererGl, &FaceOff::prepareSettings)
