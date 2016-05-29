#include "ciFaceTracker.h"
#include "cinder/app/App.h"
#include "cinder/Utilities.h"
#include "cinder/gl/gl.h"
#include <string>

using namespace ci;
using namespace std;
using namespace cv;
using namespace FACETRACKER;

#define it at<int>
#define db at<double>

// can be compiled with OpenMP for even faster threaded execution

vector<int> consecutive(int start, int end) {
    int n = end - start;
    vector<int> result(n);
    for (int i = 0; i < n; i++) {
        result[i] = start + i;
    }
    return result;
}

template <class T>
ci::PolyLineT<T> ciFaceTracker::getFeature(Feature feature, const vector<T>& points) const {
    ci::PolyLineT<T> polyline;
    if (!mIsFailed) {
        vector<int> indices = getFeatureIndices(feature);
        for (int i = 0; i < indices.size(); i++) {
            int cur = indices[i];
            if (mUseInvisible || getVisibility(cur)) {
                polyline.push_back(points[cur]);
            }
        }
    }
    return polyline;
}

vector<int> ciFaceTracker::getFeatureIndices(Feature feature) {
    static int innerMouth[] = {48,60,61,62,54,63,64,65};
    switch(feature) {
        case LEFT_JAW: return consecutive(0, 9);
        case RIGHT_JAW: return consecutive(8, 17);
        case JAW: return consecutive(0, 17);
        case LEFT_EYEBROW: return consecutive(17, 22);
        case RIGHT_EYEBROW: return consecutive(22, 27);
        case LEFT_EYE: return consecutive(36, 42);
        case RIGHT_EYE: return consecutive(42, 48);
        case OUTER_MOUTH: return consecutive(48, 60);
        case INNER_MOUTH:			
            return vector<int>(innerMouth, innerMouth + 8);
        default:
            return consecutive(0,0);
    }
}

ciFaceTracker::ciFaceTracker()
:mRescale(1)
,mIterations(10) // [1-25] 1 is fast and inaccurate, 25 is slow and accurate
,mClamp(3) // [0-4] 1 gives a very loose fit, 4 gives a very tight fit
,mTolerance(.01) // [.01-1] match tolerance
,mAttempts(1) // [1-4] 1 is fast and may not find faces, 4 is slow but will find faces
,mIsFailed(true)
,mFailCheck(true) // check for whether the tracking failed
,mFrameSkip(-1) // how often to skip frames
,mUseInvisible(true)
{
}

void ciFaceTracker::setup() {
    wSize1.resize(1);
    wSize1[0] = 7;

    wSize2.resize(3);
    wSize2[0] = 11;
    wSize2[1] = 9;
    wSize2[2] = 7;

    string ftFile = app::getAssetPath("face2.tracker").string();
    string triFile = app::getAssetPath("face.tri").string();
    string conFile = app::getAssetPath("face.con").string();

    mTracker.Load(ftFile.c_str());
    mTri = IO::LoadTri(triFile.c_str());
    mCon = IO::LoadCon(conFile.c_str());  // not being used right now
}

bool ciFaceTracker::update(Mat image) {	
    if (mRescale == 1) {
        mIm = image; 
    } else {
        resize(image, mIm, cv::Size(mRescale * image.cols, mRescale * image.rows));
    }

    int nChannels = mIm.channels();
    if (nChannels == 3)
        cvtColor(mIm, mGray, CV_RGB2GRAY);
    else if (nChannels == 4)
        cvtColor(mIm, mGray, CV_RGBA2GRAY);
    else
        mGray = mIm;
    mImgSize = { image.cols, image.rows };

    bool tryAgain = true;
    for (int i = 0; tryAgain && i < mAttempts; i++) {
        vector<int> wSize;
        if (mIsFailed) {
            wSize = wSize2;
        } else {
            wSize = wSize1;
        }

        if (mTracker.Track(mGray, wSize, mFrameSkip, mIterations, mClamp, mTolerance, mFailCheck) == 0) {
            mCurrentView = mTracker._clm.GetViewIdx();
            mIsFailed = false;
            tryAgain = false;
            updateObjectPoints();
        } else {
            mTracker.FrameReset();
            mIsFailed = true;
        }
    }
    return !mIsFailed;
}

void ciFaceTracker::draw() const{
    if (mIsFailed) {
        return;
    }

    ci::gl::draw(getImageMesh());

    int n = size();
    for (int i = 0; i < n; i++){
        if (getVisibility(i)) { 
            ci::gl::drawString(ci::toString(i), getImagePoint(i));
        }
    }
}

void ciFaceTracker::reset() {
    mTracker.FrameReset();
}

int ciFaceTracker::size() const {
    return mTracker._shape.rows / 2;
}

bool ciFaceTracker::getFound() const {
    return !mIsFailed;
}

bool ciFaceTracker::getVisibility(int i) const {
    if (mIsFailed) {
        return false;
    }
    const Mat& visi = mTracker._clm._visi[mCurrentView];
    return (visi.it(i, 0) != 0);
}

vector<ci::vec2> ciFaceTracker::getImagePoints() const {
    int n = size();
    vector<ci::vec2> imagePoints(n);
    for (int i = 0; i < n; i++) {
        imagePoints[i] = getImagePoint(i);
    }
    return imagePoints;
}

vector<ci::vec3> ciFaceTracker::getObjectPoints() const {
    int n = size();
    vector<ci::vec3> objectPoints(n);
    for (int i = 0; i < n; i++) {
        objectPoints[i] = getObjectPoint(i);
    }
    return objectPoints;
}

vector<ci::vec3> ciFaceTracker::getMeanObjectPoints() const {
    int n = size();
    vector<ci::vec3> meanObjectPoints(n);
    for (int i = 0; i < n; i++) {
        meanObjectPoints[i] = getMeanObjectPoint(i);
    }
    return meanObjectPoints;
}

ci::vec2 ciFaceTracker::getImagePoint(int i) const {
    if (mIsFailed) {
        return ci::vec2();
    }
    const Mat& shape = mTracker._shape;
    int n = shape.rows / 2;
    return ci::vec2(shape.db(i, 0), shape.db(i + n, 0)) / mRescale;
}

ci::vec3 ciFaceTracker::getObjectPoint(int i) const {
    if (mIsFailed) {
        return ci::vec3();
    }	
    int n = mObjectPoints.rows / 3;
    return ci::vec3(mObjectPoints.db(i,0), mObjectPoints.db(i+n,0), mObjectPoints.db(i+n+n,0));
}

ci::vec3 ciFaceTracker::getMeanObjectPoint(int i) const {
    const Mat& mean = mTracker._clm._pdm._M;
    int n = mean.rows / 3;
    return ci::vec3(mean.db(i,0), mean.db(i+n,0), mean.db(i+n+n,0));
}

ci::TriMesh ciFaceTracker::getImageMesh() const{
    return getMesh(getImagePoints());
}

ci::TriMesh ciFaceTracker::getObjectMesh() const {
    return getMesh(getObjectPoints());
}

ci::TriMesh ciFaceTracker::getMeanObjectMesh() const {
    return getMesh(getMeanObjectPoints());
}

const Mat& ciFaceTracker::getObjectPointsMat() const {
    return mObjectPoints;
}

ci::vec2 ciFaceTracker::getPosition() const {
    const Mat& pose = mTracker._clm._pglobl;
    return ci::vec2(pose.db(4,0), pose.db(5,0)) / mRescale;
}

// multiply by ~20-23 to get pixel units (+/-20 units in the x axis, +23/-18 on the y axis)
float ciFaceTracker::getScale() const {
    const Mat& pose = mTracker._clm._pglobl;
    return pose.db(0,0) / mRescale;
}

ci::vec3 ciFaceTracker::getOrientation() const {
    const Mat& pose = mTracker._clm._pglobl;
    ci::vec3 euler(pose.db(1, 0), pose.db(2, 0), pose.db(3, 0));
    return euler;
}

ci::mat4 ciFaceTracker::getRotationMatrix() const {
    ci::vec3 euler = getOrientation();
    ci::mat4 matrix = glm::eulerAngleYXZ(euler.y, euler.x, euler.z);
    return matrix;
}

ciFaceTracker::Direction ciFaceTracker::getDirection() const {
    if (mIsFailed) {
        return FACING_UNKNOWN;
    }
    switch(mCurrentView) {
        case 0: return FACING_FORWARD;
        case 1: return FACING_LEFT;
        case 2: return FACING_RIGHT;
        default: return FACING_UNKNOWN;;
    }
}

ci::PolyLine2 ciFaceTracker::getImageFeature(Feature feature) const {
    return getFeature(feature, getImagePoints());
}

ci::PolyLine3 ciFaceTracker::getObjectFeature(Feature feature) const {
    return getFeature(feature, getObjectPoints());
}

ci::PolyLine3 ciFaceTracker::getMeanObjectFeature(Feature feature) const {
    return getFeature(feature, getMeanObjectPoints());
}

float ciFaceTracker::getGesture(Gesture gesture) const {
    if (mIsFailed) {
        return 0;
    }
    int start = 0, end = 0;
    switch(gesture) {
        // left to right of mouth
        case MOUTH_WIDTH: start = 48; end = 54; break;
            // top to bottom of inner mouth
        case MOUTH_HEIGHT: start = 61; end = 64; break;
            // center of the eye to middle of eyebrow
        case LEFT_EYEBROW_HEIGHT: start = 38; end = 20; break;
            // center of the eye to middle of eyebrow
        case RIGHT_EYEBROW_HEIGHT: start = 43; end = 23; break;
            // upper inner eye to lower outer eye
        case LEFT_EYE_OPENNESS: start = 38; end = 41; break;
            // upper inner eye to lower outer eye
        case RIGHT_EYE_OPENNESS: start = 43; end = 46; break;
            // nose center to chin center
        case JAW_OPENNESS: start = 33; end = 8; break;
            // left side of nose to right side of nose
        case NOSTRIL_FLARE: start = 31; end = 35; break;
    }

    return glm::length(getObjectPoint(start) - getObjectPoint(end));
}

void ciFaceTracker::setRescale(float rescale) {
    mRescale = rescale;
}

void ciFaceTracker::setIterations(int iterations) {
    mIterations = iterations;
}

void ciFaceTracker::setClamp(float clamp) {
    mClamp = clamp;
}

void ciFaceTracker::setTolerance(float tolerance) {
    mTolerance = tolerance;
}

void ciFaceTracker::setAttempts(int attempts) {
    mAttempts = attempts;
}

void ciFaceTracker::setUseInvisible(bool useInvisible) {
    mUseInvisible = useInvisible;
}

void ciFaceTracker::updateObjectPoints() {
    const Mat& mean = mTracker._clm._pdm._M;
    const Mat& variation = mTracker._clm._pdm._V;
    const Mat& weights = mTracker._clm._plocal;
    mObjectPoints = mean + variation * weights;
}

void ciFaceTracker::addTriangleIndices(ci::TriMesh& mesh) const {
    int in = mTri.rows;
    for (int i = 0; i < mTri.rows; i++) {
        int i0 = mTri.it(i, 0), i1 = mTri.it(i, 1), i2 = mTri.it(i, 2);
        bool visible = getVisibility(i0) && getVisibility(i1) && getVisibility(i2);
        if (mUseInvisible || visible) {
            mesh.appendTriangle(i0, i1, i2);
        }
    }
}


ci::TriMesh ciFaceTracker::getMesh(const vector<ci::vec3>& points) const {
    ci::TriMesh mesh;
    if (!mIsFailed) {
        int n = size();
        for (int i = 0; i < n; i++) {
            mesh.appendPosition(points[i]);
            mesh.appendTexCoord(getImagePoint(i)/mImgSize);
        }
        addTriangleIndices(mesh);
    }
    return mesh;
}

ci::TriMesh ciFaceTracker::getMesh(const vector<ci::vec2>& points) const {
    ci::TriMesh mesh;
    if (!mIsFailed) {
        int n = size();
        for (int i = 0; i < n; i++) {
            mesh.appendPosition(ci::vec3(points[i],0));
            mesh.appendTexCoord(getImagePoint(i)/mImgSize);
        }
        addTriangleIndices(mesh);
    }
    return mesh;
}

const ci::vec2 ciFaceTracker::getImageSize() const
{
    return mImgSize;
}

ci::vec2 ciFaceTracker::getUVPoint( int i ) const
{
    return getImagePoint(i)/mImgSize;
}
