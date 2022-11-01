#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

// Deadline 1 Submission

void printPrompt() {
    printf("my_shell$");
}

void runCommandTest(char** command) {
    fflush(stdout);
    int execStatus;
    
    pid_t pid = fork();
    if (pid == 0) { // in child
        execStatus = execvp(command[0], command);
        exit(0);
    }
    else {
        waitpid(pid, &execStatus, 0);
    }
    return;
}

int builtinCommandTest();

char* trim(char* str) { // triming leading/trailing whitespace --> deadline 1 testing
        while (isspace(*str)) { //trimming leading 
            ++str;
        }
        
        //printf("string length is %ld \n", strlen(str));
        // printf("test string: %c", *str[3]);
        
        // int i;
        // for (i = strlen(str); i > 0; i--) {
        //     if isspace(str[i]) {
        //         str[i] = 'W';
        //     }
        //     else {
        //         break;
        //     }
        // }
        
        char* end;
        end = str + strlen(str) + 1;
        
        while(isspace(*end) && (end > str)) {
            end--;
        }
        end[1] = 0;
        // while (str2 != NULL) {
        //     printf("%c", &str2);
        //     str2 += sizeof(char);
        // }
        
        // return str;
        
        //printf("string: %c \n", str[1]);
        
        return str;
}

int main() {
    int status = 1;
    char line[512]; // initialize input

    int i, j, k; // counter values
    int cmdCount = 0;

    while (!(feof(stdin))) {
        i = 0;
        j = 0;

        fgets(line, 512, stdin); // read user input

        // if(feof(stdin)) { // check for eof
        //     //printf("\n");
        //     return 0;
        // }
        
        line[strlen(line + 1)] = 0; //

        char* p = strtok(line, "|"); // tokenize input and parse into array of command
        char* array[32];
        while (p != NULL) {
            if (j > 0) {
                printf("\n");
            }
            array[i] = p;
            if (!(feof(stdin))) {
                printf("%s", trim(array[i]));
            } 
            else {
                return 1;
            }
            p = strtok(NULL, "|");
            j++;
            i++;
        } // after loop: p points to NULL, i = total # of elements

        i = 0; //tokenize input again and parse into array of arguments
        p = strtok (*array, " ");
        char *firstCmd[strlen(*array)];
        while (p != NULL) {
            firstCmd[i++] = p;
            p = strtok(NULL, " ");
        }

        printf("\n");
        firstCmd[i] = NULL; // ensure command arguments terminate with NULL character
        //printf("i is %d and firstCmd[i] is %s and firstCmd[i - 1] is %s\n", i, firstCmd[i], firstCmd[i - 1]);
        runCommandTest(firstCmd);
        cmdCount++;
    }
    return 0;
}


