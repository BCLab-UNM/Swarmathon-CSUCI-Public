#include "PickUpController.h"
#include "Utilities.h"
#include <limits> // For numeric limits
#include <cmath> // For hypot

PickUpController::PickUpController() {
    lockTarget = false;
    timeOut = false;
    nTargetsSeen = 0;
    blockYawError = CAMERA_OFFSET_CORRECTION;
    blockDistance = 0;
    targetHeld = false;
    millTimer = 0;
    target_timer = 0;
    blockDistanceFromCamera = 0;
    blockBlock = false;
    current_time = 0;

    targetFound = false;

    result.type = precisionDriving;
    result.pd.cmdVel = 0;
    result.pd.cmdAngularError = 0;
    result.fingerAngle = -1;
    result.wristAngle = -1;
    result.PIDMode = SLOW_PID;

    this->controller = PICK_UP;
}

PickUpController::~PickUpController() { /*Destructor*/  }

void PickUpController::SetTagData(vector<Tag> tags) {

    if (!stop) {

        if (tags.size() > 0) {

            nTargetsSeen = tags.size();

            vector<Tag> candidate_tags;

            Tag chosen_tag;

            // Set to some small number, numeric_limits<double>::min() seems to fail

            double maxPriority = -1000000;

            int target_idx = -1;

            double closest_candidate_tag = std::numeric_limits<double>::max();

            int target = 0;

                //we saw a target, set target_timer

            target_timer = current_time;

            double closest = std::numeric_limits<double>::max();

            

            //this loop selects the closest visible block to makes goals for it

            for (int i = 0; i < tags.size(); i++) {

                if (tags[i].getID() == 0) {

                    // double distance = hypot(hypot(tags[target].getPositionX(), tags[target].getPositionY()),

                    //                         tags[target].getPositionZ());

                    // double priority = PickUpController::calculateTargetPriority( tags[i].getPositionX() + PickUpController::CAMERA_OFFSET_CORRECTION, distance);

                    if(fabs(tags[i].getPositionX())<0.25){

                        candidate_tags.push_back(tags[i]);

                    }

                     double test = hypot(hypot(tags[i].getPositionX(), tags[i].getPositionY()), tags[i].getPositionZ());

                    if (closest > test)

                    {

                      target = i;

                      closest = test;

                    }

                    

                    /*if (priority > maxPriority) {

                        target = i;

                        maxPriority = priority;

                    }*/

                } else {

                // If the center is seen, then don't try to pick up the cube.

                    if (tags[i].getID() == 256) {

                        stop = true;

                        std::cout << "PICK UP: stop listening to tag data" << std::endl;

                        Reset();

                        if (has_control) {

                            cout << "pickup reset return interupt free" << endl;

                            release_control = true;

                        }

                        return;

//                    if (tags[i].getID() == 256) {

//

//                    if (closest > test) {

//                        target = i;

//                        closest = test;

//                    }

//                } else {

//                    // If the center is seen, then don't try to pick up the cube.

//

                    }

                }

            }

            if(candidate_tags.size() > 0) {

                //this loop selects the closest visible block that is near the center of the screen to makes goals for it

                for ( int i = 0; i < candidate_tags.size(); i++ ) {

                    double current_tag_distance = hypot(hypot(candidate_tags[i].getPositionX(), candidate_tags[i].getPositionY()), candidate_tags[i].getPositionZ());

                    if (closest_candidate_tag > current_tag_distance)

                    {

                      target_idx = i;

                      closest_candidate_tag = current_tag_distance;

                    }

                    targetFound = true;

                    chosen_tag = tags[target_idx];

                }    

            } else {

                chosen_tag = tags[target];

                targetFound = true;

            }

            

            // using a^2 + b^2 = c^2 to find the distance to the block

            // 0.195 is the height of the camera lens above the ground in cm.

            //

            // a is the linear distance from the robot to the block, c is the

            // distance from the camera lens, and b is the height of the

            // camera above the ground.

            /*blockDistanceFromCamera = hypot(hypot(tags[target].getPositionX(), tags[target].getPositionY()),

                                            tags[target].getPositionZ());*/

            blockDistanceFromCamera = hypot(hypot(chosen_tag.getPositionX(), chosen_tag.getPositionY()),

                                            chosen_tag.getPositionZ());

            if ((blockDistanceFromCamera * blockDistanceFromCamera - 0.195 * 0.195) > 0) {

                blockDistance = sqrt(blockDistanceFromCamera * blockDistanceFromCamera - 0.195 * 0.195);

            } else {

                float epsilon = 0.00001; // A small non-zero positive number

                blockDistance = epsilon;

            }

            //cout << "blockDistance  TAGDATA:  " << blockDistance << endl;

            /*blockYawError =

                atan((tags[target].getPositionX() + PickUpController::CAMERA_OFFSET_CORRECTION) / blockDistance) *

                1.05;*/ //angle to block from bottom center of chassis on the horizontal.

            blockYawError = CAMERA_OFFSET_CORRECTION + chosen_tag.getPositionX();/*

                atan((tags[target_idx].getPositionX() + PickUpController::CAMERA_OFFSET_CORRECTION) / blockDistance) *

                1.05;*/

            // cout << "Setting blockYawError TAGDATA::::::::::::::::::::  " << blockYawError << endl;

        }

    }

    else if (spins == IGNORE_TAGS) {

        stop = false;

        std::cout << "PICK UP: listen to tag data" << std::endl;

    }

    else {

        spins++;

        std::cout << "PICK UP: increment spins" << std::endl;

    }

}


bool PickUpController::SetSonarData(float rangeCenter) {
    // If the center ultrasound sensor is blocked by a very close
    // object, then a cube has been successfully lifted.
    if (rangeCenter < 0.12 && targetFound) {
        result.type = behavior;
        result.behaviourType = nextProcess;
        result.reset = true;
        targetHeld = true;
        return true;
    }

    return false;

}

void PickUpController::ProcessData() {

    if (!targetFound) {
        //cout << "PICKUP No Target Seen!" << endl;

        // Do nothing
        return;
    }

    //diffrence between current time and millisecond time
    long int Tdiff = current_time - millTimer;
    float Td = Tdiff / 1e3;

    //cout << "PICKUP Target Seen!" << endl;

    //cout << "distance : " << blockDistanceFromCamera << " time is : " << Td << endl;

    // If the block is very close to the camera then the robot has
    // successfully lifted a target. Enter the target held state to
    // return to the center.
    if (blockDistanceFromCamera < 0.14 && Td < 3.9) {
        result.type = behavior;
        result.behaviourType = nextProcess;
        result.reset = true;
        targetHeld = true;
    }
        //Lower wrist and open fingers if no locked target -- this is the
        //case if the robot lost tracking, or missed the cube when
        //attempting to pick it up.
    else if (!lockTarget) {
        //set gripper;
        result.fingerAngle = M_PI_2;
        result.wristAngle = 1.25;
    }
}


bool PickUpController::ShouldInterrupt() {

    ProcessData();

    // saw center tags, so don't try to pick up the cube.
    if (release_control) {
        release_control = false;
        has_control = false;
        return true;
    }

    if ((targetFound && !interupted) || targetHeld) {
        interupted = true;
        has_control = false;
        return true;
    } else if (!targetFound && interupted) {
        // had a cube in sight but lost it, interrupt again to release control
        interupted = false;
        has_control = false;
        return true;
    } else {
        return false;
    }
}

Result PickUpController::DoWork() {

    logicMessage(current_time, ClassName, __func__);

    has_control = true;

    string message = "Starting PickUp Routine.";

    logMessage(current_time, "PICKUP", message);

    if (!targetHeld) {

        //threshold distance to be from the target block before attempting pickup

        float targetDistance = 0.15; //meters

        // -----------------------------------------------------------

        // millisecond time = current time if not in a counting state

        //     when timeOut is true, we are counting towards a time out

        //     when timeOut is false, we are not counting towards a time out

        //

        // In summary, when timeOut is true, the robot is executing a pre-programmed time-based block pickup

        // I routine. <(@.@)/"

        // !!!!! AND/OR !!!!!

        // The robot has started a timer so it doesn't get stuck trying to pick up a cube that doesn't exist.

        //

        // If the robot does not see a block in its camera view within the time out period, the pickup routine

        // is considered to have FAILED.

        //

        // During the pre-programmed pickup routine, a current value of "Td" is used to progress through

        // the routine. "Td" is defined below...

        // -----------------------------------------------------------

        if (!timeOut) millTimer = current_time;

        //difference between current time and millisecond time

        long int Tdifference = current_time - millTimer;

        // converts from a millisecond difference to a second difference

        // Td = [T]ime [D]ifference IN SECONDS

        float Td = Tdifference / 1e3;

        // The following nested if statement implements a time based pickup routine.

        // The sequence of events is:

        // 1. Target aquisition phase: Align the robot with the closest visible target cube, if near enough to get a target lock then start the pickup timer (Td)

        // 2. Approach Target phase: until *grasp_time_begin* seconds

        // 3. Stop and close fingers (hopefully on a block - we won't be able to see it remember): at *grasp_time_begin* seconds

        // 4. Raise the gripper - does the rover see a block or did it miss it: at *raise_time_begin* seconds

        // 5. If we get here the target was not seen in the robots gripper so drive backwards and and try to get a new lock on a target: at *target_require_begin* seconds

        // 6. If we get here we give up and release control with a task failed flag: for *target_pickup_task_time_limit* seconds

        // If we don't see any blocks or cubes turn towards the location of the last cube we saw.

        // I.E., try to re-aquire the last cube we saw.

        float grasp_time_begin = 1.5;

        float raise_time_begin = 2.0;

        float lower_gripper_time_begin = 4.0;

        float target_reaquire_begin = 4.2;

        float target_pickup_task_time_limit = 4.8;

        //cout << "blockDistance DOWORK:  " << blockDistance << endl;

        //Calculate time difference between last seen tag

        float target_timeout = (current_time - target_timer) / 1e3;

        //delay between the camera refresh and rover runtime is 6/10's of a second

        float target_timeout_limit = 0.61;

        //Timer to deal with delay in refresh from camera and the runtime of rover code

        if (target_timeout >= target_timeout_limit) {

            //Has to be set back to 0

            nTargetsSeen = 0;

        }

        if (nTargetsSeen == 0 && !lockTarget) {

            // This if statement causes us to time out if we don't re-aquire a block within the time limit.

            if (!timeOut) {

                result.pd.cmdVel = 0.0;

                result.wristAngle = 1.25;

                // result.fingerAngle does not need to be set here

                string message = "Attempting to PickUp resource";

                logMessage(current_time, "PICKUP", message);

                // We are getting ready to start the pre-programmed pickup routine now! Maybe? <(^_^)/"

                // This is a guard against being stuck permanently trying to pick up something that doesn't exist.

                timeOut = true;

                // Rotate towards the block that we are seeing.

                // The error is negated in order to turn in a way that minimizes error.

                result.pd.cmdAngularError = 0.0;//-blockYawError; // We shouldn't have a valid blockYawError if nTargetsSeen==0.

            }

                //If in a counting state and has been counting for 1 second.

            else if (Td > 1.0 && Td < target_pickup_task_time_limit) {

                // The rover will reverse straight backwards without turning.

                result.pd.cmdVel = -0.15;

                result.pd.cmdAngularError = 0.0;

            }

        } else if (blockDistance > targetDistance &&

                   !lockTarget) //if a target is detected but not locked, and not too close.

        {

            // this is a 3-line P controller, where Kp = 0.20

            float vel = blockDistance * 0.20;

            if (vel < 0.1) vel = 0.1;

            if (vel > 0.2) vel = 0.2;

            result.pd.cmdVel = 0.1; // was vel

            timeOut = false;

            if(fabs(blockYawError) < 0.02) {

                result.pd.cmdAngularError = 0.0;     

            } else {

                cout << "Using blockYawError DOWORK::::::::::::::::::::  " << blockYawError << endl;
                result.pd.cmdAngularError = -2.0*blockYawError; // was cmdAngular not sure which to use.

            }

            return result;

        } else if (!lockTarget) //if a target hasn't been locked lock it and enter a counting state while slowly driving forward.

        {

            lockTarget = true;

            //result.pd.cmdVel = 0.18;
            //result.pd.cmdAngularError = 0.0;
	    // try distance driving
	    result.type = distance_driving;
	    
            timeOut = true;

            ignoreCenterSonar = true;

        } else if (Td > raise_time_begin) //raise the wrist

        {

            result.pd.cmdVel = -0.15; // was -0.15

            result.pd.cmdAngularError = 0.0;

            result.wristAngle = 0;

            //set a flag for the last cube found

            Point cubeLocation = currentLocation;

            cubeLocation.x + .2*cos(currentLocation.theta);

            cubeLocation.y + .2*sin(currentLocation.theta);

            SetLastCubeLocation(cubeLocation);

        } else if (Td > grasp_time_begin) //close the fingers and stop driving

        {

            result.pd.cmdVel = 0.0;

            result.pd.cmdAngularError = 0.0;

            result.fingerAngle = 0;

            return result;

        }

        // the magic numbers compared to Td must be in order from greater(top) to smaller(bottom) numbers

        if (Td > target_reaquire_begin && timeOut) {

            lockTarget = false;

            ignoreCenterSonar = true;

        }

            //if enough time has passed enter a recovery state to re-attempt a pickup

        else if (Td > lower_gripper_time_begin && timeOut) {

            result.pd.cmdVel = -0.15;

            result.pd.cmdAngularError = 0.0;

            //set gripper to open and down

            result.fingerAngle = M_PI_2;

            result.wristAngle = 0;

            string message = "Re-Attempting to PickUp resource";

            logMessage(current_time, "PICKUP", message);

        }

        //if no targets are found after too long a period go back to search pattern

        if (Td > target_pickup_task_time_limit && timeOut) {

            Reset();

            interupted = true;

            // NOTE this is a bug!!

            //result.pd.cmdVel = 0.0;

            //result.pd.cmdAngularError = 0.0;

            //ignoreCenterSonar = true;

            string message = "Exiting PickUp. Returning to Search.";

            logMessage(current_time, "PICKUP", message);

        }

    }

    return result;

}

bool PickUpController::HasWork() {
    return targetFound;
}

void PickUpController::Reset() {

    result.type = precisionDriving;
    result.PIDMode = SLOW_PID;
    lockTarget = false;
    timeOut = false;
    nTargetsSeen = 0;
    blockYawError = CAMERA_OFFSET_CORRECTION;
    blockDistance = 0;

    targetFound = false;
    interupted = false;
    targetHeld = false;

    result.pd.cmdVel = 0;
    result.pd.cmdAngularError = 0;
    result.fingerAngle = -1;
    result.wristAngle = -1;
    result.reset = false;

    ignoreCenterSonar = false;
    blockYawError = 0.0;
}

void PickUpController::SetUltraSoundData(bool blockBlock) {
    this->blockBlock = blockBlock;
}

void PickUpController::SetCurrentTimeInMilliSecs(long int time) {
    current_time = time;
}

double PickUpController::calculateTargetPriority(double xPos, double distance) {
    // How much to weight each value when computing the priority
    // These only have significance relative to each other
    static const double xPosWt = 1;
    static const double dstWt = .5;
    // Absolute distance to block x from center of camera FOV
    double absXPos = fabs(xPos);

    return xPosWt * -absXPos + dstWt * -distance;
}

void PickUpController::SetCurrentLocation(Point current) {
    this->currentLocation = current;
}
