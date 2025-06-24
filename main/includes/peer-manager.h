#ifndef PEER_MANAGER_H
#define PEER_MANAGER_H

#include <stdint.h>

#include "peer.h"

#ifdef __cplusplus
extern "C" {
#endif

void peer_manager_init();
void peer_manager_register_connection_task(void);
void peer_manager_register_signaling_task(void);
void peer_manager_register_send_task(void);

#ifdef __cplusplus
}
#endif

#endif  // PEER_MANAGER_H
