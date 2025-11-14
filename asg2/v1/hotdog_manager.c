#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h> // for the access function to check if the file exists

// defines
char *filename = "log.txt";

struct hotdog{
	int id;
	int maker_id;
};

struct poolDetails{
	struct hotdog* hotdog_pool;
	int head;
	int tail;
	int count;
};

//vairables
struct poolDetails hotdog_pool;
int N; //number of hotdogs to prepare
int M; //number of maker machines (producers)
int S; //capacity in the hotdog pool
int P; //number of packer machines (consumers)
pthread_mutex_t hotdog_mutex, log_mutex, summary_mutex; 
pthread_cond_t pool_not_full, pool_not_empty;  
int hotdog_made_counter =0;
int prod_done;
int* maker_summary;
int* packer_summary;

// HELPER FUNCTIONS
void helper_write(char *data){
	pthread_mutex_lock(&log_mutex);	
	//critical seciton of the helper
	FILE *fptr= fopen(filename,"w");
	if (fptr != NULL){
		fputs(data,fptr);
		fclose(fptr);
	}
	pthread_mutex_unlock(&log_mutex);
}

void helper_append(char *data){
	pthread_mutex_lock(&log_mutex);
	
	//similar to helper_write, the critical section of this helper is here
	FILE *fptr = fopen(filename, "a");
	if (fptr != NULL){
		fputs(data,fptr);
		fclose(fptr);
	}

	pthread_mutex_unlock(&log_mutex);
}

void helper_log(char *data){
	if (access(filename, F_OK) == 0){
		helper_append(data);
	}
	else{
		helper_write(data);
	}
}

void do_work(int n) {
	for (int i = 0; i < n; i++) {
		long m = 3000000L;
		while (m > 0) m--;
	}
}

// FUNCTIONS
void *make_hotdog(void* arg){ //cannot int*, as the use of pthread_create forces the function to only take in void args
	char buffer[256]; //buffer to write into log file
	while(1){
		int prod_id = *(int*)arg;
		int hotdog_id;

		do_work(4);

		//for adding to the pool
		pthread_mutex_lock(&hotdog_mutex);
		
		//waits when pool is full
		while(hotdog_pool.count == S){
			pthread_cond_wait(&pool_not_full, &hotdog_mutex); //wakes packer up
		}

		//checl the hotdog counter

		if (hotdog_made_counter >= N){
			pthread_mutex_unlock(&hotdog_mutex);
			break;
		}

		//cirtical section
		hotdog_made_counter++;
		hotdog_id = hotdog_made_counter;
		hotdog_pool.hotdog_pool[hotdog_pool.head].id = hotdog_id;
		hotdog_pool.hotdog_pool[hotdog_pool.head].maker_id = prod_id;
		hotdog_pool.head = (hotdog_pool.head+1)% S;
		hotdog_pool.count++;

		//append the log file
		sprintf(buffer,"maker %d puts hotdog %d\n",prod_id,hotdog_id);
		helper_append(buffer);

		//unlock the mutex
		pthread_cond_signal(&pool_not_empty);
		pthread_mutex_unlock(&hotdog_mutex);

		do_work(1);//sends to pool
		
		//maker summary stats
		pthread_mutex_lock(&summary_mutex);
		maker_summary[prod_id -1]++;
		pthread_mutex_unlock(&summary_mutex);	}
	return NULL;
}

void *pack_hotdog(void* arg){
	char buffer[256]; //buffer to write into log file
	while(1){
		int cons_id = *(int*)arg;
		struct hotdog  current_hotdog;
		pthread_mutex_lock(&hotdog_mutex);
		
		while (hotdog_pool.count ==0){
			if (prod_done ==1){
				pthread_mutex_unlock(&hotdog_mutex);
				return NULL;
			}
			pthread_cond_wait(&pool_not_empty,&hotdog_mutex);//
		}
	
		//critical section
		current_hotdog = hotdog_pool.hotdog_pool[hotdog_pool.tail];
		hotdog_pool.tail = (hotdog_pool.tail +1) %S;
		hotdog_pool.count--;

		//append the log
		sprintf(buffer,"packer %d gets hotdog %d from maker %d\n",cons_id,current_hotdog.id,current_hotdog.maker_id);
		helper_append(buffer);

		pthread_cond_signal(&pool_not_full);
		pthread_mutex_unlock(&hotdog_mutex);

		do_work(1);//1 unit of time to take the hotdog
		do_work(2); // 2 unit of time for packing the hotdog
		
		pthread_mutex_lock(&summary_mutex);
		packer_summary[cons_id -1]++;
		pthread_mutex_unlock(&summary_mutex);
		
	}
	
}


/*MAIN FUNCTION */

int main(int argc, char *argv[]){
	
	if (argc== 5){
		N = atoi(argv[1]);
		S = atoi(argv[2]);
		M = atoi(argv[3]);
		P = atoi(argv[4]);
		
		//declaration of more runtime variables
		pthread_t prod[M], cons[P];
		int prod_list[M];
		int cons_list[P];
		hotdog_pool.head =0;
		hotdog_pool.tail =0;
		hotdog_pool.count =0;
		maker_summary = (int*) calloc(M, sizeof(int));
		packer_summary = (int*) calloc(P, sizeof(int));
		char buffer[256]; //buffer to write into log file


		//succesfully assign the values. write to log.
		sprintf(buffer, "order:%d\ncapacity:%d\nmaking machine:%d\npacking machine:%d\n-----\n", N,S,M,P);
		helper_log(buffer);
		
		//create the mutex
		pthread_mutex_init(&log_mutex,NULL);
		pthread_mutex_init(&hotdog_mutex,NULL);
		pthread_mutex_init(&summary_mutex,NULL);
		pthread_cond_init(&pool_not_full,NULL);
		pthread_cond_init(&pool_not_empty,NULL);
		
		//create the mem
		hotdog_pool.hotdog_pool = (struct hotdog*) malloc(S * sizeof(struct hotdog));

		//start -c reating the producers and consumers
		for (int m =0; m < M; m++){
			prod_list[m] = m+1;
			if (pthread_create(&prod[m],NULL,make_hotdog, &prod_list[m])!=0){
				perror("Hotdog Producer thread failed to create");
			}
		}
		
		for (int p =0; p <P; p++){
			cons_list[p] = p+1;
			if (pthread_create(&cons[p],NULL,pack_hotdog,&cons_list[p])!=0){
				perror("Hotdog Packer thread failed to create");
			}
		}
		
		//wait for completion
		for (int m =0; m < M; m++){
             pthread_join(prod[m],NULL);
		}
		
		pthread_mutex_lock(&hotdog_mutex);
		prod_done =1;
		pthread_cond_broadcast(&pool_not_empty); //wakes up all threads under the packers  that are waiting
		pthread_mutex_unlock(&hotdog_mutex);
		
        for (int p =0; p < P; p++){
            pthread_join(cons[p],NULL);
        }

		sprintf(buffer,"----\nsummary:\n");
		helper_append(buffer);

		for (int m=0; m < M; m++){
			sprintf(buffer,"m%d made %d\n",m+1,maker_summary[m]);//go through hotdog list to check id?
			helper_append(buffer);
		}
		for (int p=0; p < P; p++){
			sprintf(buffer,"p%d packed %d\n",p+1,packer_summary[p]);
			helper_append(buffer);
		}
		
		sprintf(buffer,"------\n");
		helper_append(buffer);

		//exit
		free(hotdog_pool.hotdog_pool);
		free(maker_summary);
		free(packer_summary);
		pthread_mutex_destroy(&log_mutex);
		pthread_mutex_destroy(&hotdog_mutex);
		pthread_mutex_destroy(&summary_mutex);
		pthread_cond_destroy(&pool_not_full);
		pthread_cond_destroy(&pool_not_empty);
		
	}else{

		printf("Error: Insufficient information to run command.\nUsage:./hotdog_manager <N> <S> <M> <P>\n[N-number of hotdogs to make]\n[S-capacity of hotdog pool]\n[M-number of hotdog making machines]\n[P-number of packing machines]\n");

	}

	return 0;
}
