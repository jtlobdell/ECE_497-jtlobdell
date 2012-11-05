/*

 ECE 497
 Miniproject 1

 This project uses the pot connected ADC 5 (ain5) to 
 control a PWM signal to an LED, controlling its
 brightness.

 There is also a button connected that, when pressed,
 will cause a measurement to be taken using an I2C
 thermometer unit. In order for everything to run at
 once, the button does not use interrupts.

 John Lobdell

*/


#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

#define ADC_DIR "/sys/devices/platform/omap/tsc"
#define PWM_SETUP_DIR "/sys/kernel/debug/omap_mux"
#define PWM_SETTINGS_DIR "/sys/class/pwm/ehrpwm.1:0"
#define GPIO_DIR "/sys/class/gpio"
#define STRING_SIZE 100

FILE* adc_fp;
FILE* freq_fp;
FILE* duty_fp;

FILE* pwm_file;

FILE* gpio_fp;

int running;
int adc_ch;


void sigint_handler(int sig)
{
        printf("\nEnding...\n");
        running = 0;
}

FILE* adc_open(int adc_num)
{
        FILE* file;
        char filename[STRING_SIZE];

        sprintf(filename, ADC_DIR "/ain%d", adc_num);

        file = fopen(filename, "rb");
        if (file == NULL)
        {
                printf("Failed to open file: %s\n",filename);
        }

        return file;
}

int setup_pwm_pin()
{
        FILE* file;
        char filename[STRING_SIZE];

        sprintf(filename, PWM_SETUP_DIR "/gpmc_a2");
        file = fopen(filename, "ab");

        if (file == NULL)
        {
                printf("Failed to open file: %s\n", filename);
                return 1;
        }

        fwrite("6", sizeof(char), 1, file);
        fclose(file);

        return 0;
}

int setup_pwm_run()
{
        FILE* file;
        char filename[STRING_SIZE];

        sprintf(filename, PWM_SETTINGS_DIR "/run");
        file = fopen(filename, "ab");

        if (file == NULL)
        {
                printf("Failed to open file: %s\n", filename);
                return 1;
        }

        fwrite("1", sizeof(char), 1, file);
        fclose(file);

        return 0;
}

// Returns 0 if successful, 1 otherwise
int start_pwm()
{
        if (setup_pwm_pin() == 1) return 1;
        return setup_pwm_run();
}




FILE* pwm_freq_open()
{
        FILE* file;
        char filename[STRING_SIZE];

        sprintf(filename, PWM_SETTINGS_DIR "/period_freq");

        file = fopen(filename, "ab");
        if (file == NULL)
        {
                printf("Failed to open file: %s\n",filename);
        }

        return file;
}


FILE* pwm_duty_open()
{
        FILE* file;
        char filename[STRING_SIZE];

        sprintf(filename, PWM_SETTINGS_DIR "/duty_percent");

        file = fopen(filename, "ab");
        if (file == NULL)
        {
                printf("Failed to open file: %s\n",filename);
        }

        return file;
}


int gpio_open()
{
	// Write 7 to export
	FILE* file;
	char filename[STRING_SIZE];

	sprintf(filename, GPIO_DIR "/export");
	file = fopen(filename, "ab");

	if (file == NULL)
	{
		printf("Failed to open file: $s\n", filename);
		return 1;
	}

	fwrite("7", sizeof(char), 1, file);
	fclose(file);

	// Set gpio7 to input
	sprintf(filename, GPIO_DIR "/gpio7/direction");
	file = fopen(filename, "ab");

	if (file == NULL)
	{
		printf("Failed to open file: %s\n", filename);
		return 1;
	}

	fwrite("in", sizeof(char), 2, file);
	fclose(file);

	return 0;
}

FILE* gpio_file_open()
{
	FILE* file;
	char filename[STRING_SIZE];

	sprintf(filename, GPIO_DIR "/gpio7/value");
	file = fopen(filename, "rb");
	if (file == NULL)
	{
		printf("Failed to open file %s\n", filename);
	}

	return file;
}

int main(int argc, char* argv[])
{
	char buffer[STRING_SIZE];
        char adc_reading_str[STRING_SIZE];
        int adc_reading;
	int freq;
	int sw_current;
	int sw_previous;

	if (argc < 2)
        {
                printf("Usage: %s <PWM frequency>\n", argv[0]);
                exit(-1);
        }

	freq = atoi(argv[1]);
        adc_ch = 6;

        signal(SIGINT, sigint_handler);

        if (start_pwm() == 1) exit(1);

        freq_fp = pwm_freq_open();
        duty_fp = pwm_duty_open();

	fwrite("0", sizeof(char), 1, duty_fp);
	fflush(duty_fp);

	sprintf(buffer, "%d", freq);
	fwrite(buffer, sizeof(char), strlen(buffer), freq_fp);
	fflush(freq_fp);

        adc_fp = adc_open(adc_ch);
        if (adc_fp == NULL) exit(1);


	if (gpio_open() != 0) exit(1);	
	gpio_fp = gpio_file_open();
	if (gpio_fp == NULL) exit(1);

        running = 1;

	float ratio;
	int duty;

	int i = 0;
	int duty_index = 0;
	int duty_cycles[16];
	int num_duty_cycles = 0;
	int duty_avg;

	char sw_reading_str[STRING_SIZE];
	int sw_reading;

	sw_current = 0;
	sw_previous = 0;

        while (running)
        {
                rewind(adc_fp);
		rewind(duty_fp);
		rewind(gpio_fp);

		// ADC
                fread(adc_reading_str, sizeof(char), STRING_SIZE, adc_fp);
                adc_reading = atoi(adc_reading_str);
                

		// Calculate the duty cycle
		ratio = 1.0f - (float) adc_reading / 4096.0f;
		duty = 100*ratio;

		// Average the duty cycles to decrease flickering
		duty_cycles[duty_index++] = duty;
		if (duty_index == 16) duty_index = 0;
		if (num_duty_cycles < 16) num_duty_cycles++;

		duty_avg = 0;

		for (i = 0; i < num_duty_cycles; i++)
			duty_avg += duty_cycles[i];

		duty_avg /= num_duty_cycles;

		// Set the duty cycle
		sprintf(buffer, "%d", duty_avg);
		fwrite(buffer, sizeof(char), strlen(buffer), duty_fp);

		fflush(duty_fp);

		// Read the state of the switch
		sw_previous = sw_current;
		fread(sw_reading_str, sizeof(char), STRING_SIZE, gpio_fp);
		sw_current = atoi(sw_reading_str);

		if (sw_current == 1 && sw_current != sw_previous)
		{
			system("i2cget -y 3 0x48 0");
		}
		

		usleep(10000);
        }

        fclose(adc_fp);
        fclose(freq_fp);
        fclose(duty_fp);

        return 0;
}



