#include <stdio.h>
#include <unistd.h>			//Used for UART
#include <fcntl.h>			//Used for UART
#include <termios.h>		//Used for UART
#include <stdlib.h>
#include <string.h>

#include <gps.h>
#include <mpu-9150.h>
#include <bmp180.h>

#include "../../libraries/dbman.h"

#include <cc_beacon_iface.h>

#include <sensors.h>

//#define GPS_OFF
//#define IMU_OFF
//#define TEM_OFF
//#define BEACON_OFF

int 
init_gps()
{
	#ifndef GPS_OFF
	unsigned char recv_message[256];
	int uart_fd = OpenGPSIface(500);
	if (uart_fd == -1){
	    printf("No GPS Device\n");
	    return 0;
	}
	if (SetGPSMessage(uart_fd, CFG_NAV5) < 0){
		printf("Error\n");
		exit(-1);
	}
	if (GetGPSMessage(uart_fd, CFG_NAV5, recv_message) < 0){
		printf("Error\n");
		exit(-1);
	}
	if (SetGPSMessage(uart_fd, CFG_DISABLE_NMEA) < 0){
		printf("Error\n");
		exit(-1);
	}
	if (GetGPSMessage(uart_fd, CFG_DISABLE_NMEA, recv_message) < 0){
		printf("Error\n");
		exit(-1);
	}
	#endif
	return uart_fd;
}

int
init_beacon()
{
	#ifndef BEACON_OFF
	printf("Beacon connect\n");
	int beacon_handler = BeaconConnect("localhost", "52001", beacon_sender);
	printf("Beacon connect DONE\n");
	return beacon_handler;
	#endif
}

/* IMU IS NOT THREAD SAFE */
int
init_imu()
{
	#ifndef IMU_OFF
	int ret;
	printf("IMU connect\n");
	/* I2C IMU interface INIT */
	if ( ret = imupi_init( ) ) {
		switch( ret ) {
			case IMUPI_NO_ERROR:
		    break;

		  	case IMUPI_I2C_OPEN_ERROR:
		    	fprintf (stderr, "Unable to open I2C device.\n" );
		    	exit( EXIT_FAILURE );
		  	break;
		  	case IMUPI_I2C_DEV_NOT_FOUND:
				fprintf (stderr, "Device not found.\n" );
				exit( EXIT_FAILURE );
			break;
		  	case IMUPI_I2C_WRITE_ERROR:
		    	fprintf (stderr, "I2C write error.\n" );
		    	exit( EXIT_FAILURE );
		  	break;
		  	default:
		    	fprintf (stderr, "Initialization errror.\n" );
		    	exit( EXIT_FAILURE );
	    	break;
		}
	}
	printf("IMU connect DONE\n");
	#endif	
}

void *
init_temp_sensor()
{
	#ifndef TEM_OFF
  	char i2c_device[] = "/dev/i2c-1";
  	int address = 0x77;	
	printf("Temperature connect\n");
	/* I2C Temp/Baro sensor INIT */
	void *bmp = bmp180_init(address, i2c_device);
	if (bmp == NULL){
		fprintf(stderr, "Initialization error on temperature sensor\n");
		exit ( EXIT_FAILURE );
	}
	bmp180_eprom_t eprom;
	bmp180_dump_eprom(bmp, &eprom);
	bmp180_set_oss(bmp, 1);
	printf("Temperature connect DONE\n");
	return bmp;
	#endif
}

void
gps_read(_gps_data * gps_data, int uart_fd)
{
	#ifndef GPS_OFF
	unsigned char recv_message[256];
	int rx_len;
	memset(gps_data, 0, sizeof(_gps_data));
	if (rx_len = GetGPSMessage(uart_fd, NAV_PVT, recv_message), rx_len > 0 ){
		ParseUBXPVT(recv_message, rx_len, gps_data);
		printf("Lat: %f, Long: %f, GSpeed: %f\n", gps_data->lat, gps_data->lng, gps_data->gspeed);
	}
	#endif
}

void
imu_read(_motion_sensors * motion_sens)
{
	#ifndef IMU_OFF
	double a[3], g[3], m[3];		
	int ret = imupi_read( a, g, m );
    if ( ret )  {
	    switch( ret ) {
	        case IMUPI_NO_ERROR:
	          break;
	      
	        case IMUPI_INIT_ERROR:
	          fprintf (stderr, "Trying to read an uninitialized device.\n" );
	          exit( EXIT_FAILURE );
	        
	        case IMUPI_I2C_DEV_NOT_FOUND:
	          fprintf (stderr, "Device not found.\n" );
	          exit( EXIT_FAILURE );
	        
	        case IMUPI_I2C_READ_ERROR:
	          fprintf (stderr, "I2C read error.\n" );
	          exit( EXIT_FAILURE );
	        
	        default:
	          fprintf (stderr, "Read errror.\n" );
	          exit( EXIT_FAILURE );
		}
	}
	motion_sens->acc_x = a[0];
	motion_sens->acc_y = a[1];
	motion_sens->acc_z = a[2];

	motion_sens->gyro_x = g[0];
	motion_sens->gyro_y = g[1];
	motion_sens->gyro_z = g[2];

	motion_sens->mag_x = m[0];
	motion_sens->mag_y = m[1];
	motion_sens->mag_z = m[2];

	printf( "\tAx: %-8.2f Ay: %-8.2f Az: %-8.2f\n", motion_sens->acc_x, motion_sens->acc_y, motion_sens->acc_z);
    printf( "\tGx: %-8.2f Gy: %-8.2f Gz: %-8.2f\n", motion_sens->gyro_x, motion_sens->gyro_y, motion_sens->gyro_z);
    printf( "\tMx: %-8.2f My: %-8.2f Mz: %-8.2f\n", motion_sens->mag_x, motion_sens->mag_y, motion_sens->mag_z);

	#endif
}

void
temp_read(_ambient_sensors * amb_sens, void * bmp)
{
	#ifndef TEM_OFF
	amb_sens->in_temp = bmp180_temperature(bmp);
	amb_sens->in_pressure = (double) (bmp180_pressure(bmp)/100.0);
	amb_sens->in_calc_alt = bmp180_altitude(bmp);

    int t_aux = 0;
    FILE * cpufile;
    FILE * gpufile;
    /* Reads internal temperature sensors: */
    system("/opt/vc/bin/vcgencmd measure_temp > /home/pi/gpu_temp");
    if((cpufile = fopen("/sys/class/thermal/thermal_zone0/temp", "r")) == NULL) {
        perror("Error opening CPU file");
    } else {
        fscanf(cpufile, "%d", &t_aux);
        amb_sens->cpu_temp = t_aux / 1000.0;
    }
    if((gpufile = fopen("/home/pi/gpu_temp", "r")) == NULL) {
        perror("Error opening GPU file");
    } else {
        fscanf(gpufile, "%*[^=] %*[=]%f", &amb_sens->gpu_temp);
    }
    if(cpufile != NULL) {
        fclose(cpufile);
    }
    if(gpufile != NULL) {
        fclose(gpufile);
    }

	printf("\tIn Temp: %f Press: %f, Alt: %f\n", amb_sens->in_temp, amb_sens->in_pressure, amb_sens->in_calc_alt);	
	printf("\tIn Temp CPU: %f Temp GPU %f\n", amb_sens->cpu_temp, amb_sens->gpu_temp);

	#endif
}

void
beacon_write(_gps_data * gps_data, _motion_sensors * motion_sens, _ambient_sensors * amb_sens, int fd)
{
	#ifndef BEACON_OFF
	hk_data_t data;
	memcpy(&data.gps, gps_data, sizeof(_gps_data));
	memcpy(&data.mot, motion_sens, sizeof(_motion_sensors));
	memcpy(&data.amb, amb_sens, sizeof(_ambient_sensors));
	BeaconWrite(fd, (const void *) &data, sizeof(hk_data_t));
	#endif 
}

int
main (void)
{
	printf("Size of sending struct: %d\n", (int)sizeof(hk_data_t));
	printf("Size of gps_data: %d\n", (int) sizeof(_gps_data));
	printf("Size of motion_data: %d\n", (int) sizeof(_motion_sensors));
	printf("Size of ambient_data: %d\n", (int) sizeof(_ambient_sensors));
	
	_gps_data gps_data;
	_ambient_sensors amb_sens;
	_motion_sensors motion_sens;

	/* initialize sensors, beacons, sockets... */
	int beacon_fd = init_beacon();
	int gps_fd = init_gps();
	/* imu does not return any value, cause of its library implementation */
	init_imu();
	/* temp sensor is returning a *bmp object */
	void * bmp = init_temp_sensor();
	/* END OF INITS */
	/* hold 1 second before start streaming data */
	sleep(1);

	while(1)
	{
		gps_read(&gps_data, gps_fd);
		imu_read(&motion_sens);
		temp_read(&amb_sens, bmp);
		beacon_write(&gps_data, &motion_sens, &amb_sens, beacon_fd);
    	sleep(1);
		/* the following must be called */
		/*dbman_save_gps_data(&data);*/
	}
}
	