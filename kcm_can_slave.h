/*
 * kcm_can_slave.h
 *
 * Slave side. What a slave receives is the broadcast services plus whatever is
 * addressed to this node; what it sends is its own TPDOs. The master side
 * lives in kcm_can_master.* and the two share only kcm_can.h.
 */

#ifndef KCM_CAN_SLAVE_H_
#define KCM_CAN_SLAVE_H_

#include "kcm_can.h"

/*
 * \brief       dispatch one received frame to the slave hooks below
 * \param[in]   own_node_id: this node's id, 1..127
 * \param[in]   id:          11-bit COB-ID, from FDCAN_RxHeaderTypeDef.Identifier
 * \param[in]   data:        payload, may be NULL only when len is 0
 * \param[in]   len:         payload length 0..8, from .DataLength
 *
 * Broadcast services are handled first, then anything whose node id is ours.
 * Traffic for other nodes, and anything malformed, is dropped silently.
 */
void kcm_slave_on_can_rx(uint8_t own_node_id, uint32_t id, const uint8_t *data, uint8_t len);

/*
 * Weak hooks. The defaults in kcm_can_slave.c do nothing; define any of
 * them in the application to take it over -- no registration call, the linker
 * picks the strong definition.
 *
 * PDO length is mapping-dependent, so `len` is passed through as received and
 * each hook validates what it mapped.
 */
void kcm_slave_decode_rpdo1(const uint8_t *data, uint8_t len);
void kcm_slave_decode_rpdo2(const uint8_t *data, uint8_t len);
void kcm_slave_decode_rpdo3(const uint8_t *data, uint8_t len);
void kcm_slave_decode_rpdo4(const uint8_t *data, uint8_t len);

/*
 * NMT, already filtered to commands addressed to this node or broadcast.
 * These run from wherever kcm_slave_on_can_rx() is called -- keep them short
 * and set a flag if the real work is a reset.
 */
void kcm_slave_decode_nmt_start(void);
void kcm_slave_decode_nmt_stop(void);
void kcm_slave_decode_nmt_pre_operational(void);
void kcm_slave_decode_nmt_reset_node(void);
void kcm_slave_decode_nmt_reset_comm(void);

/*
 * \param[in]   has_counter: 0 for a bare SYNC, 1 when the producer is
 *                           configured to send a counter
 * \param[in]   counter:     the counter byte, 0 when has_counter is 0
 */
void kcm_slave_decode_sync(uint8_t has_counter, uint8_t counter);

/*
 * \param[in]   ms_since_midnight: 28-bit field, already masked
 * \param[in]   days_since_1984:   days since 1 January 1984
 */
void kcm_slave_decode_time(uint32_t ms_since_midnight, uint16_t days_since_1984);

/*
 * Sending, slave -> master. A TPDO is addressed by its SENDER, so what goes
 * into the COB-ID is this node's own id, 1..127 -- not a destination. Every
 * master on the bus sees it; which of them cares is their business.
 *
 * `data` may be NULL only when len is 0, and len is 0..8. Returns HAL_OK once
 * the frame is queued -- see kcm_can_send_pdo in kcm_can.h for why that is
 * not the same as transmitted.
 */
HAL_StatusTypeDef kcm_slave_send_tpdo1(myCAN_t *myCAN, uint8_t own_node_id, const uint8_t *data, uint8_t len);
HAL_StatusTypeDef kcm_slave_send_tpdo2(myCAN_t *myCAN, uint8_t own_node_id, const uint8_t *data, uint8_t len);
HAL_StatusTypeDef kcm_slave_send_tpdo3(myCAN_t *myCAN, uint8_t own_node_id, const uint8_t *data, uint8_t len);
HAL_StatusTypeDef kcm_slave_send_tpdo4(myCAN_t *myCAN, uint8_t own_node_id, const uint8_t *data, uint8_t len);

#endif /* KCM_CAN_SLAVE_H_ */
