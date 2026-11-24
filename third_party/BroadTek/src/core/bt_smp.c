/* SPDX-License-Identifier: MIT */
/* Reading the IRK off the wire.
 *
 * macOS will not hand an IRK to this controller: it decides what the
 * controller may do from its own chipset classification, not from the HCI
 * features, and a THIRD_PARTY_DONGLE is offered neither the resolving list
 * nor Apple's IRK vendor command (docs/STATE.md §3). So the IRK is taken
 * from the one place it does appear: the bonding exchange itself.
 *
 * When two LE devices bond, the key distribution phase sends SMP Identity
 * Information (the IRK) and Identity Address Information (the identity it
 * belongs to) over L2CAP channel 0x0006. Link-layer encryption is applied by
 * the controller, below HCI, so on the USB bus these are plaintext ACL data
 * on the bulk endpoint -- a pipe the plugin already has a hook on.
 *
 * This file only reads. Nothing here changes a packet, and a captured IRK is
 * not put into the resolving list: promoting one is a separate, deliberate
 * step, so that capture can be confirmed before anything depends on it. */
#include "bt_core.h"

#define L2CAP_CID_SMP       0x0006
#define SMP_IDENTITY_INFO   0x08    /* IRK[16] */
#define SMP_IDENTITY_ADDR   0x09    /* type, addr[6] */

static void bcopy_n(uint8_t *d, const uint8_t *s, size_t n)
{
    for (size_t i = 0; i < n; i++) d[i] = s[i];
}

/* The capture slot for this connection handle, creating one if there is room. */
static struct bt_smp_capture *capture_for(struct bt_ctx *c, uint16_t handle)
{
    for (int i = 0; i < BT_SMP_MAX; i++)
        if (c->smp[i].used && c->smp[i].handle == handle) return &c->smp[i];
    for (int i = 0; i < BT_SMP_MAX; i++)
        if (!c->smp[i].used) {
            c->smp[i].used = 1;
            c->smp[i].handle = handle;
            c->smp[i].have_irk = 0;
            c->smp[i].have_id = 0;
            return &c->smp[i];
        }
    return NULL;
}

int bt_acl_scan(struct bt_ctx *c, const uint8_t *pkt, size_t len)
{
    if (len < 4) return 0;

    uint16_t header = (uint16_t)(pkt[0] | (pkt[1] << 8));
    uint16_t handle = (uint16_t)(header & 0x0fff);
    uint8_t  pb     = (uint8_t)((header >> 12) & 0x03);
    uint16_t alen   = (uint16_t)(pkt[2] | (pkt[3] << 8));

    if (alen > len - 4) return 0;          /* lying length: do not read past it */

    /* 0x01 is a continuation. Reassembly is not attempted: an SMP PDU is at
     * most 21 bytes with its L2CAP header and fits any LE ACL payload, so a
     * fragmented one would be a surprise worth seeing in the counter rather
     * than a case to handle blind. */
    if (pb == 0x01) { c->acl_continuations++; return 0; }
    if (alen < 4) return 0;

    const uint8_t *l2 = &pkt[4];
    uint16_t plen = (uint16_t)(l2[0] | (l2[1] << 8));
    uint16_t cid  = (uint16_t)(l2[2] | (l2[3] << 8));

    if (cid != L2CAP_CID_SMP) return 0;
    if ((size_t)plen + 4 > alen) { c->smp_truncated++; return 0; }
    if (plen < 1) return 0;

    const uint8_t *smp = &l2[4];
    c->smp_pdus++;

    switch (smp[0]) {
    case SMP_IDENTITY_INFO: {
        if (plen < 1 + 16) { c->smp_truncated++; return 0; }
        struct bt_smp_capture *cap = capture_for(c, handle);
        if (cap == NULL) return 0;
        bcopy_n(cap->irk, &smp[1], 16);    /* SMP sends it least significant octet first,
                                            * which is the order bt_rpa_matches wants */
        cap->have_irk = 1;
        c->irks_seen++;
        return 1;
    }
    case SMP_IDENTITY_ADDR: {
        if (plen < 1 + 7) { c->smp_truncated++; return 0; }
        struct bt_smp_capture *cap = capture_for(c, handle);
        if (cap == NULL) return 0;
        cap->id_type = (uint8_t)(smp[1] & 1);
        bcopy_n(cap->id_addr, &smp[2], 6);
        cap->have_id = 1;
        return 1;
    }
    default:
        break;
    }
    return 0;
}

/* How many captures have both halves: an IRK and the identity it belongs to.
 * One without the other is not usable. */
int bt_smp_complete(const struct bt_ctx *c)
{
    int n = 0;
    for (int i = 0; i < BT_SMP_MAX; i++)
        if (c->smp[i].used && c->smp[i].have_irk && c->smp[i].have_id) n++;
    return n;
}

/* Move finished captures into the resolving list. Separate from capture on
 * purpose: reading a key off the wire is one decision, trusting it enough to
 * resolve with it is another, and the kext makes the second one explicitly. */
int bt_promote_captures(struct bt_ctx *c)
{
    int n = 0;
    for (int i = 0; i < BT_SMP_MAX; i++) {
        struct bt_smp_capture *cap = &c->smp[i];
        if (!cap->used || !cap->have_irk || !cap->have_id) continue;
        if (bt_rl_install(c, cap->id_type, cap->id_addr, cap->irk)) n++;
    }
    return n;
}
