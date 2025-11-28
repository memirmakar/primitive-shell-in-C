#include "builtins.h"

struct builtin_func builtins[] = {
    {"echo", shell_echo},
    {"exit", shell_exit},
    {"type", shell_type},
    {"pwd", shell_pwd},
    {"cd", shell_cd},
    {NULL, NULL}};

int shell_exit(struct ASTNode *node)
{
    exit(0);
    return 0;
};

int shell_echo(struct ASTNode *node)
{
    char **args = node->args;
    for (int i = 1; args[i] != NULL; i++)
    {
        // Iterate character by character to handle escapes like \n
        for (int j = 0; args[i][j] != '\0'; j++)
        {
            if (args[i][j] == '\\' && args[i][j+1] != '\0')
            {
                char next = args[i][j+1];
                if (next == 'n')
                {
                    putchar('\n');
                    j++; // Skip the 'n'
                }
                else if (next == '\\')
                {
                    putchar('\\');
                    j++; // Skip the second backslash
                }
                else
                {
                    // For other sequences (like \t), you can add cases here.
                    // Otherwise, just print the backslash.
                    putchar('\\');
                }
            }
            else
            {
                putchar(args[i][j]);
            }
        }

        if (args[i + 1] != NULL)
            printf(" ");
    }
    printf("\n");
    return 0;
};

int shell_pwd(struct ASTNode *node)
{
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) != NULL)
    {
        printf("%s\n", cwd);
    }
    else
    {
        printf("getcwd() error\n");
        return 1;
    }
    return 0;
};

int shell_cd(struct ASTNode *node)
{
    char **args = node->args;
    if (args[1] == NULL)
    {
        fprintf(stderr, "cd: expected argument\n");
        return 1;
    }
    if (chdir(args[1]) != 0)
    {
        fprintf(stderr,"cd: %s: No such file or directory\n", args[1]);
        return 1;
    }
    return 0;
};

int shell_type(struct ASTNode *node)
{
    char **args = node->args;
    if (args[1] == NULL)
    {
        fprintf(stderr, "type: expected argument\n");
        return 1;
    }
    for (int i = 1; args[i]!= NULL; i++)
    {
        bool found = false;

        for (int j = 0; builtins[j].name != NULL; j++)
        {
            if (strcmp(args[i], builtins[j].name) == 0)
            {
                found = true;
                printf("%s is a shell builtin\n", args[i]);
                return 0;
            }
        }

        if (found != true)
        {
            found = find_executable_in_path(args[i]);
        }

        if (found != true)
        {
            printf("%s: not found\n", args[i]);
        }
    }
    return 0;
};

bool find_executable_in_path(const char *arg)
{
    char *path_env = getenv("PATH");
    if (!path_env)
        return false;

    char *path_copy = strdup(path_env);
    char *dir = strtok(path_copy, ":");
    bool found = false;

    while (dir != NULL)
    {
        char full_path[1024];
        snprintf(full_path, sizeof(full_path), "%s/%s", dir, arg);

        if (access(full_path, X_OK) == 0)
        {
            printf("%s is %s\n", arg, full_path);
            found = true;
            break;
        }
        dir = strtok(NULL, ":");
    }

    free(path_copy);
    return found;
}