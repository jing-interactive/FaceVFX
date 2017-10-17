#pragma once

#include <cinder/cinder.h>
#include <cinder/Surface.h>
#include "cinder/TriMesh.h"
#include "BaseFaceTracker.h"

namespace jing
{
    struct OpenFaceTracker : public BaseFaceTracker
    {
        virtual bool update(const ci::Surface& surface);
        virtual void reset();
        virtual int size() const;
        virtual bool getFound() const;
        virtual ci::vec3 getImagePoint(int i) const;
        virtual ci::TriMesh getImageMesh() const;
        virtual const ci::vec2 getImageSize() const;
        virtual void addTriangleIndices(ci::TriMesh& mesh) const;
    private:
        virtual void setup();
    };
}