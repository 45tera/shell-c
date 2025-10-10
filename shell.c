#include <stdio.h>  //for print,perror
#include <stdlib.h>  // for free, malloc,realloc,free,execvp - cannot be confused with exit()
#include <string.h> //for strcmp and strtok
#include <unistd.h>  //for fork,pid_t
#include <errno.h> //for handling errors
#include <sys/wait.h> //for waitpid

// STRUCTS
typedef enum{
	RUNNING =0,
	READY =1,
	STOPPED=2,
	TERMINATED =3
} ProcessState;

typedef struct PCB {
	int pid;
	ProcessState state;
	int priority;
	int arrival_order;
	char *file_name;
	char **args;
	struct Process *next;
} PCB;

// Global Variables
#define MAX_RUNNING 3
PCB* all_process_table[100];

int count = 0; //counter for the arrival order
PCB **p_ready_queue = NULL;
PCB *p_running_queue[MAX_RUNNING] = {NULL};
PCB **p_terminated_queue = NULL;
PCB **p_stopped_queue = NULL;

int ready_queue_count =0;
int running_queue_count =0;
int terminated_queue_count =0;
int stopped_queue_count =0;


/*FUNCTIONS*/

// System helpers
PCB* get_process(__pid_t given_pid){
	for (int i =0; i< count; i++){
		if (all_process_table[i]-> pid == given_pid){
			return all_process_table[i];
		}
	}
	return NULL;
}

// EXtracts the arguments from the user input (i.e -al, -nvlp..etc)
char **extracted_args(char ** full_arg){
	int arg_count = 0;
	while (full_arg[arg_count] != NULL) {
		arg_count++;
	}
	char **filename_args = (char **)malloc((arg_count + 1) * sizeof(char*));


	for (int i = 0; i < arg_count; i++) {
		filename_args[i] = strdup(full_arg[i]); 
	}

	filename_args[arg_count] = NULL; 

	return filename_args;
}

// Accepts a state, and returns the state's plaintext name. Added this in because 0 and 1s were a bit too hard to remember
const char* get_state_string(ProcessState state) {
    switch (state) {
        case RUNNING:
            return "RUNNING";
        case READY:
            return "READY";
        case STOPPED:
            return "STOPPED";
        case TERMINATED:
            return "TERMINATED";
        default:
            return "UNKNOWN";
    }
}


// Queue Helpers
PCB** add_pcb_to_queue(PCB **queue,int *count,PCB *to_add_pcb){
	size_t new_size = (*count + 1) * sizeof(PCB*);
	PCB **changed_queue = realloc(queue,new_size);
	 if (changed_queue == NULL) {
        perror("FATAL: realloc failed for dynamic queue in add_pcb");
        exit(EXIT_FAILURE); 
    }
	changed_queue[*count] = to_add_pcb;
	(*count)++;
	return changed_queue;
}


PCB** remove_pcb_frm_queue(PCB **queue,int *count,PCB *to_remove_pcb){

	int found =0;

	if (queue == NULL || *count <= 0) {
        return NULL; 
    }

	for (int i =0; i< *count; i++){
		if (queue[i]== to_remove_pcb){
			for (int q =i; q< *count-1;q++){
				queue[q] = queue[q+1];
			}
			found =1;
			break;
		} 
	}

	
	
	if (found == 1){
		(*count)--;
		
		if (*count == 0) {
        	free(queue); 
        	return NULL; 
    	}

		size_t new_size = (*count)* sizeof(PCB*);
		
		PCB **changed_queue = realloc(queue,new_size);
		if (changed_queue == NULL) {
            fprintf(stderr, "Warning: Failed to shrink queue memory via realloc.\n");
            return queue;
        }
		return changed_queue;
	}else{
		return queue;
	}
	

}

void remove_from_running_queue(PCB *pcb_to_remove){
	for (int i =0; i<running_queue_count; i++){
		if (p_running_queue[i] ==pcb_to_remove){
			
			for (int z=i; z <running_queue_count-1;z++){
				p_running_queue[z]= p_running_queue[z+1];
			}
			
			break;
		}
	}
	running_queue_count--;
	p_running_queue[running_queue_count] = NULL;
		
}


//scheduling related functions

void dispatch_next(void){
	// stop dispatcher  
	if (running_queue_count >= MAX_RUNNING || ready_queue_count == 0) {
        return;
    }

	// select the process to go next, based on priority, then FCFS
	PCB *current_first_p = NULL;

	for(int i=0; i< ready_queue_count; i++){
		PCB *contested_p = p_ready_queue[i];

		// selects highest priority number first, then by FCFS (lowest arrival order number)
		if (current_first_p == NULL || contested_p->priority < current_first_p->priority || (contested_p->priority == current_first_p->priority && contested_p->arrival_order < current_first_p->arrival_order) ){
			current_first_p = contested_p;
		}
	}

	// once selected, then assign to the queue
	if (current_first_p != NULL){
		p_ready_queue = remove_pcb_frm_queue(p_ready_queue, &ready_queue_count, current_first_p);
    
		// dispatch the selected
		if (current_first_p-> pid == -1){
			__pid_t pid = fork();

			if (pid ==-1 ){
				perror("Process failed to dispatch");
				return;
			}
			else if (pid == 0){
				//from pcb extract out the commands
				char **filename_args = extracted_args(current_first_p->args);
 				execvp(current_first_p->args[0],filename_args);
				fprintf(stderr, "Execution of the program %s failed: %s\n", current_first_p->args[0], strerror(errno));

			}else{
				current_first_p->pid = pid;
				current_first_p->state = RUNNING;
			}
		}
		else{
			kill(current_first_p -> pid, SIGCONT);
			current_first_p->state = RUNNING;
		}

		p_running_queue[running_queue_count] = current_first_p;
		running_queue_count++;
	}


}

// used to check for empty space, kind of like a timer.
void check(){
	__pid_t pid;
	int status;
	while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0) {
		PCB *pcb = get_process(pid);

		if (WIFEXITED(status) || WIFSIGNALED(status)) {
			if (pcb-> state == RUNNING){

				//remove pcb from running queue, and shift back the values
				remove_from_running_queue(pcb);

				pcb -> state = TERMINATED;

				terminated_queue_count++;
				p_terminated_queue =add_pcb_to_queue(p_terminated_queue,&terminated_queue_count,pcb);
				dispatch_next();
			}


		}
	}
}


// run

void run(char **args,char *priority){

	// Check if args and priority_str are valid
    if (args == NULL || priority == NULL || args[0] == NULL) {
        fprintf(stderr, "Error: Invalid arguments for run command. Usage: run [program] [arguments] [priority]\n");
        return;
    }

	//1. Create PCB
	PCB *new_pcb = calloc(1, sizeof(PCB));
	if (!new_pcb) { // if pcb not null
        perror("malloc");
        return;
    }

	//2. Extract out the object/filename
	new_pcb->file_name = strdup(args[0]);

	char **filename_args = extracted_args(args);

	new_pcb->args = filename_args;

	//3. add to mem table
	int priority_number;
	sscanf(priority, "P%d", &priority_number);
	new_pcb ->priority = priority_number;
	new_pcb->arrival_order = count;

	//2. Add to PCB table	
	all_process_table[count] = new_pcb;
	count++;

	//if <3; put in running state
	if (running_queue_count < 3){
		new_pcb->state = RUNNING; 
		__pid_t pid = fork();
	

		if (pid == -1){
			perror("Creation of new process failed");
			new_pcb -> state = TERMINATED;
		}
		else if( pid ==0){
			execvp(args[0],filename_args);
			fprintf(stderr, "Execution of the program %s failed: %s\n", args[0], strerror(errno));
			new_pcb -> state = TERMINATED;
		}
		else{
			//parent here
			new_pcb -> pid = pid;

			p_running_queue[running_queue_count] = new_pcb;
            running_queue_count++;

		}
		

	}else{ //else if >= 3 then put in ready state
		new_pcb-> state = READY;
		new_pcb -> pid = -1;

		//Assign to the ready quque
		p_ready_queue = add_pcb_to_queue(p_ready_queue,&ready_queue_count,new_pcb);
	}

	
	
	

	//once finished, placed in the stopped state
}

// stop

void stop(int target_pid){
	PCB* current = NULL;
	int found =0;
	for (int i= 0; i< running_queue_count; i++){
		current = p_running_queue[i];
		if (current-> pid== target_pid){
			if (kill(target_pid, SIGSTOP) == 0) {
				found =1;
				printf("Stopping %d",target_pid);
				break;
			}
		}
	}

	if (found ==1){
		remove_from_running_queue(current);
		
		p_stopped_queue = add_pcb_to_queue(p_stopped_queue,&stopped_queue_count,current);
		current-> state = STOPPED;

		dispatch_next();

	}

}


// kill

void process_kill(int target_pid){
	PCB *pcb_to_kill = get_process(target_pid);
	
	ProcessState original_state = pcb_to_kill->state;

	if (kill(pcb_to_kill->pid,SIGTERM)==0){
			waitpid(pcb_to_kill->pid, NULL, 0);    
		if (original_state == 1){
			p_ready_queue = remove_pcb_frm_queue(p_ready_queue,&ready_queue_count,pcb_to_kill);
			pcb_to_kill->state = TERMINATED;
			p_terminated_queue = add_pcb_to_queue(p_terminated_queue,&terminated_queue_count,pcb_to_kill);			}
		
		else if (original_state == 2){
			p_stopped_queue= remove_pcb_frm_queue(p_stopped_queue,&stopped_queue_count,pcb_to_kill);
			pcb_to_kill->state = TERMINATED;
			p_terminated_queue = add_pcb_to_queue(p_terminated_queue,&terminated_queue_count,pcb_to_kill);	
		}
		
						
		else if (original_state ==0){
			
			remove_from_running_queue(pcb_to_kill);
			pcb_to_kill->state = TERMINATED;
			p_terminated_queue = add_pcb_to_queue(p_terminated_queue,&terminated_queue_count,pcb_to_kill);	

		
			dispatch_next();
		}
			
		
	}	
	

}


//resume

void resume(int target_pid){
	PCB* current = NULL;
	int found =0;
	for (int i= 0; i< stopped_queue_count; i++){
		current = p_stopped_queue[i];
		if (current-> pid== target_pid){
			found =1;
			break;
		}
	}
	if (found ==1){
		printf("Resuming %d", target_pid);
		p_stopped_queue= remove_pcb_frm_queue(p_stopped_queue,&stopped_queue_count,current);
		
		if (running_queue_count <3){
			if(kill(target_pid,SIGCONT) ==0){
			
				current-> state = RUNNING;

				//add to running
				p_running_queue[running_queue_count] = current;
				running_queue_count++;
		

			}
		}
		else{


			current -> state = READY;
			p_ready_queue = add_pcb_to_queue(p_ready_queue,&ready_queue_count,current);
		}

		
	}else{
		//not found ?
	}


	
}


//list
void list(){

	//Header
	printf("%s\t%s\t\t%s\n","PID","STATE","PRIORITY");
	if (count > 0){
		for (int i =0; i <count; i++){
			PCB *current = all_process_table[i];
			printf("%d\t%d[%s]\tP%d\n",current->pid,current->state,get_state_string(current->state),current->priority);
		}
	}else{
		fprintf(stdout,"\t%s\t", "No processes are running. Start by running one.\n");
	}
	

}

// exit
void exit_procmon(void){

	//terminate all child processes
	for (int i=0; i<count; i++){
		PCB * current_pcb = all_process_table[i];

		if (current_pcb != NULL && current_pcb->state != TERMINATED && current_pcb->pid >0){
			if (kill(current_pcb->pid,SIGKILL)==0){
				printf("Killed child PID %d.\n",current_pcb->pid);
			}
		}

	}
	   while (wait(NULL) > 0) {
        // waits until there is no more children
    }

    for (int i = 0; i < count; i++) {
        PCB *pcb = all_process_table[i];
        if (pcb != NULL) {
            if (pcb->args != NULL) {
                for (int j = 0; pcb->args[j] != NULL; j++) {
                    free(pcb->args[j]);
                }
                free(pcb->args);
            }
            free(pcb->file_name); 
            free(pcb);
        }
    }

	//free the queues
	if (p_ready_queue != NULL) {
		free(p_ready_queue);
		p_ready_queue = NULL; //ensures that the queue pointers are set to NULL after freed so that the pointer is not corrupted
	}
	if (p_stopped_queue != NULL) {
		free(p_stopped_queue);
		p_stopped_queue = NULL; 
	}
	if (p_terminated_queue != NULL) {
		free(p_terminated_queue);
		p_terminated_queue = NULL; 
	}	

	//exit from parent program
	exit(EXIT_SUCCESS);
}




//Readline Helpers

char *readline_output(void){
	char *buf;
	size_t len = 0;
	__ssize_t output;

	buf = NULL;

	printf("\nym's procman> ");

	if (getline(&buf,&len,stdin)== -1){
		if(feof(stdin)){
			printf("EOF");
		}
		else{
			printf("Getline failed");
		}
	}

	return buf;

}

char **evaluate_command(char *input){
	int bufsize = BUFSIZ;
	char **tokenised_command = malloc(bufsize * sizeof(char*));
	int position =0;

	for (char *token = strtok(input," ");token; token =strtok(NULL," ")){
		tokenised_command[position++] = token;

		if(position >= bufsize){
			bufsize *=2; //doubles the buffer size for dynamic allocaition

			tokenised_command = realloc(tokenised_command, bufsize * sizeof(char*));

			if(!tokenised_command){
				perror("realloc");
				exit(EXIT_FAILURE);
			}

		}
	}

    tokenised_command[position] = NULL; 

	return tokenised_command;
}


/*MAIN FUNCTION */

int main(int argc, char *argv[]){

//vairables
char *raw_input;
char **input_command;





//repl concept
// loop
while(1){
	check();

	//read -get line 
	raw_input = readline_output();

	raw_input[strcspn(raw_input, "\n")] = 0;

	//evaluate via tokenizer
	input_command = evaluate_command(raw_input);

	if (input_command == NULL || input_command[0] == NULL) {
		
		free(raw_input);
		free(input_command);
		continue; 
	}

	//extracted out parameters
	int num_tokens = 0;
	while (input_command[num_tokens] != NULL) {
		num_tokens++;
	}

	//main command - ie. run/stop/kill/list/exit
	char* command = input_command[0];

	//evaluate/print
	if (strcmp(command, "run")==0){

		//priority command - ie. P1/P2
		char* command_priority = input_command[num_tokens-1];

		int num_args = num_tokens - 2;  // everything in between
		char **command_args = malloc((num_args + 1) * sizeof(char*)); 

		for (int i = 0; i < num_args; i++) {
			command_args[i] = input_command[i + 1]; 
		}
		command_args[num_args] = NULL; //NULL terminator to signify the end of the string


		run(command_args,command_priority);
		
	}
	 else if (strcmp(command, "stop") == 0) {
            if (num_tokens < 2 || input_command[1] == NULL) {
                fprintf(stderr, "Error: 'stop' requires a PID. Usage: stop [PID]\n");
                free(raw_input);
                free(input_command);
                continue;
            }
			char *endptr;
		    long target_pid = strtol(input_command[1], &endptr, 10);

            //int target_pid = atoi(input_command[1]); - no using atoi, because it skips over the cases where there are characters and numbers mixed.
            if (*endptr != '\0' || target_pid <= 0) {
                fprintf(stderr, "Error: Invalid PID '%s'. Usage: stop [PID] \n", input_command[1]);
                free(raw_input);
                free(input_command);
                continue;
            }

            stop((int)target_pid);
        }
	
	else if (strcmp(command, "kill") == 0) {
            if (num_tokens < 2 || input_command[1] == NULL) {
                fprintf(stderr, "Error: 'kill' requires a PID. Usage: kill [PID]\n");
                free(raw_input);
                free(input_command);
                continue;
            }

            char *endptr;
		    long target_pid = strtol(input_command[1], &endptr, 10);

            if (*endptr != '\0' || target_pid <= 0) {
                fprintf(stderr, "Error: Invalid PID '%s'. Usage: kill [PID]\n", input_command[1]);
                free(raw_input);
                free(input_command);
                continue;
            }

            process_kill((int)target_pid);
        }
	else if (strcmp(command, "resume") == 0) {
            if (num_tokens < 2 || input_command[1] == NULL) {
                fprintf(stderr, "Error: 'resume' requires a PID. Usage: resume [PID]\n");
                free(raw_input);
                free(input_command);
                continue;
            }

            char *endptr;
		    long target_pid = strtol(input_command[1], &endptr, 10);
            if (*endptr != '\0' || target_pid <= 0) {
                fprintf(stderr, "Error: Invalid PID '%s'.\n", input_command[1]);
                free(raw_input);
                free(input_command);
                continue;
            }

            resume((int)target_pid);
        }
	else if (strcmp(command, "list")==0){
		list();
		
	}
	else if (strcmp(command, "exit")==0){
		exit_procmon();

		free(raw_input);
		free(input_command);

		return EXIT_SUCCESS;
	}
	else{
		printf("\nUnrecognized command: %s\n", command);
		printf("\n%s\n%s\n","Usage: [command] [program] [arguments] [priority]","Commands Available: run, stop, kill, list, exit");
	}

}

return 0;
}
