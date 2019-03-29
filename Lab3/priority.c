#include "types.h"
#include "user.h"

void test_priority_order();
void test_round_robin();


const unsigned int NUM_CHILDREN = 20;

int main()
{
	test_priority_order();

	test_round_robin();

	exit();
}

void test_priority_order()
{
	printf(1, "Testing priority order:\n\n");

	int priority = 200; // start processes with the lowest priority
	int i;
	for (i=0; i<NUM_CHILDREN; i++) {
		if (fork() == 0) {
			printf(1, "changing priority to %d\n", priority);
			setpriority(priority);
			printf(1, "%d\n", priority);
			exit();		// have the children process exit
		} else {
			// create next process with a new priority
			if (i % 2 == 0) {
				priority = priority - 3;
			} else {
				priority = priority + 2;
			}
		}
	}

	
	// wait on each child
	i = 0;
	while (i != -1) {
		i = wait();
	}
}

void test_round_robin()
{
	printf(1, "testing round robin scheduling\n\n");

	int priority = 200; // start processes with the lowest priority
	int i;
	for (i=0; i<NUM_CHILDREN; i++) {
		if (fork() == 0) {
			printf(1, "changing priority of child %d to %d\n", i, priority);
			setpriority(priority);
			printf(1, "child %d, priority %d\n", i, priority);
			exit();		// have the children process exit
		} else {
			if (i % 5 == 0) {
				priority--;
			}
		}
	}

	// wait on each child
	i = 0;
	while (i != -1) {
		i = wait();
	}
}