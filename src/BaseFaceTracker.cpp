#include "BaseFaceTracker.h"
#include "JasonFaceTracker.h"
#include "OpenFaceTracker.h"

namespace jing
{
    using namespace std;
    using namespace ci;

    BaseFaceTrackerRef BaseFaceTracker::create(Option option)
    {
        BaseFaceTracker* tracker = nullptr;

        if (option.method == "JasonFaceTracker")
        {
            auto ciTracker = new ciFaceTracker;
            ciTracker->setRescale(option.scale);

            tracker = ciTracker;
        }
        else if (option.method == "OpenFaceTracker")
        {
            auto openFaceTracker = new OpenFaceTracker;

            tracker = openFaceTracker;
        }
        else
        {
            throw ci::Exception("Unknown method");
        }

        auto trackerRef = BaseFaceTrackerRef(tracker);
        trackerRef->setup();

        return trackerRef;
    }
}
