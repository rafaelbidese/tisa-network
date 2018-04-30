#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <time.h>
#include <math.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/types.h>

#define BUF_SIZE		1000
#define NSEC_PER_SEC    1000000000 

#define COMM_RECEIVER		1
#define COMM_TRANSMITTER	2
#define COMM_SERVER_IP			"127.0.0.1"			
#define COMM_CLIENT_IP			"127.0.0.1" 		
#define COMM_UDP_PORT_TO_SERVER	"4546"    	
#define COMM_UDP_PORT_TO_CLIENT	"4547"    	
#define MAX_PENDING_MSG	5 


#define DATA_NULL	  0
#define DATA_BOOL	  1
#define DATA_INT	  2
#define DATA_FLOAT	3
#define DATA_STRING 4

#define MSG_PREFIX  				         "das"
#define MSG_ACK											 01
#define MSG_NACK									   02  
#define MSG_RESPONSE	  					   03
#define MSG_REQUEST						       04

typedef bool (*generic_thread)(void);

struct thread_info {
	int interval;
	generic_thread thread;	
};

struct message
{	
	char prefix[BUF_SIZE]; 	
	unsigned int msg_id;		
	unsigned int packet_id;	
	unsigned int checksum;	
	float value; 			
};

struct message_queue
{	
	unsigned int n_msg;					
	struct message message[MAX_PENDING_MSG];	
};

int create_socket(char *p_host, char *p_port, unsigned int type);

void read_global(pthread_mutex_t *mtx, void *p_global_var, void *p_local_var, unsigned int type);

void write_global(pthread_mutex_t *mtx, void *p_global_var, void *p_new_value, unsigned int type);

void queue_init(pthread_mutex_t *mtx, struct message_queue *p_msg_queue);

bool message_enqueue(pthread_mutex_t *mtx, struct message_queue *p_msg_queue, struct message *p_msg);

bool message_dequeue(pthread_mutex_t *mtx, struct message_queue *p_msg_queue, struct message *p_msg);

int queue_find(pthread_mutex_t *mtx, struct message_queue *p_msg_queue, unsigned int packet_id);

bool queue_next(pthread_mutex_t *mtx, struct message_queue *p_msg_queue, struct message *p_msg);

void print_data(pthread_mutex_t *mtx, bool *p_print, unsigned int *p_line, char *p_text, 
					void *p_value, unsigned int type);

void message_encode_string(struct message *p_msg, char *p_msg_buffer);

void message_decode_string(char *p_msg_buffer, struct message *p_msg);

unsigned int calculate_checksum(struct message *p_msg, pthread_mutex_t *mtx, bool *p_checksum_error_on);

bool check_checksum(struct message *p_msg);

void *periodic_thread(void *arg);



int create_socket(char *p_host, char *p_port, unsigned int type){
	
	int socket_fd;
	struct addrinfo addr_sim;
	int res_get_info;
	struct addrinfo *p_result_addr, *p_i_result;	
		
	if(type == COMM_RECEIVER)
	{
		printf("Establishing receiver socket on port %s \n", p_port);		

		memset(&addr_sim, 0, sizeof(struct addrinfo));
		addr_sim.ai_family = AF_UNSPEC; 		
		addr_sim.ai_socktype = SOCK_DGRAM; 	
		addr_sim.ai_flags = AI_PASSIVE;		
		addr_sim.ai_protocol = 0;          	
		addr_sim.ai_canonname = NULL;
		addr_sim.ai_addr = NULL;
		addr_sim.ai_next = NULL;
	}
	else if(type == COMM_TRANSMITTER)
	{
		printf("Establishing transmitter socket with IP %s port %s \n", 
		p_host, p_port);		

		memset(&addr_sim, 0, sizeof(struct addrinfo));
		addr_sim.ai_family = AF_UNSPEC; 		
		addr_sim.ai_socktype = SOCK_DGRAM; 	
		addr_sim.ai_flags = 0;
		addr_sim.ai_protocol = 0;          	
	}
 
   
	res_get_info = getaddrinfo(p_host, p_port, &addr_sim, &p_result_addr);
  if (res_get_info != 0) 
  {
	// Error.
      printf("\nERROR Did not get address info.\n\n");
      exit(EXIT_FAILURE);
  }
  
	for (p_i_result = p_result_addr; p_i_result != NULL; p_i_result = p_i_result->ai_next) 
	{
		socket_fd = socket(p_i_result->ai_family, p_i_result->ai_socktype, p_i_result->ai_protocol);
	      if (socket_fd == -1) 
	          continue;

		if(type == COMM_RECEIVER)
		{
			if (bind(socket_fd, p_i_result->ai_addr, p_i_result->ai_addrlen) == 0)
				break;	
		}
		else if(type == COMM_TRANSMITTER)
		{
			if (connect(socket_fd, p_i_result->ai_addr, p_i_result->ai_addrlen) != -1)
				break;	
		}

	      close(socket_fd); 
	  }    

	  if (p_i_result == NULL) 
	  {               
	      printf("\nERROR Could not connect.\n\n");
	      exit(EXIT_FAILURE);
	  }

  freeaddrinfo(p_result_addr); 

	return socket_fd;
}


void read_global(pthread_mutex_t *mtx, void *p_global_var, void *p_local_var, unsigned int type){

	pthread_mutex_lock(mtx);
		switch(type)
		{
			case DATA_BOOL:
				*(bool*)p_local_var = *(bool*)p_global_var; 
				break;
				
			case DATA_INT:
				*(int*)p_local_var = *(int*)p_global_var; 
				break;
				
			case DATA_FLOAT:
				*(float*)p_local_var = *(float*)p_global_var;
				break;
			default:
			  break;
		}		 
    pthread_mutex_unlock(mtx); 
}

void write_global(pthread_mutex_t *mtx, void *p_global_var, void *p_new_value, unsigned int type){
	pthread_mutex_lock(mtx);
		switch(type)
		{
			case DATA_BOOL:
				*(bool*)p_global_var = *(bool*)p_new_value; 
				break;
				
			case DATA_INT:
				*(int*)p_global_var = *(int*)p_new_value; 
				break;
				
			case DATA_FLOAT:
				*(float*)p_global_var = *(float*)p_new_value;
				break;
			default:
			  break;
		}		 
  pthread_mutex_unlock(mtx); 
}

void queue_init(pthread_mutex_t *mtx, struct message_queue *p_msg_queue){
	pthread_mutex_lock(mtx);
	
		p_msg_queue->n_msg = 0;

		for (int i=0; i < MAX_PENDING_MSG; i++)
		{
			strcpy(p_msg_queue->message[i].prefix, "");
			p_msg_queue->message[i].msg_id = 0;
			p_msg_queue->message[i].packet_id = 0;
			p_msg_queue->message[i].checksum = 0;
			p_msg_queue->message[i].value = 0.0f;		
		}
				
  pthread_mutex_unlock(mtx);
}

bool message_enqueue(pthread_mutex_t *mtx, struct message_queue *p_msg_queue, struct message *p_msg){
	
	pthread_mutex_lock(mtx);
		unsigned int n_msg = p_msg_queue->n_msg;
	pthread_mutex_unlock(mtx);	

	if (n_msg == MAX_PENDING_MSG)
	{
		return false;
	}
	
	int i_enqueue = queue_find(mtx, p_msg_queue, 0);

	if (i_enqueue == -1)
	{
		return false;
	}
	
	pthread_mutex_lock(mtx);		
	
		strcpy(p_msg_queue->message[i_enqueue].prefix, p_msg->prefix);
		p_msg_queue->message[i_enqueue].msg_id = p_msg->msg_id;
		p_msg_queue->message[i_enqueue].packet_id = p_msg->packet_id; 
		p_msg_queue->message[i_enqueue].checksum = p_msg->checksum;
		p_msg_queue->message[i_enqueue].value = p_msg->value;		
		
		p_msg_queue->n_msg = p_msg_queue->n_msg + 1;
				
  pthread_mutex_unlock(mtx);				
   
  return true;
}


bool message_dequeue(pthread_mutex_t *mtx, struct message_queue *p_msg_queue, struct message *p_msg){
	
	int i_dequeue = queue_find(mtx, p_msg_queue, p_msg->packet_id);

	if (i_dequeue == -1)
	{
		return false;
	}
	
	pthread_mutex_lock(mtx);		
	
		strcpy(p_msg_queue->message[i_dequeue].prefix, "");
		p_msg_queue->message[i_dequeue].msg_id = 0;
		p_msg_queue->message[i_dequeue].packet_id = 0;
		p_msg_queue->message[i_dequeue].checksum = 0;
		p_msg_queue->message[i_dequeue].value = 0.0f;		
		
		p_msg_queue->n_msg = p_msg_queue->n_msg - 1;
				
  pthread_mutex_unlock(mtx);				
   
  return true;
}

int queue_find(pthread_mutex_t *mtx, struct message_queue *p_msg_queue, unsigned int packet_id){
	
	pthread_mutex_lock(mtx);

		for (int i=0; i < MAX_PENDING_MSG; i++)
		{
			if (p_msg_queue->message[i].packet_id == packet_id)
			{
   			pthread_mutex_unlock(mtx);
				return i;
			}
		}

	pthread_mutex_unlock(mtx);

  return -1;
}

bool queue_next(pthread_mutex_t *mtx, struct message_queue *p_msg_queue, struct message *p_msg){

	int packet_id_min = 999999;
	
	pthread_mutex_lock(mtx);
		unsigned int n_msg = p_msg_queue->n_msg;
	pthread_mutex_unlock(mtx);
	
	if (n_msg == 0)
	{
		return false;
	}
	
	pthread_mutex_lock(mtx);

	unsigned int id_min;
	
		for (int i=0; i < MAX_PENDING_MSG; i++)
		{
			if (p_msg_queue->message[i].packet_id != 0)
			{
				if (p_msg_queue->message[i].packet_id < packet_id_min)
				{
					packet_id_min = p_msg_queue->message[i].packet_id;
					id_min = i;
				}
			}
		}
		
		strcpy(p_msg->prefix, p_msg_queue->message[id_min].prefix);
		p_msg->msg_id = p_msg_queue->message[id_min].msg_id;
		p_msg->packet_id = p_msg_queue->message[id_min].packet_id; 
		p_msg->checksum = p_msg_queue->message[id_min].checksum;
		p_msg->value = p_msg_queue->message[id_min].value;
		
	pthread_mutex_unlock(mtx);

  return true;
}

void print_data(pthread_mutex_t *mtx, bool *p_print_screen_on, unsigned int *p_line, char *p_text, void *p_value, unsigned int type){
	
	bool print;
	
	read_global(mtx, (void*) p_print_screen_on, &print, DATA_BOOL);
	
	if (print == true)
	{	
	
		printf("%s", p_text); 

		switch(type)
		{
			case DATA_NULL:
				break;
				
			case DATA_BOOL:
				printf("%1u", *(bool*)p_value); 
				break;
				
			case DATA_INT:
				printf("%d", *(int*)p_value);
				break;
				
			case DATA_FLOAT:
				printf("%6.2f", *(float*)p_value);
				break;
				
			case DATA_STRING:
				printf("%s", (char*)p_value);
				break;
			
			default:
				break;	
		}
		printf("\n");
	}

}


void message_encode_string(struct message *p_msg, char *p_msg_buffer){
	
	//MSG_PREFIX[3]+MSG_ID[1]+PACKET_ID[4]+CHECKSUM[8]+VALUE 
	
	char temp[BUF_SIZE];	
	
	strcpy(p_msg_buffer, "");
	strcpy(p_msg_buffer, p_msg->prefix);

	strcpy(temp, "");
	sprintf(temp, "-%02u", p_msg->msg_id);

	strcat(p_msg_buffer, temp);
	strcpy(temp, "");
	sprintf(temp, "-%04u", p_msg->packet_id);	

	strcat(p_msg_buffer, temp);
	strcpy(temp, "");
	sprintf(temp, "-%08d", p_msg->checksum);

	strcat(p_msg_buffer, temp);
	strcpy(temp, "");
	sprintf(temp, "-%06.2f", p_msg->value);

	strcat(p_msg_buffer, temp);
	
	printf("Message: '%s'.", p_msg_buffer);


}

void message_decode_string(char *p_msg_buffer, struct message *p_msg){
 
	sscanf(p_msg_buffer, "%3s-%2u-%4u-%8d-%f", p_msg->prefix, &(p_msg->msg_id),
			&(p_msg->packet_id), &(p_msg->checksum), &(p_msg->value));

	printf("Message: '%s'.", p_msg_buffer);

}

unsigned int calculate_checksum(struct message *p_msg, pthread_mutex_t *mtx, bool *p_checksum_error_on){
	
	bool checksum_error;
	unsigned int checksum;

	checksum = p_msg->msg_id ^ p_msg->packet_id	^ (unsigned int) p_msg->value;
	
	if (p_checksum_error_on != NULL)
	{			
		read_global(mtx, (void*) p_checksum_error_on,	&checksum_error, DATA_BOOL);			
		if(checksum_error == true)
		{
			checksum = !checksum;
		}
	}				
	
	return checksum;	

}

bool check_checksum(struct message *p_msg){
	if(p_msg->checksum == calculate_checksum(p_msg, NULL, NULL))
		return true;
	else
		return false;
}

void *periodic_thread(void *arg){

	bool quit = false;

	struct thread_info *p_thread_info = (struct thread_info *) arg;
	struct thread_info ti = *p_thread_info;

	unsigned long int thread_period_sec = (unsigned long int) ti.interval;
	unsigned long int thread_period_nsec = (unsigned long int) ((ti.interval-thread_period_sec)*NSEC_PER_SEC);

 	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC ,&t);
	t.tv_sec++;

	while(!quit){
		clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &t, NULL);

		quit = ti.thread();

		t.tv_sec += thread_period_sec;
		t.tv_nsec += thread_period_nsec;

		while (t.tv_nsec >= NSEC_PER_SEC) {
		       t.tv_nsec -= NSEC_PER_SEC;
		        t.tv_sec++;
  	}
  }
  pthread_exit(NULL);
}