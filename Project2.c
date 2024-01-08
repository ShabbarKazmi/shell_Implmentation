/**************************************************
**Project_Name: Project2
**Description : Program implements a simplified C shell that allows the user to run any command in /bin.
**Additional features include running commmands as asynchronous, redirecting process output to an output file
**, and piping the output of one process to another.
**Author : Teagen Protheroe, Shabbar Kazmi
** 3/29/22
***************************************************/

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <ctype.h>

int done;
int is_display_hist;
int hist_index = 0;
int pid;
char *curr_token;
char *hist_str;

const int READ = 0;
const int WRITE = 1;
const int REVERSE_HISTORY_INDEX = 5;
const int REPLACE_OLDEST_HISTORY_COMMAND = 4;
const int MAX_HISTORY_STORAGE = 500;
int num_pipes;
char *historical_commands[MAX_HISTORY_STORAGE];

#define MAX_USER_INPUT 100

/**
 * Removes the white space before and after a token
 */
char *clean_token(char *cleaned_token, int token_length)
{
    if (cleaned_token[token_length - 1] == ' ')
    {
        cleaned_token[token_length - 1] = '\0';
    }
    if (cleaned_token[0] == ' ')
    {
        memmove(cleaned_token, cleaned_token + 1, token_length);
    }
    return cleaned_token;
}

/**
 * Splits up the users input based on a certain char delimiter
 */
int tokenize(char line[], char *tokens[], char delim[])
{
    curr_token = strtok(line, delim);
    int num_tokens = 0;
    while (curr_token != NULL)
    {
        tokens[num_tokens] = malloc(sizeof(curr_token) + 1);
        strcpy(tokens[num_tokens], curr_token);
        tokens[num_tokens] = clean_token(tokens[num_tokens], strlen(tokens[num_tokens]));
        curr_token = strtok(NULL, delim);
        num_tokens++;
    }
    tokens[num_tokens++] = NULL;
    return num_tokens;
}

/**
 * Runs commands that only have a redirect and async symbol as special features
 */
int run_redirect_command(char *myargs[], char *async_flag)
{
    char *valid_tokens[MAX_USER_INPUT];
    tokenize(myargs[0], valid_tokens, " ");
    int new_file;
    pid = fork();
    if (pid < 0)
    {
        fprintf(stderr, "Error: Fork Failed \n"); // fork failed
        return 1;
    }
    else if (pid == 0)
    {
        new_file = open(myargs[1], O_CREAT | O_WRONLY | O_TRUNC, S_IRWXU);

        if (new_file < 0)
        {
            fprintf(stderr, "Error: File Could Not Be Opened \n");
            return -1;
        }
        dup2(new_file, 1);
        execvp(valid_tokens[0], valid_tokens); // run command
    }
    else
    {
        if (async_flag)
        {
            return 1;
        }
        wait(NULL); // parent process waits for child process to finish
    }
    return 1;
}
/**
 * Run commands with only the async symbol as special features
 */
int run_simple_command(char *myargs[], char *async_flag)
{
    pid = fork();
    if (pid < 0)
    {
        fprintf(stderr, "Error: Fork Failed \n"); // fork failed
        return 1;
    }
    else if (pid == 0)
    {
        execvp(myargs[0], myargs); // run command
    }
    else
    {
        if (async_flag)
        {
            return 1;
        }
        wait(NULL); // parent process waits for child process to finish
    }
    return 1;
}

/**
 * Runs commands that can have both piping and redirects
 */
int run_composite_command(char *myargs[], char *async_flag, char *redirect_flag, int num_tokens)
{
    int new_file;
    char *piped_commands[MAX_USER_INPUT];
    char *next_command[MAX_USER_INPUT];
    int valid_tokens = num_tokens - 1;

    if (redirect_flag)
    {
        valid_tokens = tokenize(myargs[0], piped_commands, "|") - 1;

        new_file = open(myargs[1], O_CREAT | O_WRONLY | O_TRUNC, S_IRWXU);

        if (new_file < 0)
        {
            fprintf(stderr, "Error: File Could Not Be Opened \n");
            return 1;
        }
    }
    else
    {
        memcpy(piped_commands, myargs, MAX_USER_INPUT);
    }
    int num_pipes = valid_tokens - 1;
    int pipes[num_pipes][2];
    int pid, status;

    for (int i = 0; i < valid_tokens; i++)
    {
        tokenize(piped_commands[i], next_command, " ");
        if (i != valid_tokens - 1)
        {
            if (pipe(pipes[i]) < 0)
            {
                fprintf(stderr, "Error: Pipe Not Created \n");
                return -1;
            }
        }
        pid = fork();
        if (pid < 0)
        {
            fprintf(stderr, "Error: Fork Failed \n"); // fork failed
            return 1;
        }
        else if (pid == 0)
        {
            if (i != valid_tokens - 1)
            {
                // Left child writes into WRITE end of pipe:
                dup2(pipes[i][WRITE], STDOUT_FILENO);

                close(pipes[i][READ]);
                close(pipes[i][WRITE]);
            }
            else if (i != 0)
            {
                // Right child reads from READ end of pipe:
                dup2(pipes[i - 1][READ], STDIN_FILENO);

                close(pipes[i - 1][READ]);
                close(pipes[i - 1][WRITE]);
            }
            if (i == valid_tokens - 1)
            {
                dup2(new_file, 1);
            }
            execvp(next_command[0], next_command);
        }
        else
        {
            // Parent
            if (async_flag && i == num_tokens)
            {
                close(pipes[i - 1][READ]);
                close(pipes[i - 1][WRITE]);
                return 1;
            }
            if (i > 0)
            {
                close(pipes[i - 1][READ]);
                close(pipes[i - 1][WRITE]);
            }
            waitpid(pid, &status, 0);
        }
    }
    return 1;
}
/**
 * Update the array storing recently run commands
 */
void record_history(char *line)
{
    if (historical_commands[REPLACE_OLDEST_HISTORY_COMMAND] != NULL)
    {
        memmove(historical_commands, historical_commands + 1, MAX_USER_INPUT);
        historical_commands[REPLACE_OLDEST_HISTORY_COMMAND] = strdup(line);
    }
    else
    {
        historical_commands[hist_index] = strdup(line);
        hist_index++;
    }
}

/**
 * Print all commands stored in the history array
 */
void print_history()
{
    for (int i = 0; i < hist_index; i++)
    {
        printf("%s \n", historical_commands[i]);
    }
}

/**
 * Run the command specified at the index of the history array
 */
void run_history(char command_index, char *line)
{
    if (isdigit(command_index) && historical_commands[REVERSE_HISTORY_INDEX - (command_index - '0')] != NULL)
    {
        memcpy(line, historical_commands[REVERSE_HISTORY_INDEX - (command_index - '0')], MAX_USER_INPUT);
    }
    else
    {
        fprintf(stderr, "Error: Invalid Index \n");
    }
}

/**
 * Read in commands inputted by user
 */
char *read_command()
{
    char *input = (char *)malloc(MAX_USER_INPUT * sizeof(char));
    fgets(input, MAX_USER_INPUT, stdin);
    input[strlen(input) - 1] = '\0';
    hist_str = strstr(input, "r ");
    if (hist_str && strlen(input) == 3)
    {
        run_history(input[2], input);
    }
    else if (strcmp(input, "hist") == 0)
    {
        is_display_hist = 1;
    }
    if (strcmp(input, "quit") == 0)
    {
        done = 1;
    }
    return input;
}

int main(int argc, char *argv[])
{
    char *line;
    char *async_str;
    char *piped_str;
    char *redirect_str;
    int num_tokens;
    char *input_tokens[MAX_USER_INPUT];

    printf("> ");

    // read user input
    line = read_command();

    while (!done)
    {
        if (!is_display_hist)
        {
            // checks for async pipe, and ,redirect symbols
            async_str = strstr(line, "&");
            piped_str = strstr(line, "|");
            redirect_str = strstr(line, ">");

            if (!hist_str)
            {
                record_history(line);
            }
            if (piped_str)
            {
                if (async_str)
                {
                    line[strlen(line) - 1] = '\0';
                }

                if (redirect_str)
                {
                    num_tokens = tokenize(line, input_tokens, ">");
                }
                else
                {
                    num_tokens = tokenize(line, input_tokens, "|");
                }
                run_composite_command(input_tokens, async_str, redirect_str, num_tokens);
            }
            else
            {
                if (async_str)
                {
                    line[strlen(line) - 1] = '\0';
                }
                if (redirect_str)
                {
                    num_tokens = tokenize(line, input_tokens, ">");
                    run_redirect_command(input_tokens, async_str);
                }
                else
                {
                    num_tokens = tokenize(line, input_tokens, " ");
                    run_simple_command(input_tokens, async_str);
                }
            }
        }
        else
        {
            print_history();
            is_display_hist = 0;
        }
        printf("> ");
        line = read_command();
    }
    return 1;
}
