#include "DriveController.h"

DriveController::DriveController() {

    fastVelPID.SetConfiguration(fastVelConfig());
    fastYawPID.SetConfiguration(fastYawConfig());

    slowVelPID.SetConfiguration(slowVelConfig());
    slowYawPID.SetConfiguration(slowYawConfig());

    constVelPID.SetConfiguration(constVelConfig());
    constYawPID.SetConfiguration(constYawConfig());




}

DriveController::~DriveController() {}



void DriveController::SetCurrentTimeInMilliSecs( long int time )
{
  current_time = time;
}


void DriveController::Reset()
{
  waypoints.clear();

    if (stateMachineState == STATE_MACHINE_ROTATE || stateMachineState == STATE_MACHINE_SKID_STEER) {
        stateMachineState = STATE_MACHINE_WAYPOINTS;
    }
}



Result DriveController::DoWork()
{
  
  ///WARNING waypoint input must use FAST_PID at this point in time failure to set fast pid will result in no movment

    ///WARNING waypoint input must use FAST_PID at this point in time failure to set fast pid will result in no movment
    if (result.type == behavior) {
        if (result.behaviourType == noChange) {
            //if drive controller gets a no change command it is allowed to continue its previous action
            //normally this will be to follow waypoints but it is not specified as such.
        } else if (result.behaviourType == wait) {
            //do nothing till told otherwise
            left = 0.0;
            right = 0.0;
            stateMachineState = STATE_MACHINE_WAITING;
        }

    } else if (result.type == precisionDriving) {

        //interpret input result as a precision driving command
        stateMachineState = STATE_MACHINE_PRECISION_DRIVING;
        logicMessage(current_time, ClassName, "Precision Driving");

    } else if (result.type == waypoint) {
        //interpret input result as new waypoints to add into the queue
        Point lastLocation = GetLastCubeLocation();
        if (lastLocation.x == 0 && lastLocation.y == 0) //check if the waypoint was erased, and look around you
        {
            cout << "no known last location, clearing waypoints and going to vector " << endl;
            //add waypoints around you using theta%something or set a boolean lock so that it only happens once when a value for lastLocaiotn is set once
            spun = false;
            result.waypoints.clear();
            result.type = vectorDriving;
        }
        ProcessData();

    } else if (result.type == vectorDriving) {
        ProcessData();
    }

    switch (stateMachineState) {

        //Handlers and the final state of STATE_MACHINE are the only parts allowed to call INTERUPT
        //This should be d one as little as possible. I suggest using timeouts to set control bools to false.
        //Then only call INTERUPT if bool switches to true.
        case STATE_MACHINE_PRECISION_DRIVING: {
            logicMessage(current_time, ClassName, "Precision Driving");
            ProcessData();
            break;
        }


        case STATE_MACHINE_WAYPOINTS: {
            logicMessage(current_time, ClassName, "Waypoint Driving");

            //Handles route planning and navigation as well as making sure all waypoints are valid.

            bool tooClose = true;
            //while we have waypoints and they are tooClose to drive to



            while (!waypoints.empty() && tooClose) {
                //check next waypoint for distance
                if (distance_between_points(waypoints.back(),currentLocation) < waypointTolerance) {
                    //if too close remove it
                    Point lastLocation = GetLastCubeLocation();
                    if (distance_between_points(lastLocation, currentLocation) < waypointTolerance) {
                            lastLocation.x = 0;
                            lastLocation.y = 0;
                            SetLastCubeLocation(lastLocation);
                            cout << "reset the lastLocation " << endl;
                    }
                    waypoints.pop_back();
                } else {
                    //this waypoint is far enough to be worth driving to
                    tooClose = false;
                }
            }

            //if we are out of waypoints then interupt and return to logic controller
            if (waypoints.empty()) {
                cout << " waypoints are empty " << endl;
                stateMachineState = STATE_MACHINE_WAITING;
                result.type = behavior;
                interupt = true;
                return result;
            } else {
                //select setpoint for heading and begin driving to the next waypoint

                stateMachineState = STATE_MACHINE_ROTATE;
                waypoints.back().theta = angle_between_points(waypoints.back(),currentLocation);
                result.pd.setPointYaw = waypoints.back().theta;

                //cout << "**************************************************************************" << endl; //DEBUGGING CODE
                //cout << "Waypoint x : " << waypoints.back().x << " y : " << waypoints.back().y << endl; //DEBUGGING CODE
                //fall through on purpose
            }
        }

        case STATE_MACHINE_ROTATE: {

            // Calculate angle between currentLocation.theta and waypoints.front().theta
            // Rotate left or right depending on sign of angle
            // Stay in this state until angle is minimized
            // printf("rotate\n");
            if (result.type == vectorDriving) {
                float errorYaw = angles::shortest_angular_distance(result.desired_heading, currentLocation.theta);
                result.pd.setPointVel = 0.0;
                float abs_error = fabs(errorYaw);
                if (abs_error > .5) {
                    // rotate but dont drive.
                    outputValidation(velPID(fastVelPID, 0, 0), yawPID(constYawPID, errorYaw, result.desired_heading));
                    break;
                } else {
                    stateMachineState = STATE_MACHINE_SKID_STEER;
                }
                //fall through on purpose.
            } else {
                // Calculate the diffrence between current and desired heading in radians.
                float errorYaw = difference_between_angles(currentLocation,waypoints.back());
                waypoints.back().theta = angle_between_points(waypoints.back(),currentLocation);

                // Calculate the diffrence between current and desired heading in radians.
                // cout << "ROTATE Error yaw:  " << errorYaw << " target heading : " << waypoints.back().theta << " current heading : " << currentLocation.theta << endl; //DEBUGGING CODE
                cout << "Waypoint x : " << waypoints.back().x << " y : " << waypoints.back().y << " currentLoc x : " << currentLocation.x << " y : " << currentLocation.y << endl; //DEBUGGING CODE

                result.pd.setPointVel = 0.0;
                //Calculate absolute value of angle

                float abs_error = fabs(errorYaw);


                // If angle > rotateOnlyAngleTolerance radians rotate but dont drive forward.
                if (abs_error > .2) {
                    rotationTimeBreak++;
                    // rotate but dont drive.
                    outputValidation(velPID(fastVelPID, 0.0, result.pd.setPointVel), yawPID(fastYawPID, errorYaw, result.pd.setPointYaw));
                    if (rotationTimeBreak >= 70) {
                        stateMachineState = STATE_MACHINE_SKID_STEER;
                    }
                    break;
                } else {
                    //move to differential drive step
                    stateMachineState = STATE_MACHINE_SKID_STEER;
                    rotationTimeBreak = 0;
                    //fall through on purpose.
                }
            }
        }

        case STATE_MACHINE_SKID_STEER: {
            // Calculate angle between currentLocation.x/y and waypoints.back().x/y
            // Drive forward
            // Stay in this state until angle is at least PI/2

            // calculate the distance between current and desired heading in radians
            if (result.type == vectorDriving) {
                //fastPID(.3,0,5,0);
                //outputValidation(velPID(fastVelPID,.3,5),yawPID(fastYawPID,0,0));
                outputValidation(90,0.0); // just floor it.
                break;
            } else if (result.type == distance_driving) {
	      // DRIVE some distance
	      cout << ",.~'`'~., DISTANCE DRIVING ,.~'`'~.," << endl;
	      if(result.distance_driving_values.start_odom.x == 0.0 && result.distance_driving_values.start_odom.y == 0.0) {
		cout << ",.~'`'~.,.~'`'~.,.~'`'~. FIRST TIME DISTANCE DRIVING ,.~'`'~.,.~'`'~.,.~'`'~.,.~'`'~." << endl;
		result.distance_driving_values.start_odom.x = currentLocation.x; // which is odom
		result.distance_driving_values.start_odom.y = currentLocation.y;
		cout << currentLocation.x << "," << currentLocation.y << endl;
	      } else {
		float distance = distance_between_points(result.distance_driving_values.start_odom, currentLocation);
		result.distance_driving_values.completed_distance += distance;
		cout << ",.~'`'~.,.~'`'~.,.~'`'~. GONE: " << distance << ",.~'`'~.,.~'`'~.,.~'`'~.,.~'`'~." << endl;
		if(result.distance_driving_values.completed_distance > 3.0) { // > result.distance_driving_values.desired_distance TODO
		  break;
		  cout << ",.~'`'~.,.~'`'~.,.~'`'~. DONEZOOOOOOOO ,.~'`'~.,.~'`'~.,.~'`'~.,.~'`'~." << endl;
		} else {
		  outputValidation(90,0.0); // FIDLAR
		}
	      }
	      break;
            } else {
                waypoints.back().theta = angle_between_points(waypoints.back(), currentLocation);
                float errorYaw = difference_between_angles(currentLocation, waypoints.back());
                float distance = distance_between_points(waypoints.back(), currentLocation);



                //cout << "Skid steer, Error yaw:  " << errorYaw << " target heading : " << waypoints.back().theta << " current heading : " << currentLocation.theta << " error distance : " << distance << endl; //DEBUGGING CODE
                //cout << "Waypoint x : " << waypoints.back().x << " y : " << waypoints.back().y << " currentLoc x : " << currentLocation.x << " y : " << currentLocation.y << endl; //DEBUGGING CODE

                    // goal not yet reached drive while maintaining proper heading.
                if (fabs(errorYaw) < M_PI_2 && distance > waypointTolerance) {
                    // drive and turn simultaniously
                    result.pd.setPointVel = searchVelocity;
                        //cout << "linear velocity:  " << linearVelocity << endl; //DEBUGGING CODE
                        outputValidation(velPID(fastVelPID, (searchVelocity - linearVelocity), result.pd.setPointVel), yawPID(fastYawPID, errorYaw, result.pd.setPointYaw));
                } else {
                    // stopno change
                    left = 0.0;
                    right = 0.0;
                        // move back to transform step
                    stateMachineState = STATE_MACHINE_WAYPOINTS;
                }
                break;
            }
            
        }

        default: {
            break;
        }


    }

    //package data for left and right values into result struct for use in ROSAdapter
    result.pd.right = right;
    result.pd.left = left;

    //return modified struct
    return result;

}

bool DriveController::ShouldInterrupt() {
    if (interupt) {
        interupt = false;
        return true;
    } else {
        return false;
    }
}

bool DriveController::HasWork() {}


void DriveController::ProcessData() {
    //determine if the drive commands are waypoint or precision driving
    if (result.type == waypoint) {

        //sets logic controller into stand by mode while drive controller works
        result.type = behavior;
        result.behaviourType = noChange;

        if (result.reset) {
            waypoints.clear();
        }

        //add waypoints onto stack and change state to start following them
        if (!result.waypoints.empty()) {
            waypoints.insert(waypoints.end(), result.waypoints.begin(), result.waypoints.end());
            stateMachineState = STATE_MACHINE_WAYPOINTS;
        }
    } else if (result.type == vectorDriving || result.type == distance_driving) {
        stateMachineState = STATE_MACHINE_SKID_STEER;

    } else if (result.type == precisionDriving) {

        //calculate inputs into the PIDS for precision driving
        if (result.PIDMode == FAST_PID) {
            float vel = result.pd.cmdVel - linearVelocity;
            float setVel = result.pd.cmdVel;
            outputValidation(velPID(fastVelPID, vel, setVel), yawPID(fastYawPID, result.pd.cmdAngularError, result.pd.setPointYaw));
        } else if (result.PIDMode == SLOW_PID) {
            //will take longer to reach the setPoint but has less chanse of an overshoot especially with slow feedback
            float vel = result.pd.cmdVel - linearVelocity;
            float setVel = result.pd.cmdVel;
            outputValidation(velPID(slowVelPID, vel, setVel), yawPID(slowYawPID, result.pd.cmdAngularError, result.pd.setPointYaw));
        } else if (result.PIDMode == CONST_PID) {
            //vel is the same as fast PID however
            //this setup takes a target angular velocity to constantly turn at instead of a target heading
            float vel = result.pd.cmdVel - linearVelocity;
            float angular = result.pd.cmdAngular - angularVelocity;

            //cout << "Ang. Vel.  " << angularVelocity << "  Ang. Error" << angular << endl; //DEBUGGING CODE

            outputValidation(velPID(constVelPID, vel, result.pd.setPointVel), yawPID(constYawPID, angular, result.pd.setPointYaw));
        }
    } 

}


float DriveController::velPID(PID &vpid, float errorVel, float setPointVel) {
    float velOut = vpid.PIDOut(errorVel, setPointVel);
    return velOut;
}


float DriveController::yawPID(PID &ypid, float errorYaw, float setPointYaw) {
    float yawOut = ypid.PIDOut(errorYaw, setPointYaw);
    return yawOut;
}


void DriveController::SetVelocityData(float linearVelocity, float angularVelocity) {
    this->linearVelocity = linearVelocity;
    this->angularVelocity = angularVelocity;
}


PIDConfig DriveController::fastVelConfig() {
    PIDConfig config;

    config.Kp = 60; //proportional constant
    config.Ki = 10; //integral constant
    config.Kd = 2; //derivative constant
    config.satUpper = 255; //upper limit for PID output
    config.satLower = -255; //lower limit for PID output
    config.antiWindup = config.satUpper; //prevent integral from acruing error untill proportional output drops below a certain limit
    config.errorHistLength = 4; //how many time steps to average error over
    config.alwaysIntegral = true; //should the integral alway be on or only when there is error
    config.resetOnSetpoint = true; //reset the integral and error history whent he setpoint changes
    config.feedForwardMultiplier = 610; //gives 127 pwm at 0.4 commandedspeed  ORIG:320
    config.integralDeadZone = 0.01; //set the integral dead zone, prevented integral from growing or shrinking do to noise
    config.integralErrorHistoryLength = 10000; //how many time ticks should error history should be stored for integration
    config.integralMax = config.satUpper / 2; //what is the limit of the integral output for the PID
    config.derivativeAlpha = 0.7; //dead code not used

    return config;

}

PIDConfig DriveController::fastYawConfig() {
    PIDConfig config;

    config.Kp = 60;
    config.Ki = 15;
    config.Kd = 5;
    config.satUpper = 255;
    config.satLower = -255;
    config.antiWindup = config.satUpper / 6;
    config.errorHistLength = 4;
    config.alwaysIntegral = false;
    config.resetOnSetpoint = true;
    config.feedForwardMultiplier = 0;
    config.integralDeadZone = 0.01;
    config.integralErrorHistoryLength = 10000;
    config.integralMax = config.satUpper / 3;
    config.derivativeAlpha = 0.7;

    return config;

}

PIDConfig DriveController::slowVelConfig() {
    PIDConfig config;

    config.Kp = 100;
    config.Ki = 8;
    config.Kd = 1.1;
    config.satUpper = 255;
    config.satLower = -255;
    config.antiWindup = config.satUpper / 2;
    config.errorHistLength = 4;
    config.alwaysIntegral = true;
    config.resetOnSetpoint = true;
    config.feedForwardMultiplier = 320; //gives 127 pwm at 0.4 commandedspeed
    config.integralDeadZone = 0.01;
    config.integralErrorHistoryLength = 10000;
    config.integralMax = config.satUpper / 2;
    config.derivativeAlpha = 0.7;

    return config;

}

PIDConfig DriveController::slowYawConfig() {
    PIDConfig config;

    config.Kp = 70;
    config.Ki = 16;
    config.Kd = 10;
    config.satUpper = 255;
    config.satLower = -255;
    config.antiWindup = config.satUpper / 4;
    config.errorHistLength = 4;
    config.alwaysIntegral = false;
    config.resetOnSetpoint = true;
    config.feedForwardMultiplier = 0;
    config.integralDeadZone = 0.01;
    config.integralErrorHistoryLength = 10000;
    config.integralMax = config.satUpper / 6;
    config.derivativeAlpha = 0.7;

    return config;

}

PIDConfig DriveController::constVelConfig() {
    PIDConfig config;

    config.Kp = 60;
    config.Ki = 10;
    config.Kd = 2;
    config.satUpper = 255;
    config.satLower = -255;
    config.antiWindup = config.satUpper;
    config.errorHistLength = 4;
    config.alwaysIntegral = true;
    config.resetOnSetpoint = true;
    config.feedForwardMultiplier = 610; //gives 127 pwm at 0.4 commandedspeed  ORIG:320
    config.integralDeadZone = 0.01;
    config.integralErrorHistoryLength = 10000;
    config.integralMax = config.satUpper / 2;
    config.derivativeAlpha = 0.7;

    return config;

}

PIDConfig DriveController::constYawConfig() {
    PIDConfig config;

    config.Kp = 5;
    config.Ki = 5;
    config.Kd = 0;
    config.satUpper = 255;
    config.satLower = -255;
    config.antiWindup = config.satUpper / 4;
    config.errorHistLength = 4;
    config.alwaysIntegral = true;
    config.resetOnSetpoint = true;
    config.feedForwardMultiplier = 0;
    config.integralDeadZone = 0.01;
    config.integralErrorHistoryLength = 10000;
    config.integralMax = config.satUpper;
    config.derivativeAlpha = 0.6;

    return config;

}

void DriveController::outputValidation(float velOut, float yawOut) {
    int left = velOut - yawOut; //combine yaw and vel PWM values
    int right = velOut + yawOut; //left and right are the same for vel output but opposite for yaw output

    //prevent combine output from going over tihs value
    int sat = 180;

    this->left = saturation_check(left,sat);
    this->right = saturation_check(right,sat);
    // cout << "LEFT: " << this->left << endl;
    // cout << "RIGHT " << this->right << endl;
}
