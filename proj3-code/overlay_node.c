/*
 * CS 1652 Project 3 
 * (c) Amy Babay, 2022
 * (c) <Student names here>
 * 
 * Computer Science Department
 * University of Pittsburgh
 */


#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/errno.h>

#include <spu_alarm.h>
#include <spu_events.h>

#include "packets.h"
#include "client_list.h"
#include "node_list.h"
#include "edge_list.h"

#define PRINT_DEBUG 1

#define MAX_CONF_LINE 1024

enum mode {
    MODE_NONE,
    MODE_LINK_STATE,
    MODE_DISTANCE_VECTOR,
};
struct row{
    uint32_t dest;
    uint32_t incoming_link;
    uint32_t outgoing_link;
};
struct forwarding_table{
    struct row *entry;
    struct forwarding_table *next;
};
struct D_val{
    uint32_t     p;
    uint32_t      dst_id;
    uint32_t      cost;
};
struct LinkedList_node{
    struct LinkedList_node *next;
    uint32_t id;
};


static uint32_t INFINITY = 50000;
static uint32_t           My_IP      = 0;
static uint32_t           My_ID      = 0;
static uint16_t           My_Port    = 0;
static enum mode          Route_Mode = MODE_NONE;
static struct client_list Client_List;
static struct node_list   Node_List;
static struct edge_list   Edge_List;
struct forwarding_table * table = NULL;

void build_table(){
    printf("build_table\n");
    // initialization
    int node_arr_len = Node_List.num_nodes;
    struct LinkedList_node * N_curr = malloc(sizeof(struct LinkedList_node));
    struct LinkedList_node * N_head = N_curr;
    struct LinkedList_node * v_curr = malloc(sizeof(struct LinkedList_node));
    struct LinkedList_node * v_head = v_curr;

    N_curr->id = My_ID; //compute least cost path from u to all other nodes

    struct D_val * D[node_arr_len-1];

    int count = 0;
    for(int i = 0; i < node_arr_len; i ++){
        // set up D
        if(Node_List.nodes[i]->id != My_ID){
            v_curr->id = Node_List.nodes[i]->id;
            D[count] = malloc(sizeof(struct D_val));
            D[count]->cost = INFINITY;
            D[count]->dst_id = v_curr->id;

            // if adjacent
            for(int j = 0; j < Edge_List.num_edges; j++){
                if(My_ID == Edge_List.edges[j]->src_id && v_curr->id ==  Edge_List.edges[j]->dst_id){
                    D[count]->cost = Edge_List.edges[j]->cost;
                    D[count]->p = My_ID;
                }
            }
            count++;
            if(i+1 != node_arr_len){
                v_curr->next = malloc(sizeof(struct LinkedList_node));
                v_curr = v_curr->next;
            }

        }
    }

    // v_curr = v_head;
    struct LinkedList_node * v_temp = v_head;
    struct LinkedList_node * v_prev = NULL;
    while(v_head != NULL){ //impl rest of dijkstras
        // find min in v
        int min_index = -1;
        for(int j = 0; j < count; j++){
            v_curr = v_head;
            while(v_curr != NULL){
                if(D[j]->dst_id == v_curr->id){
                    if(min_index == -1|| D[j]->cost < D[min_index]->cost){
                        min_index = j;
                    }
                }
                v_curr = v_curr->next;
            }
        }

        // add to N
        N_curr->next = malloc(sizeof(struct LinkedList_node));
        N_curr = N_curr->next;
        N_curr->id = D[min_index]->dst_id;
        
        // remove from v
        v_temp = v_head;
        v_prev = NULL;
        while(v_temp != NULL){ 
            if(v_temp->id == D[min_index]->dst_id){ // remove head
                if(v_prev == NULL){
                    v_head = v_head->next;
                    v_temp = v_head;
                    break;
                } else{ // remove node from body
                    v_prev->next = v_temp->next;
                    v_temp = v_prev;
                    v_temp = v_temp->next;
                }
                break;
            }
            v_prev = v_temp;
            v_temp = v_temp->next;
        }
        // update D
        for(int i = 0; i < count; i++){ // for every D
            for(int j = 0; j < Edge_List.num_edges; j++){ // check every egde
                uint32_t combined = D[min_index]->cost;
                if(D[min_index]->dst_id== Edge_List.edges[j]->src_id && D[i]->dst_id == Edge_List.edges[j]->dst_id){ 
                    combined += Edge_List.edges[j]->cost;
                    if(combined < D[i]->cost){ // update if combined is lower cost
                        D[i]->cost = combined;
                        D[i]->p = D[min_index]->dst_id;
                    }
                }
            }
        }
    }

    // make forwarding table 
    table = malloc(sizeof(struct forwarding_table));
    struct forwarding_table * table_curr = table;

    for(int i = 0; i<count; i++){
        int idx = i;
        while(D[idx]->p != My_ID){
            for(int j = 0; j < count; j++){
                if(D[j]->dst_id == D[idx]->p){
                    idx = j;
                }
            }
        }
        table_curr->entry = malloc(sizeof(struct row));
        table_curr->entry->dest = D[i]->dst_id;
        table_curr->entry->incoming_link = My_ID;
        table_curr->entry->outgoing_link = D[idx]->dst_id;
        if(i+1 < count){
            table_curr->next = malloc(sizeof(struct forwarding_table));
            table_curr = table_curr->next;
        }
    }
    // //////// test - print - start ////////
    // N_curr = N_head;
    // while(N_curr != NULL){ //impl rest of dijkstras
    //     printf("N_curr->id = %d\n", N_curr->id);
    //     N_curr = N_curr->next;
        
    // }
    // for(int i = 0; i < count; i++){
    //     printf("after v = %d, p = %d, D = %d\n", D[i]->dst_id, D[i]->p, D[i]->cost);
    // }
    
    // table_curr = table;
    // while(table_curr != NULL){
    //     printf("row->dest = %d, row->incoming_link = %d, row->outgoing_link = %d\n",table_curr->entry->dest, table_curr->entry->incoming_link, table_curr->entry->outgoing_link);
    //     table_curr = table_curr->next;
    // }
    // //////// test - print - end //////////
}
/* Forward the packet to the next-hop node based on forwarding table */
void forward_data(struct data_pkt *pkt)
{
    printf("forward_data\n");
    Alarm(DEBUG, "overlay_node: forwarding data to overlay node %u, client port %u\n", pkt->hdr.dst_id, pkt->hdr.dst_port);

    printf("AHHHHH --- num_clients = %d\n", Client_List.num_clients);
    printf("MyPort%d\n", My_Port);
    struct client_conn *clients[Client_List.num_clients];
    for(int i = 0; i<Client_List.num_clients; i++){
        clients[i] = Client_List.clients[i];
        printf("client_list local_port = %d, remote_port = %d\n", clients[i]->data_local_port, clients[i]->data_remote_port);
    }
    /*
     * Students fill in! Do forwarding table lookup, update path information in
     * N_header (see deliver_locally for an example), and send packet to next hop
     * */
    // send packet to next hop
    if(Route_Mode == MODE_NONE){
        printf("Route_Mode = MODE_NONE\n");

    }
    else if(Route_Mode == MODE_LINK_STATE){
        printf("Route_Mode = MODE_LINK_STATE\n");
        // send packet to next hop
        if(pkt->hdr.dst_id != My_ID){
            if(table == NULL){
                build_table();
            } 

            // look up in forwarding table
            struct forwarding_table * curr_table = table;
            uint32_t out;
            while (curr_table != NULL)
            {
                if(curr_table->entry->dest == pkt->hdr.dst_id){
                    out = curr_table->entry->outgoing_link;
                    break;
                }
                curr_table = curr_table->next;
            }

            /// ----- Wrong Start ----- ///
            int bytes = 0;
            int ret = -1;

            struct node * n_test = get_node_from_id(&Node_List, out);
            printf("%d\n",pkt->hdr.src_port);
            
            for(int i = 0; i<Client_List.num_clients; i++){
                clients[i] = Client_List.clients[i];
                printf("client_list local_port = %d, remote_port = %d\n", clients[i]->data_local_port, clients[i]->data_remote_port);
            }

            struct client_conn *c = get_client_from_port(&Client_List, pkt->hdr.dst_port);

            /* Check whether we have a local client with this port to deliver to. If
            * not, nothing to do */
            if (c == NULL) {
                Alarm(PRINT, "overlay_node: received data for client that does not "
                            "exist! overlay node %d : client port %u\n",
                            pkt->hdr.dst_id, pkt->hdr.dst_port);
                return;
            }

            Alarm(DEBUG, "overlay_node: Delivering data to client with local port %d\n", pkt->hdr.dst_port);

            // update path information in N_header
            /* stamp packet so we can see the path taken */
            int path_len = pkt->hdr.path_len;
            if (path_len < MAX_PATH) {
                pkt->hdr.path[path_len] = My_ID;
                pkt->hdr.path_len++;
            }
            pkt->hdr.src_id = My_ID;
            pkt->hdr.src_port = My_Port;

            // ret = sendto(n_from_id->addr, pkt, bytes, 0, (struct sockaddr *)&n_from_id->addr.sin_port, sizeof(n_from_id->addr.sin_port));


            /* Send data to client */
            bytes = sizeof(struct data_pkt) - MAX_PAYLOAD_SIZE + pkt->hdr.data_len;
            printf("c->data_remote_addr = %d\n", c->data_remote_addr);
            ret = sendto(c->data_sock, pkt, bytes, 0, (struct sockaddr *)&c->data_remote_addr, sizeof(c->data_remote_addr));

            if (ret < 0) {
                Alarm(PRINT, "Error sending to client with sock  %d %d:%d\n",
                    c->data_sock, c->data_local_port, c->data_remote_port);
                goto err;
            }

            return;
           
            err:
                remove_client_with_sock(&Client_List, c->control_sock);
            /// ----- Wrong End ----- ///
        }
        else{
            printf("staying\n");
        }
    } 
    else if(Route_Mode == MODE_DISTANCE_VECTOR){
        printf("Route_Mode = MODE_DISTANCE_VECTOR\n");
        
    } else{
        //report error - sophie
        printf("Route_Mode = BAD\n");
        Alarm(DEBUG, "Route_Mode does not exist\n");
    }
}

/* Deliver packet to one of my local clients */
void deliver_locally(struct data_pkt *pkt)
{
    printf("deliver_locally\n");
    int path_len = 0;
    int bytes = 0;
    int ret = -1;
    struct client_conn *c = get_client_from_port(&Client_List, pkt->hdr.dst_port);

    /* Check whether we have a local client with this port to deliver to. If
     * not, nothing to do */
    if (c == NULL) {
        Alarm(PRINT, "overlay_node: received data for client that does not "
                     "exist! overlay node %d : client port %u\n",
                     pkt->hdr.dst_id, pkt->hdr.dst_port);
        return;
    }

    Alarm(DEBUG, "overlay_node: Delivering data locally to client with local "
                 "port %d\n", c->data_local_port);

    /* stamp packet so we can see the path taken */
    path_len = pkt->hdr.path_len;
    if (path_len < MAX_PATH) {
        pkt->hdr.path[path_len] = My_ID;
        pkt->hdr.path_len++;
    }

    /* Send data to client */
    bytes = sizeof(struct data_pkt) - MAX_PAYLOAD_SIZE + pkt->hdr.data_len;
    ret = sendto(c->data_sock, pkt, bytes, 0,
                 (struct sockaddr *)&c->data_remote_addr,
                 sizeof(c->data_remote_addr));
    if (ret < 0) {
        Alarm(PRINT, "Error sending to client with sock %d %d:%d\n",
              c->data_sock, c->data_local_port, c->data_remote_port);
        goto err;
    }

    return;

    err:
        remove_client_with_sock(&Client_List, c->control_sock);
}

/* Handle incoming data message from another overlay node. Check whether we
 * need to deliver locally to a connected client, or forward to the next hop
 * overlay node */
void handle_overlay_data(int sock, int code, void *data)
{
    printf("handle_overlay_data\n");
    int bytes;
    struct data_pkt pkt;
    struct sockaddr_in recv_addr;
    socklen_t fromlen;

    Alarm(DEBUG, "overlay_node: received overlay data msg!\n");

    fromlen = sizeof(recv_addr);
    bytes = recvfrom(sock, &pkt, sizeof(pkt), 0, (struct sockaddr *)&recv_addr,
                     &fromlen);
    if (bytes < 0) {
        Alarm(EXIT, "overlay node: Error receiving overlay data: %s\n",
              strerror(errno));
    }

    /* If there is data to forward, find next hop and forward it */
    if (pkt.hdr.data_len > 0) {
        char tmp_payload[MAX_PAYLOAD_SIZE+1];
        memcpy(tmp_payload, pkt.payload, pkt.hdr.data_len);
        tmp_payload[pkt.hdr.data_len] = '\0';
        Alarm(DEBUG, "Got forwarded data packet of %d bytes: %s\n",
              pkt.hdr.data_len, tmp_payload);

        if (pkt.hdr.dst_id == My_ID) {
            deliver_locally(&pkt);
        } else {
            forward_data(&pkt);
        }
    }
}

/* Respond to heartbeat message by sending heartbeat echo */
void handle_heartbeat(struct heartbeat_pkt *pkt)
{
    printf("handle_heartbeat\n");
    // periodically send heartbeat on each local link (once per sec)
    //neighbor should resp with heatbeat echo
    printf("inside handle_heartbeat\n");
    if (pkt->hdr.type != CTRL_HEARTBEAT) {
        Alarm(PRINT, "Error: non-heartbeat msg in handle_heartbeat\n");
        return;
    }

    Alarm(DEBUG, "Got heartbeat from %d\n", pkt->hdr.src_id);

     /* Students fill in! */
}

/* Handle heartbeat echo. This indicates that the link is alive, so update our
 * link weights and send update if we previously thought this link was down.
 * Push forward timer for considering the link dead */
void handle_heartbeat_echo(struct heartbeat_echo_pkt *pkt)
{
    printf("handle_heartbeat_echo\n");
    printf("inside handle_heartbeat_echo\n");
    if (pkt->hdr.type != CTRL_HEARTBEAT_ECHO) {
        Alarm(PRINT, "Error: non-heartbeat_echo msg in "
                     "handle_heartbeat_echo\n");
        return;
    }

    Alarm(DEBUG, "Got heartbeat_echo from %d\n", pkt->hdr.src_id);

     /* Students fill in! */

}

/* Process received link state advertisement */
void handle_lsa(struct lsa_pkt *pkt)
{
    printf("handle_lsa\n");
    if (pkt->hdr.type != CTRL_LSA) {
        Alarm(PRINT, "Error: non-lsa msg in handle_lsa\n");
        return;
    }

    if (Route_Mode != MODE_LINK_STATE) {
        Alarm(PRINT, "Error: LSA msg but not in link state routing mode\n");
    }

    Alarm(DEBUG, "Got lsa from %d\n", pkt->hdr.src_id);

     /* Students fill in! */

     /*“Does sending the LSA packet just tell other nodes to recompute their forwarding table because a change in the edge list occurred?”
      — yes, that’s right. If the status of a link changes (i.e. goes down or comes back up), the node that detects that needs to send an
       LSA to let everyone else know they should update the cost/status of that edge and recompute their routes*/
       /*Each node does not need to send the whole topology. Just the status of its adjacent links*/
       build_table();
}

/* Process received distance vector update */
void handle_dv(struct dv_pkt *pkt)
{
    printf("handle_dv\n");
    if (pkt->hdr.type != CTRL_DV) {
        Alarm(PRINT, "Error: non-dv msg in handle_dv\n");
        return;
    }

    if (Route_Mode != MODE_DISTANCE_VECTOR) {
        Alarm(PRINT, "Error: Distance Vector Update msg but not in distance "
                     "vector routing mode\n");
    }

    Alarm(DEBUG, "Got dv from %d\n", pkt->hdr.src_id);

     /* Students fill in! */
}

/* Process received overlay control message. Identify message type and call the
 * relevant "handle" function */
void handle_overlay_ctrl(int sock, int code, void *data)
{
    printf("handle_overlay_ctrl\n");

    char buf[MAX_CTRL_SIZE];
    struct sockaddr_in recv_addr;
    socklen_t fromlen;
    struct ctrl_hdr * hdr = NULL;
    int bytes = 0;

    Alarm(DEBUG, "overlay_node: received overlay control msg!\n");

    fromlen = sizeof(recv_addr);
    bytes = recvfrom(sock, buf, sizeof(buf), 0, (struct sockaddr *)&recv_addr,
                     &fromlen);
    if (bytes < 0) {
        Alarm(EXIT, "overlay node: Error receiving ctrl message: %s\n",
              strerror(errno));
    }
    hdr = (struct ctrl_hdr *)buf;

    /* sanity check */
    if (hdr->dst_id != My_ID) {
        Alarm(PRINT, "overlay_node: Error: got ctrl msg with invalid dst_id: "
              "%d\n", hdr->dst_id);
    }

    if (hdr->type == CTRL_HEARTBEAT) {
        /* handle heartbeat */
        handle_heartbeat((struct heartbeat_pkt *)buf);
    } else if (hdr->type == CTRL_HEARTBEAT_ECHO) {
        /* handle heartbeat echo */
        handle_heartbeat_echo((struct heartbeat_echo_pkt *)buf);
    } else if (hdr->type == CTRL_LSA) {
        /* handle link state update */
        handle_lsa((struct lsa_pkt *)buf);
    } else if (hdr->type == CTRL_DV) {
        /* handle distance vector update */
        handle_dv((struct dv_pkt *)buf);
    }
}

void handle_client_data(int sock, int unused, void *data)
{
    printf("handle_client_data\n");

    int ret, bytes;
    struct data_pkt pkt;
    struct sockaddr_in recv_addr;
    socklen_t fromlen;
    struct client_conn *c;

    Alarm(DEBUG, "Handle client data\n");
    
    c = (struct client_conn *) data;
    if (sock != c->data_sock) {
        Alarm(EXIT, "Bad state! sock %d != data sock\n", sock, c->data_sock);
    }

    fromlen = sizeof(recv_addr);
    bytes = recvfrom(sock, &pkt, sizeof(pkt), 0, (struct sockaddr *)&recv_addr,
                     &fromlen);
    if (bytes < 0) {
        Alarm(PRINT, "overlay node: Error receiving from client: %s\n",
              strerror(errno));
        goto err;
    }

    /* Special case: initial data packet from this client. Use it to set the
     * source port, then ack it */
    if (c->data_remote_port == 0) {
        c->data_remote_addr = recv_addr;
        c->data_remote_port = ntohs(recv_addr.sin_port);
        Alarm(DEBUG, "Got initial data msg from client with sock %d local port "
                     "%u remote port %u\n", sock, c->data_local_port,
                     c->data_remote_port);

        /* echo pkt back to acknowledge */
        ret = sendto(c->data_sock, &pkt, bytes, 0,
                     (struct sockaddr *)&c->data_remote_addr,
                     sizeof(c->data_remote_addr));
        if (ret < 0) {
            Alarm(PRINT, "Error sending to client with sock %d %d:%d\n", sock,
                  c->data_local_port, c->data_remote_port);
            goto err;
        }
    }

    /* If there is data to forward, find next hop and forward it */
    if (pkt.hdr.data_len > 0) {
        char tmp_payload[MAX_PAYLOAD_SIZE+1];
        memcpy(tmp_payload, pkt.payload, pkt.hdr.data_len);
        tmp_payload[pkt.hdr.data_len] = '\0';
        Alarm(DEBUG, "Got data packet of %d bytes: %s\n", pkt.hdr.data_len, tmp_payload);

        /* Set up N_header with my info */
        pkt.hdr.src_id = My_ID;
        pkt.hdr.src_port = c->data_local_port;

        /* Deliver / Forward */
        if (pkt.hdr.dst_id == My_ID) {
            deliver_locally(&pkt);
        } else {
            forward_data(&pkt);
        }
    }

    return;

err:
    remove_client_with_sock(&Client_List, c->control_sock);
    
}

void handle_client_ctrl_msg(int sock, int unused, void *data)
{
    printf("handle_client_ctrl_msg\n");

    int bytes_read = 0;
    int bytes_sent = 0;
    int bytes_expected = sizeof(struct conn_req_pkt);
    struct conn_req_pkt rcv_req;
    struct conn_ack_pkt ack;
    int ret = -1;
    int ret_code = 0;
    char * err_str = "client closed connection";
    struct sockaddr_in saddr;
    struct client_conn *c;

    Alarm(DEBUG, "Client ctrl message, sock %d\n", sock);

    /* Get client info */
    c = (struct client_conn *) data;
    if (sock != c->control_sock) {
        Alarm(EXIT, "Bad state! sock %d != data sock\n", sock, c->control_sock);
    }

    if (c == NULL) {
        Alarm(PRINT, "Failed to find client with sock %d\n", sock);
        ret_code = -1;
        goto end;
    }

    /* Read message from client */
    while (bytes_read < bytes_expected &&
           (ret = recv(sock, ((char *)&rcv_req)+bytes_read,
                       sizeof(rcv_req)-bytes_read, 0)) > 0) {
        bytes_read += ret;
    }
    if (ret <= 0) {
        if (ret < 0) err_str = strerror(errno);
        Alarm(PRINT, "Recv returned %d; Removing client with control sock %d: "
                     "%s\n", ret, sock, err_str);
        ret_code = -1;
        goto end;
    }

    if (c->data_local_port != 0) {
        Alarm(PRINT, "Received req from already connected client with sock "
                     "%d\n", sock);
        ret_code = -1;
        goto end;
    }

    /* Set up UDP socket requested for this client */
    if ((c->data_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
        Alarm(PRINT, "overlay_node: client UDP socket error: %s\n", strerror(errno));
        ret_code = -1;
        goto send_resp;
    }

    /* set server address */
    memset(&saddr, 0, sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = INADDR_ANY;
    saddr.sin_port = htons(rcv_req.port);

    /* bind UDP socket */
    if (bind(c->data_sock, (struct sockaddr *)&saddr, sizeof(saddr)) < 0) {
        Alarm(PRINT, "overlay_node: client UDP bind error: %s\n", strerror(errno));
        ret_code = -1;
        goto send_resp;
    }

    /* Register socket with event handling system */
    ret = E_attach_fd(c->data_sock, READ_FD, handle_client_data, 0, c, MEDIUM_PRIORITY);
    if (ret < 0) {
        Alarm(PRINT, "Failed to register client UDP sock in event handling system\n");
        ret_code = -1;
        goto send_resp;
    }

send_resp:
    /* Send response */
    if (ret_code == 0) { /* all worked correctly */
        c->data_local_port = rcv_req.port;
        ack.id = My_ID;
    } else {
        ack.id = 0;
    }
    bytes_expected = sizeof(ack);
    Alarm(DEBUG, "Sending response to client with control sock %d, UDP port "
                 "%d\n", sock, c->data_local_port);
    while (bytes_sent < bytes_expected) {
        ret = send(sock, ((char *)&ack)+bytes_sent, sizeof(ack)-bytes_sent, 0);
        if (ret < 0) {
            Alarm(PRINT, "Send error for client with sock %d (removing...): "
                         "%s\n", sock, strerror(ret));
            ret_code = -1;
            goto end;
        }
        bytes_sent += ret;
    }

end:
    if (ret_code != 0 && c != NULL) remove_client_with_sock(&Client_List, sock);
}

void handle_client_conn(int sock, int unused, void *data)
{
    printf("handle_client_conn\n");
    int conn_sock;
    struct client_conn new_conn;
    struct client_conn *ret_conn;
    int ret;

    Alarm(DEBUG, "Handle client connection\n");

    /* Accept the connection */
    conn_sock = accept(sock, NULL, NULL);
    if (conn_sock < 0) {
        Alarm(PRINT, "accept error: %s\n", strerror(errno));
        goto err;
    }

    /* Set up the connection struct for this new client */
    new_conn.control_sock     = conn_sock;
    new_conn.data_sock        = -1;
    new_conn.data_local_port  = 0;
    new_conn.data_remote_port = 0;
    ret_conn = add_client_to_list(&Client_List, new_conn);
    if (ret_conn == NULL) {
        goto err;
    }

    /* Register the control socket for this client */
    ret = E_attach_fd(new_conn.control_sock, READ_FD, handle_client_ctrl_msg,
                      0, ret_conn, MEDIUM_PRIORITY);
    if (ret < 0) {
        goto err;
    }

    return;

err:
    if (conn_sock >= 0) close(conn_sock);
}

void init_overlay_data_sock(int port)
{
    int sock = -1;
    int ret = -1;
    struct sockaddr_in saddr;

    if ((sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
        Alarm(EXIT, "overlay_node: data socket error: %s\n", strerror(errno));
    }

    /* set server address */
    memset(&saddr, 0, sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = INADDR_ANY;
    saddr.sin_port = htons(port);

    /* bind listening socket */
    if (bind(sock, (struct sockaddr *)&saddr, sizeof(saddr)) < 0) {
        Alarm(EXIT, "overlay_node: data bind error: %s\n", strerror(errno));
    }

    /* Register socket with event handling system */
    ret = E_attach_fd(sock, READ_FD, handle_overlay_data, 0, NULL, MEDIUM_PRIORITY);
    if (ret < 0) {
        Alarm(EXIT, "Failed to register overlay data sock in event handling system\n");
    }

}

void init_overlay_ctrl_sock(int port)
{
    printf("init_overlay_ctrl_sock\n");
    int sock = -1;
    int ret = -1;
    struct sockaddr_in saddr;

    if ((sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
        Alarm(EXIT, "overlay_node: ctrl socket error: %s\n", strerror(errno));
    }

    /* set server address */
    memset(&saddr, 0, sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = INADDR_ANY;
    saddr.sin_port = htons(port);

    /* bind listening socket */
    if (bind(sock, (struct sockaddr *)&saddr, sizeof(saddr)) < 0) {
        Alarm(EXIT, "overlay_node: ctrl bind error: %s\n", strerror(errno));
    }

    /* Register socket with event handling system */
    ret = E_attach_fd(sock, READ_FD, handle_overlay_ctrl, 0, NULL, MEDIUM_PRIORITY);
    if (ret < 0) {
        Alarm(EXIT, "Failed to register overlay ctrl sock in event handling system\n");
    }
}

void init_client_sock(int client_port)
{
    printf("init_client_sock\n");
    int client_sock = -1;
    int ret = -1;
    struct sockaddr_in saddr;

    if ((client_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
        Alarm(EXIT, "overlay_node: client socket error: %s\n", strerror(errno));
    }

    /* set server address */
    memset(&saddr, 0, sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = INADDR_ANY;
    saddr.sin_port = htons(client_port);

    /* bind listening socket */
    if (bind(client_sock, (struct sockaddr *)&saddr, sizeof(saddr)) < 0) {
        Alarm(EXIT, "overlay_node: client bind error: %s\n", strerror(errno));
    }

    /* start listening */
    if (listen(client_sock, 32) < 0) {
        Alarm(EXIT, "overlay_node: client bind error: %s\n", strerror(errno));
        exit(-1);
    }

    /* Register socket with event handling system */
    ret = E_attach_fd(client_sock, READ_FD, handle_client_conn, 0, NULL, MEDIUM_PRIORITY);
    if (ret < 0) {
        Alarm(EXIT, "Failed to register client sock in event handling system\n");
    }

}

void init_link_state()
{
    // What is it that we are supposed to send in init_link_state()? I would assume that I would tell the neighboring nodes that 
    // my links to them are active, but wouldn't that just happen naturally through the heartbeat and echo system
    // network flooding of the link state advertisements here
    printf("init_link_state\n");
    Alarm(DEBUG, "init link state\n");
}

void init_distance_vector()
{
    printf("init_distance_vector\n");
    Alarm(DEBUG, "init distance vector\n");
}

uint32_t ip_from_str(char *ip)
{
    printf("ip_from_str\n");
    struct in_addr addr;

    inet_pton(AF_INET, ip, &addr);
    return ntohl(addr.s_addr);
}

void process_conf(char *fname, int my_id)
{
    printf("process_conf\n");
    char     buf[MAX_CONF_LINE];
    char     ip_str[MAX_CONF_LINE];
    FILE *   f        = NULL;
    uint32_t id       = 0;
    uint16_t port     = 0;
    uint32_t src      = 0;
    uint32_t dst      = 0;
    uint32_t cost     = 0;
    int node_sec_done = 0;
    int ret           = -1;
    struct node n;
    struct edge e;
    struct node *retn = NULL;
    struct edge *rete = NULL;

    Alarm(DEBUG, "Processing configuration file %s\n", fname);

    /* Open configuration file */
    f = fopen(fname, "r");
    if (f == NULL) {
        Alarm(EXIT, "overlay_node: error: failed to open conf file %s : %s\n",
              fname, strerror(errno));
    }

    /* Read list of nodes from conf file */
    while (fgets(buf, MAX_CONF_LINE, f)) {
        Alarm(DEBUG, "Read line: %s", buf);

        if (!node_sec_done) {
            // sscanf
            ret = sscanf(buf, "%u %s %hu", &id, ip_str, &port);
            Alarm(DEBUG, "    Node ID: %u, Node IP %s, Port: %u\n", id, ip_str, port);
            if (ret != 3) {
                Alarm(DEBUG, "done reading nodes\n");
                node_sec_done = 1;
                continue;
            }

            if (id == my_id) {
                Alarm(DEBUG, "Found my ID (%u). Setting IP and port\n", id);
                My_Port = port;
                My_IP = ip_from_str(ip_str);
            }

            n.id = id;
            memset(&n.addr, 0, sizeof(n.addr));
            n.addr.sin_family = AF_INET;
            n.addr.sin_addr.s_addr = htonl(ip_from_str(ip_str));
            n.addr.sin_port = htons(port);
            n.next_hop = NULL;
            retn = add_node_to_list(&Node_List, n);
            if (retn == NULL) {
                Alarm(EXIT, "Failed to add node to list\n");
            }

        } else { /* Edge section */
            ret = sscanf(buf, "%u %u %u", &src, &dst, &cost);
            Alarm(DEBUG, "    Src ID: %u, Dst ID %u, Cost: %u\n", src, dst, cost);
            if (ret != 3) {
                Alarm(DEBUG, "done reading nodes\n");
                node_sec_done = 1;
                continue;
            }

            e.src_id = src;
            e.dst_id = dst;
            e.cost = cost;
            printf("set cost: %d\n", cost);
            e.src_node = get_node_from_id(&Node_List, e.src_id);
            e.dst_node = get_node_from_id(&Node_List, e.dst_id);
            if (e.src_node == NULL || e.dst_node == NULL) {
                Alarm(EXIT, "Failed to find node for edge (%u, %u)\n", src, dst);
            }
            rete = add_edge_to_list(&Edge_List, e);
            if (rete == NULL) {
                Alarm(EXIT, "Failed to add edge to list\n");
            }
        }
    }
}

int 
main(int argc, char ** argv) 
{

    printf("main\n");
    char * conf_fname    = NULL;

    if (PRINT_DEBUG) {
        Alarm_set_types(DEBUG);
    }

    /* parse args */
    if (argc != 4) {
        Alarm(EXIT, "usage: overlay_node <id> <config_file> <mode: LS/DV>\n");
    }

    My_ID      = atoi(argv[1]);
    conf_fname = argv[2];

    if (!strncmp("LS", argv[3], 3)) {
        Route_Mode = MODE_LINK_STATE;
    } else if (!strncmp("DV", argv[3], 3)) {
        Route_Mode = MODE_DISTANCE_VECTOR;
    } else {
        Alarm(EXIT, "Invalid mode %s: should be LS or DV\n", argv[5]);
    }

    Alarm(DEBUG, "My ID             : %d\n", My_ID);
    Alarm(DEBUG, "Configuration file: %s\n", conf_fname);
    Alarm(DEBUG, "Mode              : %d\n\n", Route_Mode);

    process_conf(conf_fname, My_ID);
    Alarm(DEBUG, "My IP             : "IPF"\n", IP(My_IP));
    Alarm(DEBUG, "My Port           : %u\n", My_Port);

    { /* print node and edge lists from conf */
        int i;
        struct node *n;
        struct edge *e;
        for (i = 0; i < Node_List.num_nodes; i++) {
            n = Node_List.nodes[i];
            Alarm(DEBUG, "Node %u : "IPF":%u\n", n->id,
                  IP(ntohl(n->addr.sin_addr.s_addr)),
                  ntohs(n->addr.sin_port));
        }

        for (i = 0; i < Edge_List.num_edges; i++) {
            e = Edge_List.edges[i];
            Alarm(DEBUG, "Edge (%u, %u) : "IPF":%u -> "IPF":%u\n",
                  e->src_id, e->dst_id,
                  IP(ntohl(e->src_node->addr.sin_addr.s_addr)),
                  ntohs(e->src_node->addr.sin_port),
                  IP(ntohl(e->dst_node->addr.sin_addr.s_addr)),
                  ntohs(e->dst_node->addr.sin_port));
        }
    }
    
    /* Initialize event system */
    E_init();

    /* Set up TCP socket for client connection requests */
    init_client_sock(My_Port);

    /* Set up UDP sockets for sending and receiving messages from other
     * overlay nodes */
    init_overlay_data_sock(My_Port);
    init_overlay_ctrl_sock(My_Port+1);

    if (Route_Mode == MODE_LINK_STATE) {
        init_link_state();
    } else {
        init_distance_vector();
    }

    /* Enter event handling loop */
    Alarm(DEBUG, "Entering event loop!\n");
    E_handle_events();

    return 0;
}
