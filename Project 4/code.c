#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <math.h>
#include <poll.h>
#include <time.h>
#include "mraa/aio.h"

const int B = 4275;               // B value of the thermistor
const int R0 = 100000;            // R0 = 100k

void error(char* msg) {
  fprintf(stderr, "%s\n", msg);
  exit(1);
}

int
main (int argc, char **argv)
{
  mraa_aio_context adc_a0;
  mraa_gpio_context adc_d3;
  uint16_t adc_value = 0;
  float adc_value_float = 0.0;

  // Initialize an analog input device, connected to pin 0
  adc_a0 = mraa_aio_init(0);
  if (adc_a0 == NULL) {
    error("Could not initialize from pin 0");
  }

  // Same for pin 3 (D3), the button
  adc_d3 = mraa_gpio_init(3);
  if (adc_d3 == NULL) {
    error("Could not read pin 3");
  }
  mraa_gpio_dir(adc_d3, MRAA_GPIO_IN);
  
  /* Read options (if any) */
  static struct option long_options[] =
    {
      {"period", required_argument, 0, 'p'},
      {"scale", required_argument, 0, 's'},
      {"log", required_argument, 0, 'l'},
      {0, 0, 0, 0}
    };
  
  /*option flags*/
  int period = 1;
  char tempScale = 'F';
  char * logFile = NULL;
  
  /* getopt_long stores the option index here. */
  int option_index = 0;
  char c;
  
  while ((c = getopt_long(argc, argv, "p:s:l:", long_options, &option_index)) != -1 ||
	 (c = getopt(argc, argv, "p:s:l:")) != -1) {
    switch (c)
      {
      case 'p':
	period = atoi(optarg);
	break;

      case 's':
	if (optarg[0] != 'F' || optarg[0] != 'C')
	  error("ERROR: --scale must be C or F");
	tempScale = optarg[0];
	break;

      case 'l':
	logFile = optarg;
	break;
	
      case '?':
	/* getopt_long already printed an error message. */
	fprintf(stderr, "Usage: ./code [OPTIONS]\n");
	exit(1);
	
      default:
	abort ();
      }
  }

  int logfd = -1;
  if (logFile != NULL) {
    if ((logfd = creat(logFile, 0666)) == -1)
      error("Error in creating log file");
  }
  
  struct pollfd fds[2];
  fds[0].fd = adc_a0;
  fds[0].events = POLLIN | POLLHUP | POLLERR;
  fds[0].fd = adc_d3;
  fds[0].events = POLLIN | POLLHUP | POLLERR;
  
  int i = 0;
  if (fds[0].revents & POLLIN) {
    for (;;) {
      // Read the input voltage
      adc_value = mraa_aio_read(adc_a0);
      if (adc_value == -1)
	error("Unable to read input voltage");
      
      
      float R = 1023.0 / adc_value - 1.0;
      R = R0*R;
      
      // convert to temperature via datasheet
      float temperature = 1.0/(log(R/R0)/B+1/298.15)-273.15; 
      if (tempScale == 'F')
	temperature = (temperature * 1.8) + 32;
      
      const time_t t = time(NULL);
      struct tm * tm = localtime(&t);
      char timeString[80];

      // format time
      strftime(timeString, 80, "%T", tm);
      if (logfd == -1)
	fprintf(stdout, "%s %.1f\n", timeString, temperature);
      else {
	dprintf(logfd, "%s %.1f\n", timeString, temperature);
      }
      sleep(period);
    }
  }

  if (fds[0].revents & (POLLHUP + POLLERR)) {
    fprintf(stderr, "Finished reading from pipe 0\n");
  }

  if (fds[1].revents & POLLIN) {
    // Read the input voltage
    adc_value = mraa_gpio_read(adc_d3);
    if (adc_value == -1)
      error("Unable to read input voltage");
    
    const time_t t = time(NULL);
    struct tm * tm = localtime(&t);
    char timeString[80];
    
    // format time
    strftime(timeString, 80, "%T", tm);
    if (logfd == -1)
      fprintf(stdout, "%s SHUTDOWN\n");
    else {
      dprintf(logfd, "%s SHUTDOWN\n");
    }
  }
    
  mraa_aio_close(adc_a0);
  mraa_gpio_close(adc_d3);
  return MRAA_SUCCESS;
}
