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

//translated to Cinder version by
//vinjn (vinjn.z@gmail.com)

#pragma once

#include "cinder/TriMesh.h"
#include "cinder/PolyLine.h"
#include "FaceTracker/Tracker.h" 

using std::vector;

#ifdef _MSC_VER
#	pragma warning(disable:4244)  //conversion from 'const double' to 'float'
#	pragma warning(disable:4018)  //signed/unsigned mismatch
#endif

class ciFaceTracker {
public:
	ciFaceTracker();
	void setup();
	bool update(cv::Mat image);
	void draw() const;
	void reset();
	
	int size() const;
	bool getFound() const;
	bool getVisibility(int i) const;
	
	vector<ci::vec2> getImagePoints() const;
	vector<ci::vec3> getObjectPoints() const;
	vector<ci::vec3> getMeanObjectPoints() const;
	
	//(u,v) ~ [0,1)
	ci::vec2 getUVPoint(int i) const;

	//(x,y) ~ [0,width) X [0,height)
	ci::vec2 getImagePoint(int i) const;
	ci::vec3 getObjectPoint(int i) const;
	ci::vec3 getMeanObjectPoint(int i) const;
	
	ci::TriMesh getImageMesh() const;
	ci::TriMesh getObjectMesh() const;
	ci::TriMesh getMeanObjectMesh() const;
	
	const cv::Mat& getObjectPointsMat() const;
	const ci::vec2 getImageSize() const;

	void addTriangleIndices(ci::TriMesh& mesh) const;
	
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
	ci::PolyLine2 getImageFeature(Feature feature) const;
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
	template <class T>
    ci::PolyLineT<T>  getFeature(Feature feature, const vector<T>& points) const;
	
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

