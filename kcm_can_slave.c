/*
 * kcm_can_slave.c
 *
 * Slave side. Call kcm_slave_on_can_rx() with each frame drained by
 * can_get_from_rbbuffer(); it validates the frame and hands the payload to
 * the matching weak hook. The other direction is kcm_slave_send_tpdo1..4().
 */

#include "kcm_can_slave.h"

void kcm_slave_on_can_rx(uint8_t own_node_id, uint32_t id, const uint8_t *data, uint8_t len)
{
    if (own_node_id == 0u || own_node_id > CAN_NODE_ID_MAX) return;
    if (id > CAN_COB_ID_MAX || len > 8u || (!data && len)) return;

    /* Fixed-ID services precede the PDO node-ID check: their ids carry no node
     * id at all, so testing the low 7 bits against ours would be meaningless.
     * Each one returns, so nothing can fall through into the PDO switch. */
    switch (id)
    {
        case CAN_FUNC_NMT: /* NMT: [command][target], 0 = all nodes */
            if (len != CAN_NMT_DLC) return;
            if (data[1] != 0u && data[1] != own_node_id) return;
            switch (data[0]) {
            case CAN_NMT_START:           kcm_slave_decode_nmt_start();           break;
            case CAN_NMT_STOP:            kcm_slave_decode_nmt_stop();            break;
            case CAN_NMT_PRE_OPERATIONAL: kcm_slave_decode_nmt_pre_operational(); break;
            case CAN_NMT_RESET_NODE:      kcm_slave_decode_nmt_reset_node();      break;
            case CAN_NMT_RESET_COMM:      kcm_slave_decode_nmt_reset_comm();      break;
            default: break;
            }
            return;

        case CAN_FUNC_SYNC: /* bare SYNC, or one byte of counter */
            if (len > 1u) return;
            kcm_slave_decode_sync(len == 1u ? 1u : 0u, len == 1u ? data[0] : 0u);
            return;

        case CAN_FUNC_TIME: /* 28-bit ms after midnight, then 16-bit days */
            if (len != CAN_TIME_DLC) return;
            kcm_slave_decode_time(((uint32_t)data[0]) |
                                  ((uint32_t)data[1] << 8) |
                                  ((uint32_t)data[2] << 16) |
                                  (((uint32_t)data[3] & 0x0Fu) << 24),
                                  (uint16_t)((uint16_t)data[4] | ((uint16_t)data[5] << 8)));
            return;

        default: break;
    }

    //! --------------------------------------------------------------
    if ((id & CAN_COB_NODE_MASK) != own_node_id) return;

    switch (id & CAN_COB_FUNC_MASK) {
        case CAN_FUNC_RPDO1:
            kcm_slave_decode_rpdo1(data, len);
            break;
        case CAN_FUNC_RPDO2:
            kcm_slave_decode_rpdo2(data, len);
            break;
        case CAN_FUNC_RPDO3:
            kcm_slave_decode_rpdo3(data, len);
            break;
        case CAN_FUNC_RPDO4:
            kcm_slave_decode_rpdo4(data, len);
            break;
        default: break;
    }
}

/* ---------------------------------------------------------------------------
 * Sending. The four differ only in the function code: 0x180/0x280/0x380/0x480
 * plus our own node id.
 * ------------------------------------------------------------------------ */

HAL_StatusTypeDef kcm_slave_send_tpdo1(myCAN_t *myCAN, uint8_t own_node_id, const uint8_t *data, uint8_t len)
{
    return kcm_can_send_pdo(myCAN, CAN_FUNC_TPDO1, own_node_id, data, len);
}

HAL_StatusTypeDef kcm_slave_send_tpdo2(myCAN_t *myCAN, uint8_t own_node_id, const uint8_t *data, uint8_t len)
{
    return kcm_can_send_pdo(myCAN, CAN_FUNC_TPDO2, own_node_id, data, len);
}

HAL_StatusTypeDef kcm_slave_send_tpdo3(myCAN_t *myCAN, uint8_t own_node_id, const uint8_t *data, uint8_t len)
{
    return kcm_can_send_pdo(myCAN, CAN_FUNC_TPDO3, own_node_id, data, len);
}

HAL_StatusTypeDef kcm_slave_send_tpdo4(myCAN_t *myCAN, uint8_t own_node_id, const uint8_t *data, uint8_t len)
{
    return kcm_can_send_pdo(myCAN, CAN_FUNC_TPDO4, own_node_id, data, len);
}

/* ---------------------------------------------------------------------------
 * Weak defaults. Every one of these is a no-op; redefine the ones you need in
 * the application, with the same signature and WITHOUT __weak.
 * ------------------------------------------------------------------------ */

__weak void kcm_slave_decode_rpdo1(const uint8_t *data, uint8_t len)
{
    UNUSED(data);
    UNUSED(len);
}

__weak void kcm_slave_decode_rpdo2(const uint8_t *data, uint8_t len)
{
    UNUSED(data);
    UNUSED(len);
}

__weak void kcm_slave_decode_rpdo3(const uint8_t *data, uint8_t len)
{
    UNUSED(data);
    UNUSED(len);
}

__weak void kcm_slave_decode_rpdo4(const uint8_t *data, uint8_t len)
{
    UNUSED(data);
    UNUSED(len);
}

__weak void kcm_slave_decode_nmt_start(void)
{
}

__weak void kcm_slave_decode_nmt_stop(void)
{
}

__weak void kcm_slave_decode_nmt_pre_operational(void)
{
}

__weak void kcm_slave_decode_nmt_reset_node(void)
{
}

__weak void kcm_slave_decode_nmt_reset_comm(void)
{
}

__weak void kcm_slave_decode_sync(uint8_t has_counter, uint8_t counter)
{
    UNUSED(has_counter);
    UNUSED(counter);
}

__weak void kcm_slave_decode_time(uint32_t ms_since_midnight, uint16_t days_since_1984)
{
    UNUSED(ms_since_midnight);
    UNUSED(days_since_1984);
}
