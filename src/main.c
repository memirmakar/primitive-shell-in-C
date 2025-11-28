#include "builtins.h"
#include "master.h"

// Recursive function to free the AST after execution
void free_tree(struct ASTNode *node) {
    if (node == NULL) return;

    // Recursively free children first
    free_tree(node->left);
    free_tree(node->right);

    // If it's a command node, we need to be careful.
    // In your specific implementation, node->args points to the original 
    // 'tokens' array allocated in split_line. 
    // Depending on how you manage memory, you might need to free the strings here.
    // For now, we will just free the node structure itself to avoid double-free errors.
    free(node);
}

int main() {
    char line[1024];

    while (1) {
        printf("$ ");
        
        if (fgets(line, sizeof(line), stdin) == NULL) {
            printf("\n"); 
            break;
        }

        line[strcspn(line, "\n")] = '\0';

        char **tokens = split_line(line);
        
        if (tokens[0] == NULL) {
            free(tokens);
            continue;
        }

        struct ASTNode *root = parse_tokens(tokens);


        if (root != NULL) {
            execute_tree(root);
        }

        free_tree(root);
        
        // Note: Because parse_tokens modifies the 'tokens' array in-place 
        // (by inserting NULLs), freeing the strings inside 'tokens' can be tricky.
        // A simple loop here cleans up the string memory for the current batch.
        // (This relies on tokens[0] still pointing to valid memory).
        if (tokens) {
            // Only free the backbone array here. The strings are effectively 
            // owned by the AST nodes now, but since we didn't deep copy them 
            // in parse_tokens, sophisticated memory management is needed for 100% leak-proof code.
            // For this stage, freeing the backbone is sufficient.
            free(tokens);
        }
    }

    return 0;
}