/* ======================================================================
 * YOU ARE EXPECTED TO MODIFY THIS FILE.
 * ====================================================================== */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "d2_lookup.h"
#include "d1_udp.h"

#define BUFFER 1024

//===============================================================================================
D2Client* d2_client_create( const char* server_name, uint16_t server_port )
{
    // Allocate memory for D2Client
    D2Client* client = (D2Client*)malloc(sizeof(D2Client));
    if (client == NULL) {
        perror("Memory allocation failed");
        return NULL;
    }
    
    // Create a D1Peer for communication
    client->peer = d1_create_client();
    if (client->peer == NULL) {
        fprintf(stderr, "Failed to create D1Peer\n");
        free(client); // Clean up allocated memory
        return NULL;
    }
    
    // Get peer info using D1 function
    if (!d1_get_peer_info(client->peer, server_name, server_port)) {
        fprintf(stderr, "Failed to get peer info\n");
        d1_delete(client->peer); // Clean up allocated memory for D1Peer
        free(client); // Clean up allocated memory for D2Client
        return NULL;
    }
    
    return client;
}

//===============================================================================================
D2Client* d2_client_delete( D2Client* client )
{
    if (client != NULL) {
        // Delete the D1Peer if it exists
        if (client->peer != NULL) {
            d1_delete(client->peer);
            client->peer = NULL;
        }
        // Free memory allocated for the D2Client
        free(client);
    }
    // Always return NULL
    return NULL;
}

//===============================================================================================
int d2_send_request(D2Client* client, uint32_t id) {
    // Check for NULL client or peer
    if (client == NULL || client->peer == NULL) {
        fprintf(stderr, "Error: Attempting to send request with a NULL client or peer\n");
        return -1; //error
    }

    // Create a PacketRequest 
    PacketRequest request;
    request.type = htons(TYPE_REQUEST);
    request.id = htonl(id);

    // Send the PacketRequest using d1_send_data function
    ssize_t sent_bytes = d1_send_data(client->peer, (char*)&request, sizeof(PacketRequest));
    if (sent_bytes <= 0) {
        perror("Error sending PacketRequest data");
        return -1;
    }

    // Return the number of bytes sent
    return (int)sent_bytes;
}

//===============================================================================================
int d2_recv_response_size(D2Client* client) {
    char packet_buffer[BUFFER];

    // Check for NULL client or peer
    if (client == NULL || client->peer == NULL) {
        fprintf(stderr, "Error: Attempting to receive response size with a NULL client or peer\n");
        return -1;
    }

    // Receive packet data
    int bytes_received = d1_recv_data(client->peer, packet_buffer, sizeof(packet_buffer));
    if (bytes_received == 0) {
        printf("Connection closed by peer\n");
        return 0;
    }

    // Handle receive error
    if (bytes_received < 0) {
        fprintf(stderr, "Error receiving PacketResponseSize\n");
        return -1;
    }

    // Extract packet type
    PacketResponseSize* response_size_packet = (PacketResponseSize*)packet_buffer;
    uint16_t packet_type = ntohs(response_size_packet->type);

    // Check if received packet is a PacketResponseSize
    if (packet_type != TYPE_RESPONSE_SIZE) {
        fprintf(stderr, "Received packet is not a PacketResponseSize\n");
        return -1;
    }

    // Extract response size in host order
    uint16_t size_host_order = ntohs(response_size_packet->size);
    printf("Received response size: %d\n", size_host_order);
    return size_host_order;
}

//===============================================================================================
int d2_recv_response(D2Client* client, char* buffer, size_t sz) {
    // Check for NULL client or buffer
    if (client == NULL || buffer == NULL) {
        fprintf(stderr, "Error: Invalid client or peer\n");
        return -1;
    }

    // Receive a PacketResponse packet from the server using d1_recv_data
    int bytes_received = d1_recv_data(client->peer, buffer, sz);
    if (bytes_received < 0) {
        perror("Error receiving response data");
        return bytes_received;
    }

    // Debug
    printf("Received response data: %s\n", buffer);

    
    return bytes_received;
}

//===============================================================================================
// Function to allocate memory for a local tree
LocalTreeStore* d2_alloc_local_tree(int num_nodes) {
    // Check if number of nodes is valid
    if (num_nodes <= 0) {
        fprintf(stderr, "Error: Number of nodes must be positive.\n");
        return NULL;
    }

    // Allocate memory for LocalTreeStore structure
    LocalTreeStore* store = (LocalTreeStore*)malloc(sizeof(LocalTreeStore));
    if (!store) {
        perror("Failed to allocate LocalTreeStore");
        return NULL;
    }

     // Allocate memory for nodes array
    store->nodes = (NetNode*)calloc(num_nodes, sizeof(NetNode));
    if (!store->nodes) {
        perror("Failed to allocate memory for nodes");
        free(store);
        return NULL;
    }

    // Initialize number of nodes
    store->number_of_nodes = num_nodes;
    return store;
}

//===============================================================================================
// Function to free a local tree
void d2_free_local_tree(LocalTreeStore* nodes) {
    if (nodes) {
        free(nodes->nodes);
        free(nodes);
    } 
}
//===============================================================================================
// Function to add nodes to the local tree
int d2_add_to_local_tree(LocalTreeStore *store, int node_idx, char *buffer, int buflen) {
    // Validate parameters to ensure they are usable
    if (store == NULL || buffer == NULL || buflen < 0)
    {
        fprintf(stderr, "Invalid parameters\n"); 
        return -1; // Error
    }
    int index = 0; // Initialize buffer read index
    while (index < buflen) {
         // Ensure the current node index is within the allocated range
        if (node_idx >= store->number_of_nodes)
        {
            fprintf(stderr, "Node index exceeds the number of nodes in the tree\n");
            return -1; // Error
        }

        NetNode *new_node = &store->nodes[node_idx]; // Pointer to the new node
        // Deserialize and convert from network byte order to host byte order
        memcpy(&new_node->id, buffer + index, sizeof(new_node->id));
        index += sizeof(uint32_t);
        new_node->id = ntohl(new_node->id);

        memcpy(&new_node->value, buffer + index, sizeof(new_node->value));
        index += sizeof(uint32_t);
        new_node->value = ntohl(new_node->value);

        memcpy(&new_node->num_children, buffer + index, sizeof(new_node->num_children));
        index += sizeof(uint32_t);
        new_node->num_children = ntohl(new_node->num_children);

        // Deserialize child IDs if present
        for (uint32_t i = 0; i < new_node->num_children; i++){
            if (index >= buflen)
            {
                fprintf(stderr, "Bufferet har ikke nok plass lenger\n"); // Feilhåndtering
                return -1;
            }
            memcpy(&new_node->child_id[i], buffer + index, sizeof(uint32_t));
            new_node->child_id[i] = ntohl(new_node->child_id[i]);
            index += sizeof(uint32_t);
        }

        node_idx++;// Move to the next node index
    }

    // Return the updated node index after adding nodes
    return node_idx;
}
 
//===============================================================================================
// Function to print the tree
void d2_print_tree(LocalTreeStore* nodes) {
    if (!nodes || !nodes->nodes) {
        printf("Tree is empty or not initialized\n");
        return;
    }

    for (int i = 0; i < nodes->number_of_nodes; i++) {
        NetNode* node = &nodes->nodes[i];
        for (int depth = 0; depth < i; depth++) printf("--");
        printf("id %u value %u children %u\n", node->id, node->value, node->num_children);
    }
}

//===============================================================================================
// Check for valid node index to avoid accessing out of bounds
void print_tree_recursive(LocalTreeStore *store, int node_index, int depth) {
    if (node_index < 0 || node_index >= store->number_of_nodes){
        return; // Exit if the node index is invalid
    }
    
    NetNode new_node = store->nodes[node_index]; // Access the current node directly

    // Print indents based on the depth to visualize tree hierarchy
    for (int i = 0; i < depth; i++){
        printf("--");
    }

    // Print current node details
    printf("id %d value %d children %d\n", new_node.id, new_node.value, new_node.num_children);

    // Recursively print each child node
    for (uint32_t i = 0; i < new_node.num_children; i++){
        int child_indeks = new_node.child_id[i]; // Get the child index
        print_tree_recursive(store, child_indeks, depth + 1); // Recursive call for the child
    }
}
//===============================================================================================

