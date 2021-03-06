/*
 * mpu9150.c
 *
 *  Created on: 25 Jun 2013
 *      Author: Dominik Beste
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "i2c.h"
#include "include/mpu9150.h"
#include "lib/sensors.h"

uint8_t SAMPLE_RATE_CONFIG = 19;
uint8_t packet_size = 0;
static struct mpu9150_data ba;
uint16_t fifo_count;
static uint8_t buffer;
FILE *file;
//uint8_t SAMPLE_RATE_CONFIG = 13;

/*############################## INIT FUNCTION ###################################  */
void mpu9150_init(void) {
  printf("Initialising power management...\n");
  // Chip starts in sleep mode and we need to turn it on: (REG:107)
  uint8_t cmd[] = {MPU9150_PWR_MGMT_1, 0x00} ;


  i2c_transmitinit( MPU9150_I2C_ADDR, 2, cmd ) ;
  while(!i2c_transferred()) /* Wait for transfer */ ;
  printf("Power management initialised!\n");
  file = fopen("testfile","w");
}
/*############################## STOP FUNCTION ###################################  */
void mpu9150_stop(void) {
  uint8_t cmd[] = {MPU9150_PWR_MGMT_1, 0x40} ;

  i2c_transmitinit( MPU9150_I2C_ADDR, 2, cmd ) ;
  while(!i2c_transferred()) /* Wait for transfer */ ;
}
/*############################## START FUNCTION ##################################  */
void mpu9150_start(void) {
  printf("Setting sample rate...\n");
  /*=== Set sample rate divider: (REG:25) ===*/
  uint8_t cmd[] = {MPU9150_SMPLRT_DIV, SAMPLE_RATE_CONFIG};
  i2c_transmitinit( MPU9150_I2C_ADDR, 2, cmd );
  while(!i2c_transferred()) /* Wait for transfer */ ;
  printf("Sample rate set to: %i!\n", SAMPLE_RATE_CONFIG);
  
  /*=== Set up FSYNC and dlpf: (REG:26) ===*/
  printf("Setting fsync dlpf config...\n");
  uint8_t fsync_dlpf_config = 0b00000011; //FSYNC disabled and dlpf_cfg set to 3.
  cmd[0] = MPU9150_CONFIG;
  cmd[1] = fsync_dlpf_config;
  i2c_transmitinit( MPU9150_I2C_ADDR, 2, cmd );
  while(!i2c_transferred()) /* Wait for transfer */ ;
  printf("FSYNC rate set!\n");
  /*
   * With current dlpf_cfg (3) the sample rate of the accelerometer is 1kHz and the 
   * gyroscope is 1kHz too. The sample rate divider has been set to 20 (register 20).
   * Sample rate = Gyroscope output rate / (1 + sample rate divider)
   *             =                  1000 / (1 + 19)
   *             =                       50Hz
   */
  /* Set up the fifo_config parameter and enable sensors if they are defined in the header file: */
  uint8_t fifo_config = 0b00000000;
  /*=== Trigger the gyroscopes self-test and set the scale range: (REG:27) ===*/
  if (INV_XYZ_GYRO) {
    fifo_config = fifo_config | 0b01110000;
    printf("Starting up gyroscope...\n");
    //Self test xyz and set full scale range to +-2000s:
    uint8_t gyro_config = 0b11111000;
    cmd[0] = MPU9150_GYRO_CONFIG;
    cmd[1] = gyro_config;
    i2c_transmitinit( MPU9150_I2C_ADDR, 2, cmd ) ;
    while(!i2c_transferred()) /* Wait for transfer */ ;
    printf("Gyroscope started!\n");
  }
 
  /*=== Trigger the accelorometer self-test and set the scale range: (REG:28) ===*/
  if (INV_XYZ_ACCEL) {
     fifo_config = fifo_config | 0b00001000;
     printf("Starting up accelorometer...\n");
     uint8_t accel_config = 0b11111000; //self test xyz, with full scale range +-16g and reset hpf
     cmd[0] = MPU9150_ACCEL_CONFIG;
     cmd[1] = accel_config;
     i2c_transmitinit( MPU9150_I2C_ADDR, 2, cmd ) ;
     while(!i2c_transferred()) /* Wait for transfer */ ;
     printf("Accelorometer started!\n");
  }
  
  /*=== Enable Temp on FIFO: ===*/
  if (INV_TEMP) {
    fifo_config = fifo_config | 0b10000000;
    printf("Adding thermometer to FIFO...\n");
  }
  
  if (INV_XYZ_ACCEL) {
        packet_size += 6;}
    if (INV_TEMP) {
        packet_size += 2;}
    if (INV_XYZ_GYRO) {
        packet_size += 6;}
    
    printf("packet size: %i\n", packet_size);
    //exit(0);
  
  /*
   * SKIPPING FREE FALL, MOTION DETECTION, ETC. CONFIGURATION
   */
  
   /*=== Setup FIFO queue: (REG:35) ===
    Bit order [1: Temp, 2-4: Gyro{x,y.z}, 5: Accel, 6-8: SLV{2,1,0}] */
  int BUF_SIZE = 9;
  char buffer[BUF_SIZE];
  buffer[BUF_SIZE - 1] = '\0';
  printf("Configuring FIFO with parameter %s...\n", int2bin(fifo_config, buffer, BUF_SIZE-1));
  cmd[0] = MPU9150_FIFO_EN;
  cmd[1] = fifo_config;
  i2c_transmitinit( MPU9150_I2C_ADDR, 2, cmd );
  while(!i2c_transferred()) /* Wait for transfer */ ;
  printf("FIFO configured!\n");
  
  /* Setup i2c master control (REG:36)
     First bit controls multi-master capability (not needed for us I think)
     Second bit controls wait for external sensors (to keep data in sync), which is not needed by us, because we do ot use external sensors
     Third bit controls FIFO_EN for salve 3 (other slaves controled in REG35)
     Fourth bit controls slave read and write mode (don't really know waht the implications of the different write modes are, think it's not significant for us?)
     Last four bits control the i2c master clock. We set the 8mhz clock divider to 19, so set this to 12 1100??? */
  printf("Setting up I2C master controller...\n");
  uint8_t i2cmc_config= 0b00001101;
  cmd[0] = MPU9150_I2C_MST_CTRL;
  cmd[1] = i2cmc_config;
  i2c_transmitinit( MPU9150_I2C_ADDR, 2, cmd );
  while(!i2c_transferred()) /* Wait for transfer */ ;
  printf("I2C master configured!\n");
  
  /* Setup user control */
  printf("Setting up user control...\n");
  uint8_t user_config= 0b01000100;
  cmd[0] = MPU9150_USER_CTRL;
  cmd[1] = user_config;
  i2c_transmitinit( MPU9150_I2C_ADDR, 2, cmd );
  while(!i2c_transferred()) /* Wait for transfer */ ;
  printf("User control configured!\n");
  
  /*
   * SLAVE CONTROL REGISTERS (37-53) WE DON'T NEED TO CONFIGURE (I THINK). 54 IS THE I2C MASTER CONTROLLER,
   * ALSO NOT USED BY US, 55 IS USED TO CONFIGURE INTERRUPTS (MAYBE WE SHOULD CONFIGURE), 56 ENABLES
   * INTERRUPTS, MAYBE WE SHOULD SET THE FIFO INTERRUPT TO ONE, THEN WAIT FOR THE INTERRUPT TO OCCOUR
   * AND THEN READ ALL SENSORS IN ONE GO AND WAIT FOR NEXT INTERRUPT. REG 58 CONTAINS THE INTERRUPT STATA.
   * 59-96 CONTAINS THE LATEST VALUES FROM THE SENOSRS, BUT WE READ THEM FROM THE FIFO INSTEAD. REG 97 IS
   * MOTION DETETION STATUS, 98-102 HAS THE DATAOUT FOR THE IC2 SLAVES, 103 CONTROLS IC2 MASTER DEALY, 104
   * CAN BE USED TO RESET THE PATHS FOR THE INTERNAL SENSORS, 105 IS MOTION DETECTION CONTROL, 106 USER CONTROL,
   * BUT WE ENABLE THE FIFO IN A DIFFERENT REGISTER, SO SON'T NEED TO REENABLE IT HERE. 107 AND 108 ARE
   * POWER MANAGEMENT.
   * NOW TO GET DATA, WE SHOULD LOOK AT REGISTERS 114 AND 115 TO GET THE COUNT OF NEW MEASUREMENTS AND THEN
   * READ THEM FROM THE FIFO QUEUE:
   */
}

/*################ FUNCTION TO CALL BEFORE READING DATA FROM A REGISTER ##############  */
void prepareRead(uint8_t slv_adr) {
  i2c_transmitinit( MPU9150_I2C_ADDR, 1, &slv_adr ) ;
  while(!i2c_transferred()) /* Wait for transfer */ ;
}

/*############################## GET DATA FUNCTION ##################################  */
void mpu9150_get_data(uint8_t *buffer) {
  // Poll number of new measurements to read from the FIFO:
  //uint8_t set[] = {MPU9150_FIFO_COUNT_H,0};
  prepareRead(MPU9150_FIFO_R_W);
  
  i2c_receiveinit( MPU9150_I2C_ADDR, 2, buffer ) ;
  while(!i2c_transferred());
  
  uint16_t fifo_count = ((buffer[0]<<8) | buffer[1]);
}
/*######################### GET DATA FUNCTION WITH SIZE ############################## */
void mpu9150_get_data_size(uint8_t *buffer, int size) {
  // Poll number of new measurements to read from the FIFO:
  prepareRead(MPU9150_FIFO_R_W);
  
  //size = 2;
  size = 18;
  
  printf("Prepared FIFO for reading %i bytes...\n",size);
  
  i2c_receiveinit( MPU9150_I2C_ADDR, size, buffer ) ;
  while(!i2c_transferred());
  
  uint16_t fifo_count = ((buffer[0]<<8) | buffer[1]);
}
/*############################## FIFO COUNT FUNCTION ##################################  */
uint16_t mpu9150_FIFO_count(uint8_t *buffer) {
	prepareRead(MPU9150_FIFO_COUNT_H);

	i2c_receiveinit( MPU9150_I2C_ADDR, 2, buffer ) ;
	while(!i2c_transferred()) /* Wait for transfer */ ;
	
	uint16_t fifo_count = ((buffer[0]<<8) | buffer[1]);
        printf("FIFO count in bits: %i and in bytes %i\n", fifo_count, fifo_count/8);
        return fifo_count / 8;
}
/*############################## DECODE DATA FUNCTION ##################################  */
void mpu9150_decodeTempOnly(struct mpu9150_data *const ba , uint8_t *buffer) {
  ba ->temp =  ((int16_t)(buffer[0]<<8) | buffer[1]);
}
/*############################## DECODE DATA FUNCTION ##################################  */
void mpu9150_decode(struct mpu9150_data *const ba , uint8_t *buffer) {

	ba->acc_x = ((int16_t)(buffer[0]<<8) | buffer[1]);

	ba->acc_y = ((int16_t)(buffer[2]<<8) | buffer[3]);

	ba->acc_z = ((int16_t)(buffer[4]<<8) | buffer[5]);

	ba->temp = ((int16_t)(buffer[6]<<8) | buffer[7]);

	ba->gyro_x = ((int16_t)(buffer[8]<<8) | buffer[9]);

	ba->gyro_y = ((int16_t)(buffer[10]<<8) | buffer[11]);

	ba->gyro_z = ((int16_t)(buffer[12]<<8) | buffer[13]);
}

/*############################## FUNCTION TO PRINT OUT THE FIFO ##################################  */
void mpu9150_print_accel() {
  uint8_t index = 0;
    
    fifo_count = mpu9150_FIFO_count(&buffer);
    printf("Num of min packets: %i and current fifo count: %i\n", packet_size, fifo_count);
    if (fifo_count < packet_size) {
      printf("FIFO too small!");
      return;
    }
    mpu9150_get_data_size(&buffer, fifo_count);
    //printf("Got data, now decoding it...");
    mpu9150_decode(&ba, &buffer);
    //mpu9150_decodeTempOnly(&ba, &buffer);
    char str[200];
    char *static_str = "Current gyroscope readings:\nx: %i, y: %i, z: %i\nCurrent accelorometer readings:\nx: %i, y: %i, z: %i\nCurrent Temperature: %i, in C:%i\n";
    int16_t tempC = ba.temp / 340 + 35;
    sprintf(str, static_str, ba.gyro_x, ba.gyro_y, ba.gyro_z, ba.acc_x, ba.acc_y, ba.acc_z, ba.temp, tempC);
    printf(str);
    char str2[100];
    char *out_str = "DATA,%i,%i,%i,%i,%i,%i,%i\n";
    sprintf(str2, out_str,ba.gyro_x, ba.gyro_y, ba.gyro_z, ba.acc_x, ba.acc_y, ba.acc_z, tempC);
    
    printf(str2);
    //int charsToDelete = strlen(str);
    //printf("length: %i\n", charsToDelete);
    //int j;
    //for (j = 0; j < charsToDelete; j++) {
    //   printf("\b");
    //}
    //printf("\033[2J\033[1;1H");
    //printf("\033[H");
}

void resetFIFO() {
  uint8_t user_config= 0b01000100;
  uint8_t cmd[] = {MPU9150_USER_CTRL, user_config} ;
  i2c_transmitinit( MPU9150_I2C_ADDR, 2, cmd );
  while(!i2c_transferred()) /* Wait for transfer */ ;
  printf("User control configured!\n");
}


/*
 * Function to return the WHO_AM_I value
 * of the mpu9150 chip.
*/
void mpu9150_who_am_i(uint8_t *buffer) {
   prepareRead(MPU9150_WHO_AM_I);
     
   i2c_receiveinit( MPU9150_I2C_ADDR, 2, buffer ); //need to receive two bytes, otherwise we get ERROR I2C: Arbitration lost
   while(!i2c_transferred()); /* Wait for transfer */
}

/**
 * Function to perform a logical shift right.
 */
uint8_t lsr(uint8_t x, uint8_t n) {
  return (uint8_t)((unsigned int)x >> n);
}

/**
 * Function to perform a logical shift left.
 */
uint8_t lsl(uint8_t x, uint8_t n) {
  return (uint8_t)((unsigned int)x << n);
}

static int
value(int type)
{
    // TODO
    return -1;
}

static int
status(int type)
{
    // TODO
    return 1;
}

static int
configure(int type, int c)
{
	mpu9150_init();
	mpu9150_start();
  return 1;
}

char *int2bin(uint8_t a, char *buffer, int buf_size) {
    buffer += (buf_size - 1);

    int i = 7;
    for (i ; i >= 0; i--) {
        *buffer-- = (a & 1) + '0';

        a >>= 1;
    }

    return buffer;
}

// Instantiate the sensor
SENSORS_SENSOR(mpu9150_sensor, "MPU9150-Sensor", value, configure, status);
