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

/* Use this variable to remember original terminal attributes. */
struct termios saved_terminal_modes;

/* Pid for forking */
pid_t pid;

void reset_input_mode ()
{
  if (tcsetattr (STDIN_FILENO, TCSANOW, &saved_terminal_modes) == -1) {
    fprintf(stderr, "%s\n", strerror(errno));
    exit(1);
  }
}

void set_input_mode ()
{
  /* Make sure stdin is a terminal. */
  if (!isatty (STDIN_FILENO))
  {
    fprintf (stderr, "Not a terminal.\n");
    exit (EXIT_FAILURE);
  }

  struct termios new_termios;
  
  /* Save the current terminal modes so we can restore them later. */
  if (tcgetattr (STDIN_FILENO, &saved_terminal_modes) == -1) {
    fprintf(stderr, "%s\n", strerror(errno));
    exit(1);
  }
  atexit(reset_input_mode);

  /* Set the modes of the new termios. */
  if (tcgetattr (STDIN_FILENO, &new_termios) < 0) {
    fprintf(stderr, "%s\n", strerror(errno));
    exit(1);
  };

  new_termios.c_iflag = ISTRIP;
  new_termios.c_oflag = 0;
  new_termios.c_lflag = 0;

  /* Set it immediately as the active terminal */
  if (tcsetattr (STDIN_FILENO, TCSANOW, &new_termios) == -1) {
    fprintf(stderr, "%s\n", strerror(errno));
    exit(1);
  }
}

void process_shutdown(pid_t pid) {
  /* Wait for the program to shut down and print its exit signal/status */
  pid_t w;
  int status;

  do {
    if ((w = waitpid(pid, &status, WUNTRACED | WCONTINUED)) == -1) {
      fprintf(stderr, "%s\n", strerror(errno));
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

int
main (int argc, char **argv)
{
  /* Read options (if any) */
  static struct option long_options[] =
   {
     {"shell", no_argument, 0, 's'},
     {0, 0, 0, 0}
   };

  /*option flags*/
  int shellFlag = 0;
  
  /* getopt_long stores the option index here. */
  int option_index = 0;
  char c;

  while ((c = getopt_long(argc, argv, "s", long_options, &option_index)) != -1 ||
	 (c = getopt(argc, argv, "s")) != -1) {
    switch (c)
     {
     case 's':
       shellFlag = 1;
       break;
       
     case '?':
       /* getopt_long already printed an error message. */
       fprintf(stderr, "Usage: ./lab1a [OPTIONS]\n");
       exit(1);

     default:
       abort ();
     }
  }

  /* Set up buffers for reading/writing */
  const size_t BUFFER_SIZE = 1024;
  char buf[BUFFER_SIZE];
  char crlf[3] = {'\r', '\n', '\0'};
  char lf[2] = {'\n', '\0'};
  ssize_t readSize;

  /* Change the input mode */
  set_input_mode();
  
  if (shellFlag == 1) {
    /* Create pipes */
    int pipeFromShell[2];
    int pipeToShell[2];
    if (pipe(pipeFromShell) == -1) {
      fprintf(stderr, "%s\n", strerror(errno));
      exit(1);
    }
    if (pipe(pipeToShell) == -1) {
      fprintf(stderr, "%s\n", strerror(errno));
      exit(1);
    }
   
    /* Create two pollfd structures for stdin from keyboard and pipe from shell output */
    struct pollfd fds[2];
    fds[0].fd = STDIN_FILENO;
    fds[1].fd = pipeFromShell[0];
    fds[0].events = POLLIN | POLLHUP | POLLERR;
    fds[1].events = POLLIN | POLLHUP | POLLERR;
    
    /* Fork a child process */
    pid = fork();
    if (pid == -1) {
      fprintf(stderr, "%s\n", strerror(errno));
      exit(1);
    }

    /* Executed by child process */
    if (pid == 0) {
      printf("In the child with pid %d\r\n", (int)getpid(), pid);
 
      /* Close file descriptors for use for child */
      if (close(STDIN_FILENO) == -1) {
	fprintf(stderr, "%s\n", strerror(errno));
	exit(1);
      }
      if (dup(pipeToShell[0]) == -1) {
	fprintf(stderr, "%s\n", strerror(errno));
	exit(1);
      }
      if (close(STDOUT_FILENO) == -1) {
	fprintf(stderr, "%s\n", strerror(errno));
        exit(1);
      }
      if (dup(pipeFromShell[1]) == -1) {
	fprintf(stderr, "%s\n", strerror(errno));
	exit(1);
      }
      if (close(STDERR_FILENO) == -1) {
	fprintf(stderr, "%s\n", strerror(errno));
        exit(1);
      }
      if (dup(pipeFromShell[1]) == -1) {
	fprintf(stderr, "%s\n", strerror(errno));
        exit(1);
      }
      if (close(pipeToShell[1]) == -1) {
	fprintf(stderr, "%s\n", strerror(errno));
        exit(1);
      }
      if (close(pipeFromShell[0]) == -1) {
	fprintf(stderr, "%s\n", strerror(errno));
        exit(1);
      }

      char* execArgs[2] = {"/bin/bash", NULL};
      if (execvp(execArgs[0], execArgs) == -1) {
	fprintf(stderr, "%s\n", strerror(errno));
	exit(1);
      }
    }

    /* Executed by parent process */
    else {
      /* Signal to catch SIGPIPE */
      signal(SIGPIPE, &sigpipe_handler);

      /* Main loop */
      int canBreak = 0;
      while (canBreak == 0) {
	if (poll(fds, 2, 0) == -1) {
	  fprintf(stderr, "%s\n", strerror(errno));
	  exit(1);
	}
	int i;

	if (fds[0].revents & POLLIN) {
	    if ((readSize = read(fds[0].fd, buf, BUFFER_SIZE)) < 0) {
	      fprintf(stderr, "%s\n", strerror(errno));
              exit(1);
	    }

            for (i = 0; i < readSize; i++) {
              switch (buf[i]) {
             case '\003':
	       if (kill(pid, SIGINT) == -1) {
		 fprintf(stderr, "%s\n", strerror(errno));
		 exit(1);
	       }
		canBreak = 1;
                break;
              case '\004':		
		/* Close the pipe to the shell */
		if (close(pipeToShell[1]) == -1) {
		  fprintf(stderr, "%s\n", strerror(errno));
		  exit(1);
		}
		canBreak = 1;
		break;
              case '\r':
		write(STDOUT_FILENO, crlf, 3);
                write(pipeToShell[1], lf, 2);
                break;
              case '\n':
		write(STDOUT_FILENO, crlf, 3);
                write(pipeToShell[1], lf, 2);
                break;
              default:
		write(STDOUT_FILENO, &(buf[i]), 1);
                write(pipeToShell[1], &(buf[i]), 1);
              }
            }
	}
	
	if (fds[1].revents & POLLIN) {
	    if ((readSize = read(fds[1].fd, buf, BUFFER_SIZE)) < 0) {
	      fprintf(stderr, "%s\n", strerror(errno));
	      exit(1);
	    }
	    
	    for (i = 0; i < readSize; i++) {
	      switch (buf[i]) {
	      case '\004':
		break;
	      case '\r':
		write(STDOUT_FILENO, crlf, 3);
		break;
	      case '\n':
		write(STDOUT_FILENO, crlf, 3);
		break;
	      default:
		write(STDOUT_FILENO, &(buf[i]), 1);
	      }
	    }
 	}

	if (fds[1].revents & (POLLHUP + POLLERR)) {
	  fprintf(stderr, "Finished reading from shell\r\n");
	  canBreak = 1;
	  break;
	}
      }
    
      process_shutdown(pid);
    }  
  }

  /* Executes when Shellflag not specified */
  else {
    int canBreak = 0;
    while (canBreak == 0) {
      if ((readSize = read(STDIN_FILENO, buf, BUFFER_SIZE)) < 0) {
	fprintf(stderr, "%s\n", strerror(errno));
	exit(1);
      }
      
      int i;
      for (i = 0; i < readSize; i++) {
	switch(buf[i]) {
	case '\004':
	  canBreak = 1;
	  break;
	case '\r':
	  write(STDOUT_FILENO, crlf, 3);
	  break;
	case '\n':
	  write(STDOUT_FILENO, crlf, 3);
	  break;
	default:
	  write(STDOUT_FILENO, buf, 1);
	}
      }
    } 
  }
  
  return EXIT_SUCCESS;
}
