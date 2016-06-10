
// reasonably generic utility functions
// put function prototypes in util.h

#include "defines.h"
#include "eeprom.h"
#include "utility/crc32.h"
#include "DAC.h"
#include "util.h"
#include "serial.h"

//apply the calibration values for magnetometer from EEPROM
void applyMagCal(float * arr) {

  arr[0] -= eeprom->mag_bias[0];
  arr[1] -= eeprom->mag_bias[1];
  arr[2] -= eeprom->mag_bias[2];


  float tempY = arr[0] * eeprom->mag_cal[0][0] + arr[1] * eeprom->mag_cal[0][1] + arr[2] * eeprom->mag_cal[0][2];
  float tempX = arr[0] * eeprom->mag_cal[1][0] + arr[1] * eeprom->mag_cal[1][1] + arr[2] * eeprom->mag_cal[1][2];
  float tempZ = arr[0] * eeprom->mag_cal[2][0] + arr[1] * eeprom->mag_cal[2][1] + arr[2] * eeprom->mag_cal[2][2];

  arr[0] = tempX;
  arr[1] = tempY;
  arr[2] = tempZ;

  //arr[1] *= -1;
  arr[0] *= -1;
  arr[2] *= -1;
}

void applyAccCal(int * arr) {
  //arr[1] *= -1;
  arr[2] *= -1;
}

void rad_to_deg(float* roll, float* pitch, float* compass) {
  *roll *= 180 / PI;
  *pitch *= 180 / PI;
  *compass *= 180 / PI;
}

//return compass heading (RADIANS) given pitch, roll and magentotmeter measurements
float getCompass(const float magX, const float magY, const float magZ, const float & pitch, const float & roll) {

  float negBfy = magZ * sine_internal(roll) - magY * cosine_internal(roll);
  float Bfx = magX * cosine_internal(pitch) + magY * sine_internal(roll) * sine_internal(pitch) + magZ * sine_internal(pitch) * cosine_internal(roll);
  float compass = atan2(negBfy, Bfx);

  compass += PI / 2;

  if (compass < 0) {
    compass += 2 * PI;
  }
  if (compass > 2 * PI) {
    compass -= 2 * PI;
  }


  return compass;
}

//return roll (RADIANS) from accelerometer measurements
float getRoll(const int accelY, const int accelZ) {
  return atan2(accelY, accelZ);
}

//return pitch (RADIANS) from accelerometer measurements + roll
float getPitch(const int accelX, const int accelY, const int accelZ, const float & roll) {

  return atan(-1 * accelX / (accelY * sine_internal(roll) + accelZ * cosine_internal(roll)));

}

// return 0-7 based on 8 segments of the compass

int compass_segment(float angle)    // in degrees, assume no negatives
{
  return (round( angle / 45) % 8);
}

//get the direction (N/S/E/W/NW/NE/SW/SE) from the compass heading
const char * getDirection(int segment) {

  if (segment > 7 || segment < 0) {
    return "\"Invalid compass segment\"";
  }

  const char *names[] = {"\"N\"", "\"NE\"", "\"E\"", "\"SE\"", "\"S\"", "\"SW\"", "\"W\"", "\"NW\""};

  return names[segment];
}

//calculate tilt angle and tilt direction given roll, pitch, compass heading
Tilt calculateTilt(float roll, float pitch, float compass) {

  Tilt deviceTilt;

  //equation derived from rotation matricies in AN4248 by Freescale
  float a = (cosine_internal(roll) * cosine_internal(pitch));
  float b = sqrt((sine_internal(roll) * sine_internal(roll) + (sine_internal(pitch) * sine_internal(pitch) * cosine_internal(roll) * cosine_internal(roll))));
  deviceTilt.angle = atan2(a, b);

  deviceTilt.angle *= 180 / PI;

  deviceTilt.angle  = 90 - deviceTilt.angle;

  compass *= 180 / PI;

  if (0 >= compass || compass >= 360) {
    deviceTilt.angle_direction = "\"Invalid compass heading\"";
  }

  float tilt_angle = atan2(-1 * sine_internal(roll), cosine_internal(roll) * sine_internal(pitch));

  tilt_angle *= 180 / PI;

  if (tilt_angle < 0) {
    tilt_angle += 360;
  }


  int tilt_segment = compass_segment(tilt_angle);

  int comp_segment = compass_segment(compass) + tilt_segment + 2;
  comp_segment = comp_segment % 8;

  deviceTilt.angle_direction = getDirection(comp_segment);

  return deviceTilt;

}

//Internal sine calculation in RADIANS
float sine_internal(float angle) {
  if (angle > PI) {
    angle = 2 * PI - angle;
  }

  if ( angle < -1 * PI) {
    angle = 2 * PI + angle;
  }

  return angle - angle * angle * angle / 6 +
         angle * angle * angle * angle * angle / 120 -
         angle * angle * angle * angle * angle * angle * angle / 5040 +
         angle * angle * angle * angle * angle * angle * angle * angle * angle / 362880;
}

//Internal cosine calculation in RADIANS
float cosine_internal(float angle) {
  if (angle > PI) {
    angle = 2 * PI - angle;
  }

  if ( angle < -1 * PI) {
    angle = 2 * PI + angle;
  }

  return 1 - angle * angle / 2 + angle * angle * angle * angle / 24 -
         angle * angle * angle * angle * angle * angle  / 720 +
         angle * angle * angle * angle * angle * angle * angle * angle / 40320 -
         angle * angle * angle * angle * angle * angle * angle * angle * angle * angle / 3628800;
}
//this arctan approximation only works for -pi/4 to pi/4 - can be modified for that to work, but atan2 and atan
//only takes up <2kb, if we need the space I'll fix it but otherwise I'll leave the originals in place
/*
  //Internal arctangent calculation in RADIANS
  float arctan_internal(float x, float y){
  float small, large;
  int sign = 1;
  if((x < 0 && y > 0) || (x > 0 && y < 0)){
    sign *= -1;
    (x < 0) ? x *= -1 : y *= -1;
  }

  large = float_max(x, y);
  small = float_min(x, y);

  float angle = small / large;

  return PI / 4 * angle - angle * (angle - 1) * (0.2447 + 0.0663 * angle);
  }

  float float_max(float x, float y){
  return (x > y) ? x : y;
  }

  float float_min(float x, float y){
  return (x < y) ? x : y;
  }
*/

float measure_hall() {
  float hall_value = (analogRead(HALL_OUT) + analogRead(HALL_OUT) + analogRead(HALL_OUT)) / 3;
  //  Serial_Printf("final hall_value: %f",hall_value);
  return hall_value;
}

void start_on_open_close() {
  // take an initial measurement as a baseline (closed position)
  for (uint16_t i = 0; i < 10; i++) {                            // throw away values to make sure the first value is correct
    measure_hall();
  }
  float start_position = measure_hall();
  float current_position = start_position;

  // now measure every 200ms until you see the value change to > 10000 counts
  while (current_position - start_position < 8000) {
    current_position = measure_hall();
    //        Serial_Printf("start: %f, current: %f\n", start_position, current_position);
    delay(200);                                                               // measure every 100ms
    if (current_position - start_position < -2000) {                              // if the person opened it first (ie they did it wrong and started it with clamp open) - detect and skip to end
      //        Serial_Print("made it");
      goto end;
    }
  }
  // now measure again every 200ms until you see the value change to < 5000 counts
  while (current_position  - start_position > 6000) {
    current_position = measure_hall();
    //        Serial_Printf("start: %f, current: %f\n", start_position, current_position);
    delay(200);                                                               // measure every 200ms
  }
end:
  delay(500);                                                               // make sure the clamp has time to settle onto the leaf.
}

