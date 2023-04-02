#define MAX_SIZE 1024

extern struct mutex input_mutex;

struct shared_data {
	char message[MAX_SIZE];
	short message_size; 
};

