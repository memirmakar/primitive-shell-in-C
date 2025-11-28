#include "builtins.h"
#include "master.h"
#include <fcntl.h> 
struct ASTNode *new_node(NodeType type)
{
    struct ASTNode *node = malloc(sizeof(struct ASTNode));
    node->type = type;
    node->args = NULL;
    node->right = NULL;
    node->left = NULL;
    return node;
}

// Helper to remove 2 arguments (operator + filename) and shift the array left
void remove_redirection_args(char **args, int index)
{
    // Free the memory for the operator (e.g., ">") and the filename
    if (args[index]) free(args[index]);
    if (args[index + 1]) free(args[index + 1]);

    // Shift everything after them to the left by 2 positions
    int i = index;
    while (args[i + 2] != NULL)
    {
        args[i] = args[i + 2];
        i++;
    }
    // Null terminate the new end of the list
    args[i] = NULL;
    args[i + 1] = NULL; // Safety clear
}

int handle_redirection(char **args, int *saved_stdout, int *saved_stderr)
{
    *saved_stdout = -2; // -2 indicates "not redirected"
    *saved_stderr = -2;

    int i = 0;
    while (args[i] != NULL)
    {
        int fd_to_redirect = -1;
        int open_flags = 0;
        char *operator = args[i];

        if (strcmp(operator, ">") == 0 || strcmp(operator, "1>") == 0)
        {
            fd_to_redirect = STDOUT_FILENO;
            open_flags = O_WRONLY | O_CREAT | O_TRUNC;
        }
        else if (strcmp(operator, ">>") == 0 || strcmp(operator, "1>>") == 0)
        {
            fd_to_redirect = STDOUT_FILENO;
            open_flags = O_WRONLY | O_CREAT | O_APPEND;
        }
        else if (strcmp(operator, "2>") == 0)
        {
            fd_to_redirect = STDERR_FILENO;
            open_flags = O_WRONLY | O_CREAT | O_TRUNC;
        }
        else if (strcmp(operator, "2>>") == 0)
        {
            fd_to_redirect = STDERR_FILENO;
            open_flags = O_WRONLY | O_CREAT | O_APPEND;
        }

        // 2. If valid operator found, process it
        if (fd_to_redirect != -1)
        {
            if (args[i + 1] == NULL)
            {
                fprintf(stderr, "Syntax error: Expected file after %s\n", operator);
                return -1;
            }
            char *filename = args[i + 1];

            // 3. Open the file
            int fd = open(filename, open_flags, 0644);
            if (fd < 0)
            {
                perror("open");
                return -1;
            }

            // 4. Save the original FD (only the first time we redirect it)
            if (fd_to_redirect == STDOUT_FILENO)
            {
                if (*saved_stdout == -2) *saved_stdout = dup(STDOUT_FILENO);
            }
            else if (fd_to_redirect == STDERR_FILENO)
            {
                if (*saved_stderr == -2) *saved_stderr = dup(STDERR_FILENO);
            }

            // 5. Replace the standard FD with our file
            if (dup2(fd, fd_to_redirect) < 0)
            {
                perror("dup2");
                close(fd);
                return -1;
            }
            close(fd);

            remove_redirection_args(args, i);
            
            continue; 
        }

        i++;
    }

    return 0;
}

int process_command(struct ASTNode *node)
{
    int saved_stdout = -2;
    int saved_stderr = -2;

    // STEP 1: Handle Redirections
    // We pass addresses of saved_stdout/stderr to track what changed
    if (handle_redirection(node->args, &saved_stdout, &saved_stderr) == -1) {
        return 1; // Redirection error
    }

    // Safety check: command might be empty now (e.g. user typed just "> file")
    if (node->args[0] == NULL) {
        // Restore FDs before returning
        if (saved_stdout != -2) { dup2(saved_stdout, STDOUT_FILENO); close(saved_stdout); }
        if (saved_stderr != -2) { dup2(saved_stderr, STDERR_FILENO); close(saved_stderr); }
        return 1;
    }

    bool is_builtin = false;
    for (int i = 0; builtins[i].name != NULL; i++)
    {
        if (strcmp(node->args[0], builtins[i].name) == 0)
        {
            is_builtin = true;
            builtins[i].func(node);
            
            // IMPORTANT: Flush stdout if it was redirected to a file
            if (saved_stdout != -2) fflush(stdout); 
            break;
        }
    }

    if (is_builtin == false)
    {
        pid_t pid = fork();

        if (pid == 0)
        {
            execvp(node->args[0], node->args);
            fprintf(stderr, "%s: command not found\n", node->args[0]);
            exit(1);
        }
        else if (pid < 0)
        {
            perror("fork");
        }
        else
        {
            wait(NULL);
        }
    }

    // STEP 2: Restore STDOUT and STDERR
    if (saved_stdout != -2)
    {
        dup2(saved_stdout, STDOUT_FILENO);
        close(saved_stdout);
    }
    if (saved_stderr != -2)
    {
        dup2(saved_stderr, STDERR_FILENO);
        close(saved_stderr);
    }

    return 1;
}

char **split_line(char *line)
{
    char **tokens = malloc(MAX_ARGS * sizeof(char *));
    int position = 0;
    int i = 0;

    while (line[i] != '\0')
    {
        // Skip first spaces
        while (line[i] == ' ' || line[i] == '\t')
        {
            i++;
        }
        if (line[i] == '\0')
            break;

        char *token = malloc(1024);
        int token_index = 0;
        bool in_single_quote = false;
        bool in_double_quote = false;

        while (line[i] != '\0')
        {
            // 1. Handle Quotes 
            if (line[i] == '\'' && !in_double_quote)
            {
                in_single_quote = !in_single_quote;
            }
            else if (line[i] == '\"' && !in_single_quote)
            {
                in_double_quote = !in_double_quote;
            }
            // 2. Handle Escapes
            else if (line[i] == '\\' && !in_single_quote)
            {
                // Only consume the next char immediately if we are in a special case
                // Otherwise, let the bottom increment handle it.
                if (in_double_quote) {
                    char next = line[i+1];
                    if (next == '\"' || next == '$' || next == '\\' || next == '`') {
                        i++; 
                        if (line[i] != '\0') token[token_index++] = line[i];
                    } else {
                        token[token_index++] = '\\'; // Keep backslash
                    }
                } else {
                    i++; 
                    if (line[i] != '\0') token[token_index++] = line[i];
                }
            }
            // 3. Handle Delimiters (Space/Pipe)
            else if ((line[i] == ' ' || line[i] == '\t') && !in_single_quote && !in_double_quote)
            {
                break; 
            }
            else if (line[i] == '|' && !in_single_quote && !in_double_quote)
            {
                if (token_index == 0) {
                    token[token_index++] = '|';
                    i++;
                }
                break; 
            }
            // 4. Normal Character
            else
            {
                token[token_index++] = line[i];
            }
            
            if (line[i] != '\0') i++;
        }
        
        token[token_index] = '\0';
        tokens[position++] = token;
    }
    tokens[position] = NULL;
    return tokens;
};

char *expand_home(char *arg)
{
    if (arg[0] != '~') return NULL;
    if (arg[1] != '/' && arg[1] != '\0') return NULL;

    char *home = getenv("HOME");
    if (!home) return NULL;

    char *new_arg = malloc(strlen(home) + strlen(arg) + 1);
    if (!new_arg) return NULL;

    strcpy(new_arg, home);
    strcat(new_arg, arg + 1);

    return new_arg;
}

void expand_args(char **args)
{
    for (int i = 0; args[i] != NULL; i++)
    {
        char *expanded = expand_home(args[i]);
        if (expanded != NULL)
        {
            free(args[i]); 
            args[i] = expanded;
        }
    }
}

struct ASTNode *parse_tokens(char **tokens)
{
    if (tokens == NULL || tokens[0] == NULL)
        return NULL;

    int pipe_index = -1;
    for (int i = 0; tokens[i] != NULL; i++)
    {
        if (strcmp(tokens[i], "|") == 0)
        {
            pipe_index = i;
            break;
        }
    }

    if (pipe_index != -1)
    {
        tokens[pipe_index] = NULL; 

        struct ASTNode *node = new_node(NODE_PIPE);
        node->left = parse_tokens(tokens); 
        node->right = parse_tokens(&tokens[pipe_index + 1]);
        return node;
    }
    else
    {
        struct ASTNode *node = new_node(NODE_COMMAND);
        node->args = tokens;
        expand_args(node->args);
        return node;
    }
}

int execute_tree(struct ASTNode *node)
{
    if (node == NULL) return 1;

    switch (node->type)
    {
    case NODE_COMMAND:
        return process_command(node);
    case NODE_PIPE:
        return run_pipeline(node->left, node->right);
    case NODE_AND:
        int AND_status = execute_tree(node->left);
        if (AND_status == 1) 
        {                    
            return execute_tree(node->right);
        }
        return AND_status;
    case NODE_OR:
        int OR_status = execute_tree(node->left);
        if (OR_status != 1) 
        {
            return execute_tree(node->right);
        }
        return OR_status;
    }
    return 1;
}

int run_pipeline(struct ASTNode *left, struct ASTNode *right)
{
    int pipefd[2];
    if (pipe(pipefd) == -1)
    {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    pid_t pid1 = fork();
    if (pid1 == 0)
    {
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[0]);
        close(pipefd[1]);
        execute_tree(left); 
        exit(EXIT_FAILURE);
    }

    pid_t pid2 = fork();
    if (pid2 == 0)
    {
        dup2(pipefd[0], STDIN_FILENO);
        close(pipefd[1]);
        close(pipefd[0]);
        execute_tree(right);
        exit(EXIT_FAILURE);
    }

    close(pipefd[0]);
    close(pipefd[1]);
    waitpid(pid1, NULL, 0);
    waitpid(pid2, NULL, 0);
    return 1;
}