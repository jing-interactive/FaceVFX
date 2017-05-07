/*
 ofxFaceTracker provides an interface to Jason Saragih's FaceTracker library.

 getImagePoint()/getImageMesh() are in image space. This means that all the
 points will line up with the pixel coordinates of the image you fed into
 ofxFaceTracker.

 getObjectPoint()/getObjectMesh() are in 3d object space. This is a product of
 the mean mesh with only the expression applied. There is no rotation or
 translation applied to the object space.

 getMeanObjectPoint()/getMeanObjectMesh() are also in 3d object space. However,
 there is no expression applied to the mesh.
 */

// Ported to Cinder version by
// vinjn (vinjn.z@gmail.com)

#pragma once

#include "cinder/TriMesh.h"
#include "cinder/PolyLine.h"
#include "lib/Tracker.h"
#include "../IFaceTracker.h"

using std::vector;

namespace cinder {
    typedef PolyLineT<vec3> PolyLine3;
}

#ifdef _MSC_VER
#	pragma warning(disable:4244)  //conversion from 'const double' to 'float'
#	pragma warning(disable:4018)  //signed/unsigned mismatch
#endif

class ciFaceTracker : public ft::IFaceTracker {
public:
    ciFaceTracker();
    void setup() override;
    bool update(const ci::Surface& surface) override;

    void draw() const;
    void reset() override;

    int size() const override;
    bool getFound() const override;
    bool getVisibility(int i) const;

    vector<ci::vec3> getImagePoints() const;
    vector<ci::vec3> getObjectPoints() const;
    vector<ci::vec3> getMeanObjectPoints() const;

    //(x,y) ~ [0,width) X [0,height)
    ci::vec3 getImagePoint(int i) const override;
    ci::vec3 getObjectPoint(int i) const;
    ci::vec3 getMeanObjectPoint(int i) const;

    ci::TriMesh getImageMesh() const override;
    ci::TriMesh getObjectMesh() const;
    ci::TriMesh getMeanObjectMesh() const;

    const cv::Mat& getObjectPointsMat() const;
    const ci::vec2 getImageSize() const override;

    void addTriangleIndices(ci::TriMesh& mesh) const override;

    ci::vec2 getPosition() const;
    float getScale() const;
    ci::vec3 getOrientation() const;
    ci::mat4 getRotationMatrix() const;

    enum Direction {
        FACING_FORWARD,
        FACING_LEFT, FACING_RIGHT,
        FACING_UNKNOWN
    };
    Direction getDirection() const;

    enum Feature {
        LEFT_EYEBROW, RIGHT_EYEBROW,
        LEFT_EYE, RIGHT_EYE,
        LEFT_JAW, RIGHT_JAW, JAW,
        OUTER_MOUTH, INNER_MOUTH
    };
    ci::PolyLine3 getImageFeature(Feature feature) const;
    ci::PolyLine3 getObjectFeature(Feature feature) const;
    ci::PolyLine3 getMeanObjectFeature(Feature feature) const;

    enum Gesture {
        MOUTH_WIDTH, MOUTH_HEIGHT,
        LEFT_EYEBROW_HEIGHT, RIGHT_EYEBROW_HEIGHT,
        LEFT_EYE_OPENNESS, RIGHT_EYE_OPENNESS,
        JAW_OPENNESS,
        NOSTRIL_FLARE
    };
    float getGesture(Gesture gesture) const;

    void setRescale(float rescale);
    void setIterations(int iterations);
    void setClamp(float clamp);
    void setTolerance(float tolerance);
    void setAttempts(int attempts);
    void setUseInvisible(bool useInvisible);

protected:
    ci::TriMesh getMesh(const vector<ci::vec3>& points) const;
    ci::TriMesh getMesh(const vector<ci::vec2>& points) const;

    void updateObjectPoints();

    static vector<int> getFeatureIndices(Feature feature);
    ci::PolyLine3  getFeature(Feature feature, const vector<ci::vec3>& points) const;

    bool mIsFailed;
    int mCurrentView;

    bool mFailCheck;
    float mRescale;
    int mFrameSkip;

    vector<int> wSize1, wSize2;
    int mIterations;
    int mAttempts;
    double mClamp, mTolerance;
    bool mUseInvisible;

    FACETRACKER::Tracker mTracker;
    cv::Mat mTri, mCon;

    cv::Mat mIm, mGray;
    cv::Mat mObjectPoints;
    ci::vec2 mImgSize;
};

