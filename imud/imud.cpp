#include "I2Cdev.h"
#include "MPU6050_6Axis_MotionApps20.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <fcntl.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include <sys/stat.h>

const int SIZE = 50;
const char *name = "MPU";
int shm_fd;
void *ptr;

void shm_init() {
  /* create the shared memory segment */
  shm_fd = shm_open(name, O_CREAT | O_RDWR, 0666);

  /* configure the size of the shared memory segment */
  ftruncate(shm_fd, SIZE);

  ptr = mmap(0, SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
  memset(ptr, 0, SIZE);
}

void sigint_handler(int s) {
  /* printf("Caught signal %d\n",s); */
  shm_unlink(name);
  exit(1);
}

// class default I2C address is 0x68
// specific I2C addresses may be passed as a parameter here
// AD0 low = 0x68 (default for SparkFun breakout and InvenSense evaluation
// board) AD0 high = 0x69
MPU6050 mpu;

// MPU control/status vars
bool dmpReady = false; // set true if DMP init was successful
uint8_t mpuIntStatus;  // holds actual interrupt status byte from MPU
uint8_t devStatus; // return status after each device operation (0 = success, !0
                   // = error)
uint16_t packetSize;    // expected DMP packet size (default is 42 bytes)
uint16_t fifoCount;     // count of all bytes currently in FIFO
uint8_t fifoBuffer[64]; // FIFO storage buffer

Quaternion q;        // [w, x, y, z]         quaternion container
VectorFloat gravity; // [x, y, z]            gravity vector
float
    ypr[3]; // [yaw, pitch, roll]   yaw/pitch/roll container and gravity vector

void setup() {
  // initialize device
  mpu.initialize();

  // verify connection
  /* printf(mpu.testConnection() ? "MPU6050 connection successful\n" : "MPU6050
   * connection failed\n"); */

  // load and configure the DMP
  devStatus = mpu.dmpInitialize();

  // make sure it worked (returns 0 if so)
  if (devStatus == 0) {
    // turn on the DMP, now that it's ready
    /* printf("Enabling DMP...\n"); */
    mpu.setDMPEnabled(true);

    mpuIntStatus = mpu.getIntStatus();

    // set our DMP Ready flag so the main loop() function knows it's okay to use
    // it
    /* printf("DMP ready!\n"); */
    dmpReady = true;

    // get expected DMP packet size for later comparison
    packetSize = mpu.dmpGetFIFOPacketSize();
  } else {
    // ERROR!
    // 1 = initial memory load failed
    // 2 = DMP configuration updates failed
    // (if it's going to break, usually the code will be 1)
    printf("DMP Initialization failed (code %d)\n", devStatus);
  }
}

void loop() {
  // if programming failed, don't try to do anything
  if (!dmpReady)
    return;
  // get current FIFO count
  fifoCount = mpu.getFIFOCount();

  if (fifoCount == 1024) {
    // reset so we can continue cleanly
    mpu.resetFIFO();

  } else if (fifoCount >= 42) {
    // read a packet from FIFO
    mpu.getFIFOBytes(fifoBuffer, packetSize);

    mpu.dmpGetQuaternion(&q, fifoBuffer);
    mpu.dmpGetGravity(&gravity, &q);
    mpu.dmpGetYawPitchRoll(ypr, &q, &gravity);
    /* printf("%7.2f %7.2f %7.2f\n", ypr[0] * 180/M_PI, ypr[1] * 180/M_PI,
     * ypr[2] * 180/M_PI); */

    // Write to SHM pointer
    sprintf((char *)ptr, "%7.2f %7.2f %7.2f\n", ypr[0] * 180 / M_PI,
            ypr[1] * 180 / M_PI, ypr[2] * 180 / M_PI);
  }
}

int main() {
  shm_init();
  signal(SIGINT, sigint_handler);
  setup();
  usleep(1500);
  for (;;) {
    loop();
    usleep(10000);
  }
  return 0;
}
