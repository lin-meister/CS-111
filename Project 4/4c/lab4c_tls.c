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
#include <netinet/in.h>
#include <netdb.h>
#include <openssl/ssl.h>
#include "mraa/aio.h"

const int B = 4275;               // B value of the thermistor
const int R0 = 100000;            // R0 = 100k

mraa_aio_context adc_a0;
mraa_gpio_context adc_d3;
uint16_t adc_value = 0;

int sockfd;
int logfd;
int canLog;

const SSL_METHOD *method;
SSL_CTX *ctx;
SSL *ssl;

void error();

void closeSensors() {
  mraa_aio_close(adc_a0);
  mraa_gpio_close(adc_d3);
}

void shutdownSensors() {
  const time_t t = time(NULL);
  struct tm * local = localtime(&t);
  char timeString[100];
  
  // format time
  strftime(timeString, 100, "%T", local);
  if (logfd != -1) {
    dprintf(logfd, "%s SHUTDOWN\n", timeString);
    close(logfd);
  }
  char tempBuf[100];   
  sprintf(tempBuf, "%s SHUTDOWN\n", timeString);
  if (SSL_write(ssl, tempBuf, strlen(tempBuf)) <= 0)
    error("Unable to SSL write");

  exit(0);
}

void invalid(char* msg) {
  closeSensors();
  fprintf(stderr, "%s\n", msg);
  exit(1);
}

void error(char* msg) {
  closeSensors();
  fprintf(stderr, "%s\n", msg);
  exit(2);
}

void initializeSSL() {
  if (SSL_library_init() < 0)
    error("Unable to initialize OpenSSL library");
  method = TLSv1_method();
  ctx = SSL_CTX_new(method);
  if (ctx == NULL)
    error("Unable to create new SSL context");

  ssl = SSL_new(ctx);
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
      {"id", required_argument, 0, 'i'},
      {"host", required_argument, 0, 'h'},
      {"log", required_argument, 0, 'l'},
      {0, 0, 0, 0}
    };
  
  /*option flags*/
  char * id = "004674598";
  char * hostName = "lever.cs.ucla.edu";
  char * logFile = NULL;

  // Get the port number from the last argument
  if (argc < 2)
    invalid("Must specify port number");
  unsigned long long int portNum = atoi(argv[argc-1]);
  
  /* getopt_long stores the option index here. */
  int option_index = 0;
  char c;
  
  while ((c = getopt_long(argc, argv, "i:h:l:", long_options, &option_index)) != -1 ||
	 (c = getopt(argc, argv, "i:h:l:")) != -1) {
    switch (c)
      {
      case 'i':
	if (strlen(optarg) != 9) {
	  invalid("ID must be 9 digits\n");
	}
	id = optarg;
	break;

      case 'h':
	hostName = optarg;
	break;

      case 'l':
	logFile = optarg;
	break;
	
      case '?':
	/* getopt_long already printed an error message. */
	fprintf(stderr, "Usage: ./lab4c_tls --id=######### --host=HOSTNAME --log=LOGFILE PORTNUM\n");
	exit(1);
	
      default:
	abort ();
      }
  }

  struct sockaddr_in serv_addr;
  struct hostent *server;

  // Initialize SSL connection
  initializeSSL();
  
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0)
    invalid("ERROR opening socket");
  server = gethostbyname(hostName);
  if (server == NULL)
    invalid("ERROR, no such host\n");

  bzero((char *) &serv_addr, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  bcopy((char *)server->h_addr,
	(char *)&serv_addr.sin_addr.s_addr,
	server->h_length);
  serv_addr.sin_port = htons(portNum);
  if (connect(sockfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr)) < 0)
    invalid("ERROR connecting");

  // Associate socket with ssl
  SSL_set_fd(ssl, sockfd);
  SSL_connect(ssl);
  
  // Immediately send an ID to server
  char sslWriteBuf[13];
  sprintf(sslWriteBuf, "ID=%s\n", id);

  // Set up log file
  canLog = 1;
  logfd = -1;
  if (logFile != NULL) {
    if ((logfd = creat(logFile, 0666)) == -1)
      error("Error in creating log file");
    canLog = 1;
  }

  // Immediately write ID to server
  if (SSL_write(ssl, sslWriteBuf, strlen(sslWriteBuf)) <= 0)
    error("Unable to SSL write");
  if (logfd != -1)
    dprintf(logfd, sslWriteBuf);

  // Set up polls
  struct pollfd fds[1];
  fds[0].fd = sockfd;
  fds[0].events = POLLIN | POLLHUP | POLLERR;

  clock_t lastReadTime = -1; // initial value
  char buf[1024];
  size_t nbytes = sizeof(buf);
  int period = 1;
  char tempScale = 'F';
  
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

      int ten = 10;
      if (canLog) {
	char tempBuf[100];
	
	if ((int) temperature < ten) {
	  sprintf(tempBuf, "%s 0%.1f\n", timeString, temperature);
	  if (SSL_write(ssl, tempBuf, strlen(tempBuf)) <= 0)
	    error("Unable to SSL write"); 

	  if (logfd != -1)
	    dprintf(logfd, "%s 0%.1f\n", timeString, temperature);
	}
	else {
	  sprintf(tempBuf, "%s %.1f\n", timeString, temperature);
	  if (SSL_write(ssl, tempBuf, strlen(tempBuf)) <= 0)
	    error("Unable to SSL write");
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
      if ((readBytes = SSL_read(ssl, buf, nbytes)) == -1)
	error("Unable to read from server");
      else if (readBytes == 0)
	shutdownSensors();

      buf[readBytes-1] = '\0';

      char * userBuf = strtok(buf, "\n");
      while (userBuf != NULL) {
	if (strcmp(userBuf, "OFF") == 0) {
	  if (logfd != -1)
	    dprintf(logfd, "OFF\n");
	  shutdownSensors();
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
		strcat(userBuf, "\n");
		if (SSL_write(ssl, userBuf, strlen(userBuf)) < 0)
		  error("Unable to SSL write");
		if (logfd != -1)
		  dprintf(logfd, "%s\n", userBuf);

		//error("Invalid command");
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
	    //	    error("Invalid command");
	  }
	}
	userBuf = strtok(NULL, "\n");
      }
    }
    
    if (mraa_gpio_read(adc_d3) == 1)
      shutdownSensors();

    if (mraa_gpio_read(adc_d3) == -1)
      error("Unable to read from pin 3");
  }

  if (fds[0].revents & (POLLHUP + POLLERR)) {
    shutdownSensors();
  }

  return MRAA_SUCCESS;
}
