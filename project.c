#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#define SHELL_PROMPT "[techmasters]$ "
#define HISTORY_SIZE 10  // Set the maximum history size

typedef struct {
    char *history[HISTORY_SIZE];
    int history_index;
    int history_count;
} ShellHistory;

void add_to_history(ShellHistory *history, const char *command) {
    if (history->history_count < HISTORY_SIZE) {
        history->history[history->history_count++] = strdup(command);
    } else {
        free(history->history[0]);  // Free the oldest history entry
        for (int i = 1; i < HISTORY_SIZE; i++) {
            history->history[i - 1] = history->history[i];  // Shift history
        }
        history->history[HISTORY_SIZE - 1] = strdup(command);  // Add new command
    }
}

void execute_command(char **args) {
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    char command[1024] = "cmd.exe /c ";

    for (int i = 0; args[i] != NULL; i++) {
        strcat(command, args[i]);
        strcat(command, " ");
    }

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    if (!CreateProcess(NULL, command, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        fprintf(stderr, "Error: Unable to execute command\n");
        return;
    }

    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
}

char **parse_command(char *command) {
    size_t args_size = 8;
    char **args = malloc(sizeof(char*) * args_size);
    if (!args) return NULL;

    size_t args_count = 0;
    char *token = strtok(command, " ");
    while (token != NULL) {
        if (args_count + 2 >= args_size) {
            args_size *= 2;
            char **resized = realloc(args, sizeof(char*) * args_size);
            if (!resized) {
                free(args);
                return NULL;
            }
            args = resized;
        }
        args[args_count++] = token;
        token = strtok(NULL, " ");
    }
    args[args_count] = NULL;
    return args;
}

void change_directory(char *path) {
    if (SetCurrentDirectory(path)) {
        printf("Changed directory to %s\n", path);
    } else {
        fprintf(stderr, "Error: Unable to change directory\n");
    }
}

void execute_piped_command(char *command) {
    char *left_cmd = strtok(command, "|");
    char *right_cmd = strtok(NULL, "|");

    if (left_cmd && right_cmd) {
        HANDLE pipe_read, pipe_write;
        SECURITY_ATTRIBUTES saAttr;
        saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
        saAttr.bInheritHandle = TRUE;
        saAttr.lpSecurityDescriptor = NULL;

        if (!CreatePipe(&pipe_read, &pipe_write, &saAttr, 0)) {
            perror("CreatePipe");
            return;
        }

        STARTUPINFO siLeft;
        PROCESS_INFORMATION piLeft;
        ZeroMemory(&siLeft, sizeof(siLeft));
        siLeft.cb = sizeof(siLeft);
        siLeft.hStdOutput = pipe_write;
        siLeft.dwFlags |= STARTF_USESTDHANDLES;

        if (!CreateProcess(NULL, left_cmd, NULL, NULL, TRUE, 0, NULL, NULL, &siLeft, &piLeft)) {
            perror("CreateProcess for left command");
            return;
        }

        STARTUPINFO siRight;
        PROCESS_INFORMATION piRight;
        ZeroMemory(&siRight, sizeof(siRight));
        siRight.cb = sizeof(siRight);
        siRight.hStdInput = pipe_read;
        siRight.dwFlags |= STARTF_USESTDHANDLES;

        if (!CreateProcess(NULL, right_cmd, NULL, NULL, TRUE, 0, NULL, NULL, &siRight, &piRight)) {
            perror("CreateProcess for right command");
            return;
        }

        CloseHandle(pipe_read);
        CloseHandle(pipe_write);

        WaitForSingleObject(piLeft.hProcess, INFINITE);
        WaitForSingleObject(piRight.hProcess, INFINITE);

        CloseHandle(piLeft.hProcess);
        CloseHandle(piLeft.hThread);
        CloseHandle(piRight.hProcess);
        CloseHandle(piRight.hThread);
    }
}

void run_shell() {
    char input[1024];
    char cwd[1024];
    ShellHistory history = { .history_index = -1, .history_count = 0 };

    while (1) {
        GetCurrentDirectory(sizeof(cwd), cwd);
        printf("%s%s$ ", cwd, SHELL_PROMPT);
        if (!fgets(input, sizeof(input), stdin)) {
            printf("\nExiting techmasters...\n");
            break;
        }

        input[strcspn(input, "\n")] = '\0';

        // Handle empty command
        if (input[0] == '\0') {
            continue;
        }

        // Save the command in history
        add_to_history(&history, input);

        if (strcmp(input, "exit") == 0) {
            printf("Exiting techmasters...\n");
            break;
        }

        if (strchr(input, '|')) {
            execute_piped_command(input);
        } else {
            char **args = parse_command(input);
            if (args) {
                if (strcmp(args[0], "cd") == 0) {
                    if (args[1]) {
                        change_directory(args[1]);
                    } else {
                        fprintf(stderr, "Error: Missing directory argument\n");
                    }
                } else {
                    execute_command(args);
                }
                free(args);
            }
        }
    }
}

int main() {
    printf("Welcome to techmasters - Advanced Windows Shell!\n");
    run_shell();
    return 0;
}
