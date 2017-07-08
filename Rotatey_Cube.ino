/*
  
  3D Rotatey-Cube is placed under the MIT license
  
  Copyleft (c+) 2016 tobozzo 
  
  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  THE SOFTWARE.
  
  This project is heavily inspired from the work of GoblinJuicer https://www.reddit.com/user/GoblinJuicer 
  who developed and implemented the idea.
  Started from this sub https://www.reddit.com/r/arduino/comments/3vmw1k/ive_been_playing_with_a_gyroscope_and_an_lcd/
  The code is mainly a remix to make it work with u8glib and mpu6050

  Deps:
    U8glib library grabbed from: https://github.com/olikraus/u8glib
    I2Cdev library grabbed from: https://github.com/jrowberg/i2cdevlib/tree/master/Arduino/I2Cdev
    MPU650 library grabbed from: https://github.com/jrowberg/i2cdevlib/tree/master/Arduino/MPU6050
  
  Pinout OLED and MPU (shared)
    VCC => 5V
    GND => GND
    SCL => A5
    SDA => A4
  
  Pinout MPU
    AD0 => GND
    INT => D2

 */

#include "U8glib.h"
#include "MPU6050_6Axis_MotionApps20.h"
#if I2CDEV_IMPLEMENTATION == I2CDEV_ARDUINO_WIRE
    #include "Wire.h"
#endif

#define DEBUG false

// Accel and gyro data
int16_t  ax, ay, az, gx, gy, gz;
double MMPI = 1000*M_PI;

long int timeLast = -100, period = 1;
// Overall scale and perspective distance
uint8_t sZ = 4, scale = 12;
// screen center
uint8_t centerX = 64;
uint8_t centerY = 40;

// Initialize cube point arrays
const int TOTAL_POINTS = 12;
uint8_t P[TOTAL_POINTS][2];
double C[TOTAL_POINTS][3] = {
  // Cube
  {   1,    1,    1 },
  {   1,    1,   -1 },
  {   1,   -1,    1 },
  {   1,   -1,   -1 },
  {  -1,    1,    1 },
  {  -1,    1,   -1 },
  {  -1,   -1,    1 },
  {  -1,   -1,   -1 },
  // Y
  {   0,    0,    1 },
  { 0.5,  0.5,    1 },
  {-0.5,  0.5,    1 },
  {   0, -0.5,    1 }
};

// Initialize array of point indexes to use them as lines ends
const int TOTAL_LINES = 15;
uint8_t LINES[TOTAL_LINES][2] = {
                        // Cube
                        {0, 1},
                        {0, 2},
                        {0, 4},
                        {1, 3},
                        {1, 5},
                        {2, 3},
                        {2, 6},
                        {3, 7},
                        {4, 5},
                        {4, 6},
                        {5, 7},
                        {6, 7},
                        // Y
                        {8, 9},
                        {8, 10},
                        {8, 11}
                      };

U8GLIB_SH1106_128X64 u8g(U8G_I2C_OPT_NO_ACK);	// Display which does not send ACK
MPU6050 mpu;


void setup() {
  
  // assign default color value
  
  if ( u8g.getMode() == U8G_MODE_R3G3B2 ) {
    u8g.setColorIndex(255);     // white
  }
  else if ( u8g.getMode() == U8G_MODE_GRAY2BIT ) {
    u8g.setColorIndex(3);         // max intensity
  }
  else if ( u8g.getMode() == U8G_MODE_BW ) {
    u8g.setColorIndex(1);         // pixel on
  }
  else if ( u8g.getMode() == U8G_MODE_HICOLOR ) {
    u8g.setHiColorByRGB(255,255,255);
  }
  
  // join I2C bus (I2Cdev library doesn't do this automatically)
  #if I2CDEV_IMPLEMENTATION == I2CDEV_ARDUINO_WIRE
      Wire.begin();
      TWBR = 24; // 400kHz I2C clock (200kHz if CPU is 8MHz)
  #elif I2CDEV_IMPLEMENTATION == I2CDEV_BUILTIN_FASTWIRE
      Fastwire::setup(400, true);
  #endif

  #if DEBUG == true
      // initialize serial communication
      Serial.begin(115200);
  #endif

  mpu.initialize();

  if (mpu.dmpInitialize() == 0) {
      // turn on the DMP, now that it's ready
      mpu.setDMPEnabled(true);
      // supply your own gyro offsets here, scaled for min sensitivity
      mpu.setXGyroOffset(161);
      mpu.setYGyroOffset(48);
      mpu.setZGyroOffset(43);
      mpu.setZAccelOffset(1788); // 1688 factory default for my test chip
  }
  
  // Initialize cube points coords
  for(int i=0;i<TOTAL_POINTS; i++){
        P[i][0]=0;
        P[i][1]=0;
  } 
}


void loop() {
  u8g.firstPage();  
  do {
    cubeloop();
  } 
  while( u8g.nextPage() );  
}



void cubeloop() {

  period = millis()- timeLast;
  timeLast = millis(); 
  // precalc
  double MMPI_TIME = MMPI*period;

  //Read gyro, apply calibration, ignore small values
  mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);

  // ignore low values (supply uour own values here, based on Serial console output)
  if(abs(gx)<=40){
    gx = 0;
  }
  if(abs(gy)<=30){
    gy = 0;
  }
  if(abs(gz)<=26){
    gz = 0;
  }

  // scale angles down, rotate
  vectRotXYZ((double)gy/MMPI_TIME, 1); // X
  vectRotXYZ((double)-gx/MMPI_TIME, 2); // Y
  vectRotXYZ((double)gz/MMPI_TIME, 3); // Z

  #if DEBUG == true
    Serial.print(scale);
    Serial.print("\t");  
    Serial.print(ax);
    Serial.print("\t");
    Serial.print(ay);
    Serial.print("\t");
    Serial.print(az);
    Serial.print("\t");
    Serial.print((uint8_t) gx);
    Serial.print("\t");
    Serial.print((uint8_t) gy);
    Serial.print("\t");
    Serial.println((uint8_t) gz);
  #endif

  // calculate each point coords
  for(int i=0; i<TOTAL_POINTS; i++) {
     P[i][0] = centerX + scale/(1+C[i][2]/sZ)*C[i][0];
     P[i][1] = centerY + scale/(1+C[i][2]/sZ)*C[i][1];
  }
  
  // draw each line
  for(int i=0; i<TOTAL_LINES; i++){
    u8g.drawLine(P[LINES[i][0]][0], P[LINES[i][0]][1], P[LINES[i][1]][0], P[LINES[i][1]][1]);
  }
 
}


void vectRotXYZ(double angle, int axe) { 
  int8_t m1; // coords polarity
  uint8_t i1, i2; // coords index
  
  switch(axe) {
    case 1: // X
      i1 = 1; // y
      i2 = 2; // z
      m1 = 1;  // multiply by -1 to rotate sensor at 180 deg.
    break;
    case 2: // Y
      i1 = 0; // x
      i2 = 2; // z
      m1 = 1;
    break;
    case 3: // Z
      i1 = 0; // x
      i2 = 1; // y
      m1 = -1; // multiply by -1 to rotate sensor at 180 deg.
    break;
  }

  for(int i=0; i<TOTAL_POINTS; i++) {
    double t1 = C[i][i1];
    double t2 = C[i][i2];
    C[i][i1] = t1*cos(angle)+(m1*t2)*sin(angle);
    C[i][i2] = (-m1*t1)*sin(angle)+t2*cos(angle);
  }
  
}

