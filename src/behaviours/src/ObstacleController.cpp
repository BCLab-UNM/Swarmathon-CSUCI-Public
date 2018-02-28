#include <angles/angles.h>
#include "ObstacleController.h"
#include "ObstacleAssistant.h"

ObstacleController::ObstacleController() {
    /*
     * Turn sonar On/Off
     */
    this->acceptDetections = true;

    obstacleAvoided = true;
//    this->detection_declaration = NO_OBSTACLE;
    obstacleDetected = false;
    obstacleInterrupt = false;
    result.PIDMode = CONST_PID; //use the const PID to turn at a constant speed

    /*
     * Create 'OBSTACLE' structures to contain all evaluations and obstacle decision of the sonar readings.
     * Contains:
     * -> 'type' of obstacle
     * -> 'sonar_map' of feedback from each sensor
     */
    std::map<SONAR, ObstacleAssistant> assistant_map = {
            {LEFT, ObstacleAssistant(LEFT)},
            {CENTER, ObstacleAssistant(CENTER)},
            {RIGHT, ObstacleAssistant(RIGHT)}
    };
    OBSTACLE obstacle = {true, NO_OBSTACLE, assistant_map};
    this->monitor_map = {
            {INIT, obstacle},
            {STAG, obstacle}
    };
    this->stag = 0;

    /*
     * Create reflection structure
     */
    this->reflection.can_start = false;
    this->reflection.can_end = true;
}


//note, not a full reset as this could cause a bad state
//resets the interupt and knowledge of an obstacle or obstacle avoidance only.
void ObstacleController::Reset() {
    obstacleAvoided =  true;
//    this->detection_declaration = NO_OBSTACLE;
    obstacleDetected = false;
    obstacleInterrupt = false;
    delay = current_time;
}

// Avoid crashing into objects detected by the ultraound
void ObstacleController::avoidObstacle() {
    logicMessage(current_time, ClassName, __func__);

    result.type = precisionDriving;
    result.pd.setPointVel = 0.0;
    result.pd.cmdVel = 0.0;
    result.pd.setPointYaw = 0;
    /*
     * Based on detection location reflect off of obstacle in
     * opposing direction.
     * NOTE: only initial detection is recorded by 'reflect' function
     */
    std::cout << "OBSTACLE CONTROLLER: avoidObstacle" << std::endl;
    switch (this->detection_declaration) {
        case OBS_LEFT:
            std::cout << "left" << std::endl;
            this->reflect({LEFT_LOW, LEFT_HIGH});
            result.pd.cmdAngular = -K_angular; // Turn right to avoid obstacle (NOTE: negated K_angular for all in avoidObstacle TODO:)
            break;
        case OBS_CENTER:
        std::cout << "center" << std::endl;
            this->reflect({LEFT_LOW, LEFT_HIGH});
            result.pd.cmdAngular = -K_angular; // Turn right to avoid obstacle
            break;
        case OBS_RIGHT:
        std::cout << "right" << std::endl;
            this->reflect({RIGHT_LOW, RIGHT_HIGH});
            result.pd.cmdAngular = K_angular; // Turn left to avoid obstacle
            break;
        case OBS_LEFT_CENTER:
        std::cout << "center left" << std::endl;
            this->reflect({LEFT_LOW, LEFT_HIGH});
            result.pd.cmdAngular = -K_angular; // Turn right to avoid obstacle
            break;
        case OBS_RIGHT_CENTER:
        std::cout << "center right" << std::endl;
            this->reflect({RIGHT_LOW, RIGHT_HIGH});
            result.pd.cmdAngular = K_angular; // Turn left to avoid obstacle
            break;
        default:
            std::cout << "OBSTACLE_CONTROLLER: avoidObstacle(), hit default" << std::endl;
    }
}

// A collection zone was seen in front of the rover and we are not carrying a target
// so avoid running over the collection zone and possibly pushing cubes out.
void ObstacleController::avoidCollectionZone() {
    logicMessage(current_time, ClassName, __func__);

    result.type = precisionDriving;
    result.pd.cmdVel = 0.0;

    // Decide which side of the rover sees the most april tags and turn away
    // from that side
//    if (count_left_collection_zone_tags < count_right_collection_zone_tags) {
    if (count_left_collection_zone_tags > count_right_collection_zone_tags) { // Todo: MYCODE inequality seemed backwards
        this->reflect({LEFT_LOW, LEFT_HIGH}); // Todo:
        result.pd.cmdAngular = K_angular; // Home on the left; turn right
    } else {
        this->reflect({RIGHT_LOW, RIGHT_HIGH}); // Todo:
        result.pd.cmdAngular = -K_angular; // Home on the right; turn left
    }

    result.pd.setPointVel = 0.0;
    result.pd.cmdVel = 0.0;
    result.pd.setPointYaw = 0;
}


Result ObstacleController::DoWork() {
//    std::cout << "ObstacleController -> DoWork" << std::endl;
    logicMessage(current_time, ClassName, __func__);

    clearWaypoints = true;
    set_waypoint = true;
    result.PIDMode = CONST_PID;
    string msg;

//    if (this->detection_declaration != NO_OBSTACLE) {
    if (obstacleDetected) {
        /*
         * Update "/logger" publisher -> Starting avoidance
         */
        if (!logInit) {
            msg = "Starting obstacle avoidance routine.";
            logMessage(current_time, "ObstacleController", msg);
            logInit = true;
        }

        /*
         * The obstacle is an april tag marking the collection zone
         * HOME retains highest priority since it is checked first
         */
        if (collection_zone_seen) {
            avoidCollectionZone();
        } else {
            avoidObstacle();
        }
    }


    //if an obstacle has been avoided
    if (can_set_waypoint) {
        /*
         * Update "/logger" publisher -> Exiting avoidance
         */
        msg = "Exiting obstacle avoidance.";
        logMessage(current_time, "ObstacleController", msg);
        logInit = false;

        can_set_waypoint = false; //only one waypoint is set
        set_waypoint = false;
        clearWaypoints = false;
        // TODO: obstacle traversal
        if (targetHeld)
            result.type = waypoint;
        else
            result.type = vectorDriving;
        result.desired_heading = currentLocation.theta;
        result.PIDMode = FAST_PID; //use fast pid for waypoints
        Point forward;            //waypoint is directly ahead of current heading
        forward.x = currentLocation.x + (0.5 * cos(currentLocation.theta));
        forward.y = currentLocation.y + (0.5 * sin(currentLocation.theta));
        result.waypoints.clear();
        result.waypoints.push_back(forward);
    }

    return result;
}


void ObstacleController::setSonarData(float sonarleft, float sonarcenter, float sonarright) {
    left = sonarleft;
    right = sonarright;
    center = sonarcenter;

    ProcessData();
}

void ObstacleController::setCurrentLocation(Point currentLocation) {
    this->currentLocation = currentLocation;
}

void ObstacleController::ProcessData() {
    //timeout timer for no tag messages
    //this is used to set collection zone seen to false beacuse
    //there is no report of 0 tags seen
    long int Tdifference = current_time - timeSinceTags;
    float Td = Tdifference / 1e3;
    if (Td >= 0.5) {
        collection_zone_seen = false;
        phys = false;
        if (!obstacleAvoided) {
//        if (this->detection_declaration != NO_OBSTACLE) {
            can_set_waypoint = true;
        }
    }

    //If we are ignoring the center sonar
    if (ignore_center_sonar) {
        //If the center distance is longer than the reactivation threshold
//        if (center > reactivate_center_sonar_threshold) {
        if (center > MIN_THRESH) {
            //currently do not re-enable the center sonar instead ignore it till the block is dropped off
            //ignore_center_sonar = false; //look at sonar again beacuse center ultrasound has gone long
        } else {
            //set the center distance to "max" to simulated no obstacle
            center = 3;
        }
    } else {
        //this code is to protect against a held block causing a false short distance
        //currently pointless due to above code
        if (center < 3.0) {
            result.wristAngle = 0.7;
        } else {
            result.wristAngle = -1;
        }
    }

    /**
     * Each iteration through should have to recalc detection.
     * Final detection should be checked if it is allowed to end.
     */
    for (auto monitor : monitor_map) {
        this->resetDetections(monitor.first);
    }
//    this->resetDetections();

    /*
     * TODO: Not sure if this is needed
     * Can't find the trigger that checks whether the rover can see home,
     * then flags 'obstacleDetected'.
     */
//    if (collection_zone_seen) {
//        obstacleDetected = true;
//        this->detection_declaration = HOME;
//    }


    // Delay the start of the second set of monitors
    for (auto monitor : this->monitor_map) {
        // Every even iteration will start another monitor if one is available and hasn't been started
        if (!monitor.second.allowed && this->stag % DELAY_ITERATION == 0) {
            monitor.second.allowed = true;
            this->monitor_map.at(monitor.first) = monitor.second;
//            this->stag++;
            break;
        }
        else if (this->stag < this->monitor_map.size()){
            this->stag++;
        }
    }

    /*
     * DETECTION METHODOLOGY
     * 1) Is below 'MAX_THRESH'?
     * 2) Check is any of the monitors are at capacity
     * 3) Is valid detection? Is below 'MIN_THRESH'
     * 4) Determine avoidance type and measures
     * 5) Verify that MONITORS agree, then assign agreement detection
     */


    /* 1)
     * Monitor the detection values that fall below our 'MAX_THRESH'
     * INIT: should always be running from the start
     * STAG: needs to wait two iterations before starting, then should always be running
     */
    for (auto monitor : this->monitor_map) {
        if (left <= MAX_THRESH) {
            this->sonarMonitor(monitor.first, left, LEFT);
        }
        if (center <= MAX_THRESH) {
            this->sonarMonitor(monitor.first, center, CENTER);
        }
        if (right <= MAX_THRESH) {
            this->sonarMonitor(monitor.first, right, RIGHT);
        }
    }

    /* 2)
     * Check if any of the monitors are at capacity
     */
    for (auto monitor : this->monitor_map) {
        if (monitor.second.allowed) {
            this->sonarAnalysis(monitor.first);
        }
    }

    /* 3)
     * If any of the monitors are valid, check if they have cross the 'MIN_THRESH'.
     * Populate temporary map with acceptable detections. Then clear respective monitor so new detections can be made.
     */
    auto *accepted_monitor_map = new std::map<DELAY_TYPE, OBSTACLE>();
    for (auto monitor : this->monitor_map) {
        if (monitor.second.allowed) {
            auto *accepted_sonar_map = new std::map<SONAR, ObstacleAssistant>();
            for (auto assistant : monitor.second.sonar_map) {
                if (assistant.second.detections.good_detection) {
                    accepted_sonar_map->insert(std::pair<SONAR, ObstacleAssistant>(assistant.first, assistant.second));
                    // Reset acceptable monitor after it has been copied
//                    assistant.second.monitor->clear();
                }
            }
//            monitor.second.sonar_map.clear();
//            std::cout << "sonarMap size: " << monitor.second.sonar_map.size() << std::endl;
            monitor.second.sonar_map = *accepted_sonar_map;
            accepted_sonar_map->clear();
            accepted_monitor_map->insert(std::pair<DELAY_TYPE, OBSTACLE>(monitor.first, monitor.second));
        }
    }

    /* 4)
     * Iterate through temporary sonar maps and determine correct avoidance type and measures
     */
    for (auto monitor : *accepted_monitor_map) {
        if (!monitor.second.sonar_map.empty()) {
            obstacleContactDir(monitor.second.sonar_map, monitor.first);
            // Drop temp monitor mapping
            accepted_monitor_map->clear();
        }

    }


    /* 5)
     * Only accept a detection if both monitors agree that there is an obstacle. If one flags but not the other
     * then there is the possibility that it's still a false detection.
     * NOTE: There shouldn't be any obstacle impeeding the rovers path 0.2 secs after its been
     * turned on, so this statement should be caught by 'obstacle_init' and skipped before
     * the initialization of 'obstacle_stag'
     */
    bool can_see_obs = false;
    for (auto monitor : this->monitor_map) {
        if (monitor.second.allowed && monitor.second.type != NO_OBSTACLE) {
            can_see_obs = true;
        }
        else {
            can_see_obs = false;
        }
    }
    if (can_see_obs && acceptDetections) {
        if (this->reflection.can_end) {
            this->detection_declaration = this->monitor_map.at(INIT).type;
            std::cout << "OBSTACLE CONTROLLER: processData() detection made: " << this->detection_declaration << std::endl;
        }
        for (auto monitor : this->monitor_map) {
            resetObstacle(monitor.first);
        }
    }
    if (this->reflection.can_start)
        std::cout << "Can Start" << std::endl;
    if (this->reflection.can_end)
        std::cout << "Can End" << std::endl;
    if (this->detection_declaration != NO_OBSTACLE) {
        if (collection_zone_seen) {
            std::cout << "Collection zone seen" << std::endl;
            this->detection_declaration = HOME;
        }
        phys = true;
        timeSinceTags = current_time;
        obstacleDetected = true;
        obstacleAvoided = false;
        can_set_waypoint = false;
    }
    else {
        // Verify that we've completed the reflection off the obstacle
        if (this->reflection.can_end) {
            std::cout << "processData() obstacle avoided" << std::endl;
            obstacleAvoided = true;

        }
    }

    /*
     * Detection Logger
     */
    if (detection_declaration != NO_OBSTACLE) {
        detect_msg = "Detection: " + to_string(this->detection_declaration);
        detectionMessage(current_time, "ObstacleController", detect_msg);
    }
}

// Report April tags seen by the rovers camera so it can avoid
// the collection zone
// Added relative pose information so we know whether the
// top of the AprilTag is pointing towards the rover or away.
// If the top of the tags are away from the rover then treat them as obstacles.
void ObstacleController::setTagData(vector <Tag> tags) {
    collection_zone_seen = false;
    count_left_collection_zone_tags = 0;
    count_right_collection_zone_tags = 0;
    x_home_tag_orientation = 0; // Todo:

    // this loop is to get the number of center tags
    if (!targetHeld) {
        for (int i = 0; i < tags.size(); i++) { //redundant for loop
            if (tags[i].getID() == 256) {
                collection_zone_seen = checkForCollectionZoneTags(tags);
                timeSinceTags = current_time;
            }
        }
    }
}

bool ObstacleController::checkForCollectionZoneTags(vector<Tag> tags) {

    for (auto &tag : tags) {

        // Check the orientation of the tag. If we are outside the collection zone the yaw will be positive so treat the collection zone as an obstacle.
        //If the yaw is negative the robot is inside the collection zone and the boundary should not be treated as an obstacle.
        //This allows the robot to leave the collection zone after dropping off a target.
        if (tag.calcYaw() > 0) {

            // TODO: this only counts the number of detection on the left or right side
            // TODO: consider checking if we see any tags that have positive yaw
            // TODO: distance calculations can be made else where

            // checks if tag is on the right or left side of the image
            if (tag.getPositionX() + camera_offset_correction > 0) {
                count_right_collection_zone_tags++;
            } else {
                count_left_collection_zone_tags++;
            }
            this->x_home_tag_orientation += tag.getPositionX() + camera_offset_correction;
        }
    }

    // Did any tags indicate that the robot is inside the collection zone?
    return count_left_collection_zone_tags + count_right_collection_zone_tags > 0;

}

//obstacle controller should inrerupt is based upon the transition from not seeing and obstacle to seeing an obstacle
bool ObstacleController::ShouldInterrupt() {

    //if we see and obstacle and havent thrown an interrupt yet
    if (obstacleDetected && !obstacleInterrupt) {
//    if ((this->detection_declaration != NO_OBSTACLE) && !obstacleInterrupt) {
        obstacleInterrupt = true;
        return true;
    } else {
        //if the obstacle has been avoided and we had previously detected one interrupt to change to waypoints
        if ((obstacleAvoided) && obstacleDetected) {
//        if (obstacleAvoided && (this->detection_declaration != NO_OBSTACLE)) {
            Reset();
            return true;
        } else {
            return false;
        }
    }
}

bool ObstacleController::HasWork() {
    //there is work if a waypoint needs to be set or the obstacle hasnt been avoided
    if (can_set_waypoint && set_waypoint) {
        return true;
    }

    return !obstacleAvoided;
}

//ignore center ultrasound
void ObstacleController::setIgnoreCenterSonar() {
    ignore_center_sonar = true;
}

void ObstacleController::SetCurrentTimeInMilliSecs(long int time) {
    current_time = time;
}

void ObstacleController::setTargetHeld() {
    targetHeld = true;

    //adjust current state on transition from no cube held to cube held
    if (previousTargetState == false) {
        obstacleAvoided = true;
//        this->detection_declaration = NO_OBSTACLE;
        obstacleInterrupt = false;
        obstacleDetected = false;
        previousTargetState = true;
    }
}

void ObstacleController::setTargetHeldClear() {
    //adjust current state on transition from cube held to cube not held
    if (targetHeld) {
        Reset();
        targetHeld = false;
        previousTargetState = false;
        ignore_center_sonar = false;
    }
}

void ObstacleController::reflect(std::vector<double> bounds) {
    std::cout << "OBSTACLE CONTROLLER: reflect" << std::endl; // Only want to deal with positive values
//    double abs_heading = std::fabs(currentLocation.theta);
    if (!this->reflection.can_start) {
        std::cout << "reflect() create angle" << std::endl;
        int rand();
        double range = bounds.at(1) - bounds.at(0);
        this->reflection.desired_heading = std::fabs(fmod(rand(), range) + bounds.at(0));
//        this->reflection.reflect_angle = this->reflection.desired_heading;
        this->reflection.reflect_angle = angles::shortest_angular_distance(this->reflection.desired_heading, currentLocation.theta);
        this->reflection.can_start = true;
        this->reflection.can_end = false;
    }
    else if (this->reflection.reflect_angle <= 0) {
        std::cout << "reflect() rotation complete" << std::endl;
//        this->reflection.reflect_angle = 0;
        this->reflection.can_start = false;
        this->reflection.can_end = true;
    }
    else {
        std::cout << "reflect() still rotating" << std::endl;
//        this->reflection.reflect_angle -= this->reflection.desired_heading * K_angular * K_FACTOR;
        this->reflection.reflect_angle = angles::shortest_angular_distance(this->reflection.desired_heading, currentLocation.theta);
        std::cout << "reflect() radians left: " << this->reflection.reflect_angle << std::endl;
    }
}

/**
 * If accepted detections cross 'MIN_THRESH' mark the direction of the obstacle
 * @param accepted_sonar : temporary map of accepted sonar vectors
 * @param delay : which monitor are we editing
 */
void ObstacleController::obstacleContactDir(std::map<SONAR, ObstacleAssistant> accepted_sonar, DELAY_TYPE delay_type) {
    OBSTACLE obstacle = this->monitor_map.at(delay_type);

    std::vector<bool> has_detection = {false, false, false};
    bool admit = false;
    ObstacleAssistant *left_assist = NULL;
    ObstacleAssistant *center_assist = NULL;
    ObstacleAssistant *right_assist = NULL;
    for (std::map<SONAR, ObstacleAssistant>::iterator it = accepted_sonar.begin(); it != accepted_sonar.end(); ++it) {
        if (it->second.detections.smallest_detection < MIN_THRESH) {
            switch (it->first) {
                case LEFT:
                    left_assist = &it->second;
                    has_detection.at(0) = true;
                    break;
                case CENTER:
                    center_assist = &it->second;
                    has_detection.at(1) = true;
                    break;
                case RIGHT:
                    right_assist = &it->second;
                    has_detection.at(2) = true;
                    break;
                default:
                    std::cout << "OBSTACLE_CONTROLLER: hit default in 'ObstacleController::obstacleContactDir()" << std::endl;
                    break;
            }
        }
        else {
            // Restart respective sonar monitor
            obstacle.sonar_map.at(it->first).monitor->clear();
        }
    }
    /*
     * Check if there are any good detections
     */
    for (auto accept : has_detection) {
        if (accept == true) {
            admit = true;
            break;
        }
    }
    std::cout << "Admit : " << admit << std::endl;
    if (admit) {
        if (left_assist && center_assist && right_assist) {
            std::cout << "Left range: " << left_assist->detections.smallest_detection << std::endl;
            std::cout << "Center range: " << center_assist->detections.smallest_detection << std::endl;
            std::cout << "Right range: " << right_assist->detections.smallest_detection << std::endl;
            // Base Case: if the rover meets a flat object head-on
            if (left_assist->detections.smallest_detection == center_assist->detections.smallest_detection &&
                center_assist->detections.smallest_detection == right_assist->detections.smallest_detection) {
                std::cout << "OBS CENTER" << std::endl;
                obstacle.type = OBS_CENTER;
            }
            else if (left_assist->detections.smallest_detection <= right_assist->detections.smallest_detection) {
                std::cout << "OBS CENTER LEFT" << std::endl;
                obstacle.type = OBS_LEFT_CENTER;
            }
            else {
                std::cout << "OBS CENTER RIGHT" << std::endl;
                obstacle.type = OBS_RIGHT_CENTER;
            }
        } else if (center_assist && left_assist) {
            obstacle.type = OBS_LEFT_CENTER;
        } else if (center_assist && right_assist) {
            obstacle.type = OBS_RIGHT_CENTER;
        } else if (center_assist) {
            obstacle.type = OBS_CENTER;
        } else if (left_assist) {
            obstacle.type = OBS_LEFT;
        } else if (right_assist) {
            obstacle.type = OBS_RIGHT;
        }
        this->monitor_map.at(delay_type) = obstacle;
    }
    else {
        this->resetDetections(delay_type);
    }
    std::cout << "End of obstacleContactDir()" << std::endl;
}



/**
 * Iterates of the passed structure and verifies detection are of acceptable range.
 * Note: Acceptable values are messured against 'DELTA' (calculated max dist. the rover can cover in 1 sec.)
 * @param assistant : the passed obstacle assistant (type, detections, monitor)
 * @param delay : which monitor are we editing
 */
void ObstacleController::sonarAnalysis(DELAY_TYPE delay_type) {
    OBSTACLE obstacle = this->monitor_map.at(delay_type);
    for (auto assistant : obstacle.sonar_map) {
        float prev;
        float curr;
        bool has_begun = false;
        SONAR sonar = assistant.second.sonar;
        obstacle.sonar_map.at(sonar).detections.good_detection = false;
        // Check to make sure the passed monitor has been populated
//        if (!assistant.second.monitor->empty()) {
        if (assistant.second.detections.init_detection) {
            for (auto detection_range : *assistant.second.monitor) {
                if (!has_begun) {
                    curr = detection_range;
                    has_begun = true;
                }
                else {
                    prev = curr;
                    curr = detection_range;
                    double diff = std::fabs(prev - curr);
                    if (diff < DELTA) {
                        obstacle.sonar_map.at(sonar).detections.good_detection = true;
                        obstacle.sonar_map.at(sonar).detections.smallest_detection = curr;
                    }
                    else {
                        obstacle.sonar_map.at(sonar).detections.good_detection = false;
                    }
                }
            }
        }
        this->monitor_map.at(delay_type) = obstacle;
    }
}

/**
 * Adds passed values of sonar detections to respective structures, then checks
 * to make sure structure doesn't exceed specified size 'VECTOR_MAX'
 * @param obstacle : the entire obstacle structure (INIT, STAG, etc) <-- if more are added
 * @param range : the distance of the respective detection
 * @param sonar : which sensor are we referencing
 */
void ObstacleController::sonarMonitor(DELAY_TYPE delay_type, float range, SONAR sonar) {
    OBSTACLE obstacle = this->monitor_map.at(delay_type);
    if (obstacle.allowed) {
        obstacle.sonar_map.at(sonar).monitor->push_back(range);
        if (obstacle.sonar_map.at(sonar).monitor->size() >= VECTOR_MAX) {
            obstacle.sonar_map.at(sonar).detections.init_detection = true;
        }
        else {
            obstacle.sonar_map.at(sonar).detections.init_detection = false;
        }
        this->monitor_map.at(delay_type) = obstacle; // Assign new values
    }
}

/**
 * Reset the ObstacleAssistant structure when no good detection are made
 * @param delay_type : which 'assistant' is being passed
 */
void ObstacleController::resetObstacle(DELAY_TYPE delay_type) {
    this->monitor_map.at(delay_type).type = NO_OBSTACLE;
    this->monitor_map.at(delay_type).sonar_map = {
            {LEFT, ObstacleAssistant(LEFT)},
            {CENTER, ObstacleAssistant(CENTER)},
            {RIGHT, ObstacleAssistant(RIGHT)}
    };
}

/**
 * Each iteration through should have to recalc detection.
 * Final detection should be checked if it is allowed to end.
 */
void ObstacleController::resetDetections(DELAY_TYPE delay_type) {
    this->monitor_map.at(delay_type).type = NO_OBSTACLE;
    // Only reset main detection after previous reflection has finished
    if (this->reflection.can_end) {
        this->detection_declaration = NO_OBSTACLE;
        obstacleAvoided = true;
    }
}


