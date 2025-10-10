#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>

/**
 * prog - Process Simulation Program
 * * Usage: ./prog [filename] [seconds]
 * * Writes a status line to the specified file every second, for the given duration.
 * This program simulates the actual CPU time required by a process.
 */
int main(int argc, char *argv[]) {
    // Check for correct number of arguments (Program name + filename + seconds = 3)
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <filename> <seconds_to_run>\n", argv[0]);
        return 1;
    }

    const char *filename = argv[1];
    int total_seconds = atoi(argv[2]);
    int elapsed_seconds = 0;
    pid_t pid = getpid();
    
    // Check if the seconds argument is valid
    if (total_seconds <= 0) {
        fprintf(stderr, "Error: Duration must be a positive integer.\n");
        return 1;
    }

    // Open the file in append mode to log execution status
    FILE *file = fopen(filename, "a");
    if (file == NULL) {
        perror("Error opening log file");
        return 1;
    }

    fprintf(file, "PID %d starting execution. Total duration: %d seconds.\n", pid, total_seconds);
    fflush(file);

    // Main execution loop: simulates work by sleeping for 1 second per cycle
    while (elapsed_seconds < total_seconds) {
        // Log status to the file
        fprintf(file, "PID %d ran %d out of %d secs.\n", pid, elapsed_seconds + 1, total_seconds);
        fflush(file); // Flush buffer immediately to ensure output is written

        // Check for stop signal (SIGSTOP handler)
        // If the process receives SIGSTOP, execution pauses here until SIGCONT.
        
        sleep(1); // Simulate 1 second of work
        elapsed_seconds++;
    }

    fprintf(file, "PID %d completed successfully after %d seconds.\n", pid, total_seconds);
    fclose(file);

    return 0; // Exit successfully
}
