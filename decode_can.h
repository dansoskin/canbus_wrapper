/*
 * decode_can.h
 *
 * CANopen predefined-connection-set constants, shared by the master and the
 * slave decoder. Both decode_can_master.c and decode_can_slave.c include this
 * and nothing else of each other, so the two sides can be worked on in
 * parallel without meeting in the same file. Keep this header to things BOTH
 * sides need -- anything one-sided belongs in that side's own header.
 */

#ifndef CAN_DECODE_CAN_H_
#define CAN_DECODE_CAN_H_

#include "can.h"

/* An 11-bit COB-ID splits into a 4-bit function code (bits 10..7) and a 7-bit
 * node id (bits 6..0). Node id 0 is not a valid address, which is what makes
 * the broadcast services (NMT 0x000, SYNC 0x080, TIME 0x100) unambiguous. */
#define CAN_COB_FUNC_MASK               0x780u
#define CAN_COB_NODE_MASK               0x07Fu
#define CAN_COB_ID_MAX                  0x7FFu
#define CAN_NODE_ID_MAX                 127u

/* Function codes, node id stripped. 0x080 is shared: the bare id 0x080 is
 * SYNC, 0x080 + node is an emergency frame from that node. Whoever decodes
 * one must have ruled out the other first. */
#define CAN_FUNC_NMT                    0x000u  /* broadcast, no node id     */
#define CAN_FUNC_SYNC                   0x080u  /* exact id, no node id      */
#define CAN_FUNC_EMCY                   0x080u  /* 0x080 + node              */
#define CAN_FUNC_TIME                   0x100u  /* broadcast, no node id     */
#define CAN_FUNC_TPDO1                  0x180u  /* slave -> master           */
#define CAN_FUNC_RPDO1                  0x200u  /* master -> slave           */
#define CAN_FUNC_TPDO2                  0x280u
#define CAN_FUNC_RPDO2                  0x300u
#define CAN_FUNC_TPDO3                  0x380u
#define CAN_FUNC_RPDO3                  0x400u
#define CAN_FUNC_TPDO4                  0x480u
#define CAN_FUNC_RPDO4                  0x500u
#define CAN_FUNC_SDO_TX                 0x580u  /* slave -> master           */
#define CAN_FUNC_SDO_RX                 0x600u  /* master -> slave           */
#define CAN_FUNC_HEARTBEAT              0x700u  /* also carries boot-up      */

/* NMT command, byte 0 of a 0x000 frame. Byte 1 is the target node, 0 = all. */
#define CAN_NMT_START                   0x01u
#define CAN_NMT_STOP                    0x02u
#define CAN_NMT_PRE_OPERATIONAL         0x80u
#define CAN_NMT_RESET_NODE              0x81u
#define CAN_NMT_RESET_COMM              0x82u

/* Heartbeat state byte with bit 7 masked off -- that bit is the node-guarding
 * toggle, and it is not part of the state. */
#define CAN_NMT_STATE_MASK              0x7Fu
#define CAN_NMT_STATE_BOOTUP            0x00u
#define CAN_NMT_STATE_STOPPED           0x04u
#define CAN_NMT_STATE_OPERATIONAL       0x05u
#define CAN_NMT_STATE_PRE_OPERATIONAL   0x7Fu

/* Fixed payload lengths of the services that have one. */
#define CAN_NMT_DLC                     2u
#define CAN_TIME_DLC                    6u
#define CAN_HEARTBEAT_DLC               1u

#endif /* CAN_DECODE_CAN_H_ */
