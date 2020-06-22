#include "ne.h"
#include "router.h"
#include <stdbool.h>


/* ----- GLOBAL VARIABLES ----- */
struct route_entry routingTable[MAX_ROUTERS];
int NumRoutes;


int getNbrEntry(unsigned int nbr){ //used by UDP_update thread to get the current cost to nbr
	return routingTable[nbr].cost;
}

////////////////////////////////////////////////////////////////
void InitRoutingTbl (struct pkt_INIT_RESPONSE *InitResponse, int myID){
	/* ----- YOUR CODE HERE ----- */
	//Function arguments:
	//1. (struct pkt_INIT_RESPONSE *) - The INIT_RESPONSE from Network Emulator
	//2. int - My router's id received from command line argument.

	/*	This routine is called after receiving the INIT_RESPONSE message from the Network Emulator. 
 	*   It initializes the routing table with the bootstrap neighbor information in INIT_RESPONSE.  
 	*   Also sets up a route to itself (self-route) with next_hop as itself and cost as 0.
 	*/
/*  
struct pkt_INIT_RESPONSE { //This is the packet struct taken as an argument - Carson
  unsigned int no_nbr; //number of directly connected neighbors 
  struct nbr_cost nbrcost[MAX_ROUTERS]; //array holding the cost to each neighbor 
}
struct nbr_cost {
  unsigned int nbr; // neighbor id 
  unsigned int cost; // cost to neighbor
};
*/
/*
struct route_entry { //This is the struct to be initialized - Carson
  unsigned int dest_id; // destination router id 
  unsigned int next_hop; // next hop on the shortest path to dest_id 
  unsigned int cost; // cost to desintation router 
#ifdef PATHVECTOR
  unsigned int path_len; // length of loop-free path to dest_id, eg: with path R1 -> R2 -> R3, the length is 3; self loop R0 -> R0 is length 1 
  unsigned int path[MAX_PATH_LEN]; // array containing id's of routers along the path, this includes the source node, all intermediate nodes, and the destination node; self loop R0 -> R0 should only contain one instance of R0 in path 
#endif
};
*/ 	
	// init path to self
	struct route_entry newEntry; //create a new entry
	newEntry.dest_id = myID; //first destination is itself
	newEntry.next_hop = myID; //next hop is to itself
	newEntry.cost = 0; //cost to reach self is always 0
	bzero(newEntry.path, sizeof(newEntry.path[0]) * MAX_PATH_LEN);
	newEntry.path_len = 1; //length to self is 1
	newEntry.path[0] = myID; //set first router in path to myID
	routingTable[0] = newEntry; //save this struct into the routing table as he first entry
	// init path to neighbors
	int i;
	for(i=1; i<=(InitResponse -> no_nbr); i++){ //for each entry in Init Response's array of nbr_cost structs
		struct nbr_cost curr = (InitResponse -> nbrcost)[i-1]; //save the current nbr_cost struct in a temp value
		struct route_entry newEntry; //create a new entry
		newEntry.dest_id = curr.nbr; //copy the router id
		newEntry.next_hop = curr.nbr; //copy the next hop id to be the neighboring router
		newEntry.cost = curr.cost; //copy the cost of the new entry to be the cost to the neighbor
		newEntry.path_len = 2; //for all neighboring routers, the starting path length will be 2, the router itself and the neighbor
		bzero(newEntry.path, sizeof(newEntry.path[0]) * MAX_PATH_LEN);
		newEntry.path[0] = myID; //set first router in path to myID
		newEntry.path[1] = curr.nbr; //set second router in path to neighbor's ID
		routingTable[i] = newEntry; //save this struct into the routing table
	}
	NumRoutes = (InitResponse -> no_nbr)+1; //set the number of routes to the number of neighbors AND itself
	return;
}


////////////////////////////////////////////////////////////////
/* Routine Name    : UpdateRoutes
 * INPUT ARGUMENTS : 1. (struct pkt_RT_UPDATE *) - The Route Update message from one of the neighbors of the router.
 *                   2. int - The direct cost to the neighbor who sent the update. 
 *                   3. int - My router's id received from command line argument.
 * RETURN VALUE    : int - Return 1 : if the routing table has changed on running the function.
 *                         Return 0 : Otherwise.
 * USAGE           : This routine is called after receiving the route update from any neighbor. 
 *                   The routing table is then updated after running the distance vector protocol. 
 *                   It installs any new route received, that is previously unknown. For known routes, 
 *                   it finds the shortest path using current cost and received cost. 
 *                   It also implements the forced update and split horizon rules. My router's id
 *                   that is passed as argument may be useful in applying split horizon rule.
 */
/*
struct pkt_RT_UPDATE {
  unsigned int sender_id; // id of router sending the message 
  unsigned int dest_id; // id of neighbor router to which routing table is sent 
  unsigned int no_routes; // number of routes in my routing table 
  struct route_entry route[MAX_ROUTERS]; // array containing rows of routing table 
};
*/
/*
struct route_entry { //This is the struct to be initialized - Carson
  unsigned int dest_id; // destination router id 
  unsigned int next_hop; // next hop on the shortest path to dest_id 
  unsigned int cost; // cost to desintation router 
#ifdef PATHVECTOR
  unsigned int path_len; // length of loop-free path to dest_id, eg: with path R1 -> R2 -> R3, the length is 3; self loop R0 -> R0 is length 1 
  unsigned int path[MAX_PATH_LEN]; // array containing id's of routers along the path, this includes the source node, all intermediate nodes, and the destination node; self loop R0 -> R0 should only contain one instance of R0 in path 
#endif
};
*/ 
int UpdateRoutes(struct pkt_RT_UPDATE *RecvdUpdatePacket, int costToNbr, int myID){
	/* ----- YOUR CODE HERE ----- */

	int hasChanged = 0;
	/*
	if(costToNbr >= INFINITY){ //if the sender of the update is thought to be unreachable
		int i;
		for(i=0; i<RecvdUpdatePacket->no_routes; i++){ //find the entry for this router in the sender's routes
			if(RecvdUpdatePacket->route[i].next_hop == myID){ //finds 
				break; // index found
			}
		}
		for(int j=0; j<NumRoutes;j++){ //find this rout
			if(routingTable[j].dest_id == RecvdUpdatePacket->sender_id){ //set the 
				routingTable[j].cost = RecvdUpdatePacket->route[i].cost;
				costToNbr = RecvdUpdatePacket->route[i].cost;
				routingTable[j].path[1] = RecvdUpdatePacket->sender_id;
				routingTable[j].path_len = 2;
				routingTable[j].next_hop = RecvdUpdatePacket->sender_id;
			}
		}
	}
	*/
	int i;
	for(i=0; i<(RecvdUpdatePacket->no_routes); i++) { //for each route in the incoming packet
		int j=0;
		//j<NumtRoutes: check if we have incremented out of range, this means the destination not found, to be added
		//RecvdUpdatePacket->route[i].dest_id  != routingTable[j].dest_id: check if this entry is the same id as that of the recv table, if so lets compare
		while(j<NumRoutes && RecvdUpdatePacket->route[i].dest_id  != routingTable[j].dest_id){
			j++;
		}
		unsigned int potentialNewCost = (RecvdUpdatePacket->route)[i].cost + costToNbr; //add cost from nbr to dest + cost to nbr
		//printf("\nPotential cost: %d to %d\n", potentialNewCost,RecvdUpdatePacket->route[i].dest_id);
		//printf("\nCurrent cost: %d to %d\n",routingTable[j].cost,routingTable[j].dest_id);
		potentialNewCost = potentialNewCost >= INFINITY ? INFINITY : potentialNewCost;
		//j==NumRoutes: if the destination was not found
		//routingTable[j].next_hop == RecvdUpdatePacket->sender_id: if the sender is the current next hop (Forced Update)
		//potentialNewCost < routingTable[j].cost: if the recvd packet has a smaller cost
		if(j==NumRoutes || routingTable[j].next_hop == RecvdUpdatePacket->sender_id || potentialNewCost < routingTable[j].cost){ //condition to update
			bool selfInPath = false;
			int s;
			for(s=1; s<RecvdUpdatePacket->route[i].path_len; s++){ //split horizon
				if(RecvdUpdatePacket->route[i].path[s] == myID) //if an element in the path is myID
					selfInPath = true; //set selfInPath to true
			}
			if(!selfInPath && potentialNewCost != routingTable[j].cost){ //if myID is not in path, add the path
				
				hasChanged = 1; // cost has changed in this routing table
				
				struct route_entry newRoute;
				newRoute.cost = potentialNewCost; // assign new cost
				newRoute.dest_id = RecvdUpdatePacket->route[i].dest_id; //set destination ID
				//newRoute.next_hop = RecvdUpdatePacket->sender_id; //set next hop to the sender ID
				newRoute.path_len = RecvdUpdatePacket->route[i].path_len + 1; //set new path length
				newRoute.path[0] = myID;
				int k;
				struct route_entry tempSenderEntry;
				for(k=1; k<NumRoutes; k++){
					if(RecvdUpdatePacket->sender_id == routingTable[k].dest_id){ //found the sender as a destination, index k contains path in routingTable[k]
						tempSenderEntry = routingTable[k];
						break;
					}
				}
				int y;
				for(y=1; y<tempSenderEntry.path_len; y++){ //copy path to sender
					newRoute.path[y] = tempSenderEntry.path[y];
				}
				newRoute.path_len += y - 2;

				for(k=1; k<RecvdUpdatePacket->route[i].path_len; k++){
					newRoute.path[y] = RecvdUpdatePacket->route[i].path[k];
					y++;
				}
				newRoute.next_hop = newRoute.path[1]; //set next hop to the next hop in the path
				routingTable[j] = newRoute; // assign the new route to the table
				
				if(j==NumRoutes) {
					NumRoutes++; //increment NumRoutes if this value is being added for the first time
				}
			}
		}
	}

	return hasChanged;
}


////////////////////////////////////////////////////////////////
void ConvertTabletoPkt(struct pkt_RT_UPDATE *UpdatePacketToSend, int myID){
	/* ----- YOUR CODE HERE ----- */
/*
struct pkt_RT_UPDATE {
unsigned int sender_id; // id of router sending the message
unsigned int dest_id; // id of neighbor router to which routing table is sent
unsigned int no_routes; // number of routes in my routing table
struct route_entry route[MAX_ROUTERS]; // array containing rows of routing table 
};*/
	
/* Routine Name    : ConvertTabletoPkt
 * INPUT ARGUMENTS : 1. (struct pkt_RT_UPDATE *) - An empty pkt_RT_UPDATE structure
 *                   2. int - My router's id received from command line argument.
 * RETURN VALUE    : void
 * USAGE           : This routine fills the routing table into the empty struct pkt_RT_UPDATE. 
 *                   My router's id  is copied to the sender_id in pkt_RT_UPDATE. 
 *                   Note that the dest_id is not filled in this function. When this update message 
 *                   is sent to all neighbors of the router, the dest_id is filled.
 */
	bzero(UpdatePacketToSend, sizeof(*UpdatePacketToSend)); //zero out the UpdatePacketToSend
	//routing table info is sent to UpdatePacketToSend
	UpdatePacketToSend->sender_id = myID;
	//do not fill out dest_id
	UpdatePacketToSend->no_routes = NumRoutes; // total number of routes in table

	int i;
	for(i = 0; i < NumRoutes; i++) { // fill out info for all routes in table
		struct route_entry currRoute;
		currRoute = routingTable[i];


		UpdatePacketToSend->route[i] = currRoute; //
	}

	return;
}


////////////////////////////////////////////////////////////////
//It is highly recommended that you do not change this function!
void PrintRoutes (FILE* Logfile, int myID){
	/* ----- PRINT ALL ROUTES TO LOG FILE ----- */
	int i;
	int j;
	for(i = 0; i < NumRoutes; i++){
		fprintf(Logfile, "<R%d -> R%d> Path: R%d", myID, routingTable[i].dest_id, myID);

		/* ----- PRINT PATH VECTOR ----- */
		for(j = 1; j < routingTable[i].path_len; j++){
			fprintf(Logfile, " -> R%d", routingTable[i].path[j]);	
		}
		fprintf(Logfile, ", Cost: %d\n", routingTable[i].cost);
	}
	fprintf(Logfile, "\n");
	fflush(Logfile);
}


////////////////////////////////////////////////////////////////
void UninstallRoutesOnNbrDeath(int DeadNbr){
	/* ----- YOUR CODE HERE ----- */
/* Routine Name    : UninstallRoutesOnNbrDeath
 * INPUT ARGUMENTS : 1. int - The id of the inactive neighbor 
 *                   (one who didn't send Route Update for FAILURE_DETECTION seconds).
 *                   
 * RETURN VALUE    : void
 * USAGE           : This function is invoked when a nbr is found to be dead. The function checks all routes that
 *                   use this nbr as next hop, and changes the cost to INFINITY.
 */

/*struct route_entry {
  unsigned int dest_id;  destination router id 
  unsigned int next_hop;  next hop on the shortest path to dest_id 
  unsigned int cost;  cost to desintation router 
#ifdef PATHVECTOR
  unsigned int path_len;  length of loop-free path to dest_id, eg: with path R1 -> R2 -> R3, the length is 3; self loop R0 -> R0 is length 1 
  unsigned int path[MAX_PATH_LEN];  array containing id's of routers along the path, this includes the source node, all intermediate nodes, and the destination node; self loop R0 -> R0 should only contain one instance of R0 in path 
#endif
};*/
	int i;
	int j;
	for(i = 0; i < NumRoutes; i++) {
		for(j = 0; j < routingTable[i].path_len; j++) {
			if(routingTable[i].path[j] == DeadNbr) {
				routingTable[i].cost = INFINITY;
				routingTable[i].path_len = 1;
			}
		}
	}

	return;
}
