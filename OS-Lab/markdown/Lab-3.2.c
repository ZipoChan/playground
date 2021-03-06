#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define MAXARGLEN 64
#define MAXARGS 128
#define BUFFER_SIZE 1024

#define TOK_END 0
#define TOK_STR 1
#define TOK_APP 2
#define TOK_OUT 3
#define TOK_INF 4
#define TOK_PIP 5

#define TYPE_IN 1
#define TYPE_OUT 2
#define TYPE_APPEND 3
#define TYPE_INOUT 4
#define TYPE_INAPPEND 5

static char command_buffer[BUFFER_SIZE];
static char token_buffer[BUFFER_SIZE];
static int command_index;

typedef enum {
    TYPE_EXEC,
    TYPE_PIPE,
    TYPE_REDI,
    TYPE_EROR
} type_t;

typedef struct meta_command {
    type_t type;
} meta_command;

typedef struct exec_command {
    type_t type;
    int argc;
    char *argv[MAXARGS];
} exec_command;

typedef struct redi_command {
    type_t type;
    exec_command *real_command;
    char in_file[MAXARGLEN];
    char out_file[MAXARGLEN];
    int redi_type;
} redi_command;

typedef struct pipe_command {
    type_t type;
    meta_command *left;
    meta_command *right;
} pipe_command;

void get_command() {
    command_index = 0;
	for(int i = 0; i < 1024; i++)
		command_buffer[i] = 0;
    printf("=> ");
    gets(command_buffer);
}

bool is_valid_char(char c) {
    if(c == '>' || c == '<' || c == '|' || c == '\t' || c == '\n' || c == ' ')
        return false;
    return true;
}

int get_basic_string() {
    int token_index = 0;
    while(command_buffer[command_index] != '\0' &&
            is_valid_char(command_buffer[command_index])) {
        token_buffer[token_index++] = command_buffer[command_index];
        command_index ++;
    }
    token_buffer[token_index++] = '\0';
    return token_index - 1 == 0 ? TOK_END : TOK_STR;
}

int lex() {
    int token_index = 0;
    token_buffer[0] = '\0';
    while(command_buffer[command_index] != '\0') {
        switch (command_buffer[command_index]) {
            case ' ':
            case '\n':
            case '\t':
            case '\r':
                command_index += 1;
                break;
            case '>':
                if(command_buffer[command_index + 1] == '>') {
                    token_buffer[0] = '>';
                    token_buffer[1] = '>';
                    token_buffer[2] = '\0';
                    command_index += 2;
                    return TOK_APP;
                } else {
                    token_buffer[0] = '>';
                    token_buffer[1] = '\0';
                    command_index += 1;
                    return TOK_OUT;
                }
            case '<':
                token_buffer[0] = '<';
                token_buffer[1] = '\0';
                command_index += 1;
                return TOK_INF;
            case '|':
                token_buffer[0] = '|';
                token_buffer[1] = '\0';
                command_index += 1;
                return TOK_PIP;
            default:
                return get_basic_string();
        }
    }
    return TOK_END;
}

char peek() {
    int i = 0;
    while(command_buffer[command_index + i] != '\0' && (command_buffer[command_index + i] == ' ' || command_buffer[command_index + i] == '\t'))
        i ++;
    return command_buffer[command_index + i];
}

pipe_command *build_pipe_command(meta_command *left, meta_command *right) {
    pipe_command *command = malloc(sizeof(pipe_command));
    memset(command, 0, sizeof(pipe_command));
    command->type = TYPE_PIPE;
    command->left = left;
    command->right = right;
    return command;
}

redi_command *build_redi_command(exec_command *real_command, char *in_file, char *out_file, int redi_type) {
    redi_command *command = malloc(sizeof(redi_command));
    memset(command, 0, sizeof(redi_command));
    command->type = TYPE_REDI;
    command->real_command = real_command;
    strcpy(command->in_file, in_file);
    strcpy(command->out_file, out_file);
    command->redi_type = redi_type;
    return command;
}

meta_command *parse_command() {
    exec_command *command = malloc(sizeof(exec_command));
    memset(command, 0, sizeof(exec_command));
    command->type = TYPE_EXEC;
    int token_type;
    while ((token_type = lex()) == TOK_STR) {
        command->argv[command->argc] = malloc(strlen(token_buffer) + 1);
        strcpy(command->argv[command->argc], token_buffer);
        command->argc ++;
    }
    switch (token_type) {
        case TOK_PIP:
            return (meta_command *)build_pipe_command((meta_command *)command,  (meta_command *)parse_command());
        case TOK_INF:
        case TOK_OUT:
        case TOK_APP:
            lex();
            int curr_type = token_type;
            char buffered_op[MAXARGLEN];
            strcpy(buffered_op, token_buffer);
            int next_type = lex();
            if(next_type == TOK_STR) {
                command->type = TYPE_EROR;
                return (meta_command *)command;
            } else if(next_type == TOK_PIP) {
                if(curr_type == TOK_INF)
                    return (meta_command *)build_pipe_command((meta_command *) build_redi_command(
                            command, buffered_op, "", TYPE_IN), (meta_command *)parse_command());
                if(curr_type == TOK_OUT)
                    return (meta_command *)build_pipe_command((meta_command *) build_redi_command(
                            command, "", buffered_op, TYPE_OUT), (meta_command *)parse_command());
                if(curr_type == TOK_APP)
                    return (meta_command *)build_pipe_command((meta_command *) build_redi_command(
                            command, "", buffered_op, TYPE_APPEND), (meta_command *)parse_command());
            } else if(next_type == TOK_END) {
                if(curr_type == TOK_INF)
                    return (meta_command *) build_redi_command(command, buffered_op, "", TYPE_IN);
                if(curr_type == TOK_OUT)
                    return (meta_command *) build_redi_command(command, "", buffered_op, TYPE_OUT);
                if(curr_type == TOK_APP)
                    return (meta_command *) build_redi_command(command, "", buffered_op, TYPE_APPEND);
            } else if (next_type == TOK_OUT || next_type == TOK_APP) {
                if(curr_type == TOK_OUT || curr_type == TOK_APP || lex() != TOK_STR) {
                    command->type = TYPE_EROR;
                    return (meta_command *)command;
                } else {
                    return (meta_command *) build_redi_command(command, token_buffer,
                            buffered_op, next_type == TOK_OUT ? TYPE_INOUT : TYPE_INAPPEND);
                }
            } else if(next_type == TOK_INF) {
                if(curr_type == TOK_INF || lex() != TOK_STR) {
                    command->type = TYPE_EROR;
                    return (meta_command *)command;
                } else {
                    return (meta_command *) build_redi_command(command, buffered_op, token_buffer, curr_type == TOK_OUT ? TYPE_INOUT : TYPE_INAPPEND);
                }
            }
            break;
        default:
            return (meta_command *)command;
    }
}

void run_command(meta_command *command) {
    int fd;
    int pipe_fd[2];
    switch (command->type) {
    case TYPE_EXEC:
        execvp(((exec_command *)command)->argv[0], ((exec_command *)command)->argv);
        break;
    case TYPE_REDI:
        switch(((redi_command *)command)->redi_type) {
        case TYPE_IN:
            if((fd = open(((redi_command *)command)->in_file, O_RDONLY)) == -1) {
                puts("Open file failed.");
                return;
            }
            dup2(fd, STDIN_FILENO);
            close(fd);
            break;
        case TYPE_APPEND:
            if((fd = open(((redi_command *)command)->out_file, O_WRONLY | O_APPEND | O_CREAT, 0666)) == -1) {
                puts("Open file failed.");
                return;
            }
            dup2(fd, STDOUT_FILENO);
            close(fd);
            break;
        case TYPE_OUT:
            if((fd = open(((redi_command *)command)->out_file, O_WRONLY | O_CREAT, 0666)) == -1) {
                puts("Open file failed.");
                return;
            }
            dup2(fd, STDOUT_FILENO);
            close(fd);
            break;
        case TYPE_INOUT:
            if((fd = open(((redi_command *)command)->out_file, O_WRONLY | O_CREAT, 0666)) == -1) {
                puts("Open file failed.");
                return;
            }
            dup2(fd, STDOUT_FILENO);
            close(fd);
            if((fd = open(((redi_command *)command)->in_file, O_RDONLY)) == -1) {
                puts("Open file failed.");
                return;
            }
            dup2(fd, STDIN_FILENO);
            close(fd);
            break;
        case TYPE_INAPPEND:
            if((fd = open(((redi_command *)command)->out_file, O_WRONLY | O_APPEND | O_CREAT, 0666)) == -1) {
                puts("Open file failed.");
                return;
            }
            dup2(fd, STDOUT_FILENO);
            close(fd);
            if((fd = open(((redi_command *)command)->in_file, O_RDONLY)) == -1) {
                puts("Open file failed.");
                return;
            }
            dup2(fd, STDIN_FILENO);
            close(fd);
            break;
        }
        run_command((meta_command *)((redi_command *)command)->real_command);
        break;
    case TYPE_PIPE:
        pipe(pipe_fd);
        if(fork() == 0) {
            dup2(pipe_fd[1], STDOUT_FILENO);
            close(pipe_fd[0]);
            close(pipe_fd[1]);
            run_command((meta_command *)((pipe_command *)command)->left);
        }
        if(fork() == 0) {
            dup2(pipe_fd[0], STDIN_FILENO);
            close(pipe_fd[0]);
            close(pipe_fd[1]);
            run_command((meta_command *)((pipe_command *)command)->left);
        }
        wait();
        wait();
        break;
    default:
        break;
    }
}

int main() {
    while(1) {
        get_command();
		puts(command_buffer);
        if (strncmp(command_buffer, "exit", 4) == 0) {
            exit(0);
        }
        if(fork() == 0) {
            meta_command *command = parse_command();
            run_command(command);
        }
        wait();
    }
    return 0;
}
