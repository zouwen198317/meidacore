#include "log/log.h"
#include "ipcopcode.h"

void initAviAmfWriter();
void uninitAviAmfWriter();
void generateGPSBlk(gpsData_t * gps);
void generateGSensorBlk(gSensorData_t* gsensor);
void generateOtherSensorBlk(otherSensorData_t* otherSensor);



