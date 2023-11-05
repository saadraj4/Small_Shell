// Header Files
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>

// Global Variables
#define INPUTLENGTH 2048
int allowBackground = 1;

// Function Prototypes
void getInput(char *[], int *, char[], char[], int);
void execCmd(char *[], int *, struct sigaction, int *, char[], char[]);
void catchSIGTSTP(int);
void printExitStatus(int);

// Prompts the user and parses the input into an array of words

void getInput(char *arr[], int *background, char inputName[], char outputName[], int pid)
{
	char input[INPUTLENGTH];
	int i, j;
	// Get user input
	printf(": ");
	fflush(stdout);
	fgets(input, INPUTLENGTH, stdin);
	// Remove the newline character from input
	int found = 0;
	for (i = 0; !found && i < INPUTLENGTH; i++)
	{
		if (input[i] == '\n')
		{
			input[i] = '\0';
			found = 1;
		}
	}

	// If the input is blank, return an array with an empty string
	if (!strcmp(input, ""))
	{
		arr[0] = strdup("");
		return;
	}

	// Tokenize the raw input into individual strings
	const char space[2] = " ";
	char *token = strtok(input, space);

	for (i = 0; token; i++)
	{
		// Check for '&' to determine if it's a background process
		if (!strcmp(token, "&"))
		{
			*background = 1;
		}
		// Check for '<' to denote an input file
		else if (!strcmp(token, "<"))
		{
			token = strtok(NULL, space);
			strcpy(inputName, token);
		}
		// Check for '>' to denote an output file
		else if (!strcmp(token, ">"))
		{
			token = strtok(NULL, space);
			strcpy(outputName, token);
		}
		// Otherwise, it's part of the command
		else
		{
			arr[i] = strdup(token);

			// Replace "$$" with the process ID (pid)
			// This only occurs at the end of a string in the test script
			for (j = 0; arr[i][j]; j++)
			{
				if (arr[i][j] == '$' &&
					arr[i][j + 1] == '$')
				{
					arr[i][j] = '\0';
					snprintf(arr[i], 256, "%s%d", arr[i], pid);
				}
			}
		}
		// Move to the next token
		token = strtok(NULL, space);
	}
}

// function to execute a command, including handling input and output redirection, and managing background and foreground processes.
void execCmd(char *arr[], int *childExitStatus, struct sigaction sa, int *background, char inputName[], char outputName[])
{
	int input, output, result;
	pid_t spawnPid = -5; // Initialize spawnPid to an invalid value

	// Fork a new process
	spawnPid = fork();

	switch (spawnPid)
	{
	case -1:
		perror("Fork Error!\n"); // Error handling for fork failure
		exit(1);
		break;

	case 0:
		// Child process
		// Set the process to handle SIGINT (Ctrl-C) with default behavior
		sa.sa_handler = SIG_DFL;
		sigaction(SIGINT, &sa, NULL);

		// Handle input redirection if an input file is specified
		if (strcmp(inputName, "") > 0)
		{
			input = open(inputName, O_RDONLY); // Open the input file
			if (input == -1)
			{
				exit(1); // Error handling for opening the input file
			}
			result = dup2(input, 0); // Redirect standard input to the input file
			if (result == -1)
			{
				perror("Unable to assign input file\n");
				exit(2); // Error handling for input redirection
			}
			close(input); // Close the input file descriptor after duplication
		}

		// Handle output redirection if an output file is specified
		if (strcmp(outputName, "") > 0)
		{
			output = open(outputName, O_WRONLY | O_CREAT | O_TRUNC, 0666); // Open the output file
			if (output == -1)
			{
				perror("Unable to open output file\n"); // Error handling for opening the output file
				exit(1);
			}
			result = dup2(output, 1); // Redirect standard output to the output file
			if (result == -1)
			{
				perror("Unable to assign output file\n");
				exit(2); // Error handling for output redirection
			}
			close(output); // Close the output file descriptor after duplication
		}

		// Execute the command specified in arr[0]
		if (execvp(arr[0], arr) == -1)
		{
			perror("bash: badfile "); // Error message for command execution failure
			exit(2);
		}
		break;

	default:
		// Parent process
		if (*background && allowBackground)
		{
			// Background process handling
			printf("background pid is %d\n", spawnPid);
			fflush(stdout);
		}
		else
		{
			// Wait for the child process to complete
			pid_t actualPid = waitpid(spawnPid, childExitStatus, 0);
		}

		// Check for terminated background processes
		while ((spawnPid = waitpid(-1, childExitStatus, WNOHANG)) > 0)
		{
			printf("child %d terminated\n", spawnPid);
			printExitStatus(*childExitStatus);
			fflush(stdout);
		}
		break;
	}
}

// function toggles the allowBackground boolean variable between 1 and 0, displaying appropriate messages when switching between foreground-only and background modes.
void catchSIGTSTP(int signo)
{
	// If it's 1, set it to 0 and display a message
	if (allowBackground == 1)
	{
		char *message = "\nEntering foreground-only mode (& is now ignored)\n";
		write(1, message, 49); // Write the message to the standard output
		fflush(stdout);		   // Flush the standard output buffer
		allowBackground = 0;   // Set the background flag to 0
	}

	// If it's 0, set it to 1 and display a message reentrantly
	else
	{
		char *message = "Exiting foreground-only mode\n";
		write(1, message, 30); // Write the message to the standard output
		fflush(stdout);		   // Flush the standard output buffer
		allowBackground = 1;   // Set the background flag to 1
	}
}

// prints the exit value
void printExitStatus(int childExitMethod)
{
	if (WIFEXITED(childExitMethod))
	{
		// If exited by status
		printf("exit value %d\n", WEXITSTATUS(childExitMethod));
	}
	else
	{
		// If terminated by signal
		printf("terminated by signal %d\n", WTERMSIG(childExitMethod));
	}
}

// main function to run the code and small shell commands
int main()
{
	int pid = getpid();
	int cont = 1;
	int i;
	int exitStatus = 0;
	int background = 0;

	char inputFile[256] = "";
	char outputFile[256] = "";
	char *input[512];

	// Initialize the input array to NULL
	for (i = 0; i < 512; i++)
	{
		input[i] = NULL;
	}

	// Signal Handlers

	// Ignore ^C (Ctrl+C)
	struct sigaction sa_sigint = {0};
	sa_sigint.sa_handler = SIG_IGN;
	sigfillset(&sa_sigint.sa_mask);
	sa_sigint.sa_flags = 0;
	sigaction(SIGINT, &sa_sigint, NULL);

	// Redirect ^Z (Ctrl+Z) to the catchSIGTSTP() function
	struct sigaction sa_sigtstp = {0};
	sa_sigtstp.sa_handler = catchSIGTSTP;
	sigfillset(&sa_sigtstp.sa_mask);
	sa_sigtstp.sa_flags = 0;
	sigaction(SIGTSTP, &sa_sigtstp, NULL);

	do
	{
		// Get user input and parse it
		getInput(input, &background, inputFile, outputFile, pid);

		// Handle COMMENT OR BLANK
		if (input[0][0] == '#' || input[0][0] == '\0')
		{
			continue; // Skip comments and blank lines
		}

		// Handle EXIT command
		else if (strcmp(input[0], "exit") == 0)
		{
			cont = 0; // Set the loop exit condition
		}

		// Handle CD command
		else if (strcmp(input[0], "cd") == 0)
		{
			// Change to the directory specified or go to the home directory if none is specified
			if (input[1])
			{
				if (chdir(input[1]) == -1)
				{
					printf("Directory not found.\n");
					fflush(stdout);
				}
			}
			else
			{
				chdir(getenv("HOME"));
			}
		}

		// Handle STATUS command
		else if (strcmp(input[0], "status") == 0)
		{
			printExitStatus(exitStatus); // Print the exit status
		}

		// Handle other commands
		else
		{
			execCmd(input, &exitStatus, sa_sigint, &background, inputFile, outputFile);
		}

		// Reset variables
		for (i = 0; input[i]; i++)
		{
			input[i] = NULL;
		}
		background = 0;
		inputFile[0] = '\0';
		outputFile[0] = '\0';

	} while (cont);

	return 0;
}
