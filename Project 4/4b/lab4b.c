/*
NAME: Chaoran Lin
EMAIL: linmc@ucla.edu
ID: 004674598
*/

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <math.h>
#include <poll.h>
#include <time.h>
#include <string.h>
#include <fcntl.h>
#include <ctype.h>
#include "mraa/aio.h"

const int B = 4275;               // B value of the thermistor
const int R0 = 100000;            // R0 = 100k

mraa_aio_context adc_a0;
mraa_gpio_context adc_d3;
uint16_t adc_value = 0;

int logfd;
int canLog;

void error();

void closeSensors() {
  mraa_aio_close(adc_a0);
  mraa_gpio_close(adc_d3);
}

void shutdown() {
  const time_t t = time(NULL);
  struct tm * local = localtime(&t);
  char timeString[100];
  
  // format time
  strftime(timeString, 100, "%T", local);
  if (logfd != -1) {
    dprintf(logfd, "%s SHUTDOWN\n", timeString);
    close(logfd);
  }
  else {
    if (canLog)
      fprintf(stdout, "%s SHUTDOWN\n", timeString);
  }

  exit(0);
}

void error(char* msg) {
  closeSensors();
  fprintf(stderr, "%s\n", msg);
  exit(1);
}

int
main (int argc, char **argv)
{
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
	if (optarg[0] != 'F' && optarg[0] != 'C') {
	  fprintf(stderr, "temperature must be f or c\n");
	  exit(1);
	}
	tempScale = optarg[0];

	break;

      case 'l':
	logFile = optarg;
	break;
	
      case '?':
	/* getopt_long already printed an error message. */
	fprintf(stderr, "Usage: ./lab4b --period=# --scale=[C|F] --log=LOGFILE\n");
	exit(1);
	
      default:
	abort ();
      }
  }

  canLog = 1;
  logfd = -1;
  if (logFile != NULL) {
    if ((logfd = creat(logFile, 0666)) == -1)
      error("Error in creating log file");
    canLog = 1;
  }
  
  struct pollfd fds[1];
  fds[0].fd = STDIN_FILENO;
  fds[0].events = POLLIN | POLLHUP | POLLERR;

  clock_t lastReadTime = -1; // initial value
  char buf[1024];
  size_t nbytes = sizeof(buf);
  
  while (mraa_gpio_read(adc_d3) == 0) {
    const time_t t = time(NULL);
    struct tm * local = localtime(&t);
    char timeString[100];

    // format time
    strftime(timeString, 100, "%T", local);

    int elapsed = (float)(clock() - lastReadTime) / CLOCKS_PER_SEC;
    if (elapsed >= period || lastReadTime == -1) {
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

      float ten = 10.0;
      if (canLog) {
	if (temperature < ten) {
	  fprintf(stdout, "%s 0%.1f\n", timeString, temperature);
	  if (logfd != -1)
	    dprintf(logfd, "%s 0%.1f\n", timeString, temperature);
	}
	else {
	  fprintf(stdout, "%s %.1f\n", timeString, temperature);
	  if (logfd != -1)
	    dprintf(logfd, "%s %.1f\n", timeString, temperature);
	}
      }
      // update last read time
      lastReadTime = clock();
    }

    if (poll(fds, 1, 0) == -1)
      error("Unable to poll");
    
    if (fds[0].revents & POLLIN) {
      ssize_t readBytes;
      if ((readBytes = read(fds[0].fd, buf, nbytes)) == -1)
	error("Unable to read from stdin");
      else if (readBytes == 0)
	shutdown();
      
      buf[readBytes-1] = '\0';

      char * userBuf = strtok(buf, "\n");
      while (userBuf != NULL) {
	if (strcmp(userBuf, "OFF") == 0) {
	  if (logfd != -1)
	    dprintf(logfd, "OFF\n");
	  shutdown();
	}
	else if (strcmp(userBuf, "STOP") == 0) {
	  if (logfd != -1) {
	    dprintf(logfd, "STOP\n");
	  }
	  canLog = 0;
	}
	else if (strcmp(userBuf, "START") == 0) {
	  if (logfd != -1) {
	    dprintf(logfd, "START\n");
          }
          canLog = 1;
	}
	else if (strcmp(userBuf, "SCALE=F") == 0) {
	  if (logfd != -1)
	    dprintf(logfd, "SCALE=F\n");
	  
	  tempScale = 'F';
	}
	else if (strcmp(userBuf, "SCALE=C") == 0) {
	  if (logfd != -1)
	    dprintf(logfd, "SCALE=C\n");
	  
	  tempScale = 'C';
	}
	else {
	  char substr[7];
	  memcpy(substr, &userBuf[0], 6);
	  substr[6] = '\0';
	  if (strcmp(substr, "PERIOD") == 0 && userBuf[6] == '=') {
	    char seconds[1024];
	    int j, k;
	    for (j = 7, k = 0; userBuf[j] != '\0'; j++, k++) {
	      if (!isdigit(userBuf[j])) {
		fprintf(stdout, "%s\n", userBuf);
		if (logfd != -1)
		  dprintf(logfd, "%s\n", userBuf);
		
		error("Invalid command");
	      }
	      seconds[k] = userBuf[j];
	    }
	    
	    seconds[k] = '\0';
	    period = atoi(seconds);
	    //	    lastReadTime = -1;
	    if (logfd != -1)
	      dprintf(logfd, "PERIOD=%d\n", period);	      
	  }
	  else {
	    if (logfd != -1)
	      dprintf(logfd, "%s\n", userBuf);	   
	    error("Invalid command");
	  }
	}
	userBuf = strtok(NULL, "\n");
      }
    }
    
    if (mraa_gpio_read(adc_d3) == 1)
      shutdown();

    if (mraa_gpio_read(adc_d3) == -1)
      error("Unable to read from pin 3");
  }

  if (fds[0].revents & (POLLHUP + POLLERR)) {
    shutdown();
  }

  return MRAA_SUCCESS;
}
