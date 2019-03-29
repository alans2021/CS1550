#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>

struct list{
	struct node *head;
	struct node *tail;
};
struct node{
	int value;
	struct node *next;
	struct node *prev;
};

void upper(char *);

struct list *pageinfo[1048576];
int main(int argc, char* argv[]){	
	int numframes = 1;
	char * algor = "opt";
	int refCycles = 0;
	FILE * file;
	
	int i;
	for(i = 1; i < argc; i+=2){
		if(strncmp(argv[i], "-n", 2) == 0)
			numframes = atoi(argv[i + 1]);
		if(strncmp(argv[i], "-a", 2) == 0)
			algor = argv[i + 1];
		if(strncmp(argv[i], "-r", 2) == 0){
			if(strncmp(algor, "aging", 5) == 0)
				refCycles = atoi(argv[i+1]);
		}
	}
	file = fopen(argv[argc - 1], "r");
	if(file == NULL){
		printf("Please provide a valid file name");
		return -1;
	}

	int faults = 0;
	int diskwrites = 0;
	int memaccess = 0;

 	unsigned int addr;
	char mode;
	unsigned int cycles;
	if(strncmp(algor, "fifo", 4) == 0){
		struct list *linkedList = calloc(1, sizeof(struct list));
		linkedList->head = NULL;
		linkedList->tail = NULL;
		int size = 0;		
		while(fscanf(file, "%c %x %d\n", &mode, &addr, &cycles) == 3){	
			memaccess++;
			addr = addr / 4096; //Get the top 20 bits

			struct node *curr = linkedList->head;
			while(curr != NULL && (curr->value) / 2 != addr) //Look through linked list to find address
				curr = curr->next;

			if(curr == NULL){ //If can't find it, page fault
				faults++; 
				size++;
				struct node *store = calloc(1, sizeof(struct node));
				addr = addr * 2;
				if(mode == 's') //Set LSB to 1 if mode is 's'
					addr = addr + 1;
				store->value = addr;
				if(linkedList->head == NULL){ //Add node to empty list
					store->next = NULL;
					store->prev = NULL;
					linkedList->head = store;
					linkedList->tail = store;
				}
				else{ //Add node to existing list
					store->next = linkedList->head;
					linkedList->head->prev = store;
					linkedList->head = store;
				}

				if(size > numframes){ //Remove tail if linked list exceeding 8 nodes, evict page
					struct node *remov = linkedList->tail;
					linkedList->tail = linkedList->tail->prev;
					linkedList->tail->next = NULL;
					if((remov->value) % 2 == 1)
						diskwrites++;
					remov->prev = NULL;
					free(remov);
					size--;
				}
			}
			else{
				if(mode == 's'){
					if(curr->value % 2 == 0)
						curr->value += 1;
				}		
			}
				
		}
		while(linkedList->head != NULL){
			struct node *remov = linkedList->head; //Free nodes
			linkedList->head = remov->next;
			free(remov);
		}
		
		free(linkedList); //Free list
	}
	else if(strncmp(algor, "opt", 3) == 0){
		int * pagetable = calloc(numframes, sizeof(int)); //Pagetable
		
		for(i = 0; i < 1048576; i++){ //Allocate linked list in each index
			pageinfo[i] = calloc(1, sizeof(struct list));
			pageinfo[i]->head = NULL;
			pageinfo[i]->tail = NULL;
		}
		while(fscanf(file, "%c %x %d\n", &mode, &addr, &cycles) == 3){ //Preprocessing file
			memaccess++;
			addr = addr / 4096; //Get top 20 bits
			struct list *curr = pageinfo[addr]; //addr is index of array where linked list is stored
			struct node *add = calloc(1, sizeof(struct node)); //Add node with line number to correct linked list
			add->value = memaccess;
			add->next = NULL;
			add->prev = NULL;
			
			if(curr->head == NULL){ //If linked list has no nodes
				curr->head = add;
				curr->tail = add;
			}
			else{ //If linked list has at least one node
				curr->tail->next = add;
				add->prev = curr->tail;
				curr->tail = add;
			}	
		}
		fclose(file);
		
		file = fopen(argv[argc - 1], "r");
		int size = 0;
		int line = 0;
		while(fscanf(file, "%c %x %d\n", &mode, &addr, &cycles) == 3){
			addr = addr / 4096;
			line++;
			for(i = 0; i < numframes; i++){
				if(pagetable[i] / 2 == addr)
					break;
			}

			struct node *remov = pageinfo[addr]->head; //Remove head node, increment pointer
			pageinfo[addr]->head = remov->next;	   //Points to next line
			if(pageinfo[addr]->head != NULL)
				pageinfo[addr]->head->prev = NULL;
			free(remov);				
			
			if(i == numframes){ //Pagefault
				addr = addr * 2; 
				if(mode == 's') //Setting flags in address
					addr += 1;
				faults++;
				
				if(size < numframes){ //Simply add page to pagetable
					pagetable[size] = addr;
					size++;
				}
				else{ //Evict a page from pagetable
					int max = 0; //Maximum line number
					int maxIndex = 0; //Index in pagetable
					int val; //Address stored in pagetable
	
					for(i = 0; i < numframes; i++){ //Find which page has the maximum line number
						val = pagetable[i];
						if(pageinfo[val / 2]->head == NULL){ //If head is null, automatically a max
							max = INT_MAX;
							maxIndex = i;
							break;
						}
						if(pageinfo[val / 2]->head->value >= max){
							max = pageinfo[val / 2]->head->value;
							maxIndex = i;		
						}

					}
					
					if(pagetable[maxIndex] % 2 == 1) //Check flag to see if write to disk
						diskwrites++;
					pagetable[maxIndex] = addr; //Evict page stored in max index
				}	
								
			}
			else{
				if(mode == 's'){ //If previously unmodified, change flag
					if(pagetable[i] % 2 == 0)
						pagetable[i] = pagetable[i] + 1;
				}
			}

		}

		for(i = 0; i < 1048576; i++) //Free array of linkedlists
			free(pageinfo[i]);
		free(pagetable);
	}	

	else if(strncmp(algor, "aging", 5) == 0){
		int numcycles = 0;
		unsigned char * reftable = calloc(numframes, sizeof(char));
		unsigned char * countertable = calloc(numframes, sizeof(char));
		int * addresstable = calloc(numframes, sizeof(int)); //Stores addresses
		int size = 0;
		
		while(fscanf(file, "%c %x %d\n", &mode, &addr, &cycles) == 3){
			addr = addr / 4096;
			numcycles += cycles;
			memaccess++;
			while(numcycles >= refCycles){ //Refresh counter values
				for(i = 0; i < numframes; i++){ //Right shift by one
					countertable[i] = countertable[i]>>1;
					if(reftable[i] == 1) //If referenced
						countertable[i] = 0x80 | countertable[i]; //Sets MSB to 1
					reftable[i] = 0; //Reset reference to 0
				}
				numcycles -= refCycles;
			}

			for(i = 0; i < numframes; i++){
				if(addresstable[i] / 2 == addr && i < size){ //Page already in pagetable
					reftable[i] = 1; //Set referenced to 1
					if(addresstable[i] % 2 == 0 && mode == 's') //Set dirty bit to 1
						addresstable[i] += 1;
					break;
				}
			}
			
			if(i == numframes){ //Pagefault
				faults++;
				addr *= 2;
				if(mode == 's')
					addr += 1;

				if(size < numframes){ //No eviction necessary
					addresstable[size] = addr;
					reftable[size] = 0;
					countertable[size] = 0x80;
					size++;					
				}
				
				else{	//Evict page
					unsigned char min = countertable[0];
					int minIndex = 0;
					int minAddress = addresstable[0];
					for(i = 0; i < numframes; i++){
						if(countertable[i] < min){ //Get min counter value
							min = countertable[i];
							minIndex = i;
							minAddress = addresstable[i];
						}
						else if(countertable[i] == min){
							if(addresstable[i] % 2 == 0){
								if(minAddress % 2 == 1){
									min = countertable[i];
									minIndex = i;
									minAddress = addresstable[i];
								}
								else if(addresstable[i] < minAddress){
									min = countertable[i];
									minIndex = i;
									minAddress = addresstable[i];
								}
							}
							else{
								if(minAddress % 2 == 1 && addresstable[i] < minAddress){
									min = countertable[i];
									minIndex = i;
									minAddress = addresstable[i];
								}
							}
							
						}
					}
					
					if(addresstable[minIndex] % 2 == 1) //Increment diskwrites
						diskwrites++;
					addresstable[minIndex] = addr; //Add new page
					reftable[minIndex] = 0;
					countertable[minIndex] = 0x80;
				}

			}
			numcycles++;
		}
		free(addresstable);
		free(countertable);
		free(reftable);
	}

	else{
		printf("Please enter a valid algorithm\n");
		return -1;
	}

	fclose(file);
	upper(algor);
	printf("Algorithm: %s\n", algor);
	printf("Number of frames: %d\n", numframes);
	printf("Total memory accesses: %d\n", memaccess);
	printf("Total page faults: %d\n", faults);
	printf("Total writes to disk: %d\n", diskwrites);
	return 0;		
}	
	
void upper(char * s){
	while(*s != 0){
		*s = *s - 32;
		s = s + 1;
	}
}






