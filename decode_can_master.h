/*
 * decode_can_master.h
 *
 * Master side of the decode layer: what a master receives is what the slaves
 * send -- TPDOs and heartbeats. The slave side lives in decode_can_slave.*
 * and the two share only decode_can.h.
 */

#ifndef CAN_DECODE_CAN_MASTER_H_
#define CAN_DECODE_CAN_MASTER_H_

#include "decode_can.h"

/*
 * \brief       dispatch one received frame to the master hooks below
 * \param[in]   id:   11-bit COB-ID, straight from FDCAN_RxHeaderTypeDef.Identifier
 * \param[in]   data: payload, may be NULL only when len is 0
 * \param[in]   len:  payload length 0..8, from .DataLength (a plain byte
 *                    count for classic frames on this part)
 *
 * Anything malformed or not addressed to a valid node is dropped silently.
 */
void kcm_master_on_can_rx(uint32_t id, const uint8_t *data, uint8_t len);

/*
 * Weak hooks. The defaults in decode_can_master.c do nothing; define any of
 * them in the application to take it over -- no registration call, the linker
 * picks the strong definition. `node` is the sending slave's node id (1..127),
 * already stripped from the COB-ID.
 *
 * PDO length is mapping-dependent, so `len` is passed through as received and
 * each hook validates what it mapped.
 */
void kcm_master_decode_tpdo1(uint8_t node, const uint8_t *data, uint8_t len);
void kcm_master_decode_tpdo2(uint8_t node, const uint8_t *data, uint8_t len);
void kcm_master_decode_tpdo3(uint8_t node, const uint8_t *data, uint8_t len);
void kcm_master_decode_tpdo4(uint8_t node, const uint8_t *data, uint8_t len);

/*
 * \param[in]   state: CAN_NMT_STATE_*, toggle bit already masked off. A
 *                     boot-up frame is a heartbeat carrying
 *                     CAN_NMT_STATE_BOOTUP -- that is the node telling you it
 *                     just reset, and it is the one state worth acting on.
 */
void kcm_master_decode_heartbeat(uint8_t node, uint8_t state);

#endif /* CAN_DECODE_CAN_MASTER_H_ */
