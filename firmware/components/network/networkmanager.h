#include <stddef.h>

#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

typedef struct NetworkManager NetworkManager;

NetworkManager* network_manager_create(void);
void network_manager_destroy(NetworkManager* nm);
int network_manager_connect(NetworkManager* nm);
int network_manager_disconnect(NetworkManager* nm);
int network_manager_send(NetworkManager* nm, const void* data, size_t len);

#endif