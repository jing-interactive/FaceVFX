///////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017, Carnegie Mellon University and University of Cambridge,
// all rights reserved.
//
// ACADEMIC OR NON-PROFIT ORGANIZATION NONCOMMERCIAL RESEARCH USE ONLY
//
// BY USING OR DOWNLOADING THE SOFTWARE, YOU ARE AGREEING TO THE TERMS OF THIS LICENSE AGREEMENT.  
// IF YOU DO NOT AGREE WITH THESE TERMS, YOU MAY NOT USE OR DOWNLOAD THE SOFTWARE.
//
// License can be found in OpenFace-license.txt
//

// Precompiled headers stuff

#ifndef __STDAFX_h_
#define __STDAFX_h_

// OpenCV includes
#include <opencv2/core/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/objdetect.hpp>
#include <opencv2/calib3d.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui/highgui.hpp>

// IplImage stuff
#include <opencv2/core/core_c.h>
#include <opencv2/imgproc/imgproc_c.h>

// C++ stuff
#include <stdio.h>

#include <fstream>
#include <iostream>
#include <sstream>

#include <vector>
#include <map>

#define _USE_MATH_DEFINES
#include <cmath>

// Boost stuff
#include "cinder/Filesystem.h"

template<class _Function>
class LambdaBody : public cv::ParallelLoopBody {
public:
    typedef _Function function_type;
    typedef const _Function const_function_type;

    inline LambdaBody(const_function_type& body) :
        _body(body)
    {}

    inline void operator() (const cv::Range & range) const
    {
        this->_body(range);
    }
private:
    const_function_type _body;
};

template<class _Function>
inline void parallel_for(const cv::Range& range, const _Function& body, const double& nstride = -1.)
{
    cv::parallel_for_(range, LambdaBody<_Function>(body), nstride);
}

#endif
