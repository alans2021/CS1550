//Generate two process,
//One process - tenant
//Another process - agent
//
#include <sys/mman.h>
#include <linux/unistd.h>
#include <stdio.h>
#include <time.h>
#include "sem.h"

void down(struct cs1550_sem *sem) {
	syscall(__NR_cs1550_down, sem);
}
void up(struct cs1550_sem *sem) {
	syscall(__NR_cs1550_up, sem);
}

int tenantArrives(int tenant, time_t start){
	printf("Tenant %d arrives at time %ld\n", tenant, (time(0) - start));
	fflush(stdout);
	return 0;
}

int agentArrives(int agent, time_t start){
	printf("Agent %d arrives at time %ld\n", agent, (time(0) - start));
	fflush(stdout);
	return 0;
}

int viewApt(int tenant, time_t start){
	printf("Tenant %d inspects the apartment at time %ld\n", tenant, (time(0) - start));
	fflush(stdout);
	sleep(2);
	return 0;
}

int openApt(int agent, time_t start){
	printf("Agent %d opens the apartment for inspection at time %ld\n", agent, (time(0) - start));
	fflush(stdout);	
	return 0;
}

int tenantLeaves(int tenant, time_t start){
	printf("Tenant %d leaves the apartment at time %ld\n", tenant, (time(0) - start));
	fflush(stdout);
	return 0;
}

int agentLeaves(int agent, time_t start){
	printf("Agent %d leaves the apartment at time %ld\n", agent, (time(0) - start));
	fflush(stdout);
	return 0;
}

int main(int argc, char * argv[]){
	
	int m = 5; //number of tenants
	int k = 2; //number of agents
	int probT = 70; //prob of tenant following another tenant immed.
	int delayT = 5; //delay in seconds of a tenant
	int randT = 5; //random seed for tenant
	int probA = 75; //prob of agent following another agent immed.
	int delayA = 3; //delay in seconds of a agent
	int randA = 6; //random seed for agent
	int i;
	for(i = 1; i < argc; i+=2){
		if(strncmp(argv[i], "-m", 2) == 0)
			m = atoi(argv[i + 1]);
		if(strncmp(argv[i], "-k", 2) == 0)
			k = atoi(argv[i + 1]);
		if(strncmp(argv[i], "-pt", 3) == 0)
			probT = atoi(argv[i + 1]);
		if(strncmp(argv[i], "-dt", 3) == 0)
			delayT = atoi(argv[i + 1]);
		if(strncmp(argv[i], "-st", 3) == 0)
			randT = atoi(argv[i + 1]);
		if(strncmp(argv[i], "-pa", 3) == 0)
			probA = atoi(argv[i + 1]);
		if(strncmp(argv[i], "-da", 3) == 0)
			delayA = atoi(argv[i + 1]);
		if(strncmp(argv[i], "-sa", 3) == 0)
			randA = atoi(argv[i + 1]);		
	}
	
	printf("The apartment is now empty\n");
	fflush(stdout);

	struct cs1550_sem * agent_sem; //Mutex for agent leaving only when all tenants have left
	struct cs1550_sem * agent_open_sem; //Mutex for agent not opening if another agent has opened
	struct cs1550_sem * tenant_mutex; //Mutex for tenant not entering apartment if 10 are inspecting 
	struct cs1550_sem * open_mutex; //Mutex for agent opening apartment only when tenant is there
	struct cs1550_sem * enter_mutex; //Mutex for tenant entering apartment only when agent opens apartment
	struct cs1550_sem * count_lock; //Lock for the count variable
	int * count; //How many tenants currently arrived at apartment
	int * inspect_count; //Total number of tenants who have inspected the apartment
	
	//Calculate number of bytes needed, which is N
	void *ptr = mmap(NULL, 6*sizeof(struct cs1550_sem) + sizeof(int), PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0); 	
	agent_sem = (struct cs1550_sem *) ptr;
	agent_open_sem = agent_sem + 1;
	tenant_mutex = agent_open_sem + 1;
	open_mutex = tenant_mutex + 1;
	enter_mutex = open_mutex + 1;
	count_lock = enter_mutex + 1;
	count = (int *)(count_lock + 1);
	inspect_count = (int *)(count + 1);
	
	agent_sem->value = 0; 
	agent_open_sem->value = 1;
	tenant_mutex->value = 10;
	open_mutex->value = 0;
	enter_mutex->value = 0;
	count_lock->value = 1;
	*count = 0;
	*inspect_count = 0;
		
	int pid;
	time_t start = time(0);

	pid = fork();
	if(pid > 0){
		for(i = 0; i < m; i++){
			pid = fork();
			if(pid == 0){ //child process is tenant proces
				//Main Logic, use semaphores, to guarantee correctness of concurrent operations
				down(count_lock); //Get the count lock, ensure no race conditions
				*count = *count + 1; //Increment number of tenants that have arrived
				up(count_lock); //Release lock
				tenantArrives(i, start); //Tenant has arrived
				
				up(open_mutex); //Signals to agent process it can open apartment
				down(tenant_mutex); //Downs tenant mutex, once 10 have entered, this sleeps
				down(enter_mutex); //Tries to down enter_mutex, can't view until agent has opened apartment
				up(enter_mutex); //Immediately ups enter mutex
				viewApt(i, start); //Tenant can view apartment
			   	
				down(count_lock);
				*inspect_count = *inspect_count + 1; //Increment number of tenants who have inspected
				up(count_lock);	

				tenantLeaves(i, start); //Tenant leaves					
				down(count_lock);
				*count = *count - 1; //Decrement number of tenants who are in apartment
				up(count_lock);

				if(*inspect_count % 10 == 0 || *count == 0){ //Allow agent to leave if all tenants have left
									     // or if multiple of 10 tenants have inspected
					down(count_lock);
					while(enter_mutex->value > 0) //Adjust enter_mutex value
						down(enter_mutex);
					up(count_lock);
					up(agent_sem); //Allow agent to leave apartment
				}
					
				return 0; //Terminates child process
			}
			else{ //parent
				
				int randNum; //Do delays based off random numbers
				srand(randT);
				randNum = rand() % 100;
				if(randNum >= probT){
					sleep(delayT); //Sleep for delayT
				}
					
			}
		}
	}
	else{
		for(i = 0; i < k; i++){
			pid = fork();
			if(pid == 0){ //child process is agent process
				agentArrives(i, start);	//Agent has arrived

				down(agent_open_sem); //Signals to other agents they can't open apartment	
				down(open_mutex); //Tries to down open mutex before opening apartment
				up(enter_mutex); //Signals to tenant process they can enter
				if(*count == 0){ //If no tenants currently in apartment, allow agent to leave
					agentLeaves(i, start);
					return 0;
				}
				openApt(i, start); //Open apartment
					
				if(tenant_mutex->value < 0){ //Reset tenant mutex value 10 times, waking up remaining tenants
					int j;
					for(j = 0; j < 10; j++)
						up(tenant_mutex);
				}
					
				down(agent_sem); //Tries to down agent sem before agent leaves	
				agentLeaves(i, start); //Agent leaves
				up(agent_open_sem); //Signals to any waiting agents they can enter
					
				return 0;
			}
			else{
				//Keep generating agents
				int randNum;
				srand(randA);
				randNum = rand() % 100;
				if(randNum >= probA){
					sleep(delayA);
				}
			}
		}
	}

	int x;
	for(x = 0; x < m; x++)
		wait(NULL);
	for(x = 0; x < k; x++)
		wait(NULL);
	wait(NULL);

}
