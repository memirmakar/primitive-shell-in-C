#ifndef MASTER_H
#define MASTER_H

#include "builtins.h"

char **split_line(char *line);
struct ASTNode *parse_tokens(char **tokens);

int execute_tree(struct ASTNode *node);
int process_command(struct ASTNode *node);
int run_pipeline(struct ASTNode *left, struct ASTNode *right);

void free_tree(struct ASTNode *node);

#endif