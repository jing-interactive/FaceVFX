#include "IFaceTracker.h"
#include "jason_saragih_tracker/ciFacetracker.h"

namespace ft
{
    using namespace std;
    using namespace ci;

    FaceTrackerRef IFaceTracker::create(Option option)
    {
        IFaceTracker* tracker = nullptr;

        if (option.method == "jason_saragih_tracker")
        {
            auto ciTracker = new ciFaceTracker;
            ciTracker->setRescale(option.scale);

            tracker = ciTracker;
        }
        else
        {
            throw ci::Exception("Unknown method");
        }

        auto trackerRef = FaceTrackerRef(tracker);
        trackerRef->setup();

        return trackerRef;
    }
}
