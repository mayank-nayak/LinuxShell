#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>
#include <ctype.h>
#include <fcntl.h>

// FUNCTION PROTOTYPES
int getMode(int numArgs);
void throwError();
int checkAscii(char *string);
int getInput(char **input, size_t *size, FILE *fd);
int parseInput(char **input, char ***arguments, int *numArguments, char *delimeter);
int executeCommand(char **tokens);
int checkBuiltIn(char **arguments, int numArguments);
int executeBuiltIn(char **arguments, int numArguments);
int setPath(char **arguments, int numArguments);
int checkIf(char **arguments);
int processCommand(char **input);
int checkRedirect(char **input);

// MACROS
#define TRUE 1
#define FALSE 0

// GLOBAL VARIABLES
char **path;
int numPaths;
int returnValue;
int redirectFlag = FALSE;
char *redirectFilename = NULL;


int main(int argc, char *argv[]) {
    char mode; // 0 means interactive, 1 means batch
    char *input = NULL;
    size_t size;
    FILE *fd = stdin;

    // SET PATH VARIABLE
    path = malloc(sizeof(char *) * 1);
    while (path == NULL) path = malloc(sizeof(char *) * 1);
    *path = "/bin";
    numPaths = 1;
    
    // check mode
    mode = getMode(argc);
    // if invalid number of arguments were passed when executing ./wish 
    if (mode == -1 || (mode == 1 && !checkAscii(argv[1]))) {
        throwError();
        exit(1);
    }
    if (mode == 1) {
        fd = fopen(argv[1], "r");
        if (fd == NULL) {
            throwError();
            exit(1);
        }
    }
    
    while (1) {
        int status = TRUE;
        // get input
        if (mode == 0) {
            printf("wish> ");
        } 
        size = getInput(&input, &size, fd);
        
        if (size == -1) {
            if (mode == 0) printf("\n");
            exit(0);
        }
        
        if (!checkAscii(input)) {
            throwError();
            exit(1);

        }
        input[strlen(input) - 1] = '\0'; 
        // processCommand
        status = processCommand(&input);
        if (status == FALSE) {
            throwError();
        }
        free(input);
        input = NULL;
    }
    

    return 0;
}

int processCommand(char **input) {
    char **argumentArray = NULL;
    int status = TRUE;
    int numArguments = 0;
    redirectFlag = FALSE;
    redirectFilename = NULL;
    if (checkRedirect(input) == FALSE) return FALSE;

    status = parseInput(input, &argumentArray, &numArguments, " \t");
    if (status == FALSE) {
        free(argumentArray);
        return status;
    }
    if (numArguments == 0) {
        free(argumentArray);
        return TRUE;
    }

    int builtInFlag = checkBuiltIn(argumentArray, numArguments);

    if (builtInFlag == TRUE) {
        status = executeBuiltIn(argumentArray, numArguments); 
    } else {
        status = executeCommand(argumentArray);
    }
    if (status == FALSE) {
        //throwError();
        free(argumentArray);
        return status;
    }
    free(argumentArray);
    return status;
}

int checkRedirect(char **input) {
    int redirectCount = 0;
    for (int i = 0; i < strlen(*input); ++i) if ((*input)[i] == '>') redirectCount++; 
    if (redirectCount > 1) return FALSE;
    if (redirectCount == 0) return TRUE;
    
    int numArguments;
    char **arguments;
    parseInput(input, &arguments, &numArguments, " \t");
    if (numArguments == 0) return TRUE;
    if ((!strcmp(arguments[0], "if") && !strcmp(arguments[numArguments - 1], "fi"))) {
        for (int i = 1; i < numArguments - 1; ++i) {
            if (!strcmp(arguments[i], "then")) {
                redirectFlag = FALSE;
                return TRUE;
            }
        }
    }
    free(arguments);
    parseInput(input, &arguments, &numArguments, ">");
    if (numArguments != 2) return FALSE;
    char *tempInput = strdup(arguments[0]);
    char *tempRedirect = strdup(arguments[1]);
    free(arguments);
    parseInput(&tempRedirect, &arguments, &numArguments, " \t");
    if (numArguments != 1) {
        free(tempInput);
        free(tempRedirect);
        free(arguments);
        return FALSE;
    } else {
        free(*input);
        *input = tempInput;
        redirectFlag = TRUE;
        redirectFilename = strdup(arguments[0]);
        free(tempRedirect);
        free(arguments);
    }
    return TRUE;
}

int executeBuiltIn(char **arguments, int numArguments) {
    int status = FALSE;
    char *name = arguments[0];
    if (!strcmp(name, "exit") && numArguments == 1) {
        exit(0);
    } else if (!strcmp(name, "cd") && numArguments == 2) {
        chdir(arguments[1]);
        status = TRUE;
    } else if (!strcmp(name, "path")) {
        status = setPath(arguments, numArguments);

    } else if (!strcmp(name, "if") && !(strcmp(arguments[numArguments - 1], "fi"))) {
        int iThen = 0;
        while (strcmp(arguments[iThen], "then") && iThen < numArguments) ++iThen;
        if (iThen < 4) return FALSE;
        char *comparator = arguments[iThen - 2];
        if (!(!strcmp(comparator, "==") || !strcmp(comparator, "!="))) return FALSE;
        char *constant = arguments[iThen - 1];
        int isNum = TRUE;
        for (int i = 0; i < strlen(constant); ++i) {
            if (isdigit(constant[i]) == 0) isNum = FALSE;
        }
        if (isNum == FALSE) return FALSE;

        char *newInput = NULL;
        int length = 0;
        for (int i = 1; i < iThen - 2; ++i) {
            length = strlen(arguments[i]) + 1;
        }
        newInput = calloc(length + 1, sizeof(char));
        newInput[length] = '\0';
        strcpy(newInput, arguments[1]);
        for (int i = 2; i < iThen - 2; ++i) {
            strcat(strcat(newInput, " "), arguments[i]);
        }
        if (processCommand(&newInput) == FALSE) return FALSE;
        free(newInput);
        newInput = NULL;
        length = 0;
        for (int i = iThen + 1; i < numArguments - 1; ++i) {
            length += strlen(arguments[i]) + 1;
        }
        if (length == 0) return TRUE;

        if ((!strcmp(comparator, "==") && atoi(constant) == returnValue) || (!strcmp(comparator, "!=") && atoi(constant) != returnValue)) {
            newInput = calloc(length + 1, sizeof(char));
            newInput[length] = '\0';
            strcpy(newInput, arguments[iThen + 1]);
            for (int i = iThen + 2; i < numArguments - 1; ++i) {
                strcat(strcat(newInput, " "), arguments[i]);
            }

            if (processCommand(&newInput) == FALSE) {
                free(newInput);
                return FALSE;
            }
        }
        free(newInput);
        return TRUE;
        
    }
    return status;
}

int checkBuiltIn(char **arguments, int numArguments) {
    char *name = arguments[0];
    if (!strcmp(name, "exit") || !strcmp(name, "cd") || !strcmp(name, "path")) {
        return TRUE;
    } else if ((!strcmp(name, "if") && !strcmp(arguments[numArguments - 1], "fi"))) {
        for (int i = 1; i < numArguments - 1; ++i) {
            if (!strcmp(arguments[i], "then")) {
                return TRUE;
            }
        }
    }
    return FALSE;
}

int executeCommand(char **tokens) {
    int status = FALSE;
    if (path != NULL) {
        char *program;  
        for(int i = 0; i < numPaths; ++i) { 
            program = malloc(strlen(tokens[0]) + strlen(path[i]) + 2);
            strcpy(program, path[i]); 
            strcat(strcat(program, "/"), tokens[0]);
            if (access(program, F_OK) == 0) {
                int rc = fork();
                if (rc == 0) {
                    // this is child process
                    FILE *fd;
                    if (redirectFlag == 1) {
                        fd = fopen(redirectFilename, "w+");
                        dup2(fileno(fd), fileno(stdout));
                        dup2(fileno(fd), STDERR_FILENO);
                        free(redirectFilename);
                        redirectFilename = NULL;
                        fclose(fd);
                    }
                    
                    execv(program, tokens);
                } else if (rc > 0) {
                    // this is parent process
                    int waitPIDStatus;
                    waitpid(rc, &waitPIDStatus, 0);
                    returnValue = WEXITSTATUS(waitPIDStatus);
                    free(program);
                    status = TRUE;
                    break;
                } else {
                    status = FALSE;
                }   
            }
            free(program);          
        }    
    } 
    return status;
}

int parseInput(char **input, char ***pointToArguments, int *numArguments, char *delimeter) {
    char *duplicateInput = strdup(*input);
    char *freeDuplicate = duplicateInput;
    const char *delim = delimeter;
    char *token = "";
    int numberOfArgs = 0;

    if (duplicateInput == NULL) {
        free(duplicateInput);
        return FALSE;
    }

    while(token != NULL) {
        token = strsep(&duplicateInput, delim);
        if (token == NULL || *token == '\0') continue;
        numberOfArgs++;
    }
    *numArguments = numberOfArgs;
    free(freeDuplicate);
    if (numberOfArgs == 0) return TRUE;
    
    // allocating proper size argv array
    char **argv = calloc(numberOfArgs + 1, sizeof(char *));
    if (argv == NULL) {
        return FALSE;
    }
    argv[numberOfArgs] = NULL;

    // now seperating each token
    token = "";
    int index = 0;
    duplicateInput = strdup(*input);
    if (duplicateInput == NULL) {
        return FALSE;
    }
    freeDuplicate = duplicateInput;
    while(token != NULL) {
        token = strsep(&duplicateInput, delim);
        if (token == NULL || *token == '\0') continue;
        argv[index] = token;
        
        index++;
    }
    
    *pointToArguments = argv;
    free(duplicateInput);
    return TRUE;
}

int getInput(char **input, size_t *size, FILE *fd) { 
    int eof;
    eof = getline(input, size, fd);
    return eof;
}

int getMode(int numArgs) {
    if (numArgs == 1) return 0;
    if (numArgs == 2) return 1;
    return -1;
}

void throwError() {
    char error_message[30] = "An error has occurred\n";
    write(STDERR_FILENO, error_message, strlen(error_message)); 
}

int checkAscii(char *string) {
    for (int i = 0; i < strlen(string); ++i) {
        if (string[i] > 127 || string[i] < 0) {
            return FALSE;
        }
    }
    return TRUE;
}

// BUILT IN COMMANDS
int setPath(char **arguments, int numArguments) {
    if (numPaths != 0) {
        free(path);
    }
    if (numArguments == 1) {
        path = NULL;
        numPaths = 0;
    } else {
        path = malloc(sizeof(char *) * (numArguments - 1));
        if (path == NULL) return FALSE;
        for (int i = 1; i < numArguments; ++i) {
            path[i - 1] = arguments[i];
        }
        numPaths = numArguments - 1;
    }
    return TRUE;
}