#include "OpenFaceTracker.h"
#include "../3rdparty/OpenFace/LandmarkDetector/include/LandmarkCoreIncludes.h"

namespace jing
{
    bool OpenFaceTracker::update(const ci::Surface& surface)
    {
        return false;
    }

    void OpenFaceTracker::reset()
    {

    }

    int OpenFaceTracker::size() const
    {
        return 0;
    }

    bool OpenFaceTracker::getFound() const
    {
        return false;
    }

    ci::vec3 OpenFaceTracker::getImagePoint(int i) const
    {
        return{};
    }

    ci::TriMesh OpenFaceTracker::getImageMesh() const
    {
        return{};
    }

    const ci::vec2 OpenFaceTracker::getImageSize() const
    {
        return{};
    }

    void OpenFaceTracker::addTriangleIndices(ci::TriMesh& mesh) const
    {

    }

    void OpenFaceTracker::setup()
    {
        //LandmarkDetector::FaceModelParameters det_parameters(arguments);
    }
}
