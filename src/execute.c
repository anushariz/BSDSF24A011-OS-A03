#include "shell.h"

/*
 * apply_redirections: set up '<' and '>' for the CURRENT process.
 * open() the file, dup2() it onto stdin/stdout, then close() the extra fd.
 * Returns 0 on success, -1 on error (a message has been printed).
 */
static int apply_redirections(command_t* cmd) {
    if (cmd->infile != NULL) {
        int fd = open(cmd->infile, O_RDONLY);
        if (fd < 0) {
            perror(cmd->infile);
            return -1;
        }
        dup2(fd, STDIN_FILENO);
        close(fd);
    }
    if (cmd->outfile != NULL) {
        int fd = open(cmd->outfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd < 0) {
            perror(cmd->outfile);
            return -1;
        }
        dup2(fd, STDOUT_FILENO);
        close(fd);
    }
    return 0;
}

/*
 * execute_pipeline: run ncmds commands connected by pipes (ncmds may be 1).
 * For every command: create a pipe to the next one (if any), fork, wire up
 * stdin/stdout in the child, then exec. The parent closes its pipe ends and
 * finally waits for all children. Returns the exit status of the last command.
 */
int execute_pipeline(command_t cmds[], int ncmds) {
    int pipefd[2];
    int prev_read = -1;             // read end of the previous pipe (-1 = none)
    pid_t pids[MAX_CMDS];
    int started = 0;                // how many children were created

    fflush(stdout);                 // so buffered output is not copied into children

    for (int i = 0; i < ncmds; i++) {
        int has_next = (i < ncmds - 1);

        if (has_next && pipe(pipefd) < 0) {
            perror("pipe failed");
            if (prev_read != -1) close(prev_read);
            break;
        }

        pid_t pid = fork();
        if (pid < 0) {
            perror("fork failed");
            if (has_next) { close(pipefd[0]); close(pipefd[1]); }
            if (prev_read != -1) close(prev_read);
            break;
        }

        if (pid == 0) {             // ---- child process ----
            if (prev_read != -1) {                 // read from the previous pipe
                dup2(prev_read, STDIN_FILENO);
                close(prev_read);
            }
            if (has_next) {                        // write into the next pipe
                close(pipefd[0]);                  // child never reads its own output pipe
                dup2(pipefd[1], STDOUT_FILENO);
                close(pipefd[1]);
            }
            if (apply_redirections(&cmds[i]) < 0) {   // '<' and '>' override the pipe
                exit(1);
            }
            if (handle_builtin(cmds[i].argv)) {    // built-in inside a pipeline
                exit(0);
            }
            execvp(cmds[i].argv[0], cmds[i].argv);
            perror("Command not found");           // only reached if execvp fails
            exit(1);
        }

        // ---- parent process ----
        pids[started++] = pid;
        if (prev_read != -1) {
            close(prev_read);                      // parent no longer needs the old read end
        }
        if (has_next) {
            close(pipefd[1]);                      // parent must close the write end,
            prev_read = pipefd[0];                 // otherwise the reader never sees EOF
        }
    }

    int status = 0, last_status = 0;
    for (int i = 0; i < started; i++) {
        waitpid(pids[i], &status, 0);
        if (i == started - 1 && WIFEXITED(status)) {
            last_status = WEXITSTATUS(status);
        }
    }
    return last_status;
}

/*
 * run_builtin_in_shell: run a single built-in (cd, exit, history, ...) inside
 * the shell process itself. If it has < or >, redirect temporarily and restore
 * stdin/stdout afterwards, so the shell keeps working normally.
 */
void run_builtin_in_shell(command_t* cmd) {
    fflush(stdout);
    int saved_in = dup(STDIN_FILENO);
    int saved_out = dup(STDOUT_FILENO);

    if (apply_redirections(cmd) == 0) {
        handle_builtin(cmd->argv);
    }

    fflush(stdout);                 // write the output into the file BEFORE restoring
    dup2(saved_in, STDIN_FILENO);
    dup2(saved_out, STDOUT_FILENO);
    close(saved_in);
    close(saved_out);
}
