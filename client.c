#include "lib.h" 

#define THREAD_PERIOD		5.0f 	

bool print_on_screen;
bool quit_program;
bool request_data_on;
bool send_acknack_on;
bool checksum_error_on;
float temp_meas = 0.0f; 	
struct message_queue queue_pending_msg;

pthread_mutex_t mtx_io = PTHREAD_MUTEX_INITIALIZER;

void request_data();
void receive_data();

int main (int argc, char *argv[]) 
{

	
	bool quit, print, req_data, send_acknack, checksum_error;

	char command;	
			
	printf("\n### CLIENT ###\n");					
	
	queue_init(&mtx_io, &queue_pending_msg);
	quit = false;	
	write_global(&mtx_io, &quit_program, &quit, DATA_BOOL);	
	print = true;
	write_global(&mtx_io, &print_on_screen, &print, DATA_BOOL);
	req_data = true;
	write_global(&mtx_io, &request_data_on, &req_data, DATA_BOOL);
	send_acknack = true;
	write_global(&mtx_io, &send_acknack_on, &send_acknack, DATA_BOOL);
	checksum_error = false;
	write_global(&mtx_io, &checksum_error_on, &checksum_error, DATA_BOOL);
	
	pthread_t th_request_data, th_receive_data;	
	
	pthread_create(&th_request_data, NULL, (void*) request_data, NULL);
	pthread_create(&th_receive_data, NULL, (void*) receive_data, NULL); 		

	while(!quit)
	{
		scanf("%c", &command);		
		
		switch(command)
		{	
			case 10: 
				print = false;
				write_global(&mtx_io, &print_on_screen, 
								&print, DATA_BOOL);
				
				printf("Type '1'+<Enter> to start/stop client requests.\n");
				printf("Type '2'+<Enter> to start/stop sending ACK/NACK.\n");
				printf("Type '3'+<Enter> to start/stop sending wrong checksum.\n");
				printf("Type 'q'+<Enter> to quit program.\n");
				printf("Press <Enter> to leave menu.\n");	
				
				while(!print)
				{
					scanf("%c", &command);
					
					switch(command)
					{
						case '1':
							read_global(&mtx_io, &request_data_on, 
											&req_data, DATA_BOOL);
							if (req_data == true)
							{
								req_data = false;
								printf("Data requests turned off.\n");								
							}
							else
							{
								req_data = true;								
								printf("Data requests turned on.\n");	
							}
							write_global(&mtx_io, &request_data_on, 
												&req_data, DATA_BOOL);								
							break;
						
						case '2':
							read_global(&mtx_io, &send_acknack_on, 
											&send_acknack, DATA_BOOL);
							if (send_acknack == true)
							{
								send_acknack = false;
								printf("ACK/NACK turned off.\n");								
							}
							else
							{
								send_acknack = true;								
								printf("ACK/NACK turned on.\n");	
							}
							write_global(&mtx_io, &send_acknack_on, 
												&send_acknack, DATA_BOOL);
							break;	
							
						case '3':
							read_global(&mtx_io, &checksum_error_on, 
											&checksum_error, DATA_BOOL);
							if (checksum_error == true)
							{
								checksum_error = false;
								printf("Checksum errors turned off.\n");								
							}
							else
							{
								checksum_error = true;								
								printf("Checksum errors turned on.\n");	
							}
							write_global(&mtx_io, &checksum_error_on, 
												&checksum_error, DATA_BOOL);
							break;	
							
						case 'q':
							print = true;
							write_global(&mtx_io, &print_on_screen, 
											&print, DATA_BOOL);
							quit = true;
							write_global(&mtx_io, &quit_program, 
											&quit, DATA_BOOL);							
							break;
							
						case 10: 
							print = true;
							write_global(&mtx_io, &print_on_screen, 
											&print, DATA_BOOL);
							break;	
							
						default:
							printf("Unknown command.\n");
							break;
					}
						
				}				
				break;			
				
			default:
				break;
		}	
		read_global(&mtx_io, &quit_program, &quit, DATA_BOOL);
	}

	return 0; 
}

void request_data(){
	
	bool quit = false;
	bool print;
	bool req_data;
	char input_buffer[BUF_SIZE];	
	unsigned int line_print = 0;
	struct message msg;

	int socket_server = create_socket(COMM_SERVER_IP, COMM_UDP_PORT_TO_SERVER, 
									COMM_TRANSMITTER);	



	unsigned long int thread_period_sec = (unsigned long int) THREAD_PERIOD;
	unsigned long int thread_period_nsec = (unsigned long int) ((THREAD_PERIOD-thread_period_sec)*NSEC_PER_SEC);

 	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC ,&t);
	t.tv_sec++;

	unsigned int last_packet_id = 0;


	while(!quit){
		clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &t, NULL);

		read_global(&mtx_io, &request_data_on, &req_data, DATA_BOOL);	
		if (req_data == true)
		{				
			last_packet_id++; 
			if (last_packet_id > 9999) last_packet_id = 1;		 
			
			strcpy(msg.prefix, MSG_PREFIX);	
			msg.msg_id = MSG_REQUEST;
			msg.packet_id = last_packet_id;
			msg.checksum = 0;
			msg.value = 0.0f;		
		
			message_encode_string(&msg, input_buffer);	 
			
			print_data(&mtx_io, &print_on_screen, &line_print, 
						"CLIENT Out: Request #", &(msg.packet_id), DATA_INT);
			if (write(socket_server, input_buffer, strlen(input_buffer)) 
					!= strlen(input_buffer))
			{
				print_data(&mtx_io, &print_on_screen, &line_print, 
						"ERROR Request #", &(msg.packet_id), DATA_INT);
			}		
			
			if (message_enqueue(&mtx_io, &queue_pending_msg, &msg) == false)
			{
				print_data(&mtx_io, &print_on_screen, &line_print, 
					"ERROR Buffer full #", &(msg.packet_id), DATA_INT);
			}
		}

		t.tv_sec += thread_period_sec;
		t.tv_nsec += thread_period_nsec;

		while (t.tv_nsec >= NSEC_PER_SEC) {
		       t.tv_nsec -= NSEC_PER_SEC;
		        t.tv_sec++;
  	}
  }
  pthread_exit(NULL);
}

void receive_data(){
	bool quit = false;
	bool reply_client;
	bool send_acknack;


	char input_buffer[BUF_SIZE];
	unsigned int print = 0;
	struct sockaddr_storage client;
	socklen_t client_addr_len = sizeof(struct sockaddr_storage);

	struct message msg, msg_pending;
	
	int socket_client = create_socket(NULL, COMM_UDP_PORT_TO_CLIENT, COMM_RECEIVER);
	int socket_server = create_socket(COMM_SERVER_IP, COMM_UDP_PORT_TO_SERVER, COMM_TRANSMITTER);



	while(!quit)
	{		
			strcpy(input_buffer, "");		
			ssize_t size = recvfrom(socket_client, input_buffer, BUF_SIZE, 0, (struct sockaddr *) &client, &client_addr_len);
	    
	    if (size != -1)
	    {	
				message_decode_string(input_buffer, &msg);
					
				switch(msg.msg_id)
				{

					case MSG_RESPONSE:
									
					print_data(&mtx_io, &print_on_screen, &print, "CLIENT In Reply #", &(msg.packet_id), DATA_INT);				
					
					if (check_checksum(&msg) == true)
					{
						msg.msg_id = MSG_ACK;

						FILE *logfile = fopen("clientlog.txt", "ab");
						if (logfile == NULL)
						{
							printf("\nERROR Error opening file.\n\n");
							exit(EXIT_FAILURE);
						}

						char str[30];
						unsigned long current_time = (unsigned long)time(NULL);
						sprintf(str, "%lu;%2.2f;%d;", current_time, msg.value, msg.packet_id);
						fprintf(logfile, "%s\n", str);
						
						fclose(logfile);
						print_data(&mtx_io, &print_on_screen, &print, "Temperature: ", &(msg.value), DATA_FLOAT);
					}
					else
					{
						msg.msg_id = MSG_NACK;
						print_data(&mtx_io, &print_on_screen, &print,	"ERROR Checksum #", &(msg.packet_id), DATA_INT);															
					}	
					
					msg.checksum = calculate_checksum(&msg, &mtx_io, &checksum_error_on);
					
					message_dequeue(&mtx_io, &queue_pending_msg, &msg);	
	
					read_global(&mtx_io, &send_acknack_on,&send_acknack, DATA_BOOL);
					if (send_acknack == true)
					{
						message_encode_string(&msg, input_buffer);	
						
						if (msg.msg_id == MSG_ACK)
						{
							print_data(&mtx_io, &print_on_screen, &print, "CLIENT Out ACK #", &(msg.packet_id), DATA_INT);
						}
						else if (msg.msg_id == MSG_NACK)
						{
							print_data(&mtx_io, &print_on_screen, &print, 
								"CLIENT Out NACK #", &(msg.packet_id), DATA_INT);
						}
													
						if (write(socket_server, input_buffer,strlen(input_buffer)) != strlen(input_buffer))		
						{
							print_data(&mtx_io, &print_on_screen, &print, "ERROR ACK/NACK #", &(msg.packet_id), DATA_INT);
						}  
					}
					
					if (queue_next(&mtx_io, &queue_pending_msg, &msg_pending) == true)	
					{
						message_encode_string(&msg_pending, input_buffer);
						print_data(&mtx_io, &print_on_screen, &print, 
								"CLIENT Out New Request #", 
								&(msg_pending.packet_id), DATA_INT);								
						
						if (write(socket_server, input_buffer, strlen(input_buffer)) 
								!= strlen(input_buffer))	
						{
							print_data(&mtx_io, &print_on_screen, &print, 
								"ERROR New Request #", 
								&(msg_pending.packet_id), DATA_INT);
						}  						
					}
														
					break;					
				
				default:
					print_data(&mtx_io, &print_on_screen, &print, 
						"CLIENT In Unknown message received: ", input_buffer, DATA_STRING);
					break;
			}			
		}
		
		read_global(&mtx_io, &quit_program, &quit, DATA_BOOL);
	}
	pthread_exit(NULL);
}

