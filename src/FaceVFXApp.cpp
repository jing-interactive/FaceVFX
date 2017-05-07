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

#include "cinder/qtime/QuickTimeGl.h"

#include "IFaceTracker/IFaceTracker.h"

#include "Cinder-VNM/include/TextureHelper.h"
#include "Cinder-VNM/include/CaptureHelper.h"
#include "Cinder-VNM/include/MiniConfig.h"
#include "Cinder-VNM/include/AssetManager.h"

#include "Cinder-ImGui/include/CinderImGui.h"

#include "Clone.h"


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
        settings->setMultiTouchEnabled(false);
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

    void fileDrop(FileDropEvent event)
    {
        if (event.getNumFiles() > 0)
        {
            console() << event.getFile(0);

            ImageSourceRef img = loadImage(event.getFile(0));
            updateOfflineImage(img);
        }
    }

    void setup();
    ~FaceOff()
    {
        mShouldQuit = true;
        mTrackerThread->join();
    }

    void update();
    void draw();
    void resize();

private:
    
    void updateGui();

    void updateOfflineImage(ImageSourceRef img)
    {
        if (!img) return;
        
        mOfflineTracker->reset();
        mOfflineTracker->update(img);
        
        auto loadTexFn = [this, img]
        {
            mOfflineFaceTex = mPhotoTex = gl::Texture::create(img, gl::Texture::Format().loadTopDown());
        };
        dispatchAsync(loadTexFn);
    }
    
    void trackerThreadFn();
    void updateClone();

    TriMesh         mFaceMesh;

    ft::FaceTrackerRef   mOnlineTracker;
    ft::FaceTrackerRef   mOfflineTracker;
    gl::TextureRef mPhotoTex;

    qtime::MovieSurfaceRef   mMovie;

    //
    vector<Capture::DeviceRef>  mDevices;
    vector<string>              mDeviceNames;
    CaptureHelper   mCapture;
    shared_ptr<thread> mTrackerThread;

    vector<string>              mPeopleNames;

    gl::TextureRef mOfflineFaceTex;
    gl::FboRef     mRenderedOfflineFaceFbo, mFaceMaskFbo;
    Clone       mClone;
    bool           mHasNewRenderedFace = false;

    //  param
    int         mDeviceId = -1;
    int         mPeopleId = -1;
    bool        mDoesCaptureNeedsInit = false;

    bool        mShouldQuit = false;
};

void FaceOff::resize()
{
    APP_W = getWindowWidth();
    APP_H = getWindowHeight();
}

void FaceOff::updateClone()
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
        mHasNewRenderedFace = true;
    }
}

void FaceOff::trackerThreadFn()
{
    ft::Option option;
    option.scale = 0.5f;
    mOfflineTracker = ft::IFaceTracker::create();
    mOnlineTracker = ft::IFaceTracker::create(option);

    bool shouldInitFaceMesh = false;

    while (!mShouldQuit)
    {
        // TODO: more robust with update_signal
        if (!mCapture.checkNewFrame())
        {
            sleep(1.0f);
            continue;
        }
        
        if (mDoesCaptureNeedsInit)
        {
            // TODO: more robust with setup_signal
            mDoesCaptureNeedsInit = false;

            CAM_W = mCapture.size.x;
            CAM_H = mCapture.size.y;

            auto createFboFn = [this]
            {
                gl::Fbo::Format fboFormat;
                fboFormat.enableDepthBuffer(false);
                mRenderedOfflineFaceFbo = gl::Fbo::create(CAM_W, CAM_H, fboFormat);
                mFaceMaskFbo = gl::Fbo::create(CAM_W, CAM_H, fboFormat);

                mClone.setup(CAM_W, CAM_H);
                mClone.setStrength(16);
            };
            dispatchAsync(createFboFn);
        }

        if (mPeopleId != PEOPLE_ID)
        {
            mPeopleId = PEOPLE_ID;

            ImageSourceRef img = loadImage(loadAsset("people/" + mPeopleNames[PEOPLE_ID]));
            updateOfflineImage(img);

            shouldInitFaceMesh = true;
        }

        mOnlineTracker->update(mCapture.surface);

        if (!mOnlineTracker->getFound())
        {
            mHasNewRenderedFace = false;
            continue;
        }

        int nPoints = mOnlineTracker->size();
        if (shouldInitFaceMesh)
        {
            shouldInitFaceMesh = false;
            mFaceMesh.getBufferTexCoords0().clear();

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
            dispatchAsync(bind(&FaceOff::updateClone, this));
        }
    }
}

void FaceOff::setup()
{
    ui::initialize(ui::Options().fontGlobalScale(getWindowContentScale()));

    resize();
    
    mTrackerThread = make_shared<thread>(bind(&FaceOff::trackerThreadFn, this));

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

    mPeopleNames = am::shortPaths("people");

    // TODO: assert
    if (DEVICE_ID > mDeviceNames.size() - 1)
    {
        DEVICE_ID = 0;
    }
#ifdef CINDER_COCOA_TOUCH
    DEVICE_ID = 1; // select selfie camera
#endif

    if (PEOPLE_ID > mPeopleNames.size() - 1)
    {
        PEOPLE_ID = 0;
    }

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
        mCapture.setup(CAM_W, CAM_H, mDevices[DEVICE_ID]);
        mDoesCaptureNeedsInit = true;
    }
    
    if (mCapture.isBackCamera)
        mCapture.flip = false;
    else
        mCapture.flip = CAM_FLIP;
}

void FaceOff::updateGui()
{
    ui::ScopedWindow window("Settings", {toPixels(getWindowWidth() / 2), toPixels(getWindowHeight() / 2)}, 0.5f);
    
    if (ui::Button("Save Config"))
    {
        writeConfig();
    }
    
    if (ui::Button("Save Image"))
    {
        auto windowSurf = copyWindowSurface();
#ifdef CINDER_COCOA_TOUCH
        cocoa::writeToSavedPhotosAlbum(windowSurf);
#else
        fs::path writePath = getAppPath() / ("screenshot_" + toString(getElapsedFrames()) + ".png");
        writeImage(writePath, windowSurf);
#endif
    }
    
    if (ui::Button("Quit"))
    {
        App::get()->quit();
    }
    
    ui::Text("FPS: %d", FPS);
    ui::Checkbox("CAM_FLIP", &CAM_FLIP);
    ui::Checkbox("OFFLINE_VISIBLE", &OFFLINE_VISIBLE);
    ui::Checkbox("VFX_VISIBLE", &VFX_VISIBLE);
    ui::Checkbox("WIREFRAME_MODE", &WIREFRAME_MODE);

    ui::Checkbox("MOVIE_MODE", &MOVIE_MODE);
    ui::InputText("MOVIE_PATH", &MOVIE_PATH);

    ui::Combo("DEVICE_ID", &DEVICE_ID, mDeviceNames);
    ui::Combo("PEOPLE_ID", &PEOPLE_ID, mPeopleNames);
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

    if (!mOnlineTracker)
    {
        gl::draw(mCapture.texture, srcArea, dstRect);
        return;
    }

    gl::Texture2dRef fullscreenTex;
    if (VFX_VISIBLE && mHasNewRenderedFace)
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

    if (WIREFRAME_MODE && (MOVIE_MODE || mHasNewRenderedFace))
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
    
    updateGui();
}

CINDER_APP(FaceOff, RendererGl, &FaceOff::prepareSettings)
