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

/* Pid for forking */
pid_t pid;

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

void process_shutdown(pid_t pid) {
  /* Wait for the program to shut down and print its exit signal/status */
  pid_t w;
  int status;

  do {
    if ((w = waitpid(pid, &status, WUNTRACED | WCONTINUED)) == -1) {
      error("Error in waitpid");
      break;
    }
  } while (!(WIFEXITED(status)) && !(WIFSIGNALED(status)));

  fprintf(stderr, "SHELL EXIT SIGNAL=%d STATUS=%d\r\n",
	  WTERMSIG(status), WEXITSTATUS(status));
}

void sigpipe_handler(int signum) {
  fprintf(stderr, "Caught a SIGPIPE signal\r\n");
  process_shutdown(pid);
  exit(0);
}

void error(char* msg) {
  reset_input_mode();
  fprintf(stderr, "%s\n", msg);
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

  
  printf("Before encrypt: ");
  int i;
  for (i = 0; i < bufSize; i++)
    fprintf(stdout, "%c\n", buf[i]);
 
  mcrypt_generic(td, buf, bufSize);
  
  printf("After encrypt: ");

  for (i = 0; i < bufSize; i++)
    fprintf(stdout, "%c\n", buf[i]);
 
  /* Encryption in CFB is performed in bytes */
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

/* Helper function, reads from readfd and sends to sockfd, while writing to
   writefd */
int process_io(int readfd, int writefd, int isToClient, char* key, int keySize, int shouldCrypt) {
  /* Set up buffers for reading/writing */
  const size_t BUFFER_SIZE = 1024;
  char buf[BUFFER_SIZE];
  char lf[2] = {'\n', '\0'};
  ssize_t readSize;

  if ((readSize = read(readfd, buf, BUFFER_SIZE)) < 0) {
    if (kill(pid, SIGTERM) == -1) 
      error("Unable to send SIGTERM to child process");
    error("Error in read");
  }

  int i;
  for (i = 0; i < readSize; i++) {
    switch (buf[i]) {
    case '\003':
      if (kill(pid, SIGINT) == -1) {
	error("Unable to send SIGINT to child process");
      }
      return -1;
    case '\004':
      if (isToClient == 1) {
	if (close(writefd) == -1) {
          error("Unable to close pipe to shell");
        }
	process_shutdown(pid);
      }
      else {
	if (kill(pid, SIGTERM) == -1) {
	  error("Unable to send SIGTERM to child process");
	}
      }
      return -1;
    case '\r': {
      if (isToClient == 1) {
	char crlf[3] = {'\r', '\n', '\0'};
	if (write(writefd, crlf, 2) == -1) {
	  error("Error in write");
	}
      }
      else {
	if (write(writefd, lf, 1) == -1) {
	  error("Error in write");
	}
      }
      break;
    }
    case '\n': {
      if (isToClient == 1) {
	char crlf[3] = {'\r', '\n', '\0'};
        if (write(writefd, crlf, 2) == -1) {
          error("Error in write");
        }
      }
      else {
	if (write(writefd, lf, 1) == -1) {
	  error("Error in write");
	}
      }
      break;
    }
    default:
      if (isToClient == 1 && shouldCrypt == 1) {
	encrypt(&buf[i], 1, key, keySize);	
      }

      if (isToClient != 1 && shouldCrypt == 1) {
	decrypt(&buf[i], 1, key, keySize);
      }
      
      if (write(writefd, &(buf[i]), 1) == -1) {
        error("Error in write");
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
      {"encrypt", required_argument, 0, 'e'},
      {0, 0, 0, 0}
    };
  
  /*option flags*/
  int port = 0;
  char * logFile = NULL;
  char * keyFile = NULL;
  
  /* getopt_long stores the option index here. */
  int option_index = 0;
  char c;
  
  while ((c = getopt_long(argc, argv, "p:e:", long_options, &option_index)) != -1 ||
	 (c = getopt(argc, argv, "p:e:")) != -1) {
    switch (c)
      {
      case 'p':
	port = atoi(optarg);
	break;
	
      case 'e':
	keyFile = optarg;
	break;
	
      case '?':
	/* getopt_long already printed an error message. */
	fprintf(stderr, "Usage: ./lab1b-server [OPTIONS]\n");
	exit(1);
	
      default:
	abort ();
      }
  }
  
  set_input_mode();
  
  int sockfd, newsockfd, clilen;
  struct sockaddr_in serv_addr, cli_addr;
  
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0)
    error("ERROR opening socket");
  
  bzero((char *) &serv_addr, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  serv_addr.sin_port = htons(port);
  
  /* Bind the socket */
  if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
    error("Unable to bind");
  }
  
  // Listen to socket
  listen(sockfd, 5);
  clilen = sizeof(cli_addr);
  
  /* Accept from socket */
  newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
  if (newsockfd < 0)
    error( "Unable to accept from client");
  
  /* Create pipes */
  int pipeFromShell[2];
  if (pipe(pipeFromShell) == -1) {
    error("Unable to create pipe from shell");
  }
  int pipeToShell[2];
  if (pipe(pipeToShell) == -1) {
    error("Unable to create pipe to shell");
  }
  int pipeFromClient[2];
  if (pipe(pipeFromClient) == -1) {
    error("Unable to create pipe from client");
  }
  int pipeToClient[2];
  if (pipe(pipeToClient) == -1) {
    error("Unable to create pipe to client");
  }
  
  // Create pollfd
  struct pollfd fds[2];
  fds[0].fd = newsockfd;
  fds[0].events = POLLIN | POLLHUP | POLLERR;
  fds[1].fd = pipeFromShell[0];
  fds[1].events = POLLIN | POLLHUP | POLLERR;
  
  // Fork a child process
  pid = fork();
  if (pid == -1) {
    error("Unable to fork child process");
  }
  
  if (pid == 0) {
    printf("In the child with pid %d\r\n", (int)getpid(), pid);
    
    if (close(STDIN_FILENO) == -1) {
      error("Error in close");
    }
    if (dup(pipeToShell[0]) == -1) {
      error("Error in dup");
    }
    if (close(STDOUT_FILENO) == -1) {
      error("Error in close");
    }
    if (dup(pipeFromShell[1]) == -1) {
      error("Error in dup");
    }
    if (close(STDERR_FILENO) == -1) {
      error("Error in close");
    }
    if (dup(pipeFromShell[1]) == -1) {
      error("Error in dup");
    }
    if (close(pipeToShell[1]) == -1) {
      error("Error in close");
    }
    if (close(pipeFromShell[0]) == -1) {
      error("Error in close");
    }
    
    char* execArgs[2] = {"/bin/bash", NULL};
    if (execvp(execArgs[0], execArgs) == -1) {
      error("Unable to execute shell");
    }
    }
  
  /* Else we are in parent process */
  else {
    /* Signal to catch SIGPIPE */
    signal(SIGPIPE, &sigpipe_handler);
      // close(pipeFromShell[1]);
      // close(pipeToShell[0]);
    
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
    
    for (;;) {
      if (poll(fds, 2, 0) == -1) {
	error("Unable to poll");
      }
      if (recv(newsockfd, recvBuf, 200, MSG_PEEK | MSG_DONTWAIT) == 0) {
	if (kill(pid, SIGTERM) == -1) {
	  error("Unable to send SIGTERM to child process");
	}
	break;
      }
      // Process from port (socket), and write to shell
      if (fds[0].revents & POLLIN) {
	if (process_io(fds[0].fd, pipeToShell[1], 0, keyBuf, keyBufSize, shouldCrypt) == -1)
	  break;
      }
      // Process from shell and write to socket
      if (fds[1].revents & POLLIN) {
	if (process_io(fds[1].fd, newsockfd, 1, keyBuf, keyBufSize, shouldCrypt) == -1)
	  break;
      }
      if (fds[0].revents & (POLLHUP + POLLERR)) {
	fprintf(stderr, "Finished reading from client\r\n");
	if (kill(pid, SIGTERM) == -1) {
	  error("Unable to send SIGTERM to child process");
	}	    
	break;
      }
      if (fds[1].revents & (POLLHUP + POLLERR)) {
	fprintf(stderr, "Finished reading from shell\r\n");
	break;
      }
    }
    
    process_shutdown(pid);
  }
  
  return 0;
}
