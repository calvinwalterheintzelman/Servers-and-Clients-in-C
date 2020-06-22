#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>
#include <pthread.h>
#include <time.h>
#include <stdbool.h>
#include "router.h"

pthread_mutex_t lock_routingTable; //lock for accessing routingTable

extern struct route_entry routingTable[MAX_ROUTERS];
extern int NumRoutes;

bool router_checks[MAX_ROUTERS];
unsigned int initNbrCosts[MAX_ROUTERS];

int getNbrEntry(unsigned int);

int hasBeenUpdated;
int hasConverged;

bool converged;

// TODO FIXME REMEMBER to download original makefile to test on ECN

int open_listenfd_udp(int port) { //slightly changed for UDP from http //taken from lab1
	int listenfd, optval=1;
	struct sockaddr_in serveraddr;
	/* Create a socket descriptor */
	if ((listenfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
		return(-1);
	/* Eliminates "Address already in use" error from bind. */
	if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR,(const void *)&optval , sizeof(int)) < 0)
		return(-1);
	/* Listenfd will be an endpoint for all requests to port
	on any IP address for this host */
	bzero((char *) &serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = INADDR_ANY;//htonl(INADDR_ANY);
	serveraddr.sin_port = htons(port);
	if (bind(listenfd, (struct sockaddr*)&serveraddr, sizeof(serveraddr)) < 0)
		return(-1);
	return listenfd;
}

//to be deleted:
struct UDPlistenData { // Used as the argument for the UDP_file_descriptor_function(void *arg) argument
    int clientfd;
    int myID;
    struct sockaddr_in server_addr; //declare the sockaddr_in struct
};


int open_clientfd(char* servername, int port, struct sockaddr_in* serv_addr) { // from lab 1
	int clientfd;
	struct hostent *hp;
	struct sockaddr_in serveraddr;

	if ((clientfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		return -1;
	} // checks for errors

	if ((hp = gethostbyname(servername)) == NULL) {
		return -2; // checks for errors
	}
	
	bzero((char*) &serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	bcopy((char *)hp->h_addr, (char *)&serveraddr.sin_addr.s_addr, hp->h_length); //this is not actually an error
	serveraddr.sin_port = htons(port);

	// finally, establish connection
	/*if (connect(clientfd, (const struct sockaddr*) &serveraddr, sizeof(serveraddr)) < 0) {
        return -1;
	}*/

    *serv_addr = serveraddr;

	return clientfd;
}
/*
struct pkt_RT_UPDATE {
  unsigned int sender_id; // id of router sending the message 
  unsigned int dest_id;  //id of neighbor router to which routing table is sent 
  unsigned int no_routes;  //number of routes in my routing table 
  struct route_entry route[MAX_ROUTERS]; // array containing rows of routing table 
};
*/
void * UDP_file_descriptor_function(void *arg){
    /* to be deleted:
    int* new = (int *) arg;
    int port = (int) (*new); // port is passed in the arg
    */
    int* new = (int *) arg; 
    int listenfd = (int) (*new); // listenfd is passed in the arg    
    //int listenfd = open_listenfd_udp(port); //open a listenfd so that this thread functions as a UDP server

    struct sockaddr_in ne_clientaddr; //declare sockaddr_in ne_clientaddr
	socklen_t ne_clientlen = sizeof(ne_clientaddr); //initialize size of ne_clientaddr
	memset(&ne_clientaddr,0,ne_clientlen); //set client address to empty
    struct pkt_RT_UPDATE updatepkt; //declare the update packet
	    
    //bind socket


    while(1){
	    bzero(&updatepkt, sizeof(updatepkt)); //zero out the update packet
        //printf("cant wait to hear from everybody! Hope I dont live in limbo forever!\n");
        recvfrom(listenfd, &updatepkt, sizeof(updatepkt), 0, (struct sockaddr *) &ne_clientaddr, &ne_clientlen); //receive the update from the network emulator
        ntoh_pkt_RT_UPDATE (&updatepkt); // convert update packet from network order to host order
        //printf("IM NOT IN LIMBO :D");
        int myID = routingTable[0].dest_id; // my ID is the destination of the 0th entry in the table
        int costToNbr;
        int index;
        pthread_mutex_lock(&lock_routingTable); // wait for the lock
        

        for(index = 1; index < NumRoutes; index++){
            //if the entry for the sender has a cost of INFINITY, set the cost to the initial cost to sender
            if(routingTable[index].dest_id == updatepkt.sender_id){ //if this is the routing table entry for the sender of the update packet
                if(routingTable[index].cost == INFINITY){
                    routingTable[index].cost = initNbrCosts[updatepkt.sender_id]; //set the cost to initial cost to sender
                    routingTable[index].path[1] = updatepkt.sender_id;
                    routingTable[index].path_len = 2;
                    routingTable[index].next_hop = updatepkt.sender_id;
                    hasBeenUpdated = 1;
                }
                costToNbr = routingTable[index].cost;
                break;
            }
        }

/*
        for(int ind = 0; ind < updatepkt.no_routes; ind++){
            if(updatepkt.route[ind].dest_id == myID && updatepkt.route[ind].cost < costToNbr && updatepkt.route[ind].next_hop == myID) {
                costToNbr = updatepkt.route[ind].cost;
                routingTable[index].path[1] = updatepkt.sender_id;
                routingTable[index].path_len = 2;
                routingTable[index].next_hop = updatepkt.sender_id;
            }
        }
*/
        //printf("\nPROCESSING PACKET FROM %d BEEP BOP BOOP\n",updatepkt.sender_id);
        hasBeenUpdated |= UpdateRoutes(&updatepkt, costToNbr, myID); //Update routes using the update packet given, the costTo sender of update and myID
        pthread_mutex_unlock(&lock_routingTable); //unlock routingTable
        //printf("letting everyone knwo that my nbr %d is alive and well\n",updatepkt.sender_id);
        router_checks[updatepkt.sender_id] = true; //set the value in the array at router_checks to true
    }
    return NULL;
}



struct TimerData { // Used as the argument for the Timer (void *arg) argument
    int clientfd;
    int myID;
    struct sockaddr_in server_addr; //declare the sockaddr_in struct
    char* str_ID;
    clock_t main_start;
};

void * timer_function(void *arg){
    // CONVERGE_TIMEOUT is the number of seconds that needs to pass without updates to assume a table has converged
    // UPDATE_INTERVAL Timer, when expired:
    // call ConvertTableToPkt
    // RT_UPDATE to all neighbors
    // FAILURE_DETECTION: number of seconds during which
    // a neighbor must be heard from.  If not heard from in
    // this time, mark the neighbor as dead:
    // call UninstallRouteOnNbrDeath
    // CONVERGE_TIMEOUT: number of seconds without changes
    // to this router's table which indicates convergence:
    // on convergence: write table to log file

    struct TimerData* t_data = (struct TimerData*)(arg);
    int myID = t_data->myID;
    int clientfd = t_data->clientfd;
    struct sockaddr_in server_addr = t_data->server_addr;
    char* str_ID = t_data->str_ID;
    clock_t start;
    clock_t curr;
    clock_t conv_start;
    
    converged = false;

    bool neighbors[MAX_ROUTERS];
	
    int c;
    for(c = 0; c < MAX_ROUTERS; c++) {
        neighbors[c] = false;
    }
    int b;

    for(b = 0; b < NumRoutes; b++) {
        if(routingTable[b].next_hop == routingTable[b].dest_id) {
            neighbors[routingTable[b].dest_id] = true;
        }
    }

    clock_t router_time_starts[MAX_ROUTERS];
    
    int k;
    for(k = 0; k < MAX_ROUTERS; k++) {
        router_time_starts[k] = clock();
    }

    //double time_check; // do ((double) (end - start)) / CLOCKS_PER_SEC
    
    start = clock();
    conv_start = clock();

    while(1) {
        curr = clock();
        if(((double) (curr - start)) / CLOCKS_PER_SEC >= UPDATE_INTERVAL) {
	    int i;
            for(i = 0; i < NumRoutes; i++) {
                if(routingTable[i].dest_id != myID && neighbors[routingTable[i].dest_id]) { //this is a neighbor and not myself
                    struct pkt_RT_UPDATE updatepacket;
                    bzero(&updatepacket, sizeof(updatepacket));

                    //printf("\n\nMy id is %d\n\n", myID);
                    pthread_mutex_lock(&lock_routingTable); // wait for the lock
                    ConvertTabletoPkt(&updatepacket, myID);
                    pthread_mutex_unlock(&lock_routingTable); // relock the routingTable

                    updatepacket.dest_id = routingTable[i].dest_id;

                    //printf("\n\nsender_id: %d\ndest_id: %d\nno_routes: %d\n\n", updatepacket.sender_id, updatepacket.dest_id, updatepacket.no_routes);
                    //for(int i = 0; i < NumRoutes; i++) {
                        //printf("dest_id: %d\nnext_hop: %d\ncost: %d\n", updatepacket.route[i].dest_id, updatepacket.route[i].next_hop, updatepacket.route[i].cost);
                    //}

                    hton_pkt_RT_UPDATE(&updatepacket);

                    socklen_t sizeof_r = sizeof(server_addr); //initialized in the sendto function
                    sendto(clientfd, &updatepacket, sizeof(updatepacket), 0, (struct sockaddr *) &server_addr, sizeof_r);
                    //printf("hey its me im sending an update at time %f", (((double) (curr - start)) / CLOCKS_PER_SEC));
                    //recvfrom(clientfd, &receievedupdate, sizeof(receievedupdate), 0, (struct sockaddr *) &server_addr, &sizeof_r); //receive the INIT_RESPONSE from the network emulator
                }
            }
            start = clock();
        }

        //check if any router updates
       // bool router_updates = false;
        curr = clock();
	int j;
        for(j = 0; j < NumRoutes; j++) {
            pthread_mutex_lock(&lock_routingTable); // wait for the lock
            if(neighbors[routingTable[j].dest_id] && routingTable[j].dest_id != myID) { //this is a nieghbor and not myself
                ////printf("chekcing if my nbr %d is alive",routingTable[j].dest_id);
                if(router_checks[routingTable[j].dest_id]) {
                    //reset the clock
                    //printf("\nhi my br %d is alive\n", routingTable[j].dest_id);
                    router_time_starts[routingTable[j].dest_id] = clock();

                    router_checks[routingTable[j].dest_id] = false;
                   // router_updates = true;
                }

                //&& routingTable[j].cost != INFINITY
            
                else if(neighbors[routingTable[j].dest_id] && routingTable[j].cost != INFINITY && ((double)(curr - router_time_starts[routingTable[j].dest_id])) / CLOCKS_PER_SEC >= FAILURE_DETECTION) { //check to see if too much time has passed

                    UninstallRoutesOnNbrDeath(routingTable[j].dest_id);
                    //printf("\nHELP! MY NEIGHBOR IS DEAD :O nbr = %d\n", routingTable[j].dest_id);
                    char logfile[20] = {'\0'};
                    strcpy(logfile, "router");
                    strcat(logfile, str_ID);
                    strcat(logfile, ".log");
                    FILE* logfp = fopen(logfile, "a"); 
                    PrintRoutes(logfp, myID);
                    //fprintf(logfp, "\nIm wrote that^ because my neighbor %d died\n", routingTable[j].dest_id);
                    fclose(logfp);
                    converged = false;
                }
            }
            pthread_mutex_unlock(&lock_routingTable); // relock the routingTable

        }
    
        if(hasBeenUpdated) {
            pthread_mutex_lock(&lock_routingTable); // unlock the routing table
            converged = false;
            conv_start = clock();
            hasBeenUpdated = 0;
            char logfile[20] = {'\0'};
            strcpy(logfile, "router");
            strcat(logfile, str_ID);
            strcat(logfile, ".log");
            FILE* logfp = fopen(logfile, "a"); 
            PrintRoutes(logfp, myID);
            //fprintf(logfp, "\nIm wrote that^ because my neighbor updated my tables.\n");
            fclose(logfp);
            //printf("UPDATED\n");
            pthread_mutex_unlock(&lock_routingTable); // relock the routingTable
        }
        else if (((double)(curr - conv_start)) / CLOCKS_PER_SEC >= CONVERGE_TIMEOUT && !converged) {
            //converged
            char logfile[20] = {'\0'};
            strcpy(logfile, "router");
            strcat(logfile, str_ID);
            strcat(logfile, ".log");
            FILE* logfp = fopen(logfile, "a"); 
            //write to logfile
            //PrintRoutes(logfp, myID);
            int total_time = (int)(((double)(curr - t_data->main_start)) / CLOCKS_PER_SEC);
            fprintf(logfp,"%d:Converged\n\n",total_time);
            fclose(logfp);
            converged = true;
        }
        /*
        if(router_updates) {
            dead_start = clock();
        } else if (((double)(curr - dead_start)) / CLOCKS_PER_SEC >= CONVERGE_TIMEOUT) {
            //////printf("\ntime it took to converge: %f\n", ((double)(curr - conv_start)) / CLOCKS_PER_SEC );

            //CHANGE THIS BELOW TO ACCOUNT FOR NEIGHBOR DYING
            
        }*/
        

        //check to see if anything changed
        
    }


    return NULL;
}



int main(int argc, char **argv) {

    int a;
    for(a = 0; a < MAX_ROUTERS; a++) {
        router_checks[a] = false;
        initNbrCosts[a] = INFINITY; //set initial costs to direct nbr to INFINITY
    }
    /*
    OVERALL
    1. send init request packe//printft
    2. receive/process init response packet
    3. make 2 threads
        i. UDP file descriptor pulling thread
        ii. Timer thread
    
    */
/* 1.
struct pkt_INIT_REQUEST {
  unsigned int router_id; // my id, used to retrieve neighbor database from ne
};
*/
    if(argc != 5){ //check that an ID is passed in
        //printf("Error: run as ./router [router id] [hostname] [host port] [router port]\n");
        return 0;
    }
    int myID = atoi(argv[1]); //myID comes as a command line argument
    char* serverName = argv[2]; //server hostname comes in the 2nd argument
    int nePort = atoi(argv[3]); //FIXME IS CURRENTLY THE ROUTER PORT FOR TESTING
    int routerPort = atoi(argv[4]); //outer port comes in the 4th argument

    //int listenfd = open_listenfd_udp(routerPort); //open a listenfd so that this thread functions as a UDP server


    // 1. send UDP packet toint clientfd; 
    int clientfd;//declare the clientfd
    struct sockaddr_in server_addr; //declare the sockaddr_in struct
    struct sockaddr_in binding;
    bzero(&server_addr, sizeof(server_addr));


    clientfd = open_clientfd(serverName, nePort, &server_addr); //open a file descriptor for communication with the network emulator server with the server and receive a UDP response containing pkt_INIT_RESPONSE
    

	/* Listenfd will be an endpoint for all requests to port
	on any IP address for this host */
	bzero((char *) &binding, sizeof(binding));
	binding.sin_family = AF_INET;
	binding.sin_addr.s_addr = INADDR_ANY;//htonl(INADDR_ANY);
	binding.sin_port = htons(routerPort);
	
    bind(clientfd, (struct sockaddr*)&binding, sizeof(binding));
    
    struct pkt_INIT_REQUEST initRequest; //declare the initRequest packet
    initRequest.router_id = htonl(myID); // set router ID in the pkt_INIT_REQUEST

    //clientfd = open_clientfd(serverName, nePort, &server_addr); //open a file descriptor for communication with the network emulator

    struct pkt_INIT_RESPONSE init_response; //declare the pkt_init_response
	bzero(&init_response, sizeof(init_response)); //zero out the init_response

    socklen_t sizeof_r = sizeof(server_addr); //initialized in the sendto function
    sendto(clientfd, &initRequest, sizeof(initRequest), 0, (struct sockaddr *) &server_addr, sizeof_r); //sends the INIT_REQUEST to network emulator
    //printf("\nNot stuck\n");
    recvfrom(clientfd, &init_response, sizeof(init_response), 0, NULL, NULL); //receive the INIT_RESPONSE from the network emulator
    //printf("\nNot stuck\n");
    clock_t main_start = clock();
    ntoh_pkt_INIT_RESPONSE (&init_response); 
    InitRoutingTbl (&init_response, myID);
    //FINISHED: sending INIT_REQUEST and processing INIT_RESPONSE
    //TO DO: thread for sending updates for the table
    //TO DO: thread for timers which outputs to a logfile

    int k;
    for(k=0; k<NumRoutes; k++){
        initNbrCosts[routingTable[k].dest_id] = routingTable[k].cost;
    }


    //TODO change file name from "loggy.log" to router[id].log where [id] is replaced with myID
    char logfile[20] = {'\0'};
    strcpy(logfile, "router");
    strcat(logfile, argv[1]);
    strcat(logfile, ".log");
    FILE* logfp = fopen(logfile, "w"); 
    PrintRoutes(logfp, myID);
    fclose(logfp);

    pthread_t timer; //timer thread
    pthread_t UDP_file_descriptor; //UDP file descripor polling thread


    //int clientfd2 = open_clientfd(serverName, routerPort, &server_addr); //open a file descriptor for communication with the network emulator
/*
    struct UDPlistenData udp_thread_arg; //create the struct to be passed into the UDP listen thread
    udp_thread_arg.myID = myID;
    udp_thread_arg.server_addr = server_addr;
    udp_thread_arg.clientfd = clientfd2;
*/

    pthread_mutex_init(&lock_routingTable, NULL); //initialize the lock for accessing routingTable
    hasBeenUpdated = 0;
    struct TimerData t_data;
    t_data.clientfd = clientfd;
    t_data.myID = myID;
    t_data.server_addr = server_addr;
    t_data.str_ID = argv[1];
    t_data.main_start = main_start;


    //first NULL is definitely set as NULL, second NULL (fourth argument) may change
    pthread_create(&timer, NULL, &timer_function, &t_data);
    pthread_create(&UDP_file_descriptor, NULL, &UDP_file_descriptor_function, &clientfd);
    
    pthread_join(UDP_file_descriptor, NULL);

    return 0;
}
