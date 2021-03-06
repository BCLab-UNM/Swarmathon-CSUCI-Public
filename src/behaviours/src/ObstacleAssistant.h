//
// Created by student on 2/7/18.
//

#ifndef PROJECT_OBSTACLEASSISTANT_H
#define PROJECT_OBSTACLEASSISTANT_H

#define START_STAG 2

#define DEFAULT_RANGE 10

/*
 * Type of declared obstacle
 */
typedef enum {
    NO_OBSTACLE = 0,
    OBS_LEFT,
    OBS_CENTER,
    OBS_RIGHT,
    OBS_LEFT_CENTER,
    OBS_RIGHT_CENTER,
    HOME
} OBS_TYPE;

/*
 * The two type of monitors
 */
typedef enum {
    INIT = 0,
    STAG
} DELAY_TYPE;

/*
 * Sonar keys to be referenced in the 'sonar_map'
 */
typedef enum {
    LEFT = 0,
    CENTER,
    RIGHT
} SONAR;

class ObstacleAssistant {
private:
    typedef struct {
        bool init_detection;
        bool good_detection;
        double smallest_detection;
    } DETECTIONS;

public:
    SONAR sonar;
    DETECTIONS detections;
    std::vector<float> *monitor;

    // TODO: maybe need make not explicit
    explicit ObstacleAssistant(SONAR s) :
            sonar(s),
            monitor(new std::vector<float>),
            detections({false, false, DEFAULT_RANGE}) {};

};


#endif //PROJECT_OBSTACLEASSISTANT_H
