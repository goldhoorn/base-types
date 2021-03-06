#ifndef __TRAJECTORY_HH__
#define __TRAJECTORY_HH__

#include <base/geometry/Spline.hpp>

namespace base
{

struct Trajectory
{
    Trajectory(): speed(0)
    {
    }
    
    /**
     * The speed in which the trajectory should be 
     * traversed. 
     * */
    double speed;
    
    /**
     * Returns true if the robot should drive forward 
     * on the trajectory. 
     * False if the robot should drive backwards.
     * */
    bool driveForward() const{
	return speed >= 0;
    }
    
    /**
     * A spline representing the trajectory that should 
     * be driven. 
     * */
    base::geometry::Spline<3> spline;
};

}

#endif
