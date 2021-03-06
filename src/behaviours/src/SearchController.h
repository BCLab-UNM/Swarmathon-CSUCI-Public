#ifndef SEARCH_CONTROLLER
#define SEARCH_CONTROLLER

#include <random_numbers/random_numbers.h>
#include "Controller.h"
#include "Utilities.h"

/**
 * This class implements the search control algorithm for the rovers. The code
 * here should be modified and enhanced to improve search performance.
 */

extern void logicMessage(long int currentTime, string component, string message);

class SearchController : virtual Controller {

public:

    SearchController();

    void Reset() override;

    // performs search pattern
    Result DoWork() override;

    bool ShouldInterrupt() override;

    bool HasWork() override;

    // sets the value of the current location
    //void UpdateData(geometry_msgs::Pose2D currentLocation, geometry_msgs::Pose2D centerLocation);
    void SetCurrentLocation(Point currentLocation);

    void SetCenterLocation(Point centerLocation);

    void SetSuccesfullPickup();

    void SetCurrentTimeInMilliSecs(long int time);


    float GetNewHeading(float beta, bool search_mode);

protected:

    void ProcessData();

private:

    random_numbers::RandomNumberGenerator *rng;
    Point currentLocation;
    Point centerLocation;
    Point searchLocation;
    int attemptCount = 0;
    //struct for returning data to ROS adapter
    Result result;

    // Search state
    // Flag to allow special behaviour for the first waypoint
    bool first_waypoint = true;
    bool succesfullPickup = false;

    //current ROS time from the RosAdapter
    long int current_time;

    string ClassName = "Search Controller";

    float waypoint_outside_wall_timer = 0;
    float waypoint_search_timer_start = 0;
    bool abandonShip = false;
};

#endif /* SEARCH_CONTROLLER */
