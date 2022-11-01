#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>

int fd;
static char* args[512];
char line[512];
char *cmdArray[32];
pid_t pid;
int outputRedirect, inputRedirect, background;
int pid, execStatus;
char *inputRedirectFile;
char *outputRedirectFile;
static int cmdRun(int, int, int, char *cmdArray);

char *mystrdup(const char *c) { // implementing strdup (not supported by version of C on Bandit?)
    char *p = malloc(strlen(c) + 1);  // allocate memory space for new string
    if (p) {    // checking p is non-empty
        memcpy(p, c, strlen(c) + 1);  // copy contents from original string to new string
    }
    return p;
}

void reset() {  // clearing global variables
    fd = 0;     // reset file descripter variable
    pid = 0;    // reset process identifier variable
    line[0] = 0;    // initialize input commands as null
    outputRedirect = 0;     // flag for output redirection
    inputRedirect = 0;      // flag for input redirection
    background = 0; // flag for background process call
}

char* trim(char* str) { // triming leading/trailing whitespace --> deadline 1 testing
        while (isspace(*str)) { //trimming leading
            ++str;
        }

        char* end;
        end = str + strlen(str) + 1;

        while(isspace(*end) && (end > str)) {
            end--;
        }
        end[1] = 0;

        return str;
}

void cmdTok(char *str) {  // parse and tokenise individual command by space character
    int i = 1;

    args[0] = strtok(str," ");
    while ((args[i] = strtok(NULL," ")) != NULL)
        i++;
}

void inputOutputTok(char *cmdArray) { // parse and tokensise command by '<' and '>'
    char *inputOutputArray[32];     // arrays of command strings
    char *cmdProcessed = mystrdup(cmdArray);  // duplicate cmdArray to parse without affecting original

    int i = 1;
    inputOutputArray[0] = strtok(cmdProcessed,"<");
    while ((inputOutputArray[i] = strtok(NULL,">")) != NULL) {
        i++;
    }

    inputOutputArray[1] = trim(inputOutputArray[1]);    // clear whitespace in argument strings
    inputOutputArray[2] = trim(inputOutputArray[2]);

    inputRedirectFile = mystrdup(inputOutputArray[1]);  // set file descriptor to arguments
    outputRedirectFile = mystrdup(inputOutputArray[2]);

    cmdTok(inputOutputArray[0]);
}

void inputTok(char *cmdArray) {  // parse and tokenise command by '<'
    char *inputArray[32];
    char *cmdProcessed = mystrdup(cmdArray);
    int i = 1;

    inputArray[0] = strtok(cmdProcessed,"<");
    while ((inputArray[i] = strtok(NULL,"<")) != NULL) {
        i++;
    }

    inputArray[1] = trim(inputArray[1]);
    inputRedirectFile = mystrdup(inputArray[1]);
    cmdTok(inputArray[0]);
}

void outputTok(char *cmdArray) { // parse and tokenise command by '>'
    char *outputArray[32];
    char *cmdProcessed = mystrdup(cmdArray);

    int i = 1;
    outputArray[0] = strtok(cmdProcessed,">");
    while ((outputArray[i] = strtok(NULL,">")) != NULL) {
          i++;
    }

    outputArray[1] = trim(outputArray[1]);
    outputRedirectFile = mystrdup(outputArray[1]);
    cmdTok(outputArray[0]);
}

static int cmdRun(int input, int first, int last, char *cmdArray) {
    int pipeArray[2], pipeStatus, fdInput, fdOutput;
    pipeStatus = pipe(pipeArray);

    if(pipeStatus == -1)    // pipe error
      {
        perror("ERROR: pipe could not be created\n");
        return 1;
      }

    pid = fork();   // fork current process

    if (pid == 0) { // in child process --> check for command possibilities
        if (first == 1 && last == 0 && input == 0)
        {
            dup2(pipeArray[1], STDOUT_FILENO);  // associate output pipe end with stdout
        }
        else if (first == 0 && last == 0 && input!= 0) {
            dup2(input, STDIN_FILENO); // associate input pipe end with stdin
            dup2(pipeArray[1], STDOUT_FILENO);  // associate output pipe end with stdout
        }
        else {
            dup2(input, STDIN_FILENO); // associate input pipe end with stdin
        }

        if (strchr(cmdArray, '<') && strchr(cmdArray, '>')) {   // direct both input and output
                  inputRedirect = 1;
                  outputRedirect = 1;
                  inputOutputTok(cmdArray);
        }
        else if (strchr(cmdArray, '>')) {   // redirect output only
              outputRedirect = 1;
              outputTok(cmdArray);
        }
        else if (strchr(cmdArray, '<')) {   // redirect input only
              inputRedirect = 1;
              inputTok(cmdArray);
        }

        if (outputRedirect == 1) {   // output redirection
                fdOutput = creat(outputRedirectFile, 0666);

                if (fdOutput < 0) {
                    perror("ERROR: Failed to open file\n");
                    return(EXIT_FAILURE);
                }

                dup2(fdOutput, STDOUT_FILENO);  // assosciate write file descriptor with stdout
                close(fdOutput);
                outputRedirect = 0;
        }

        if (inputRedirect == 1) {    // input redirection
            fdInput = open(inputRedirectFile, 0666, 0);

            if (fdInput < 0) {
                perror("ERROR: Failed to open file\n");
                return(EXIT_FAILURE);
            }

            dup2(fdInput, STDIN_FILENO);    // associate read file descriptor with stdin
            close(fdInput);
            inputRedirect = 0;
        }

        execStatus = execvp(args[0], args);
        if (execStatus < 0) {
            perror("ERROR: command not found\n");
        }

        //exit(0);    // terminate child process
    }
    else {  // in parent process
        waitpid(pid, &execStatus, 0);   // wait for process to terminate
    }

    // closing pipe ends in parent
    if (last == 1) {    // final command in input
        close(pipeArray[0]);
    }
    if (input != 0) {
        close(input);
    }
    close(pipeArray[1]);

    return pipeArray[0];
}

static int separate(char *cmdArray, int input, int first, int last) {   // split command and pass into cmdRun() function
    int i = 1;
    char *cmdProcessed;
    cmdProcessed = mystrdup(cmdArray);
    args[0] = strtok(cmdArray," ");
    while ((args[i] = strtok(NULL," ")) != NULL)
        i++;
    args[i] = NULL;
    return cmdRun(input, first, last, cmdProcessed);
}

void cmdParse() {    // process initial input from stdin in main() and pass into separate() function
    int i;
    int n = 1;
    int input = 0;
    int first = 1;

    cmdArray[0] = strtok(line,"|"); // tokenize input by '|' character and parse into cmdArray
    while ((cmdArray[n] = strtok(NULL,"|")) != NULL)
          n++;
    cmdArray[n] = NULL; // ensure command terminates with null character

    for (i = 0; i < n; i++) {   // iterate through cmdArray
        if (i == n - 1) {
            input = separate(cmdArray[i], input, first, 1); // int last = 1 --> last string in input
        }
        else {
            input = separate(cmdArray[i], input, first, 0); // call separate() on each element/string in cmdArray
            first = 0;
        }
        //first = 0;//
    }
    input = 0;
    return;
}

void printPrompt() {    // function to print prompt
    char* prompt = "my_shell$";
    printf("%s ", prompt);
}

int main(int argc, char* argv[]) {
    int execStatus;
    int promptFlag = 1;

    if (strcmp(argv[argc - 1],"-n") == 0) { // unset promptFlag if program called with -n
        promptFlag = 0;
    }

    while (!(feof(stdin))) {    // loop until end of file
        reset();    // clear global variables

        if (promptFlag) {   // printing prompt
            printPrompt();
        };

        fgets(line, 512, stdin);    // read from stdin

        if(feof(stdin)) { // check for eof
            printf("\n");
            return 0;
        }

        line[strlen(line) - 1] = '\0';  // ensures input terminates correctly

        cmdParse();  // parse input
        fflush(stdout);
    }
    return 0;
}
