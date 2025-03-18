/* ======================================================================
 * YOU ARE EXPECTED TO MODIFY THIS FILE.
 * ====================================================================== */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include "d1_udp.h"
#include "d2_lookup.h" // not sure if needed

#define BUFFER 1024
 
//===============================================================================================
D1Peer* d1_create_client(){

    // Allocate memory for the D1Peer structure
    D1Peer* client = (D1Peer*)malloc(sizeof(D1Peer));
    if (client == NULL) {
        perror("Memory allocation failed!");
        return NULL;
    }

    // Create a UDP socket for the client
    client->socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (client->socket < 0) {
        perror("socket");
        free(client); // Free allocated memory
        return NULL;
    }
    // Initialize the address structure to zero
    memset(&(client->addr), 0, sizeof(struct sockaddr_in));
    // Initialize next_seqno to 0
    client->next_seqno = 0;
    
    return client;;
}

//===============================================================================================
D1Peer* d1_delete( D1Peer* peer )
{
    // Check if the peer pointer is NULL
    if (peer == NULL) {
        return NULL; // Found nothing to delete,return NULL
    }

    // Close the socket associated with the peer
    if (close(peer->socket) < 0) {
        perror("Socket close failed");
        // Proceed with deletion even if the close operation fails
    }

    // Free memory allocated for the peer structure
    free(peer);


    return NULL; // Returns NULL to signal that the deletion operation was successful.
}

//===============================================================================================
int d1_get_peer_info(struct D1Peer* client, const char* servername, uint16_t server_port) {
    // Check if client pointer is NULL
    if (client == NULL) {
        return 0;
    }
    
    // Clear the address structure
    memset(&client->addr, 0, sizeof(client->addr));
    
    // Set address family to IPv4
    client->addr.sin_family = AF_INET;
    
    // Set the port number
    client->addr.sin_port = htons(server_port);
    
    // Check if the servername is in dotted decimal format
    if (inet_pton(AF_INET, servername, &client->addr.sin_addr) == 1) {
        return 1; // Success
    }
    
    // If not in dotted decimal format, assume it's a hostname and resolve it
    struct hostent *server = gethostbyname(servername);
    if (server == NULL) {
        return 0; // Error
    }
    
    // Copy resolved address to the address structure
    memcpy(&client->addr.sin_addr.s_addr, server->h_addr, server->h_length);
    
    return 1; // Success
}
//===============================================================================================
int d1_recv_data(struct D1Peer* peer, char* buffer, size_t sz) {
    //print for debug
    printf("\nInside RECV_DATA\n");

    // Chceck if Peer pointer is NULL
    if (peer == NULL || buffer == NULL) {
        perror("Peer pointer or buffer is NULL");
        return -1; // Error
    }

    struct sockaddr_in server_addr;
    socklen_t server_addr_len = sizeof(server_addr);
    // Receive packet from server
    ssize_t bytes_received = recvfrom(peer->socket, buffer, sz, 0,(struct sockaddr*)&server_addr, &server_addr_len);

    if (bytes_received < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // No data available 
            //continue;
         } 
         else {
            perror("Error receiving packet");
            return -1; // Error: unable to receive packet
            }
        }

    // Check if received packet is empty
    if (bytes_received == 0) {
        return 0; // Empty data packet
    }

    // Validate packet size
    if ((size_t)bytes_received < sizeof(D1Header)) {
        fprintf(stderr, "The size of the received packet is too small for processing.\n");
        d1_send_ack(peer, (peer->next_seqno + 1) % 2); // Send ACK with opposite sequence number
        return -1; // Error
    }

    // Extract D1Header from the received packet
    D1Header* header = (D1Header*)buffer;

    // Validate checksum ---- For some reason, didnt work when I tried with calculate_checksum function... We LOVE C :)
    uint16_t checksum = header->checksum;
    header->checksum = 0; // Set checksum to zero before calculation.
    uint16_t calculated_checksum = 0; // set startvalue
    uint16_t* ptr = (uint16_t*)buffer;

    for (ssize_t i = 0; i < bytes_received / 2; ++i) {
        calculated_checksum ^= ptr[i];
    }

    if (checksum != calculated_checksum) {
        fprintf(stderr, "Checksum not matching\n");
        d1_send_ack(peer, (peer->next_seqno ^= 1)); // Send ACK with flipped sequence number
        return -1; // Error
    }

    // Retrieve payload from the received packet.
    size_t payload_size = bytes_received - sizeof(D1Header);
    memmove(buffer, buffer + sizeof(D1Header), payload_size);
    buffer[payload_size] = '\0'; // Null-terminate the string

    // Determine whether the received packet is an ACK packet.
    if (ntohs(header->flags) & FLAG_ACK) {
        int received_seqno = ntohs(header->flags) & ACKNO; // Comment out to remove warning
        /*printf("Received ACK header flags: 0x%X\n", ntohs(header->flags));
        printf("Received ACK sequence number: %d\n", received_seqno);*/
    }

    // Send ACK packet to the client
    d1_send_ack(peer, peer->next_seqno);
    peer->next_seqno ^= 1;

    return payload_size;
}

//===============================================================================================
int d1_wait_ack(D1Peer* peer, char* buffer, size_t sz) {
    // Set timeout for receiving ACK packets
    printf("Inside WAIT ACK\n");
    struct timeval timeout;
    timeout.tv_sec = 1;  // Timeout duration: 1 second
    timeout.tv_usec = 0;

    // Set socket option for receive timeout
    if (setsockopt(peer->socket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout)) < 0) {
        perror("Error setting socket option");
        return -1; // Error
    }

    // Continuously wait for ACK packets
    while (1) {
        D1Header ack_header;
        ssize_t bytes_received = recv(peer->socket, &ack_header, sizeof(ack_header), 0);
        
        // Handle receive errors
        if (bytes_received < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;  // No ACK received yet, continue waiting
            } else {
                perror("Error receiving ACK");
                return -1; // Error
            }
        }

        // Extract sequence number from ACK header
        int received_seqno = ntohs(ack_header.flags) & 0x01;  // Extract the sequence number
        printf("Received ACK seqno: %d, Expected ACK seqno: %d\n", received_seqno, peer->next_seqno);

        // Check if received sequence number matches expected sequence number
        if (received_seqno == peer->next_seqno) {
            peer->next_seqno ^= 1;  // Flip the expected sequence number for the next transmission
            return 1; // Success
        } else {
            printf("Unexpected sequence number in ACK. Expected %d, received %d\n", peer->next_seqno, received_seqno);
            return -1;  // Seqnr mismatch
        }
    }
}




//===============================================================================================
int d1_send_data(struct D1Peer* peer, char* buffer, size_t sz) {
    // debug print
    printf("Inside SEND_DATA\n");

    // Check for NULL pointers
    if (peer == NULL || buffer == NULL) {
        fprintf(stderr, "Error: peer pointer or buffer is NULL\n");
        return -1;
    }

    // Check if data size exceeds maximum limit
    if (sz > (BUFFER - sizeof(D1Header))) {
        fprintf(stderr, "Error: data size exceeds maximum limit\n");
        return -1;
    }

    // Initialize packet buffer
    uint8_t packet[BUFFER];

    // Calculate total packet size
    size_t total_size = sizeof(D1Header) + sz;

    // Check if total packet size exceeds buffer size
    if (total_size > BUFFER) {
        fprintf(stderr, "Error: total packet size exceeds buffer limit\n");
        return -1;
    }

    // Create header for the packet
    D1Header header;
    header.flags = htons(FLAG_DATA | (peer->next_seqno << 7));
    header.size = htonl(total_size); // Convert size to network byte order
    header.checksum = 0; // Initial checksum is zero before calculation

    // Create packet buffer and copy header and data into it
    memcpy(packet, &header, sizeof(D1Header));
    memcpy(packet + sizeof(D1Header), buffer, sz);

    // For debugging
    printf("Packet data before checksum calculation: ");
    for (size_t i = 0; i < total_size; i++) {
        printf("%02X ", packet[i]);
    }
    printf("\n");

    // Calculate checksum including header with zero checksum initially set
    header.checksum = htons(calculate_checksum(packet, total_size));
    memcpy(packet, &header, sizeof(D1Header)); // Copy back the checksum updated header

    // Debug output for packet details
    printf("Sending packet to IP: %s, Port: %d\n",
           inet_ntoa(peer->addr.sin_addr), ntohs(peer->addr.sin_port));
    printf("Packet content (header): Flags: %x, Size: %u, Checksum: %x\n",
           ntohs(*(uint16_t*)packet), ntohl(*(uint32_t*)(packet + 2)), ntohs(*(uint16_t*)(packet + 6)));


    // Send packet
    ssize_t bytes_sent = sendto(peer->socket, packet, total_size, 0, (struct sockaddr*)&(peer->addr), sizeof(peer->addr));
    if (bytes_sent < 0) {
        perror("Error sending data");
        return -1;
    }

    // Wait for ACK
    int ack_result = d1_wait_ack(peer, buffer, sz);
    if (ack_result < 0) {
        fprintf(stderr, "Error waiting for ACK\n");
        return -1;
    }
    // Flip the sequence number if ACK was successfully received
    peer->next_seqno ^= 1;

    return bytes_sent;
}

//===============================================================================================
void d1_send_ack(struct D1Peer* peer, int seqno) {
    printf("Inside SEND_ACK");

    if (peer == NULL) {
        fprintf(stderr, "Error: Peer pointer is NULL\n");
        return;
    }
    
    // Create ACK packet
    D1Header ack_header;
    ack_header.flags = htons(FLAG_ACK | seqno); // Setting FLAG_ACK and the correct sequence number
    ack_header.size = htonl(sizeof(D1Header));
    ack_header.checksum = 0;

    // Calculate checksum for the ACK packet
    uint16_t checksum = calculate_checksum(&ack_header, sizeof(D1Header)); 
    ack_header.checksum = checksum;

    // Send ACK packet to the peer
    ssize_t bytes_sent = sendto(peer->socket, (const char*)&ack_header, sizeof(D1Header), 0, (struct sockaddr*)&(peer->addr), sizeof(peer->addr));
    if (bytes_sent < 0) {
        perror("Sendto ACK failed");
        return;
    }
    printf("\n Sending ACK packet\n");
}
//===============================================================================================
// Function to calculate checksum
uint16_t calculate_checksum(const void *buff, size_t len) {
    const uint8_t *data = (const uint8_t *)buff;
    uint8_t checksum_odd = 0;
    uint8_t checksum_even = 0;
    for (size_t i = 0; i < len; i++) {
        if (i % 2 == 0) {
            checksum_odd ^= data[i];
        } else {
            checksum_even ^= data[i];
        }
    }
    return (uint16_t)(checksum_odd << 8 | checksum_even);
}
//===============================================================================================
