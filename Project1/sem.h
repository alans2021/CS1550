struct cs1550_sem{
	int value; //Value of semaphore
	struct my_queue * head; //Reference to first entry in queue
	struct my_queue * tail; //Reference to last entry in queue
};
