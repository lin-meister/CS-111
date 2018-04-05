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

void sigpipe_handler(int signum) {
  fprintf(stderr, "Caught a SIGPIPE signal\r\n");
  //process_shutdown(pid);
  exit(0);
}

void error(char* msg) {
  reset_input_mode();
  fprintf(stderr, "\n%s\n", msg);
  exit(1);
}

/* Helper function, reads from readfd and sends to sockfd, while writing to
   writefd */
int process_io(int readfd, int writefd, int sockfd, int logfd) {
  /* Set up buffers for reading/writing */
  const size_t BUFFER_SIZE = 1024;
  char buf[BUFFER_SIZE];
  char crlf[3] = {'\r', '\n', '\0'};
  char lf[2] = {'\n', '\0'};
  ssize_t readSize;

  if ((readSize = read(readfd, buf, BUFFER_SIZE)) < 0) {
    error("Error in read");
  }

  int i;
  for (i = 0; i < readSize; i++) {
    switch (buf[i]) {
    case '\004':
        if (write(writefd, &buf[i], 1) == -1) {
      error("Error in write");
        }
    case '\r':
      if (write(writefd, &buf[i], 1) == -1) {
	error("Error in write");
      }
      break;
    case '\n':
      if (write(writefd, &buf[i], 1) == -1) {
	error("Error in write");
      }
      break;
    default:
      if (write(writefd, &(buf[i]), 1) == -1) {
	error("Error in write");
      }
      write(sockfd, &(buf[i]), 1);
      if (logfd != -1) {
	if (write(logfd, &(buf[i]), 1) == -1) {
	  error("Error in write");
	}
      }
      return 0;
    }
  }
}

int
main (int argc, char **argv)
{
  /* Read options (if any) */
    static struct option long_options[] =
      {
	{"shell", no_argument, 0, 's'},
	{"port", required_argument, 0, 'p'},
	{"log", required_argument, 0, 'l'},
	{0, 0, 0, 0}
      };

    /*option flags*/
    int shellFlag = 0;
    int port = -1;
    char * logFile = NULL;

    /* getopt_long stores the option index here. */
    int option_index = 0;
    char c;

    while ((c = getopt_long(argc, argv, "sp:l:", long_options, &option_index)) != -1 ||
	   (c = getopt(argc, argv, "sp:l:")) != -1) {
      switch (c)
	{
	case 's':
	  shellFlag = 1;
	  break;

	case 'p':
	  port = atoi(optarg);
	  break;

	case 'l':
	  logFile = optarg;
	  break;

	case '?':
	  /* getopt_long already printed an error message. */
	  fprintf(stderr, "Usage: ./lab1b-client [OPTIONS]\n");
	  exit(1);

	default:
	  abort ();
	}
    }

    /* Change the input mode */
    set_input_mode();

    /* Create log file if specified */
    int logfd = -1;
    if (logFile != NULL) {
      if ((logfd = creat(logFile, 0666)) == -1) {
	error("Error in creat");
      }
    }

    if (port > 1024) {
      int sockfd;

      struct sockaddr_in serv_addr;
      struct hostent *server;

      sockfd = socket(AF_INET, SOCK_STREAM, 0);
      if (sockfd < 0)
	error("ERROR opening socket");
      server = gethostbyname("localhost");
      if (server == NULL) {
	fprintf(stderr,"ERROR, no such host\n");
	exit(0);
      }
      bzero((char *) &serv_addr, sizeof(serv_addr));
      serv_addr.sin_family = AF_INET;
      bcopy((char *)server->h_addr,
	    (char *)&serv_addr.sin_addr.s_addr,
	    server->h_length);
      serv_addr.sin_port = htons(port);
      if (connect(sockfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr)) < 0)
	error("ERROR connecting");

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

      // Poll between reading from keyboard and reading from port
      for (;;) {
	if (poll(fds, 2, 0) == -1) {
	  error("Unable to poll");
        }

   	if (fds[0].revents & POLLIN) {
	  if (process_io(fds[0].fd, STDOUT_FILENO, sockfd, logfd) == -1)
	    break;
	}
	if (fds[1].revents & POLLIN) {
    	  if (process_io(fds[1].fd, STDOUT_FILENO, sockfd, logfd) == -1)
	    break;
	}
	if (fds[1].revents & (POLLHUP + POLLERR)) {
	  fprintf(stderr, "\nFinished reading from server\r\n");
	  break;
	}
      }
    }
    return EXIT_SUCCESS;
}
