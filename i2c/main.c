/*
-Pushbutton Switch: GPIO1_16 - Header P9, pin 15 - GPIO 48
-Pusshbuton Switch V+: Header P9, pin 3

*/

#include <errno.h>
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include "i2c-dev.h"
#include "gpio.h"
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <string.h>

#define OMAP_DIR "/sys/kernel/debug/omap_mux"

int running = 1;

// Quit when ^C is pressed
void signal_handler(int sig)
{
	printf("\n");
	running = 0;
}

int main(int argc, char *argv[])
{
	// I2C variables
	char *end;
	int res, i2cbus, address, size, file;
	int daddress;
	char filename[20];

	// Polling variables
	int rc;
	struct pollfd fdset[2];
	int nfds = 2;
	int len;
	char* buf[MAX_BUF];


	/* handle (optional) flags first */
	if(argc < 3) {
		fprintf(stderr, "Usage:  %s <i2c-bus> <i2c-address> <register>\n", argv[0]);
		exit(1);
	}

	i2cbus   = atoi(argv[1]);
	address  = atoi(argv[2]);
	daddress = atoi(argv[3]);
	size = I2C_SMBUS_BYTE;

	// Handle Ctrl^C
	signal(SIGINT, signal_handler);


	//////////////////////////////////////
	// Set up I2C
	//////////////////////////////////////

	sprintf(filename, "/dev/i2c-%d", i2cbus);
	file = open(filename, O_RDWR);
	if (file<0) {
		if (errno == ENOENT) {
			fprintf(stderr, "Error: Could not open file "
				"/dev/i2c-%d: %s\n", i2cbus, strerror(ENOENT));
		} else {
			fprintf(stderr, "Error: Could not open file "
				"`%s': %s\n", filename, strerror(errno));
			if (errno == EACCES)
				fprintf(stderr, "Run as root?\n");
		}
		exit(1);
	}

	if (ioctl(file, I2C_SLAVE, address) < 0) {
		fprintf(stderr,
			"Error: Could not set address to 0x%02x: %s\n",
			address, strerror(errno));
		return -errno;
	}


	//////////////////////////////////////
	// Set up the pushbutton switch
	//////////////////////////////////////

	// Configure the pull-down resistor
	FILE *mux_ptr;
	char pulldown_str[10];
	strcpy(pulldown_str, "0x0027");
    if ((mux_ptr = fopen(OMAP_DIR "/gpmc_a0", "rb+")) == NULL)
    {
            printf("Failed to open gpmc_a0. Quitting.\n");
            exit(1);
    }
    else
	{
            fwrite(&pulldown_str, sizeof(char), 6, mux_ptr);
            fclose(mux_ptr);
    }
	
	// Configure the GPIO pin
	unsigned int gpio_sw = 48;
	int sw_val;
	int sw_fd;
	printf("Exporting sw... %d\n", gpio_export(gpio_sw));
	printf("Setting sw direction... %d\n", gpio_set_dir(gpio_sw, 0));
	printf("setting edge sw... %d\n", gpio_set_edge(gpio_sw, "rising"));
	sw_fd = gpio_fd_open(gpio_sw);
	

	//////////////////////////////////////
	// Main loop
	//////////////////////////////////////

	while (running == 1)
	{
		memset((void*)fdset, 0, sizeof(fdset));

		fdset[0].fd = STDIN_FILENO;
		fdset[0].events = POLLIN;

		fdset[1].fd = sw_fd;
		fdset[1].events = POLLPRI;

		rc = poll(fdset, nfds, 1000);

		if (rc < 0)
		{
//			printf("poll() failed.\n");
		}

		if (rc == 0)
		{
			//printf("Heartbeat\n");
			res = i2c_smbus_read_byte_data(file, daddress);
			printf("%d\n", res);
		}

		if (fdset[0].revents & POLLIN)
		{
			(void) read(fdset[0].fd, buf, 1);
		}


		// Button press - print the current temperature
		if (fdset[1].revents & POLLPRI)
		{
			lseek(fdset[1].fd, 0, SEEK_SET);
			len = read(fdset[1].fd, buf, MAX_BUF);

			// Very simple debouncing
			usleep(5000);
			gpio_get_value(gpio_sw, &sw_val);
			if (sw_val == 1)
			{
				// printf("Button Press\n");
				res = i2c_smbus_read_byte_data(file, daddress);
				printf("%d\n", res);
			}
		}

	}


	res = i2c_smbus_read_byte_data(file, daddress);



	//////////////////////////////////////
	// Clean up
	//////////////////////////////////////

	close(file);
	gpio_fd_close(sw_fd);

	if (res < 0) {
		fprintf(stderr, "Error: Read failed, res=%d\n", res);
		exit(2);
	}

	printf("0x%02x (%d)\n", res, res);

	exit(0);
}

