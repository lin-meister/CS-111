/*
  NAME: Chaoran Lin
  EMAIL: linmc@ucla.edu
  ID: 004674598
*/

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <getopt.h>
#include <sys/wait.h>
#include <poll.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <strings.h>
#include <mcrypt.h>

/* Use this variable to remember original terminal attributes. */
struct termios saved_terminal_modes;

void error();

void reset_input_mode ()
{
  if (tcsetattr (STDIN_FILENO, TCSANOW, &saved_terminal_modes) == -1) {
    error("Unable to restore terminal modes");
  }
}

void set_input_mode ()
{
  /* Make sure stdin is a terminal. */
  if (!isatty (STDIN_FILENO))
    {
      error("Not a terminal.");
    }

  struct termios new_termios;

  /* Save the current terminal modes so we can restore them later. */
  if (tcgetattr (STDIN_FILENO, &saved_terminal_modes) == -1) {
    error("Error in tcgetattr");
  }
  atexit(reset_input_mode);

  /* Set the modes of the new termios. */
  if (tcgetattr (STDIN_FILENO, &new_termios) < 0) {
    error("Error in tcgetattr");
  }

  new_termios.c_iflag = ISTRIP;
  new_termios.c_oflag = 0;
  new_termios.c_lflag = 0;

  /* Set it immediately as the active terminal */
  if (tcsetattr (STDIN_FILENO, TCSANOW, &new_termios) == -1) {
    error("Error in tcsetattr");
  }
}

void error(char* msg) {
  fprintf(stderr, "%s\r\n", msg);
  exit(1);
}

int encrypt(char* buf, int bufSize, char* key, int keySize) {
  MCRYPT td = mcrypt_module_open("twofish", NULL, "cfb", NULL);
  if (td==MCRYPT_FAILED) {
    error("Unable to open encrypt module");
  }

  char* IV = "AAAAAAAAAAAAAAAA";

  /* Initialize buffers */
  if (mcrypt_generic_init(td, key, keySize, IV) < 0) {
    error("Couldn't initialize buffers for decryption");
  }

  /* Encryption in CFB is performed in bytes */
  mcrypt_generic(td, buf, bufSize);
  mcrypt_generic_deinit(td);
  mcrypt_module_close(td);
  return 0;
}

int decrypt(char* buf, int bufSize, char* key, int keySize) {
  MCRYPT td = mcrypt_module_open("twofish", NULL, "cfb", NULL);
  if (td==MCRYPT_FAILED) {
    error("Unable to open encrypt module");
  }

  char* IV = "AAAAAAAAAAAAAAAA";

  /* Initialize buffers */
  if (mcrypt_generic_init(td, key, keySize, IV) < 0) {
    error("Couldn't initialize buffers for decryption");
  }

  /* Decryption in CFB is performed in bytes */
  mdecrypt_generic(td, buf, bufSize);
  mcrypt_generic_deinit(td);
  mcrypt_module_close(td);
  return 0;
}

/* Helper function for logging */
void log_to_file(int logfd, int isFromServer, int readBytes, char str[]) {
  if (logfd == -1)
    return;

  char msg[20];
  char received[] = "RECEIVED ";
  char sent[] = "SENT ";
  char bytes[] = " bytes: ";

  if (isFromServer == 1)
    strcpy(msg, received);
  else
    strcpy(msg, sent);

 
  sprintf(msg, "%s%d", msg, readBytes);
  strcat(msg, bytes);
  if (write(logfd, msg, strlen(msg)) < 0)
    error("Unable to write to logfile");

  if (write(logfd, str, readBytes) < 0)
    error("Unable to write to logfile");

  char newLine = '\n';
  if (write(logfd, &newLine, 1) < 0)
    error("Unable to write to logfile");
}

/* Helper function, reads from readfd and sends to sockfd, while writing to
   writefd */
int process_io(int readfd, int writefd, int sockfd, int logfd, char* key, int keySize, int shouldCrypt) {
  /* Set up buffers for reading/writing */
  const size_t BUFFER_SIZE = 1024;
  char buf[BUFFER_SIZE];
  char crlf[3] = {'\r', '\n', '\0'};
  ssize_t readSize;

  if ((readSize = read(readfd, buf, BUFFER_SIZE)) < 0) {
    error("Error in read");
  }

  int i;
  for (i = 0; i < readSize; i++) {
    if (logfd != -1 && sockfd == -1) {
      log_to_file(logfd, 1, 1, &buf[i]);
    }

    switch (buf[i]) {
    case '\r': {
      char lf[2] = {'\n', '\0'};
      if (sockfd != -1) {
	if (write(sockfd, lf, 1) == -1) {
	  error("Error in write");
	}
	if (logfd != -1)
          log_to_file(logfd, 0, 2, crlf);

	if (write(writefd, crlf, 2) == -1) {
	  error("Error in write");
	}
      }
      else {
	if (write(writefd, &buf[i], 1) == -1) {
	  error("Error in write");
	}
      }
      break;
    }
    case '\n': {
      char lf[2] = {'\n', '\0'};
      if (sockfd != -1) {
        if (write(sockfd, lf, 1) == -1) {
          error("Error in write");
        }
	if (logfd != -1)
          log_to_file(logfd, 0, 2, crlf);
	if (write(writefd, crlf, 2) == -1) {
          error("Error in write");
        }
      }
      else {
        if (write(writefd, &buf[i], 1) == -1) {
          error("Error in write");
        }
      }
      break;
    }
    default:
      // Decrypt before printing/logging
      if (sockfd == -1) {
	if (shouldCrypt == 1)
          decrypt(&buf[i], 1, key, keySize);
      }

      if (write(writefd, &(buf[i]), 1) == -1)
        error("Error in write");

      if (sockfd != -1) {
	if (shouldCrypt == 1 && buf[i] != '\003' && buf[i] != '\004')
	  encrypt(&buf[i], 1, key, keySize);
	if (write(sockfd, &(buf[i]), 1) == -1) 
	  error("Error in write");
	if (logfd != -1)
	  log_to_file(logfd, 0, 1, &buf[i]);
      }
    }
  }

  return 0;
}

int
main (int argc, char **argv)
{
  /* Read options (if any) */
    static struct option long_options[] =
      {
	{"port", required_argument, 0, 'p'},
	{"log", required_argument, 0, 'l'},
	{"encrypt", required_argument, 0, 'e'},
	{0, 0, 0, 0}
      };

    /*option flags*/
    int port = -1;
    char * logFile = NULL;
    char * keyFile = NULL;

    /* getopt_long stores the option index here. */
    int option_index = 0;
    char c;

    while ((c = getopt_long(argc, argv, "p:l:e:", long_options, &option_index)) != -1 ||
	   (c = getopt(argc, argv, "p:l:e:")) != -1) {
      switch (c)
	{
	case 'p':
	  port = atoi(optarg);
	  break;

	case 'l':
	  logFile = optarg;
	  break;

	case 'e':
	  keyFile = optarg;
	  break;

	case '?':
	  /* getopt_long already printed an error message. */
	  fprintf(stderr, "Usage: ./lab1b-client [OPTIONS]\n");
	  exit(1);

	default:
	  abort ();
	}
    }

    int sockfd;
    
    struct sockaddr_in serv_addr;
    struct hostent *server;
    
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
      error("ERROR opening socket");
    server = gethostbyname("localhost");
    if (server == NULL) {
      error("ERROR, no such host\n");
    }
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr,
	  (char *)&serv_addr.sin_addr.s_addr,
	  server->h_length);
    serv_addr.sin_port = htons(port);
    if (connect(sockfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr)) < 0)
      error("ERROR connecting");
    
    // Create log file if specified
    int logfd = -1;
    if (logFile != NULL) {
      if ((logfd = open(logFile, O_WRONLY | O_CREAT | O_TRUNC, 0666)) < 0) {
	error("Error in opening log file");
      }
    }
    
    // Change the input mode
    set_input_mode();
    
    /* Create pipes */
    int pipeFromServer[2];
    if (pipe(pipeFromServer) == -1) {
      error("Unable to create pipe to server");
    }
    
    struct pollfd fds[2];
    fds[0].fd = STDIN_FILENO;
    fds[0].events = POLLIN | POLLHUP | POLLERR;
    fds[1].fd = sockfd;
    fds[1].events = POLLIN | POLLHUP | POLLERR;
    
    const size_t keyBufSize = 16;
    char keyBuf[keyBufSize];
    int shouldCrypt = 0;
    
    if (keyFile != NULL) {
      int keyfd = open(keyFile, O_RDONLY);
      if (keyfd < 0)
	error("Unable to open keyfile");
      if (read(keyfd, keyBuf, keyBufSize) == -1)
	error("Unable to read keyfile");
      shouldCrypt = 1;
    }
    
    char recvBuf[200];
    // Poll between reading from keyboard and reading from port
    for (;;) {
      if (poll(fds, 2, 0) == -1) {
	error("Unable to poll");
      }
      
      if (recv(sockfd, recvBuf, 200, MSG_PEEK | MSG_DONTWAIT) == 0) {
	break;
      }
      
      if (fds[0].revents & POLLIN) {
	if (process_io(fds[0].fd, STDOUT_FILENO, sockfd, logfd, keyBuf, keyBufSize, shouldCrypt) == -1)
	  break;
      }
      
      if (fds[1].revents & POLLIN) {
	if (process_io(fds[1].fd, STDOUT_FILENO, -1, logfd, keyBuf, keyBufSize, shouldCrypt) == -1)
	  break;
      }
      if (fds[1].revents & (POLLHUP + POLLERR)) {
	fprintf(stderr, "Finished reading from server\r\n");
	break;
      }
    }
    
    return 0;
}

