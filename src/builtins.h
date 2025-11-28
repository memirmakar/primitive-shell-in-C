#ifndef BUILTINS_H
#define BUILTINS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <ctype.h>
#include <stddef.h>

#define MAX_ARGS 100

typedef enum {
    NODE_COMMAND,
    NODE_PIPE,
    NODE_AND, // &&
    NODE_OR // || 
} NodeType;

struct ASTNode {
    NodeType type;
    char **args;

    struct ASTNode *left;
    struct ASTNode *right;
};

bool find_executable_in_path(const char *arg);
int shell_exit(struct ASTNode *node);
int shell_echo(struct ASTNode *node);
int shell_type(struct ASTNode *node);
int shell_pwd(struct ASTNode *node);
int shell_cd(struct ASTNode *node);

typedef int (*builtin_func)(struct ASTNode *node);

struct builtin_func {
    char *name;
    builtin_func func;
};

extern struct builtin_func builtins[];

#endif
