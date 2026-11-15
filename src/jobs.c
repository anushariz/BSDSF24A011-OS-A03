#include "shell.h"

/*
 * List of running background jobs.
 * Each job remembers a small id, the PID of its (last) process and the
 * command text. Finished jobs are removed from the list by jobs_reap().
 */
typedef struct {
    int id;
    pid_t pid;
    char* cmd;
} job_t;

static job_t job_list[MAX_JOBS];
static int job_count = 0;
static int next_job_id = 1;

/* Add a background job. Returns its job id, or -1 if the list is full. */
int jobs_add(pid_t pid, const char* cmd) {
    if (job_count >= MAX_JOBS) {
        fprintf(stderr, "warning: job list full, job %d is not tracked\n", (int) pid);
        return -1;
    }
    job_list[job_count].id = next_job_id++;
    job_list[job_count].pid = pid;
    job_list[job_count].cmd = strdup(cmd);
    job_count++;
    return job_list[job_count - 1].id;
}

/*
 * Collect every finished child WITHOUT blocking, so no zombies are left.
 * waitpid(-1, ..., WNOHANG) returns the PID of one finished child,
 * or 0 if children exist but none has finished, or -1 if there are none.
 */
void jobs_reap(void) {
    int status;
    pid_t pid;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        for (int i = 0; i < job_count; i++) {
            if (job_list[i].pid == pid) {            // it was a tracked job
                printf("[%d]+ Done        %s\n", job_list[i].id, job_list[i].cmd);
                free(job_list[i].cmd);
                for (int j = i + 1; j < job_count; j++) {
                    job_list[j - 1] = job_list[j];   // remove it from the list
                }
                job_count--;
                break;
            }
        }
    }
}

/* The 'jobs' built-in: show all background jobs that are still running. */
void jobs_print(void) {
    jobs_reap();                                     // drop finished jobs first
    if (job_count == 0) {
        printf("No background jobs.\n");
        return;
    }
    for (int i = 0; i < job_count; i++) {
        printf("[%d]  %d  Running  %s\n",
               job_list[i].id, (int) job_list[i].pid, job_list[i].cmd);
    }
}
