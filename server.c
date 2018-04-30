#include "lib.h" 	

#define THREAD_PERIOD	3.0f

bool print_on_screen;
bool quit_program;
bool reply_client_on;
bool checksum_error_on;
float temp_meas = 0.0f; 	
struct message_queue queue_pending_msg;

pthread_mutex_t mtx_io = PTHREAD_MUTEX_INITIALIZER;

bool read_sensor_data();
void handle_client_request();


int main (int argc, char *argv[]) 
{
	bool quit, print, reply_client, checksum_error;

	char command;

	printf("\n### SERVER ###\n");					


	queue_init(&mtx_io, &queue_pending_msg);
	quit = false;
	write_global(&mtx_io, &quit_program, &quit, DATA_BOOL);	
	print = true;
	write_global(&mtx_io, &print_on_screen, &print, DATA_BOOL);
	reply_client = true;
	write_global(&mtx_io, &reply_client_on, &reply_client, DATA_BOOL);	
	checksum_error = false;
	write_global(&mtx_io, &checksum_error_on, &checksum_error, DATA_BOOL);

	pthread_t th_sensor_data, th_handle_client_request;	
  struct thread_info sensor_data   = {THREAD_PERIOD, read_sensor_data};

	pthread_create(&th_sensor_data, NULL,	periodic_thread, &sensor_data); 			
	pthread_create(&th_handle_client_request, NULL, (void *)handle_client_request, NULL); 

	while(!quit)
	{
		scanf("%c", &command);		
		switch(command)
		{	
			case 10:
				print = false;
				write_global(&mtx_io, &print_on_screen, 
								&print, DATA_BOOL);
				
				printf("Type '1'+<Enter> to start/stop sending reply to clients.\n");
				printf("Type '2'+<Enter> to start/stop sending wrong checksum.\n");
				printf("Type 'q'+<Enter> to quit program.\n");
				printf("Press <Enter> to leave menu.\n");	
				
				while(!print)
				{
					scanf("%c", &command);
					
					switch(command)
					{						
						case '1':
							read_global(&mtx_io, &reply_client_on, &reply_client, DATA_BOOL);
							if (reply_client == true)
							{
								reply_client = false;
								printf("Replies turned off.\n");								
							}
							else
							{
								reply_client = true;								
								printf("Replies turned on.\n");	
							}
							write_global(&mtx_io, &reply_client_on, &reply_client, DATA_BOOL);
							break;
							
						case '2':
							read_global(&mtx_io, &checksum_error_on, &checksum_error, DATA_BOOL);
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
							write_global(&mtx_io, &checksum_error_on, &checksum_error, DATA_BOOL);
							break;		
							
						case 'q':
							print = true;
							write_global(&mtx_io, &print_on_screen,	&print, DATA_BOOL);
							quit = true;
							write_global(&mtx_io, &quit_program,	&quit, DATA_BOOL);													
							break;
							
						case 10: 
							print = true;
							write_global(&mtx_io, &print_on_screen,	&print, DATA_BOOL);
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

  pthread_join(th_sensor_data, NULL);
	return 0; 
}

bool read_sensor_data(){
	
	bool quit = false;
	unsigned int print = 0;	
	float value;
	FILE *logfile = NULL;

  logfile = fopen("serverlog.txt", "rb+");
	if (logfile == NULL) 
	{
		printf("\nERROR Error opening file.\n\n");
		exit(EXIT_FAILURE);
	}	

	fseek( logfile, -2, SEEK_END ); 
  while(fgetc(logfile) != '\n')
     fseek(logfile, -2, SEEK_CUR);

  fscanf(logfile, "%f", &value); 
			
	write_global(&mtx_io, &temp_meas, &value, DATA_FLOAT);

	print_data(&mtx_io, &print_on_screen, &print,"Temperature: ", &value, DATA_FLOAT);		    

	read_global(&mtx_io, &quit_program, &quit, DATA_BOOL);

	fclose(logfile);
	return quit;
}


void handle_client_request(){

	bool quit = false;
	bool reply_client;
  char host[NI_MAXHOST], service[NI_MAXSERV];
	char input_buffer[BUF_SIZE];
	unsigned int print = 0;
	float value;
	struct sockaddr_storage client;
	socklen_t client_addr_len = sizeof(struct sockaddr_storage);

	struct message msg, msg_pending;
	
	int socket_server = create_socket(NULL, COMM_UDP_PORT_TO_SERVER, COMM_RECEIVER);
	int socket_client = create_socket(COMM_CLIENT_IP, COMM_UDP_PORT_TO_CLIENT, COMM_TRANSMITTER);	
	
	while(!quit)
	{		
		strcpy(input_buffer, "");		
		ssize_t size = recvfrom(socket_server, input_buffer, BUF_SIZE, 0, (struct sockaddr *) &client, &client_addr_len);
    
    if (size != -1)
    {	
			message_decode_string(input_buffer, &msg);
				
			switch(msg.msg_id)
			{
				case MSG_REQUEST:
					read_global(&mtx_io, &reply_client_on, &reply_client, DATA_BOOL);
					if (reply_client == true)
					{	
						
						int s_host = getnameinfo((struct sockaddr *) &client,
							client_addr_len, host, NI_MAXHOST,
							service, NI_MAXSERV, NI_NUMERICSERV);
						if (s_host != 0) { } 


						print_data(&mtx_io, &print_on_screen, &print, "SERVER In Request #", &(msg.packet_id), DATA_INT);
						
						read_global(&mtx_io, &temp_meas, &value, DATA_FLOAT);										
						
						msg.msg_id = MSG_RESPONSE;
						msg.value = value;
						msg.checksum = calculate_checksum(&msg,	&mtx_io, &checksum_error_on);	
						
						message_encode_string(&msg, input_buffer);
						
						print_data(&mtx_io, &print_on_screen, &print, "SERVER Out Reply #", &(msg.packet_id), DATA_INT);								
						
						if (write(socket_client, input_buffer, strlen(input_buffer))!= strlen(input_buffer))	
						{
							print_data(&mtx_io, &print_on_screen, &print,"ERROR Reply #", &(msg.packet_id), DATA_INT);
						}					
						
						if (queue_next(&mtx_io,	&queue_pending_msg, &msg_pending) == true)	
						{					
							msg_pending.msg_id = MSG_RESPONSE;
							msg_pending.checksum = calculate_checksum(&msg_pending, &mtx_io, &checksum_error_on);
							message_encode_string(&msg_pending, input_buffer);
							print_data(&mtx_io, &print_on_screen, &print, "SERVER Out New Reply #", &(msg_pending.packet_id), DATA_INT);								
							
							if (write(socket_client, input_buffer, strlen(input_buffer)) 
									!= strlen(input_buffer))	
							{
								print_data(&mtx_io, &print_on_screen,	&print, "ERROR New Reply #", &(msg_pending.packet_id), DATA_INT);
							}  						
						}
						
						if (message_enqueue(&mtx_io, &queue_pending_msg, &msg) == false)
						{
							print_data(&mtx_io, &print_on_screen, &print, 
								"ERROR Buffer full #", &(msg.packet_id), DATA_INT);
						}
					}
						
					break;	
					
				case MSG_ACK:
					print_data(&mtx_io, &print_on_screen, &print, 
							"SERVER In ACK #", &(msg.packet_id), DATA_INT);
					if (check_checksum(&msg) == true)
					{
						message_dequeue(&mtx_io, &queue_pending_msg, &msg);
					}	
					else
					{
						print_data(&mtx_io, &print_on_screen, &print, "ERROR Checksum #",	&(msg.packet_id), DATA_INT);	
					}
						
					break;
					
				case MSG_NACK:
					print_data(&mtx_io, &print_on_screen, &print, 
							"SERVER In NACK #", &(msg.packet_id), DATA_INT);								
					break;
				
				default:
					print_data(&mtx_io, &print_on_screen, &print, 
						"SERVER In Unknown message received: ", input_buffer, DATA_STRING);
					break;
			}			
		
		}
		
		read_global(&mtx_io, &quit_program, &quit, DATA_BOOL);
	} 

	pthread_exit(NULL);
}
