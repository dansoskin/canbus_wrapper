/*
 * decode_can_master.c
 *
 * Master side of the decode layer. Call kcm_master_on_can_rx() with each
 * frame drained by can_get_from_rbbuffer(); it validates the frame and hands
 * the payload to the matching weak hook.
 */

#include "decode_can_master.h"

void kcm_master_on_can_rx(uint32_t id, const uint8_t *data, uint8_t len)
{
    if (id > CAN_COB_ID_MAX || len > 8u || (!data && len)) return;

    uint8_t node = (uint8_t)(id & CAN_COB_NODE_MASK);

    /* Node 0 means this is one of the broadcast services (NMT, SYNC, TIME).
     * A master is the source of those, not a consumer. */
    if (node == 0u) return;

    switch (id & CAN_COB_FUNC_MASK)
    {
        case CAN_FUNC_TPDO1:
            kcm_master_decode_tpdo1(node, data, len);
            break;
        case CAN_FUNC_TPDO2:
            kcm_master_decode_tpdo2(node, data, len);
            break;
        case CAN_FUNC_TPDO3:
            kcm_master_decode_tpdo3(node, data, len);
            break;
        case CAN_FUNC_TPDO4:
            kcm_master_decode_tpdo4(node, data, len);
            break;

        case CAN_FUNC_HEARTBEAT: /* heartbeat and boot-up share this id */
            if (len != CAN_HEARTBEAT_DLC) break;
            kcm_master_decode_heartbeat(node, (uint8_t)(data[0] & CAN_NMT_STATE_MASK));
            break;

        default: break;
    }
}

/* ---------------------------------------------------------------------------
 * Weak defaults. Every one of these is a no-op; redefine the ones you need in
 * the application, with the same signature and WITHOUT __weak.
 * ------------------------------------------------------------------------ */

__weak void kcm_master_decode_tpdo1(uint8_t node, const uint8_t *data, uint8_t len)
{
    UNUSED(node);
    UNUSED(data);
    UNUSED(len);
}

__weak void kcm_master_decode_tpdo2(uint8_t node, const uint8_t *data, uint8_t len)
{
    UNUSED(node);
    UNUSED(data);
    UNUSED(len);
}

__weak void kcm_master_decode_tpdo3(uint8_t node, const uint8_t *data, uint8_t len)
{
    UNUSED(node);
    UNUSED(data);
    UNUSED(len);
}

__weak void kcm_master_decode_tpdo4(uint8_t node, const uint8_t *data, uint8_t len)
{
    UNUSED(node);
    UNUSED(data);
    UNUSED(len);
}

__weak void kcm_master_decode_heartbeat(uint8_t node, uint8_t state)
{
    UNUSED(node);
    UNUSED(state);
}
