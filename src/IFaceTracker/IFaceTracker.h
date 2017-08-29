#pragma once

#include <cinder/cinder.h>
#include <cinder/Surface.h>
#include "cinder/TriMesh.h"

namespace ft
{
    typedef std::shared_ptr<struct IFaceTracker> FaceTrackerRef;

    struct Option
    {
        Option()
        {
            method = "jason_saragih_tracker";
            scale = 1.0f;
        }

        std::string method;
        float scale;
    };

    struct IFaceTracker
    {
        static FaceTrackerRef create(Option option = Option());
        virtual bool update(const ci::Surface& surface) = 0;
        virtual void reset() = 0;
        virtual int size() const = 0;
        virtual bool getFound() const = 0;
        virtual ci::vec3 getImagePoint(int i) const = 0;
        virtual ci::TriMesh getImageMesh() const = 0;
        virtual const ci::vec2 getImageSize() const = 0;
        virtual void addTriangleIndices(ci::TriMesh& mesh) const = 0;
    private:
        virtual void setup() = 0;
    };

}