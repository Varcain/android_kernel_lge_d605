/*
 * DHD PROP_TXSTATUS Module.
 *
 * $Copyright Open Broadcom Corporation$
 *
 * $Id: dhd_wlfc.c 410252 2013-06-28 18:04:06Z $
 *
 */

#include <typedefs.h>
#include <osl.h>

#include <bcmutils.h>
#include <bcmendian.h>

#include <dngl_stats.h>
#include <dhd.h>

#include <dhd_bus.h>
#include <dhd_dbg.h>

#ifdef PROP_TXSTATUS
#include <wlfc_proto.h>
#include <dhd_wlfc.h>
#endif


/*
 * wlfc naming and lock rules:
 *
 * 1. Private functions name like _dhd_wlfc_XXX, declared as static and avoid wlfc lock operation.
 * 2. Public functions name like dhd_wlfc_XXX, use wlfc lock if needed.
 * 3. Non-Proptxstatus module call public functions only and avoid wlfc lock operation.
 *
 */


#ifdef PROP_TXSTATUS

#ifdef QMONITOR
#define DHD_WLFC_QMON_COMPLETE(entry) dhd_qmon_txcomplete(&entry->qmon)
#else
#define DHD_WLFC_QMON_COMPLETE(entry)
#endif /* QMONITOR */

/* Create a place to store all packet pointers submitted to the firmware until
	a status comes back, suppress or otherwise.

	hang-er: noun, a contrivance on which things are hung, as a hook.
*/
static void*
_dhd_wlfc_hanger_create(osl_t *osh, int max_items)
{
	int i;
	wlfc_hanger_t* hanger;

	/* allow only up to a specific size for now */
	ASSERT(max_items == WLFC_HANGER_MAXITEMS);

	if ((hanger = (wlfc_hanger_t*)MALLOC(osh, WLFC_HANGER_SIZE(max_items))) == NULL)
		return NULL;

	memset(hanger, 0, WLFC_HANGER_SIZE(max_items));
	hanger->max_items = max_items;

	for (i = 0; i < hanger->max_items; i++) {
		hanger->items[i].state = WLFC_HANGER_ITEM_STATE_FREE;
		hanger->items[i].identifier = i;
		hanger->items[i].next = (i == (hanger->max_items -1)) ?
			NULL : &(hanger->items[i+1]);
	}
	hanger->free_list_head = &(hanger->items[0]);
	hanger->free_list_tail = &(hanger->items[hanger->max_items-1]);

	return hanger;
}

static int
_dhd_wlfc_hanger_delete(osl_t *osh, void* hanger)
{
	wlfc_hanger_t* h = (wlfc_hanger_t*)hanger;

	if (h) {
		MFREE(osh, h, WLFC_HANGER_SIZE(h->max_items));
		return BCME_OK;
	}
	return BCME_BADARG;
}

static uint16
_dhd_wlfc_hanger_deque_free_slot(void* hanger)
{
	uint32 i;
	wlfc_hanger_t* h = (wlfc_hanger_t*)hanger;

	if (h) {
		if (h->free_list_head) {
			i = h->free_list_head->identifier;
			h->free_list_head = h->free_list_head->next;
			if (h->free_list_head == NULL)
				h->free_list_tail = NULL;
			h->items[i].next = NULL;
			return (uint16)i;
		}
		h->failed_slotfind++;
	}

	return WLFC_HANGER_MAXITEMS;
}

static void
_dhd_wlfc_hanger_enque_free_slot(void* hanger, uint16 slot_id)
{
	wlfc_hanger_t* h = (wlfc_hanger_t*)hanger;
	wlfc_hanger_item_t *slot;

	if (!h || (slot_id >= WLFC_HANGER_MAXITEMS)) {
		WLFC_DBGMESG(("%s: Invalid parameter h(%p), slot_id(%d)\n",
			__FUNCTION__, h, slot_id));
		return;
	}

	slot = &h->items[slot_id];
	ASSERT(slot->state == WLFC_HANGER_ITEM_STATE_FREE);

	if (h->free_list_tail)
		h->free_list_tail->next = slot;
	else
		h->free_list_head = slot;

	h->free_list_tail = slot;
}

static int
_dhd_wlfc_hanger_get_genbit(void* hanger, void* pkt, uint32 slot_id, int* gen)
{
	int rc = BCME_OK;
	wlfc_hanger_t* h = (wlfc_hanger_t*)hanger;

	*gen = 0xff;

	/* this packet was not pushed at the time it went to the firmware */
	if (slot_id == WLFC_HANGER_MAXITEMS)
		return BCME_NOTFOUND;

	if (h) {
		if ((h->items[slot_id].state == WLFC_HANGER_ITEM_STATE_INUSE) ||
			(h->items[slot_id].state == WLFC_HANGER_ITEM_STATE_INUSE_SUPPRESSED)) {
			*gen = h->items[slot_id].gen;
		}
		else {
			rc = BCME_NOTFOUND;
		}
	}
	else
		rc = BCME_BADARG;
	return rc;
}

static int
_dhd_wlfc_hanger_pushpkt(void* hanger, void* pkt, uint32 slot_id)
{
	int rc = BCME_OK;
	wlfc_hanger_t* h = (wlfc_hanger_t*)hanger;

	if (h && (slot_id < WLFC_HANGER_MAXITEMS)) {
		if (h->items[slot_id].state == WLFC_HANGER_ITEM_STATE_FREE) {
			h->items[slot_id].state = WLFC_HANGER_ITEM_STATE_INUSE;
			h->items[slot_id].pkt = pkt;
			h->pushed++;
		}
		else {
			h->failed_to_push++;
			rc = BCME_NOTFOUND;
		}
	}
	else
		rc = BCME_BADARG;
	return rc;
}

static int
_dhd_wlfc_hanger_poppkt(void* hanger, uint32 slot_id, void** pktout, int remove_from_hanger)
{
	int rc = BCME_OK;
	wlfc_hanger_t* h = (wlfc_hanger_t*)hanger;

	/* this packet was not pushed at the time it went to the firmware */
	if (slot_id == WLFC_HANGER_MAXITEMS) {
		DHD_ERROR(("%s: slot_id is invalid\n", __FUNCTION__));
		return BCME_NOTFOUND;
	}

	if (h) {
		if (h->items[slot_id].state != WLFC_HANGER_ITEM_STATE_FREE) {
			*pktout = h->items[slot_id].pkt;
			if (remove_from_hanger) {
				h->items[slot_id].state =
					WLFC_HANGER_ITEM_STATE_FREE;
				h->items[slot_id].pkt = NULL;
				h->items[slot_id].gen = 0xff;
				_dhd_wlfc_hanger_enque_free_slot(h, slot_id);
				h->popped++;
			}
		} else {
			h->failed_to_pop++;
			rc = BCME_NOTFOUND;
			DHD_ERROR(("%s: slot state is free\n", __FUNCTION__));
		}
	} else {
		rc = BCME_BADARG;
		DHD_ERROR(("%s: hanger pointer is NULL\n", __FUNCTION__));
	}
	return rc;
}

static int
_dhd_wlfc_hanger_mark_suppressed(void* hanger, uint32 slot_id, uint8 gen)
{
	int rc = BCME_OK;
	wlfc_hanger_t* h = (wlfc_hanger_t*)hanger;

	/* this packet was not pushed at the time it went to the firmware */
	if (slot_id == WLFC_HANGER_MAXITEMS)
		return BCME_NOTFOUND;
	if (h) {
		h->items[slot_id].gen = gen;
		if (h->items[slot_id].state == WLFC_HANGER_ITEM_STATE_INUSE) {
			h->items[slot_id].state = WLFC_HANGER_ITEM_STATE_INUSE_SUPPRESSED;
		}
		else
			rc = BCME_BADARG;
	}
	else
		rc = BCME_BADARG;

	return rc;
}

static int
_dhd_wlfc_pushheader(athost_wl_status_info_t* ctx, void* p, bool tim_signal,
	uint8 tim_bmp, uint8 mac_handle, uint32 htodtag)
{
	uint32 wl_pktinfo = 0;
	uint8* wlh;
	uint8 dataOffset;
	uint8 fillers;
	uint8 tim_signal_len = 0;

	struct bdc_header *h;

	if (tim_signal) {
		tim_signal_len = 1 + 1 + WLFC_CTL_VALUE_LEN_PENDING_TRAFFIC_BMP;
	}

	/* +2 is for Type[1] and Len[1] in TLV, plus TIM signal */
	dataOffset = WLFC_CTL_VALUE_LEN_PKTTAG + 2 + tim_signal_len;
	fillers = ROUNDUP(dataOffset, 4) - dataOffset;
	dataOffset += fillers;

	PKTPUSH(ctx->osh, p, dataOffset);
	wlh = (uint8*) PKTDATA(ctx->osh, p);

	wl_pktinfo = htol32(htodtag);

	wlh[0] = WLFC_CTL_TYPE_PKTTAG;
	wlh[1] = WLFC_CTL_VALUE_LEN_PKTTAG;
	memcpy(&wlh[2], &wl_pktinfo, sizeof(uint32));

	if (tim_signal_len) {
		wlh[dataOffset - fillers - tim_signal_len ] =
			WLFC_CTL_TYPE_PENDING_TRAFFIC_BMP;
		wlh[dataOffset - fillers - tim_signal_len + 1] =
			WLFC_CTL_VALUE_LEN_PENDING_TRAFFIC_BMP;
		wlh[dataOffset - fillers - tim_signal_len + 2] = mac_handle;
		wlh[dataOffset - fillers - tim_signal_len + 3] = tim_bmp;
	}
	if (fillers)
		memset(&wlh[dataOffset - fillers], WLFC_CTL_TYPE_FILLER, fillers);

	PKTPUSH(ctx->osh, p, BDC_HEADER_LEN);
	h = (struct bdc_header *)PKTDATA(ctx->osh, p);
	h->flags = (BDC_PROTO_VER << BDC_FLAG_VER_SHIFT);
	if (PKTSUMNEEDED(p))
		h->flags |= BDC_FLAG_SUM_NEEDED;


	h->priority = (PKTPRIO(p) & BDC_PRIORITY_MASK);
	h->flags2 = 0;
	h->dataOffset = dataOffset >> 2;
	BDC_SET_IF_IDX(h, DHD_PKTTAG_IF(PKTTAG(p)));
	return BCME_OK;
}

static int
_dhd_wlfc_pullheader(athost_wl_status_info_t* ctx, void* pktbuf)
{
	struct bdc_header *h;

	if (PKTLEN(ctx->osh, pktbuf) < BDC_HEADER_LEN) {
		WLFC_DBGMESG(("%s: rx data too short (%d < %d)\n", __FUNCTION__,
		           PKTLEN(ctx->osh, pktbuf), BDC_HEADER_LEN));
		return BCME_ERROR;
	}
	h = (struct bdc_header *)PKTDATA(ctx->osh, pktbuf);

	/* pull BDC header */
	PKTPULL(ctx->osh, pktbuf, BDC_HEADER_LEN);

	if (PKTLEN(ctx->osh, pktbuf) < (h->dataOffset << 2)) {
		WLFC_DBGMESG(("%s: rx data too short (%d < %d)\n", __FUNCTION__,
		           PKTLEN(ctx->osh, pktbuf), (h->dataOffset << 2)));
		return BCME_ERROR;
	}

	/* pull wl-header */
	PKTPULL(ctx->osh, pktbuf, (h->dataOffset << 2));
	return BCME_OK;
}

static wlfc_mac_descriptor_t*
_dhd_wlfc_find_table_entry(athost_wl_status_info_t* ctx, void* p)
{
	int i;
	wlfc_mac_descriptor_t* table = ctx->destination_entries.nodes;
	uint8 ifid = DHD_PKTTAG_IF(PKTTAG(p));
	uint8* dstn = DHD_PKTTAG_DSTN(PKTTAG(p));
	wlfc_mac_descriptor_t* entry = DHD_PKTTAG_ENTRY(PKTTAG(p));
	int iftype = ctx->destination_entries.interfaces[ifid].iftype;

	/* saved one exists, return it */
	if (entry)
		return entry;

	/* Multicast destination, STA and P2P clients get the interface entry.
	 * STA/GC gets the Mac Entry for TDLS destinations, TDLS destinations
	 * have their own entry.
	 */
	if ((iftype == WLC_E_IF_ROLE_STA || ETHER_ISMULTI(dstn) ||
		iftype == WLC_E_IF_ROLE_P2P_CLIENT) &&
		(ctx->destination_entries.interfaces[ifid].occupied)) {
			entry = &ctx->destination_entries.interfaces[ifid];
	}

	if (entry != NULL && ETHER_ISMULTI(dstn))
		return entry;

	for (i = 0; i < WLFC_MAC_DESC_TABLE_SIZE; i++) {
		if (table[i].occupied) {
			if (table[i].interface_id == ifid) {
				if (!memcmp(table[i].ea, dstn, ETHER_ADDR_LEN)) {
					entry = &table[i];
					break;
				}
			}
		}
	}

	if (entry == NULL)
		entry = &ctx->destination_entries.other;

	DHD_PKTTAG_SET_ENTRY(PKTTAG(p), entry);

	return entry;
}


static int
_dhd_wlfc_prec_drop(dhd_pub_t *dhdp, int prec, void* p, bool countdown)
{
	athost_wl_status_info_t* ctx;
	void *pout = NULL;

	ASSERT(dhdp && p);
	ASSERT(prec >= 0 && prec <= WLFC_PSQ_PREC_COUNT);

	ctx = (athost_wl_status_info_t*)dhdp->wlfc_state;

	if (prec & 1) {
		/* suppressed queue, need pop from hanger */
		_dhd_wlfc_hanger_poppkt(ctx->hanger, WLFC_PKTID_HSLOT_GET(DHD_PKTTAG_H2DTAG
					(PKTTAG(p))), &pout, 1);
		ASSERT(p == pout);
	}

	if (countdown)
		ctx->pkt_cnt_in_q[DHD_PKTTAG_IF(PKTTAG(p))][prec>>1]--;

	dhd_txcomplete(dhdp, p, FALSE);
	PKTFREE(ctx->osh, p, TRUE);
	ctx->stats.drop_pkts[prec]++;

	return 0;
}

static bool
_dhd_wlfc_prec_enq_with_drop(dhd_pub_t *dhdp, struct pktq *pq, void *pkt, int prec, bool qHead)
{
	void *p = NULL;
	int eprec = -1;		/* precedence to evict from */
	athost_wl_status_info_t* ctx;

	ASSERT(dhdp && pq && pkt);
	ASSERT(prec >= 0 && prec < pq->num_prec);

	ctx = (athost_wl_status_info_t*)dhdp->wlfc_state;

	/* Fast case, precedence queue is not full and we are also not
	 * exceeding total queue length
	 */
	if (!pktq_pfull(pq, prec) && !pktq_full(pq)) {
		goto exit;
	}

	/* Determine precedence from which to evict packet, if any */
	if (pktq_pfull(pq, prec))
		eprec = prec;
	else if (pktq_full(pq)) {
		p = pktq_peek_tail(pq, &eprec);
		if (!p) {
			WLFC_DBGMESG(("Error: %s():%d\n", __FUNCTION__, __LINE__));
			return FALSE;
		}
		if ((eprec > prec) || (eprec < 0)) {
			if (!pktq_pempty(pq, prec)) {
				eprec = prec;
			} else {
				return FALSE;
			}
		}
	}

	/* Evict if needed */
	if (eprec >= 0) {
		/* Detect queueing to unconfigured precedence */
		ASSERT(!pktq_pempty(pq, eprec));
		/* Evict all fragmented frames */
		dhd_prec_drop_pkts(dhdp, pq, eprec, _dhd_wlfc_prec_drop);
	}

exit:
	/* Enqueue */
	if (qHead)
		p = pktq_penq_head(pq, prec, pkt);
	else
		p = pktq_penq(pq, prec, pkt);
	if (!p) {
		WLFC_DBGMESG(("Error: %s():%d\n", __FUNCTION__, __LINE__));
		return FALSE;
	}

	ctx->pkt_cnt_in_q[DHD_PKTTAG_IF(PKTTAG(pkt))][prec>>1]++;

	return TRUE;
}


static int
_dhd_wlfc_rollback_packet_toq(athost_wl_status_info_t* ctx,
	void* p, ewlfc_packet_state_t pkt_type, uint32 hslot)
{
	/*
	put the packet back to the head of queue

	- suppressed packet goes back to suppress sub-queue
	- pull out the header, if new or delayed packet

	Note: hslot is used only when header removal is done.
	*/
	wlfc_mac_descriptor_t* entry;
	int rc = BCME_OK;
	int prec, fifo_id;

	entry = _dhd_wlfc_find_table_entry(ctx, p);
	prec = DHD_PKTTAG_FIFO(PKTTAG(p));
	fifo_id = prec << 1;
	if (pkt_type == eWLFC_PKTTYPE_SUPPRESSED)
		fifo_id += 1;
	if (entry != NULL) {
		/*
		if this packet did not count against FIFO credit, it must have
		taken a requested_credit from the firmware (for pspoll etc.)
		*/
		if ((prec != AC_COUNT) && !DHD_PKTTAG_CREDITCHECK(PKTTAG(p)))
			entry->requested_credit++;

		if (pkt_type == eWLFC_PKTTYPE_DELAYED) {
			/* decrement sequence count */
			WLFC_DECR_SEQCOUNT(entry, prec);
			/* remove header first */
			rc = _dhd_wlfc_pullheader(ctx, p);
			if (rc != BCME_OK) {
				WLFC_DBGMESG(("Error: %s():%d\n", __FUNCTION__, __LINE__));
				goto exit;
			}
		}

		if (_dhd_wlfc_prec_enq_with_drop(ctx->dhdp, &entry->psq, p, fifo_id, TRUE)
			== FALSE) {
			/* enque failed */
			WLFC_DBGMESG(("Error: %s():%d, fifo_id(%d)\n",
				__FUNCTION__, __LINE__, fifo_id));
			rc = BCME_ERROR;
		}
	} else {
		WLFC_DBGMESG(("Error: %s():%d\n", __FUNCTION__, __LINE__));
		rc = BCME_ERROR;
	}
exit:
	if (rc != BCME_OK) {
		ctx->stats.rollback_failed++;
		_dhd_wlfc_prec_drop(ctx->dhdp, fifo_id, p, FALSE);
	}
	else
		ctx->stats.rollback++;

	return rc;
}


static bool
_dhd_wlfc_allow_fc(athost_wl_status_info_t* ctx, uint8 ifid)
{
	int prec, ac_traffic = WLFC_NO_TRAFFIC;

	for (prec = 0; prec < AC_COUNT; prec++) {
		if (ctx->pkt_cnt_in_q[ifid][prec] > 0) {
			if (ac_traffic == WLFC_NO_TRAFFIC)
				ac_traffic = prec + 1;
			else if (ac_traffic != (prec + 1))
				ac_traffic = WLFC_MULTI_TRAFFIC;
		}
	}

	if (ac_traffic >= 1 && ac_traffic <= AC_COUNT) {
		/* single AC (BE/BK/VI/VO) in queue */
		if (ctx->allow_fc) {
			return TRUE;
		} else {
			uint32 delta;
			uint32 curr_t = OSL_SYSUPTIME();

			if (ctx->fc_defer_timestamp == 0) {
				/* first signle ac scenario */
				ctx->fc_defer_timestamp = curr_t;
				return FALSE;
			}

			if (curr_t > ctx->fc_defer_timestamp)
				delta = curr_t - ctx->fc_defer_timestamp;
			else
				delta = (~(uint32)0) - ctx->fc_defer_timestamp + curr_t;

			if (delta >= WLFC_FC_DEFER_PERIOD_MS) {
				ctx->allow_fc = TRUE;
			}
		}
	} else {
		/* multiple ACs or BCMC only in queue */
		ctx->allow_fc = FALSE;
		ctx->fc_defer_timestamp = 0;
	}

	return ctx->allow_fc;
}

static void
_dhd_wlfc_flow_control_check(athost_wl_status_info_t* ctx, struct pktq* pq, uint8 if_id)
{
	dhd_pub_t *dhdp;

	ASSERT(ctx);

	dhdp = (dhd_pub_t *)ctx->dhdp;
	ASSERT(dhdp);

	if ((ctx->hostif_flow_state[if_id] == OFF) && !_dhd_wlfc_allow_fc(ctx, if_id))
		return;

	if ((pq->len <= WLFC_FLOWCONTROL_LOWATER) && (ctx->hostif_flow_state[if_id] == ON)) {
		/* start traffic */
		ctx->hostif_flow_state[if_id] = OFF;
		/*
		WLFC_DBGMESG(("qlen:%02d, if:%02d, ->OFF, start traffic %s()\n",
		pq->len, if_id, __FUNCTION__));
		*/
		WLFC_DBGMESG(("F"));

		dhd_txflowcontrol(dhdp, if_id, OFF);

		ctx->toggle_host_if = 0;
	}

	if ((pq->len >= WLFC_FLOWCONTROL_HIWATER) && (ctx->hostif_flow_state[if_id] == OFF)) {
		/* stop traffic */
		ctx->hostif_flow_state[if_id] = ON;
		/*
		WLFC_DBGMESG(("qlen:%02d, if:%02d, ->ON, stop traffic   %s()\n",
		pq->len, if_id, __FUNCTION__));
		*/
		WLFC_DBGMESG(("N"));

		dhd_txflowcontrol(dhdp, if_id, ON);

		ctx->host_ifidx = if_id;
		ctx->toggle_host_if = 1;
	}

	return;
}

static int
_dhd_wlfc_send_signalonly_packet(athost_wl_status_info_t* ctx, wlfc_mac_descriptor_t* entry,
	uint8 ta_bmp)
{
	int rc = BCME_OK;
	void* p = NULL;
	int dummylen = ((dhd_pub_t *)ctx->dhdp)->hdrlen+ 12;

	/* allocate a dummy packet */
	p = PKTGET(ctx->osh, dummylen, TRUE);
	if (p) {
		PKTPULL(ctx->osh, p, dummylen);
		DHD_PKTTAG_SET_H2DTAG(PKTTAG(p), 0);
		_dhd_wlfc_pushheader(ctx, p, TRUE, ta_bmp, entry->mac_handle, 0);
		DHD_PKTTAG_SETSIGNALONLY(PKTTAG(p), 1);
		DHD_PKTTAG_WLFCPKT_SET(PKTTAG(p), 1);
#ifdef PROP_TXSTATUS_DEBUG
		ctx->stats.signal_only_pkts_sent++;
#endif
		rc = dhd_bus_txdata(((dhd_pub_t *)ctx->dhdp)->bus, p);
		if (rc != BCME_OK) {
			PKTFREE(ctx->osh, p, TRUE);
		}
	}
	else {
		DHD_ERROR(("%s: couldn't allocate new %d-byte packet\n",
		           __FUNCTION__, dummylen));
		rc = BCME_NOMEM;
	}
	return rc;
}

/* Return TRUE if traffic availability changed */
static bool
_dhd_wlfc_traffic_pending_check(athost_wl_status_info_t* ctx, wlfc_mac_descriptor_t* entry,
	int prec)
{
	bool rc = FALSE;

	if (entry->state == WLFC_STATE_CLOSE) {
		if ((pktq_plen(&entry->psq, (prec << 1)) == 0) &&
			(pktq_plen(&entry->psq, ((prec << 1) + 1)) == 0)) {

			if (entry->traffic_pending_bmp & NBITVAL(prec)) {
				rc = TRUE;
				entry->traffic_pending_bmp =
					entry->traffic_pending_bmp & ~ NBITVAL(prec);
			}
		}
		else {
			if (!(entry->traffic_pending_bmp & NBITVAL(prec))) {
				rc = TRUE;
				entry->traffic_pending_bmp =
					entry->traffic_pending_bmp | NBITVAL(prec);
			}
		}
	}
	if (rc) {
		/* request a TIM update to firmware at the next piggyback opportunity */
		if (entry->traffic_lastreported_bmp != entry->traffic_pending_bmp) {
			entry->send_tim_signal = 1;
			_dhd_wlfc_send_signalonly_packet(ctx, entry, entry->traffic_pending_bmp);
			entry->traffic_lastreported_bmp = entry->traffic_pending_bmp;
			entry->send_tim_signal = 0;
		}
		else {
			rc = FALSE;
		}
	}
	return rc;
}

static int
_dhd_wlfc_enque_suppressed(athost_wl_status_info_t* ctx, int prec, void* p)
{
	wlfc_mac_descriptor_t* entry;

	entry = _dhd_wlfc_find_table_entry(ctx, p);
	if (entry == NULL) {
		WLFC_DBGMESG(("Error: %s():%d\n", __FUNCTION__, __LINE__));
		return BCME_NOTFOUND;
	}
	/*
	- suppressed packets go to sub_queue[2*prec + 1] AND
	- delayed packets go to sub_queue[2*prec + 0] to ensure
	order of delivery.
	*/
	if (_dhd_wlfc_prec_enq_with_drop(ctx->dhdp, &entry->psq, p, ((prec << 1) + 1), FALSE)
		== FALSE) {
		ctx->stats.delayq_full_error++;
		/* WLFC_DBGMESG(("Error: %s():%d\n", __FUNCTION__, __LINE__)); */
		WLFC_DBGMESG(("s"));
		return BCME_ERROR;
	}

	/* A packet has been pushed, update traffic availability bitmap, if applicable */
	_dhd_wlfc_traffic_pending_check(ctx, entry, prec);
	_dhd_wlfc_flow_control_check(ctx, &entry->psq, DHD_PKTTAG_IF(PKTTAG(p)));
	return BCME_OK;
}

static int
_dhd_wlfc_pretx_pktprocess(athost_wl_status_info_t* ctx,
	wlfc_mac_descriptor_t* entry, void* p, int header_needed, uint32* slot)
{
	int rc = BCME_OK;
	int hslot = WLFC_HANGER_MAXITEMS;
	bool send_tim_update = FALSE;
	uint32 htod = 0;
	uint8 free_ctr;

	*slot = hslot;

	if (entry == NULL) {
		entry = _dhd_wlfc_find_table_entry(ctx, p);
	}

	if (entry == NULL) {
		WLFC_DBGMESG(("Error: %s():%d\n", __FUNCTION__, __LINE__));
		return BCME_ERROR;
	}

	if (entry->send_tim_signal) {
		send_tim_update = TRUE;
		entry->send_tim_signal = 0;
		entry->traffic_lastreported_bmp = entry->traffic_pending_bmp;
	}
	if (header_needed) {
		hslot = _dhd_wlfc_hanger_deque_free_slot(ctx->hanger);
		free_ctr = WLFC_SEQCOUNT(entry, DHD_PKTTAG_FIFO(PKTTAG(p)));
		DHD_PKTTAG_SET_H2DTAG(PKTTAG(p), htod);
		WLFC_PKTFLAG_SET_GENERATION(htod, entry->generation);
	} else {
		hslot = WLFC_PKTID_HSLOT_GET(DHD_PKTTAG_H2DTAG(PKTTAG(p)));
		free_ctr = WLFC_PKTID_FREERUNCTR_GET(DHD_PKTTAG_H2DTAG(PKTTAG(p)));
	}

	if (hslot >= WLFC_HANGER_MAXITEMS) {
		WLFC_DBGMESG(("Error: %s():no hanger slot available\n", __FUNCTION__));
		return BCME_ERROR;
	}

	WLFC_PKTID_HSLOT_SET(htod, hslot);
	WLFC_PKTID_FREERUNCTR_SET(htod, free_ctr);
	DHD_PKTTAG_SETPKTDIR(PKTTAG(p), 1);
	WL_TXSTATUS_SET_FLAGS(htod, WLFC_PKTFLAG_PKTFROMHOST);
	WL_TXSTATUS_SET_FIFO(htod, DHD_PKTTAG_FIFO(PKTTAG(p)));


	if (!DHD_PKTTAG_CREDITCHECK(PKTTAG(p))) {
		/*
		Indicate that this packet is being sent in response to an
		explicit request from the firmware side.
		*/
		WLFC_PKTFLAG_SET_PKTREQUESTED(htod);
	} else {
		WLFC_PKTFLAG_CLR_PKTREQUESTED(htod);
	}

	if (header_needed) {
		rc = _dhd_wlfc_pushheader(ctx, p, send_tim_update,
			entry->traffic_lastreported_bmp, entry->mac_handle, htod);
		if (rc == BCME_OK) {
			DHD_PKTTAG_SET_H2DTAG(PKTTAG(p), htod);
			/*
			a new header was created for this packet.
			push to hanger slot and scrub q. Since bus
			send succeeded, increment seq number as well.
			*/
			rc = _dhd_wlfc_hanger_pushpkt(ctx->hanger, p, hslot);
			if (rc == BCME_OK) {
				/* increment free running sequence count */
				WLFC_INCR_SEQCOUNT(entry, DHD_PKTTAG_FIFO(PKTTAG(p)));
#ifdef PROP_TXSTATUS_DEBUG
				((wlfc_hanger_t*)(ctx->hanger))->items[hslot].push_time =
					OSL_SYSUPTIME();
#endif
			}
			else {
				WLFC_DBGMESG(("%s() hanger_pushpkt() failed, rc: %d\n",
					__FUNCTION__, rc));
			}
		}

		if (rc != BCME_OK) {
			/* the new hanger slot is not actually used, queue back to free list */
			_dhd_wlfc_hanger_enque_free_slot(ctx->hanger, hslot);
		}
	}
	else {
		int gen;

		/* remove old header */
		rc = _dhd_wlfc_pullheader(ctx, p);
		if (rc == BCME_OK) {
			hslot = WLFC_PKTID_HSLOT_GET(DHD_PKTTAG_H2DTAG(PKTTAG(p)));
			_dhd_wlfc_hanger_get_genbit(ctx->hanger, p, hslot, &gen);

			WLFC_PKTFLAG_SET_GENERATION(htod, gen);
			free_ctr = WLFC_PKTID_FREERUNCTR_GET(DHD_PKTTAG_H2DTAG(PKTTAG(p)));
			/* push new header */
			_dhd_wlfc_pushheader(ctx, p, send_tim_update,
				entry->traffic_lastreported_bmp, entry->mac_handle, htod);
		}
	}
	*slot = hslot;
	return rc;
}

static int
_dhd_wlfc_is_destination_closed(athost_wl_status_info_t* ctx,
	wlfc_mac_descriptor_t* entry, int prec)
{
	if (ctx->destination_entries.interfaces[entry->interface_id].iftype ==
		WLC_E_IF_ROLE_P2P_GO) {
		/* - destination interface is of type p2p GO.
		For a p2pGO interface, if the destination is OPEN but the interface is
		CLOSEd, do not send traffic. But if the dstn is CLOSEd while there is
		destination-specific-credit left send packets. This is because the
		firmware storing the destination-specific-requested packet in queue.
		*/
		if ((entry->state == WLFC_STATE_CLOSE) && (entry->requested_credit == 0) &&
			(entry->requested_packet == 0))
			return 1;
	}
	/* AP, p2p_go -> unicast desc entry, STA/p2p_cl -> interface desc. entry */
	if (((entry->state == WLFC_STATE_CLOSE) && (entry->requested_credit == 0) &&
		(entry->requested_packet == 0)) ||
		(!(entry->ac_bitmap & (1 << prec))))
		return 1;

	return 0;
}

static void*
_dhd_wlfc_deque_delayedq(athost_wl_status_info_t* ctx,
	int prec, uint8* ac_credit_spent, uint8* needs_hdr, wlfc_mac_descriptor_t** entry_out)
{
	wlfc_mac_descriptor_t* entry;
	wlfc_mac_descriptor_t* table;
	uint8 token_pos;
	int total_entries;
	void* p = NULL;
	int pout;
	int i;

	*entry_out = NULL;
	token_pos = ctx->token_pos[prec];
	/* most cases a packet will count against FIFO credit */
	*ac_credit_spent = ((prec == AC_COUNT) && !ctx->bcmc_credit_supported) ? 0 : 1;
	*needs_hdr = 1;

	/* search all entries, include nodes as well as interfaces */
	table = (wlfc_mac_descriptor_t*)&ctx->destination_entries;
	total_entries = sizeof(ctx->destination_entries)/sizeof(wlfc_mac_descriptor_t);

	for (i = 0; i < total_entries; i++) {
		entry = &table[(token_pos + i) % total_entries];
		if (entry->occupied) {
			if (!_dhd_wlfc_is_destination_closed(ctx, entry, prec)) {
				p = pktq_mdeq(&entry->psq,
					/* higher precedence will be picked up first,
					 * i.e. suppressed packets before delayed ones
					 */
					NBITVAL((prec << 1) + 1), &pout);
					*needs_hdr = 0;

				if (p == NULL) {
					if (entry->suppressed == TRUE) {
						/* skip this entry */
						continue;
					}
					/* De-Q from delay Q */
					p = pktq_mdeq(&entry->psq,
						NBITVAL((prec << 1)),
						&pout);
					*needs_hdr = 1;
				}

				if (p != NULL) {
					/* did the packet come from suppress sub-queue? */
					if (entry->requested_credit > 0) {
						entry->requested_credit--;
#ifdef PROP_TXSTATUS_DEBUG
						entry->dstncredit_sent_packets++;
#endif
						/*
						if the packet was pulled out while destination is in
						closed state but had a non-zero packets requested,
						then this should not count against the FIFO credit.
						That is due to the fact that the firmware will
						most likely hold onto this packet until a suitable
						time later to push it to the appropriate  AC FIFO.
						*/
						if (entry->state == WLFC_STATE_CLOSE)
							*ac_credit_spent = 0;
					}
					else if (entry->requested_packet > 0) {
						entry->requested_packet--;
						DHD_PKTTAG_SETONETIMEPKTRQST(PKTTAG(p));
						if (entry->state == WLFC_STATE_CLOSE)
							*ac_credit_spent = 0;
					}
					/* move token to ensure fair round-robin */
					ctx->token_pos[prec] =
						(token_pos + i + 1) % total_entries;
					*entry_out = entry;
					ctx->pkt_cnt_in_q[DHD_PKTTAG_IF(PKTTAG(p))][prec]--;
					_dhd_wlfc_flow_control_check(ctx, &entry->psq,
						DHD_PKTTAG_IF(PKTTAG(p)));
					/*
					A packet has been picked up, update traffic
					availability bitmap, if applicable
					*/
					_dhd_wlfc_traffic_pending_check(ctx, entry, prec);
					return p;
				}
			}
		}
	}
	return NULL;
}

static int
_dhd_wlfc_enque_delayq(athost_wl_status_info_t* ctx, void* pktbuf, int prec)
{
	wlfc_mac_descriptor_t* entry;

	if (pktbuf != NULL) {
		entry = _dhd_wlfc_find_table_entry(ctx, pktbuf);
		if (entry == NULL) {
			WLFC_DBGMESG(("Error: %s():%d\n", __FUNCTION__, __LINE__));
			return BCME_ERROR;
		}

		/*
		- suppressed packets go to sub_queue[2*prec + 1] AND
		- delayed packets go to sub_queue[2*prec + 0] to ensure
		order of delivery.
		*/
		if (_dhd_wlfc_prec_enq_with_drop(ctx->dhdp, &entry->psq, pktbuf, (prec << 1), FALSE)
			== FALSE) {
			WLFC_DBGMESG(("D"));
			_dhd_wlfc_prec_drop(ctx->dhdp, (prec << 1), pktbuf, FALSE);
			ctx->stats.delayq_full_error++;
			return BCME_ERROR;
		}

#ifdef QMONITOR
		dhd_qmon_tx(&entry->qmon);
#endif

		/*
		A packet has been pushed, update traffic availability bitmap,
		if applicable
		*/
		_dhd_wlfc_traffic_pending_check(ctx, entry, prec);

	}
	return BCME_OK;
}

static bool _dhd_wlfc_ifpkt_fn(void* p, void *p_ifid)
{
	if (!p || !p_ifid)
		return FALSE;

	return (*((uint8 *)p_ifid) == DHD_PKTTAG_IF(PKTTAG(p)));
}

static bool _dhd_wlfc_entrypkt_fn(void* p, void *entry)
{
	if (!p || !entry)
		return FALSE;

	return (entry == DHD_PKTTAG_ENTRY(PKTTAG(p)));
}

static void
_dhd_wlfc_pktq_flush(athost_wl_status_info_t* ctx, struct pktq *pq,
	bool dir, f_processpkt_t fn, void *arg)
{
	int prec;

	/* Optimize flush, if pktq len = 0, just return.
	 * pktq len of 0 means pktq's prec q's are all empty.
	 */
	if (pq->len == 0) {
		return;
	}

	for (prec = 0; prec < pq->num_prec; prec++) {
		struct pktq_prec *q;
		void *p, *prev = NULL;

		q = &pq->q[prec];
		p = q->head;
		while (p) {
			if (fn == NULL || (*fn)(p, arg)) {
				bool head = (p == q->head);
				if (head)
					q->head = PKTLINK(p);
				else
					PKTSETLINK(prev, PKTLINK(p));
				ctx->pkt_cnt_in_q[DHD_PKTTAG_IF(PKTTAG(p))][prec>>1]--;
				PKTSETLINK(p, NULL);
				PKTFREE(ctx->osh, p, dir);
				q->len--;
				pq->len--;
				p = (head ? q->head : PKTLINK(prev));
			} else {
				prev = p;
				p = PKTLINK(p);
			}
		}

		if (q->head == NULL) {
			ASSERT(q->len == 0);
			q->tail = NULL;
		}

	}

	if (fn == NULL)
		ASSERT(pq->len == 0);
}

static void*
_dhd_wlfc_pktq_pdeq_with_fn(struct pktq *pq, int prec, f_processpkt_t fn, void *arg)
{
	struct pktq_prec *q;
	void *p, *prev = NULL;

	ASSERT(prec >= 0 && prec < pq->num_prec);

	q = &pq->q[prec];
	p = q->head;

	while (p) {
		if (fn == NULL || (*fn)(p, arg)) {
			break;
		} else {
			prev = p;
			p = PKTLINK(p);
		}
	}
	if (p == NULL)
		return NULL;

	if (prev == NULL) {
		if ((q->head = PKTLINK(p)) == NULL)
			q->tail = NULL;
	} else {
		PKTSETLINK(prev, PKTLINK(p));
	}

	q->len--;

	pq->len--;

	PKTSETLINK(p, NULL);

	return p;
}

void
_dhd_wlfc_cleanup_txq(dhd_pub_t *dhd, f_processpkt_t fn, void *arg)
{
	int prec, i;
	void *pkt = NULL;
	struct pktq *txq = (struct pktq *)dhd_bus_txq(dhd->bus);
	athost_wl_status_info_t* wlfc = (athost_wl_status_info_t*) dhd->wlfc_state;
	wlfc_hanger_t* h = (wlfc_hanger_t*)wlfc->hanger;

	for (prec = 0; prec < txq->num_prec; prec++) {
		while ((pkt = _dhd_wlfc_pktq_pdeq_with_fn(txq, prec, fn, arg))) {
			for (i = 0; i < h->max_items; i++) {
				if (pkt == h->items[i].pkt) {
					if (h->items[i].state == WLFC_HANGER_ITEM_STATE_INUSE) {
						PKTFREE(wlfc->osh, h->items[i].pkt, TRUE);
						h->items[i].state = WLFC_HANGER_ITEM_STATE_FREE;
						_dhd_wlfc_hanger_enque_free_slot(h, i);
					} else if (h->items[i].state ==
						WLFC_HANGER_ITEM_STATE_INUSE_SUPPRESSED) {
						/* These are already freed from the psq */
						h->items[i].state = WLFC_HANGER_ITEM_STATE_FREE;
						_dhd_wlfc_hanger_enque_free_slot(h, i);
					}
					break;
				}
			}

			if (i == h->max_items) {
				WLFC_DBGMESG(("%s: can't find pkt(%p) in hanger, free it anyway\n",
					__FUNCTION__, pkt));
				PKTFREE(wlfc->osh, pkt, TRUE);
			}
		}
	}
}

void
_dhd_wlfc_cleanup(dhd_pub_t *dhd, f_processpkt_t fn, void *arg)
{
	int i;
	int total_entries;
	athost_wl_status_info_t* wlfc = (athost_wl_status_info_t*) dhd->wlfc_state;
	wlfc_mac_descriptor_t* table;
	wlfc_hanger_t* h = (wlfc_hanger_t*)wlfc->hanger;

	/* flush bus->txq */
	_dhd_wlfc_cleanup_txq(dhd, fn, arg);

	/* flush remained pkt in hanger queue, not in bus->txq */
	for (i = 0; i < h->max_items; i++) {
		if (h->items[i].state == WLFC_HANGER_ITEM_STATE_INUSE) {
			if (fn == NULL || (*fn)(h->items[i].pkt, arg)) {
				table = _dhd_wlfc_find_table_entry(wlfc, h->items[i].pkt);
				table->transit_count--;
				if (table->suppressed && (--table->suppr_transit_count == 0)) {
					table->suppressed = FALSE;
				}
				PKTFREE(wlfc->osh, h->items[i].pkt, TRUE);
				h->items[i].state = WLFC_HANGER_ITEM_STATE_FREE;
				_dhd_wlfc_hanger_enque_free_slot(h, i);
			}
		} else if (h->items[i].state == WLFC_HANGER_ITEM_STATE_INUSE_SUPPRESSED) {
			if (fn == NULL || (*fn)(h->items[i].pkt, arg)) {
				/* These are freed from the psq so no need to free again */
				h->items[i].state = WLFC_HANGER_ITEM_STATE_FREE;
				_dhd_wlfc_hanger_enque_free_slot(h, i);
			}
		}
	}

	total_entries = sizeof(wlfc->destination_entries)/sizeof(wlfc_mac_descriptor_t);
	/* search all entries, include nodes as well as interfaces */
	table = (wlfc_mac_descriptor_t*)&wlfc->destination_entries;

	for (i = 0; i < total_entries; i++) {
		if (table[i].occupied) {
			if (table[i].psq.len) {
				WLFC_DBGMESG(("%s(): DELAYQ[%d].len = %d\n",
					__FUNCTION__, i, table[i].psq.len));
				/* release packets held in DELAYQ */
				_dhd_wlfc_pktq_flush(wlfc, &table[i].psq, TRUE, fn, arg);
			}
			if (fn == NULL)
				table[i].occupied = 0;
		}
	}

	return;
}

static int
_dhd_wlfc_mac_entry_update(athost_wl_status_info_t* ctx, wlfc_mac_descriptor_t* entry,
	ewlfc_mac_entry_action_t action, uint8 ifid, uint8 iftype, uint8* ea,
	f_processpkt_t fn, void *arg)
{
	int rc = BCME_OK;

#ifdef QMONITOR
	dhd_qmon_reset(&entry->qmon);
#endif

	if ((action == eWLFC_MAC_ENTRY_ACTION_ADD) || (action == eWLFC_MAC_ENTRY_ACTION_UPDATE)) {
		entry->occupied = 1;
		entry->state = WLFC_STATE_OPEN;
		entry->requested_credit = 0;
		entry->interface_id = ifid;
		entry->iftype = iftype;
		entry->ac_bitmap = 0xff; /* update this when handling APSD */
		/* for an interface entry we may not care about the MAC address */
		if (ea != NULL)
			memcpy(&entry->ea[0], ea, ETHER_ADDR_LEN);

		if (action == eWLFC_MAC_ENTRY_ACTION_ADD)
			pktq_init(&entry->psq, WLFC_PSQ_PREC_COUNT, WLFC_PSQ_LEN);
	} else if (action == eWLFC_MAC_ENTRY_ACTION_DEL) {
		/* When the entry is deleted, the packets that are queued in the entry must be
		   cleanup. The cleanup action should be before the occupied is set as 0.
		*/
		_dhd_wlfc_cleanup(ctx->dhdp, fn, arg);
		_dhd_wlfc_flow_control_check(ctx, &entry->psq, ifid);

		entry->occupied = 0;
		entry->suppressed = 0;
		entry->state = WLFC_STATE_CLOSE;
		entry->requested_credit = 0;
		entry->transit_count = 0;
		entry->suppr_transit_count = 0;
		memset(&entry->ea[0], 0, ETHER_ADDR_LEN);
	}
	return rc;
}

static int
_dhd_wlfc_borrow_credit(athost_wl_status_info_t* ctx, uint8 available_credit_map, int borrower_ac)
{
	int lender_ac;
	int rc = BCME_ERROR;

	if (ctx == NULL || available_credit_map == 0) {
		WLFC_DBGMESG(("Error: %s():%d\n", __FUNCTION__, __LINE__));
		return BCME_BADARG;
	}

	/* Borrow from lowest priority available AC (including BC/MC credits) */
	for (lender_ac = 0; lender_ac <= AC_COUNT; lender_ac++) {
		if ((available_credit_map && (1 << lender_ac)) &&
		   (ctx->FIFO_credit[lender_ac] > 0)) {
			ctx->credits_borrowed[borrower_ac][lender_ac]++;
			ctx->FIFO_credit[lender_ac]--;
			rc = BCME_OK;
			break;
		}
	}

	return rc;
}

static int
_dhd_wlfc_interface_entry_update(void* state,
	ewlfc_mac_entry_action_t action, uint8 ifid, uint8 iftype, uint8* ea)
{
	athost_wl_status_info_t* ctx = (athost_wl_status_info_t*)state;
	wlfc_mac_descriptor_t* entry;

	if (ifid >= WLFC_MAX_IFNUM)
		return BCME_BADARG;

	entry = &ctx->destination_entries.interfaces[ifid];

	return _dhd_wlfc_mac_entry_update(ctx, entry, action, ifid, iftype, ea,
		_dhd_wlfc_ifpkt_fn, &ifid);
}

static int
_dhd_wlfc_BCMCCredit_support_update(void* state)
{
	athost_wl_status_info_t* ctx = (athost_wl_status_info_t*)state;

	ctx->bcmc_credit_supported = TRUE;
	return BCME_OK;
}

static int
_dhd_wlfc_FIFOcreditmap_update(void* state, uint8* credits)
{
	athost_wl_status_info_t* ctx = (athost_wl_status_info_t*)state;

	/* update the AC FIFO credit map */
	ctx->FIFO_credit[0] = credits[0];
	ctx->FIFO_credit[1] = credits[1];
	ctx->FIFO_credit[2] = credits[2];
	ctx->FIFO_credit[3] = credits[3];
	/* credit for bc/mc packets */
	ctx->FIFO_credit[4] = credits[4];
	/* credit for ATIM FIFO is not used yet. */
	ctx->FIFO_credit[5] = 0;
	return BCME_OK;
}

static int
_dhd_wlfc_handle_packet_commit(athost_wl_status_info_t* ctx, int ac,
    dhd_wlfc_commit_info_t *commit_info, f_commitpkt_t fcommit, void* commit_ctx)
{
	uint32 hslot;
	int	rc;

	/*
		if ac_fifo_credit_spent = 0

		This packet will not count against the FIFO credit.
		To ensure the txstatus corresponding to this packet
		does not provide an implied credit (default behavior)
		mark the packet accordingly.

		if ac_fifo_credit_spent = 1

		This is a normal packet and it counts against the FIFO
		credit count.
	*/
	DHD_PKTTAG_SETCREDITCHECK(PKTTAG(commit_info->p), commit_info->ac_fifo_credit_spent);
	rc = _dhd_wlfc_pretx_pktprocess(ctx, commit_info->mac_entry, commit_info->p,
	     commit_info->needs_hdr, &hslot);

	if (rc == BCME_OK) {
		DHD_PKTTAG_WLFCPKT_SET(PKTTAG(commit_info->p), 1);
		rc = fcommit(commit_ctx, commit_info->p);
		if (rc == BCME_OK) {
			ctx->stats.pkt2bus++;
			if (commit_info->ac_fifo_credit_spent || (ac == AC_COUNT)) {
				ctx->stats.send_pkts[ac]++;
				WLFC_HOST_FIFO_CREDIT_INC_SENTCTRS(ctx, ac);
			}
		} else if (commit_info->needs_hdr) {
			void *pout = NULL;
			/* pop hanger for delayed packet */
			_dhd_wlfc_hanger_poppkt(ctx->hanger, WLFC_PKTID_HSLOT_GET(DHD_PKTTAG_H2DTAG
					(PKTTAG(commit_info->p))), &pout, 1);
			ASSERT(commit_info->p == pout);
		}
	} else {
		ctx->stats.generic_error++;
	}

	if (rc != BCME_OK) {
		/*
		   pretx pkt process or bus commit has failed, rollback.
		   - remove wl-header for a delayed packet
		   - save wl-header header for suppressed packets
		   - reset credit check flag
		*/
		_dhd_wlfc_rollback_packet_toq(ctx, commit_info->p, commit_info->pkt_type, hslot);
		DHD_PKTTAG_SETCREDITCHECK(PKTTAG(commit_info->p), 0);
	}

	return rc;
}


static uint8
_dhd_wlfc_find_mac_desc_id_from_mac(dhd_pub_t *dhdp, uint8* ea)
{
	wlfc_mac_descriptor_t* table =
		((athost_wl_status_info_t*)dhdp->wlfc_state)->destination_entries.nodes;
	uint8 table_index;

	if (ea != NULL) {
		for (table_index = 0; table_index < WLFC_MAC_DESC_TABLE_SIZE; table_index++) {
			if ((memcmp(ea, &table[table_index].ea[0], ETHER_ADDR_LEN) == 0) &&
				table[table_index].occupied)
				return table_index;
		}
	}
	return WLFC_MAC_DESC_ID_INVALID;
}

static int
_dhd_wlfc_compressed_txstatus_update(dhd_pub_t *dhd, uint8* pkt_info, uint8 len)
{
	uint8 status_flag;
	uint32 status;
	int ret;
	int remove_from_hanger = 1;
	void*	pktbuf = NULL;
	uint8	fifo_id;
	uint8 count = 0, gen = 0;
	uint32 hslot;
	wlfc_mac_descriptor_t* entry = NULL;
	athost_wl_status_info_t* wlfc = (athost_wl_status_info_t*)dhd->wlfc_state;

	memcpy(&status, pkt_info, sizeof(uint32));
	status_flag = WL_TXSTATUS_GET_FLAGS(status);
	hslot = WLFC_PKTID_HSLOT_GET(status);
	gen = WLFC_PKTID_GEN(status);

	wlfc->stats.txstatus_in += len;

	if (status_flag == WLFC_CTL_PKTFLAG_DISCARD) {
		wlfc->stats.pkt_freed += len;
	} else if (status_flag == WLFC_CTL_PKTFLAG_D11SUPPRESS) {
		wlfc->stats.d11_suppress += len;
		remove_from_hanger = 0;
	} else if (status_flag == WLFC_CTL_PKTFLAG_WLSUPPRESS) {
		wlfc->stats.wl_suppress += len;
		remove_from_hanger = 0;
	} else if (status_flag == WLFC_CTL_PKTFLAG_TOSSED_BYWLC) {
		wlfc->stats.wlc_tossed_pkts += len;
	}

	while (count < len) {
		ret = _dhd_wlfc_hanger_poppkt(wlfc->hanger, hslot, &pktbuf, remove_from_hanger);
		if (ret != BCME_OK) {
			goto cont;
		}

		fifo_id = DHD_PKTTAG_FIFO(PKTTAG(pktbuf));

		entry = _dhd_wlfc_find_table_entry(wlfc, pktbuf);

		if (!remove_from_hanger) {
			/* this packet was suppressed */
			if (!entry->suppressed || (entry->generation != gen)) {
				if (!entry->suppressed) {
					entry->suppr_transit_count = entry->transit_count;
				}
				entry->suppressed = TRUE;
			}
			entry->generation = gen;
		}

#ifdef PROP_TXSTATUS_DEBUG
		{
			uint32 new_t = OSL_SYSUPTIME();
			uint32 old_t;
			uint32 delta;
			old_t = ((wlfc_hanger_t*)(wlfc->hanger))->items[hslot].push_time;


			wlfc->stats.latency_sample_count++;
			if (new_t > old_t)
				delta = new_t - old_t;
			else
				delta = 0xffffffff + new_t - old_t;
			wlfc->stats.total_status_latency += delta;
			wlfc->stats.latency_most_recent = delta;

			wlfc->stats.deltas[wlfc->stats.idx_delta++] = delta;
			if (wlfc->stats.idx_delta == sizeof(wlfc->stats.deltas)/sizeof(uint32))
				wlfc->stats.idx_delta = 0;
		}
#endif /* PROP_TXSTATUS_DEBUG */

		/* pick up the implicit credit from this packet */
		if (DHD_PKTTAG_CREDITCHECK(PKTTAG(pktbuf))) {
			if (wlfc->proptxstatus_mode == WLFC_FCMODE_IMPLIED_CREDIT) {

				int lender, credit_returned = 0; /* Note that borrower is fifo_id */

				/* Return credits to highest priority lender first */
				for (lender = AC_COUNT; lender >= 0; lender--)	{
					if (wlfc->credits_borrowed[fifo_id][lender] > 0) {
						wlfc->FIFO_credit[lender]++;
						wlfc->credits_borrowed[fifo_id][lender]--;
						credit_returned = 1;
						break;
					}
				}

				if (!credit_returned) {
					wlfc->FIFO_credit[fifo_id]++;
				}
			}
		} else {
			/*
			if this packet did not count against FIFO credit, it must have
			taken a requested_credit from the destination entry (for pspoll etc.)
			*/
			if (!DHD_PKTTAG_ONETIMEPKTRQST(PKTTAG(pktbuf)))
				entry->requested_credit++;
#ifdef PROP_TXSTATUS_DEBUG
			entry->dstncredit_acks++;
#endif
		}

		if ((status_flag == WLFC_CTL_PKTFLAG_D11SUPPRESS) ||
			(status_flag == WLFC_CTL_PKTFLAG_WLSUPPRESS)) {

			ret = _dhd_wlfc_enque_suppressed(wlfc, fifo_id, pktbuf);
			if (ret != BCME_OK) {
				/* delay q is full, drop this packet */
				DHD_WLFC_QMON_COMPLETE(entry);
				_dhd_wlfc_prec_drop(dhd, (fifo_id << 1) + 1, pktbuf, FALSE);
			} else {
				/* Mark suppressed to avoid a double free during wlfc cleanup */

				_dhd_wlfc_hanger_mark_suppressed(wlfc->hanger, hslot, gen);
			}
		} else {
			dhd_txcomplete(dhd, pktbuf, TRUE);
			DHD_WLFC_QMON_COMPLETE(entry);
			/* free the packet */
			PKTFREE(wlfc->osh, pktbuf, TRUE);
		}
		/* pkt back from firmware side */
		entry->transit_count--;
		if (entry->suppressed && (--entry->suppr_transit_count == 0)) {
			entry->suppressed = FALSE;
		}

cont:
		hslot = (hslot + 1) & WLFC_PKTID_HSLOT_MASK;
		count++;
	}
	return BCME_OK;
}

static int
_dhd_wlfc_fifocreditback_indicate(dhd_pub_t *dhd, uint8* credits)
{
	int i;
	athost_wl_status_info_t* wlfc = (athost_wl_status_info_t*)dhd->wlfc_state;
	for (i = 0; i < WLFC_CTL_VALUE_LEN_FIFO_CREDITBACK; i++) {
#ifdef PROP_TXSTATUS_DEBUG
		wlfc->stats.fifo_credits_back[i] += credits[i];
#endif
		/* update FIFO credits */
		if (wlfc->proptxstatus_mode == WLFC_FCMODE_EXPLICIT_CREDIT)
		{
			int lender; /* Note that borrower is i */

			/* Return credits to highest priority lender first */
			for (lender = AC_COUNT; (lender >= 0) && (credits[i] > 0); lender--) {
				if (wlfc->credits_borrowed[i][lender] > 0) {
					if (credits[i] >= wlfc->credits_borrowed[i][lender]) {
						credits[i] -= wlfc->credits_borrowed[i][lender];
						wlfc->FIFO_credit[lender] +=
						    wlfc->credits_borrowed[i][lender];
						wlfc->credits_borrowed[i][lender] = 0;
					}
					else {
						wlfc->credits_borrowed[i][lender] -= credits[i];
						wlfc->FIFO_credit[lender] += credits[i];
						credits[i] = 0;
					}
				}
			}

			/* If we have more credits left over, these must belong to the AC */
			if (credits[i] > 0) {
				wlfc->FIFO_credit[i] += credits[i];
			}
		}
	}

	return BCME_OK;
}

static int
_dhd_wlfc_dbg_senum_check(dhd_pub_t *dhd, uint8 *value)
{
	uint32 timestamp;

	(void)dhd;

	bcopy(&value[2], &timestamp, sizeof(uint32));
	DHD_INFO(("RXPKT: SEQ: %d, timestamp %d\n", value[1], timestamp));
	return BCME_OK;
}

static int
_dhd_wlfc_rssi_indicate(dhd_pub_t *dhd, uint8* rssi)
{
	(void)dhd;
	(void)rssi;
	return BCME_OK;
}

static int
_dhd_wlfc_mac_table_update(dhd_pub_t *dhd, uint8* value, uint8 type)
{
	int rc;
	athost_wl_status_info_t* wlfc = (athost_wl_status_info_t*)dhd->wlfc_state;
	wlfc_mac_descriptor_t* table;
	uint8 existing_index;
	uint8 table_index;
	uint8 ifid;
	uint8* ea;

	WLFC_DBGMESG(("%s(), mac [%02x:%02x:%02x:%02x:%02x:%02x],%s,idx:%d,id:0x%02x\n",
		__FUNCTION__, value[2], value[3], value[4], value[5], value[6], value[7],
		((type == WLFC_CTL_TYPE_MACDESC_ADD) ? "ADD":"DEL"),
		WLFC_MAC_DESC_GET_LOOKUP_INDEX(value[0]), value[0]));

	table = wlfc->destination_entries.nodes;
	table_index = WLFC_MAC_DESC_GET_LOOKUP_INDEX(value[0]);
	ifid = value[1];
	ea = &value[2];

	if (type == WLFC_CTL_TYPE_MACDESC_ADD) {
		existing_index = _dhd_wlfc_find_mac_desc_id_from_mac(dhd, &value[2]);
		if ((existing_index != WLFC_MAC_DESC_ID_INVALID) &&
			(existing_index != table_index) && table[existing_index].occupied) {
			/*
			there is an existing different entry, free the old one
			and move it to new index if necessary.
			*/
			rc = _dhd_wlfc_mac_entry_update(wlfc, &table[existing_index],
				eWLFC_MAC_ENTRY_ACTION_DEL, table[existing_index].interface_id,
				table[existing_index].iftype, NULL, _dhd_wlfc_entrypkt_fn,
				&table[existing_index]);
		}

		if (!table[table_index].occupied) {
			/* this new MAC entry does not exist, create one */
			table[table_index].mac_handle = value[0];
			rc = _dhd_wlfc_mac_entry_update(wlfc, &table[table_index],
				eWLFC_MAC_ENTRY_ACTION_ADD, ifid,
				wlfc->destination_entries.interfaces[ifid].iftype,
				ea, NULL, NULL);
		} else {
			/* the space should have been empty, but it's not */
			wlfc->stats.mac_update_failed++;
		}
	}

	if (type == WLFC_CTL_TYPE_MACDESC_DEL) {
		if (table[table_index].occupied) {
				rc = _dhd_wlfc_mac_entry_update(wlfc, &table[table_index],
					eWLFC_MAC_ENTRY_ACTION_DEL, ifid,
					wlfc->destination_entries.interfaces[ifid].iftype,
					ea, _dhd_wlfc_entrypkt_fn, &table[table_index]);
		} else {
			/* the space should have been occupied, but it's not */
			wlfc->stats.mac_update_failed++;
		}
	}
	BCM_REFERENCE(rc);
	return BCME_OK;
}

static int
_dhd_wlfc_psmode_update(dhd_pub_t *dhd, uint8* value, uint8 type)
{
	/* Handle PS on/off indication */
	athost_wl_status_info_t* wlfc = (athost_wl_status_info_t*)dhd->wlfc_state;
	wlfc_mac_descriptor_t* table;
	wlfc_mac_descriptor_t* desc;
	uint8 mac_handle = value[0];
	int i;

	table = wlfc->destination_entries.nodes;
	desc = &table[WLFC_MAC_DESC_GET_LOOKUP_INDEX(mac_handle)];
	if (desc->occupied) {
		/* a fresh PS mode should wipe old ps credits? */
		desc->requested_credit = 0;
		if (type == WLFC_CTL_TYPE_MAC_OPEN) {
			desc->state = WLFC_STATE_OPEN;
			desc->ac_bitmap = 0xff;
			DHD_WLFC_CTRINC_MAC_OPEN(desc);
		}
		else {
			desc->state = WLFC_STATE_CLOSE;
			DHD_WLFC_CTRINC_MAC_CLOSE(desc);
			/*
			Indicate to firmware if there is any traffic pending.
			*/
			for (i = AC_BE; i < AC_COUNT; i++) {
				_dhd_wlfc_traffic_pending_check(wlfc, desc, i);
			}
		}
	}
	else {
		wlfc->stats.psmode_update_failed++;
	}
	return BCME_OK;
}

static int
_dhd_wlfc_interface_update(dhd_pub_t *dhd, uint8* value, uint8 type)
{
	/* Handle PS on/off indication */
	athost_wl_status_info_t* wlfc = (athost_wl_status_info_t*)dhd->wlfc_state;
	wlfc_mac_descriptor_t* table;
	uint8 if_id = value[0];

	if (if_id < WLFC_MAX_IFNUM) {
		table = wlfc->destination_entries.interfaces;
		if (table[if_id].occupied) {
			if (type == WLFC_CTL_TYPE_INTERFACE_OPEN) {
				table[if_id].state = WLFC_STATE_OPEN;
				/* WLFC_DBGMESG(("INTERFACE[%d] OPEN\n", if_id)); */
			}
			else {
				table[if_id].state = WLFC_STATE_CLOSE;
				/* WLFC_DBGMESG(("INTERFACE[%d] CLOSE\n", if_id)); */
			}
			return BCME_OK;
		}
	}
	wlfc->stats.interface_update_failed++;

	return BCME_OK;
}

static int
_dhd_wlfc_credit_request(dhd_pub_t *dhd, uint8* value)
{
	athost_wl_status_info_t* wlfc = (athost_wl_status_info_t*)dhd->wlfc_state;
	wlfc_mac_descriptor_t* table;
	wlfc_mac_descriptor_t* desc;
	uint8 mac_handle;
	uint8 credit;

	table = wlfc->destination_entries.nodes;
	mac_handle = value[1];
	credit = value[0];

	desc = &table[WLFC_MAC_DESC_GET_LOOKUP_INDEX(mac_handle)];
	if (desc->occupied) {
		desc->requested_credit = credit;

		desc->ac_bitmap = value[2] & (~(1<<AC_COUNT));
	}
	else {
		wlfc->stats.credit_request_failed++;
	}
	return BCME_OK;
}

static int
_dhd_wlfc_packet_request(dhd_pub_t *dhd, uint8* value)
{
	athost_wl_status_info_t* wlfc = (athost_wl_status_info_t*)dhd->wlfc_state;
	wlfc_mac_descriptor_t* table;
	wlfc_mac_descriptor_t* desc;
	uint8 mac_handle;
	uint8 packet_count;

	table = wlfc->destination_entries.nodes;
	mac_handle = value[1];
	packet_count = value[0];

	desc = &table[WLFC_MAC_DESC_GET_LOOKUP_INDEX(mac_handle)];
	if (desc->occupied) {
		desc->requested_packet = packet_count;

		desc->ac_bitmap = value[2] & (~(1<<AC_COUNT));
	}
	else {
		wlfc->stats.packet_request_failed++;
	}
	return BCME_OK;
}

static void
_dhd_wlfc_reorderinfo_indicate(uint8 *val, uint8 len, uchar *info_buf, uint *info_len)
{
	if (info_len) {
		if (info_buf) {
			bcopy(val, info_buf, len);
			*info_len = len;
		}
		else
			*info_len = 0;
	}
}

static int
_dhd_wlfc_enable(dhd_pub_t *dhd)
{
	int i;
	athost_wl_status_info_t* wlfc;

	if (!dhd->wlfc_enabled || dhd->wlfc_state)
		return BCME_OK;

	/* allocate space to track txstatus propagated from firmware */
	dhd->wlfc_state = MALLOC(dhd->osh, sizeof(athost_wl_status_info_t));
	if (dhd->wlfc_state == NULL)
		return BCME_NOMEM;


	/* initialize state space */
	wlfc = (athost_wl_status_info_t*)dhd->wlfc_state;
	memset(wlfc, 0, sizeof(athost_wl_status_info_t));

	/* remember osh & dhdp */
	wlfc->osh = dhd->osh;
	wlfc->dhdp = dhd;

	wlfc->hanger = _dhd_wlfc_hanger_create(dhd->osh, WLFC_HANGER_MAXITEMS);
	if (wlfc->hanger == NULL) {
		MFREE(dhd->osh, dhd->wlfc_state, sizeof(athost_wl_status_info_t));
		dhd->wlfc_state = NULL;
		return BCME_NOMEM;
	}

	/* initialize all interfaces to accept traffic */
	for (i = 0; i < WLFC_MAX_IFNUM; i++) {
		wlfc->hostif_flow_state[i] = OFF;
	}

	wlfc->destination_entries.other.state = WLFC_STATE_OPEN;
	/* bc/mc FIFO is always open [credit aside], i.e. b[5] */
	wlfc->destination_entries.other.ac_bitmap = 0x1f;
	wlfc->destination_entries.other.interface_id = 0;
	pktq_init(&wlfc->destination_entries.other.psq, WLFC_PSQ_PREC_COUNT, WLFC_PSQ_LEN);

	wlfc->proptxstatus_mode = WLFC_FCMODE_EXPLICIT_CREDIT;

	wlfc->allow_credit_borrow = 0;
	wlfc->borrow_defer_timestamp = 0;

	if (dhd->plat_enable)
		dhd->plat_enable((void *)dhd);

	return BCME_OK;
}


/*
 * public functions
 */

int
dhd_wlfc_parse_header_info(dhd_pub_t *dhd, void* pktbuf, int tlv_hdr_len, uchar *reorder_info_buf,
	uint *reorder_info_len)
{
	uint8 type, len;
	uint8* value;
	uint8* tmpbuf;
	uint16 remainder = tlv_hdr_len;
	uint16 processed = 0;
	athost_wl_status_info_t* wlfc;

	if ((dhd == NULL) || (pktbuf == NULL)) {
		WLFC_DBGMESG(("Error: %s():%d\n", __FUNCTION__, __LINE__));
		return BCME_BADARG;
	}

	dhd_os_wlfc_block(dhd);

	wlfc = (athost_wl_status_info_t*)dhd->wlfc_state;
	if (!wlfc || (wlfc->proptxstatus_mode == WLFC_FCMODE_NONE)) {
		dhd_os_wlfc_unblock(dhd);
		return WLFC_UNSUPPORTED;
	}

	tmpbuf = (uint8*)PKTDATA(dhd->osh, pktbuf);

	if (remainder) {
		while ((processed < (WLFC_MAX_PENDING_DATALEN * 2)) && (remainder > 0)) {
			type = tmpbuf[processed];
			if (type == WLFC_CTL_TYPE_FILLER) {
				remainder -= 1;
				processed += 1;
				continue;
			}

			len  = tmpbuf[processed + 1];
			value = &tmpbuf[processed + 2];

			if (remainder < (2 + len))
				break;

			remainder -= 2 + len;
			processed += 2 + len;
			if (type == WLFC_CTL_TYPE_TXSTATUS)
				_dhd_wlfc_compressed_txstatus_update(dhd, value, 1);
			if (type == WLFC_CTL_TYPE_COMP_TXSTATUS)
				_dhd_wlfc_compressed_txstatus_update(dhd, value, value[4]);

			else if (type == WLFC_CTL_TYPE_HOST_REORDER_RXPKTS)
				_dhd_wlfc_reorderinfo_indicate(value, len, reorder_info_buf,
					reorder_info_len);
			else if (type == WLFC_CTL_TYPE_FIFO_CREDITBACK)
				_dhd_wlfc_fifocreditback_indicate(dhd, value);

			else if (type == WLFC_CTL_TYPE_RSSI)
				_dhd_wlfc_rssi_indicate(dhd, value);

			else if (type == WLFC_CTL_TYPE_MAC_REQUEST_CREDIT)
				_dhd_wlfc_credit_request(dhd, value);

			else if (type == WLFC_CTL_TYPE_MAC_REQUEST_PACKET)
				_dhd_wlfc_packet_request(dhd, value);

			else if ((type == WLFC_CTL_TYPE_MAC_OPEN) ||
				(type == WLFC_CTL_TYPE_MAC_CLOSE))
				_dhd_wlfc_psmode_update(dhd, value, type);

			else if ((type == WLFC_CTL_TYPE_MACDESC_ADD) ||
				(type == WLFC_CTL_TYPE_MACDESC_DEL))
				_dhd_wlfc_mac_table_update(dhd, value, type);

			else if (type == WLFC_CTL_TYPE_TRANS_ID)
				_dhd_wlfc_dbg_senum_check(dhd, value);

			else if ((type == WLFC_CTL_TYPE_INTERFACE_OPEN) ||
				(type == WLFC_CTL_TYPE_INTERFACE_CLOSE)) {
				_dhd_wlfc_interface_update(dhd, value, type);
			}
		}
		if (remainder != 0) {
			/* trouble..., something is not right */
			wlfc->stats.tlv_parse_failed++;
		}
	}

	wlfc->stats.dhd_hdrpulls++;

	dhd_os_wlfc_unblock(dhd);
	return BCME_OK;
}

int
dhd_wlfc_commit_packets(dhd_pub_t *dhdp, f_commitpkt_t fcommit, void* commit_ctx, void *pktbuf,
	bool need_toggle_host_if)
{
	int ac;
	int rc = BCME_OK;
	dhd_wlfc_commit_info_t  commit_info;
	athost_wl_status_info_t* ctx;
	int credit_count = 0;
	int bus_retry_count = 0;
	uint8 ac_available = 0;  /* Bitmask for 4 ACs + BC/MC */
	/* -1 for no traffic, 0 for multi-ac, 1-5 for 4 ACs + BC/MC */
	int only_traffic_ac = WLFC_NO_TRAFFIC;

	if ((dhdp == NULL) || (fcommit == NULL)) {
		WLFC_DBGMESG(("Error: %s():%d\n", __FUNCTION__, __LINE__));
		return BCME_BADARG;
	}

	dhd_os_wlfc_block(dhdp);

	ctx = (athost_wl_status_info_t*)dhdp->wlfc_state;
	if (!ctx || (ctx->proptxstatus_mode == WLFC_FCMODE_NONE)) {
		if (pktbuf) {
			DHD_PKTTAG_WLFCPKT_SET(PKTTAG(pktbuf), 0);
		}
		rc = WLFC_UNSUPPORTED;
		goto exit2;
	}


	memset(&commit_info, 0, sizeof(commit_info));

	/*
	Commit packets for regular AC traffic. Higher priority first.
	First, use up FIFO credits available to each AC. Based on distribution
	and credits left, borrow from other ACs as applicable

	-NOTE:
	If the bus between the host and firmware is overwhelmed by the
	traffic from host, it is possible that higher priority traffic
	starves the lower priority queue. If that occurs often, we may
	have to employ weighted round-robin or ucode scheme to avoid
	low priority packet starvation.
	*/

	if (pktbuf) {
		ac = DHD_PKTTAG_FIFO(PKTTAG(pktbuf));
		/* en-queue the packets to respective queue. */
		rc = _dhd_wlfc_enque_delayq(ctx, pktbuf, ac);
	}

	for (ac = AC_COUNT; ac >= 0; ac--) {

		bool bQueueIdle = TRUE;

		while (1) {
			/* packets from delayQ with less priority are fresh and
			 * they'd need header and have no MAC entry
			 */
			commit_info.needs_hdr = 1;
			commit_info.mac_entry = NULL;
			commit_info.p = _dhd_wlfc_deque_delayedq(ctx, ac,
				&(commit_info.ac_fifo_credit_spent),
				&(commit_info.needs_hdr),
				&(commit_info.mac_entry));
			commit_info.pkt_type = (commit_info.needs_hdr) ? eWLFC_PKTTYPE_DELAYED :
				eWLFC_PKTTYPE_SUPPRESSED;

			if (commit_info.p == NULL)
				break;

			bQueueIdle = FALSE;
			if (only_traffic_ac == WLFC_NO_TRAFFIC) {
				/* first ac traffic */
				only_traffic_ac = ac + 1;
			} else if (only_traffic_ac != (ac +1)) {
				/* multi-ac traffic */
				only_traffic_ac = WLFC_MULTI_TRAFFIC;
			}

			if (ctx->FIFO_credit[ac] < commit_info.ac_fifo_credit_spent) {
				/* not enough credit, requeue the packet and quit */
				int fifo_id;

				fifo_id = ac << 1;
				if (commit_info.pkt_type == eWLFC_PKTTYPE_SUPPRESSED)
					fifo_id += 1;
				if (_dhd_wlfc_prec_enq_with_drop(dhdp, &commit_info.mac_entry->psq,
					commit_info.p, fifo_id, TRUE) == FALSE) {
					_dhd_wlfc_prec_drop(dhdp, fifo_id, commit_info.p, FALSE);
				}
				break;
			}

			/* here we can ensure have credit or no credit needed */
			rc = _dhd_wlfc_handle_packet_commit(ctx, ac, &commit_info, fcommit,
				commit_ctx);

			/* Bus commits may fail (e.g. flow control); abort after retries */
			if (rc == BCME_OK) {
				if (commit_info.ac_fifo_credit_spent)
					ctx->FIFO_credit[ac]--;
			} else {
				bus_retry_count++;
				if (bus_retry_count >= BUS_RETRIES) {
					DHD_ERROR(("%s: bus error %d\n", __FUNCTION__, rc));
					goto exit;
				}
			}
		}


		/* If no pkts can be dequed, the credit can be borrowed */
		if (bQueueIdle) {
			ac_available |= (1 << ac);
			credit_count += ctx->FIFO_credit[ac];
		}
	}

	if (only_traffic_ac == WLFC_NO_TRAFFIC) {
		/* no packets to send, skip borrow logic */
		rc = BCME_OK;
		goto exit;
	}

	/* We borrow if no other traffic seen for DEFER_PERIOD */
	if (only_traffic_ac > 0) {
		if (ctx->allow_credit_borrow == only_traffic_ac) {
			ac = ctx->allow_credit_borrow - 1;
		} else {
			uint32 delta;
			uint32 curr_t = OSL_SYSUPTIME();

			if (curr_t > ctx->borrow_defer_timestamp)
				delta = curr_t - ctx->borrow_defer_timestamp;
			else
				delta = 0xffffffff - ctx->borrow_defer_timestamp + curr_t;

			if (delta >= WLFC_BORROW_DEFER_PERIOD_MS) {
				/* Reset borrow but defer to next iteration (defensive borrowing) */
				ctx->allow_credit_borrow = only_traffic_ac;
				ctx->borrow_defer_timestamp = 0;
			}
			rc = BCME_OK;
			goto exit;
		}
	} else {
		/* If we have multiple AC traffic, turn off borrowing, mark time and bail out */
		ctx->allow_credit_borrow = 0;
		ctx->borrow_defer_timestamp = OSL_SYSUPTIME();
		rc = BCME_OK;
		goto exit;
	}

	/* At this point, borrow all credits only for "ac" (which should be set above to AC_BE)
	   Generically use "ac" only in case we extend to all ACs in future
	   */
	for (; (credit_count > 0);) {

		commit_info.p = _dhd_wlfc_deque_delayedq(ctx, ac,
		                &(commit_info.ac_fifo_credit_spent),
		                &(commit_info.needs_hdr),
		                &(commit_info.mac_entry));
		if (commit_info.p == NULL)
			break;

		commit_info.pkt_type = (commit_info.needs_hdr) ? eWLFC_PKTTYPE_DELAYED :
			eWLFC_PKTTYPE_SUPPRESSED;

		rc = _dhd_wlfc_handle_packet_commit(ctx, ac, &commit_info,
		     fcommit, commit_ctx);

		/* Bus commits may fail (e.g. flow control); abort after retries */
		if (rc == BCME_OK) {
			if (commit_info.ac_fifo_credit_spent) {
				(void) _dhd_wlfc_borrow_credit(ctx, ac_available, ac);
				credit_count--;
			}
		} else {
			bus_retry_count++;
			if (bus_retry_count >= BUS_RETRIES) {
				DHD_ERROR(("%s: bus error %d\n", __FUNCTION__, rc));
				goto exit;
			}
		}
	}

exit:
	if (need_toggle_host_if && ctx->toggle_host_if) {
		ctx->toggle_host_if = 0;
	}

exit2:
	dhd_os_wlfc_unblock(dhdp);
	return rc;
}

int
dhd_wlfc_txcomplete(dhd_pub_t *dhd, void *txp, bool success)
{
	athost_wl_status_info_t* wlfc;
	void* pout = NULL;
	int fifo_id;

	if ((dhd == NULL) || (txp == NULL)) {
		WLFC_DBGMESG(("Error: %s():%d\n", __FUNCTION__, __LINE__));
		return BCME_BADARG;
	}

	dhd_os_wlfc_block(dhd);

	wlfc = (athost_wl_status_info_t*)dhd->wlfc_state;
	if (!wlfc || (wlfc->proptxstatus_mode == WLFC_FCMODE_NONE)) {
		dhd_os_wlfc_unblock(dhd);
		return WLFC_UNSUPPORTED;
	}

	if (DHD_PKTTAG_SIGNALONLY(PKTTAG(txp))) {
#ifdef PROP_TXSTATUS_DEBUG
		wlfc->stats.signal_only_pkts_freed++;
#endif
		/* is this a signal-only packet? */
		if (success)
			PKTFREE(wlfc->osh, txp, TRUE);
		dhd_os_wlfc_unblock(dhd);
		return BCME_OK;
	}

	if (!success) {
		WLFC_DBGMESG(("At: %s():%d, bus_complete() failure for %p, htod_tag:0x%08x\n",
			__FUNCTION__, __LINE__, txp, DHD_PKTTAG_H2DTAG(PKTTAG(txp))));
		_dhd_wlfc_hanger_poppkt(wlfc->hanger, WLFC_PKTID_HSLOT_GET(DHD_PKTTAG_H2DTAG
			(PKTTAG(txp))), &pout, 1);
		ASSERT(txp == pout);

		/* indicate failure and free the packet */
		dhd_txcomplete(dhd, txp, FALSE);

		/* return the credit, if necessary */
		if (DHD_PKTTAG_CREDITCHECK(PKTTAG(txp))) {
			int lender, credit_returned = 0; /* Note that borrower is fifo_id */

			fifo_id = DHD_PKTTAG_FIFO(PKTTAG(txp));

			/* Return credits to highest priority lender first */
			for (lender = AC_COUNT; lender >= 0; lender--) {
				if (wlfc->credits_borrowed[fifo_id][lender] > 0) {
					wlfc->FIFO_credit[lender]++;
					wlfc->credits_borrowed[fifo_id][lender]--;
					credit_returned = 1;
					break;
				}
			}

			if (!credit_returned) {
				wlfc->FIFO_credit[fifo_id]++;
			}
		}

		PKTFREE(wlfc->osh, txp, TRUE);
	} else {
		/* bus confirmed pkt went to firmware side */
		wlfc_mac_descriptor_t *entry = _dhd_wlfc_find_table_entry(wlfc, txp);
		entry->transit_count++;
	}

	dhd_os_wlfc_unblock(dhd);
	return BCME_OK;
}

int
dhd_wlfc_init(dhd_pub_t *dhd)
{
	char iovbuf[12]; /* Room for "tlv" + '\0' + parameter */
	/* enable all signals & indicate host proptxstatus logic is active */
	uint32 tlv = dhd->wlfc_enabled ?
		WLFC_FLAGS_RSSI_SIGNALS |
		WLFC_FLAGS_XONXOFF_SIGNALS |
		WLFC_FLAGS_CREDIT_STATUS_SIGNALS |
		WLFC_FLAGS_HOST_PROPTXSTATUS_ACTIVE |
		WLFC_FLAGS_HOST_RXRERODER_ACTIVE : 0;
		/* WLFC_FLAGS_HOST_PROPTXSTATUS_ACTIVE | WLFC_FLAGS_HOST_RXRERODER_ACTIVE : 0; */


	/*
	try to enable/disable signaling by sending "tlv" iovar. if that fails,
	fallback to no flow control? Print a message for now.
	*/

	/* enable proptxtstatus signaling by default */
	bcm_mkiovar("tlv", (char *)&tlv, 4, iovbuf, sizeof(iovbuf));
	if (dhd_wl_ioctl_cmd(dhd, WLC_SET_VAR, iovbuf, sizeof(iovbuf), TRUE, 0) < 0) {
		DHD_ERROR(("dhd_wlfc_init(): failed to enable/disable bdcv2 tlv signaling\n"));
	}
	else {
		/*
		Leaving the message for now, it should be removed after a while; once
		the tlv situation is stable.
		*/
		DHD_ERROR(("dhd_wlfc_init(): successfully %s bdcv2 tlv signaling, %d\n",
			dhd->wlfc_enabled?"enabled":"disabled", tlv));
	}
	return BCME_OK;
}

int
dhd_wlfc_cleanup_txq(dhd_pub_t *dhd, f_processpkt_t fn, void *arg)
{
	athost_wl_status_info_t* wlfc;

	if (dhd == NULL) {
		WLFC_DBGMESG(("Error: %s():%d\n", __FUNCTION__, __LINE__));
		return BCME_BADARG;
	}

	dhd_os_wlfc_block(dhd);

	wlfc = (athost_wl_status_info_t*)dhd->wlfc_state;
	if (!wlfc || (wlfc->proptxstatus_mode == WLFC_FCMODE_NONE)) {
		dhd_os_wlfc_unblock(dhd);
		return WLFC_UNSUPPORTED;
	}

	_dhd_wlfc_cleanup_txq(dhd, fn, arg);

	dhd_os_wlfc_unblock(dhd);

	return BCME_OK;
}

/* release all packet resources */
int
dhd_wlfc_cleanup(dhd_pub_t *dhd, f_processpkt_t fn, void *arg)
{
	athost_wl_status_info_t* wlfc;

	if (dhd == NULL) {
		WLFC_DBGMESG(("Error: %s():%d\n", __FUNCTION__, __LINE__));
		return BCME_BADARG;
	}

	dhd_os_wlfc_block(dhd);

	wlfc = (athost_wl_status_info_t*)dhd->wlfc_state;
	if (!wlfc || (wlfc->proptxstatus_mode == WLFC_FCMODE_NONE)) {
		dhd_os_wlfc_unblock(dhd);
		return WLFC_UNSUPPORTED;
	}

	_dhd_wlfc_cleanup(dhd, fn, arg);

	dhd_os_wlfc_unblock(dhd);

	return BCME_OK;
}

int
dhd_wlfc_deinit(dhd_pub_t *dhd)
{
	/* cleanup all psq related resources */
	athost_wl_status_info_t* wlfc;

	if (dhd == NULL) {
		WLFC_DBGMESG(("Error: %s():%d\n", __FUNCTION__, __LINE__));
		return BCME_BADARG;
	}

	dhd_os_wlfc_block(dhd);

	wlfc = (athost_wl_status_info_t*)dhd->wlfc_state;
	if (!wlfc || (wlfc->proptxstatus_mode == WLFC_FCMODE_NONE)) {
		dhd_os_wlfc_unblock(dhd);
		return WLFC_UNSUPPORTED;
	}

#ifdef PROP_TXSTATUS_DEBUG
	{
		int i;
		wlfc_hanger_t* h = (wlfc_hanger_t*)wlfc->hanger;
		for (i = 0; i < h->max_items; i++) {
			if (h->items[i].state != WLFC_HANGER_ITEM_STATE_FREE) {
				WLFC_DBGMESG(("%s() pkt[%d] = 0x%p, FIFO_credit_used:%d\n",
					__FUNCTION__, i, h->items[i].pkt,
					DHD_PKTTAG_CREDITCHECK(PKTTAG(h->items[i].pkt))));
			}
		}
	}
#endif

	_dhd_wlfc_cleanup(dhd, NULL, NULL);

	/* delete hanger */
	_dhd_wlfc_hanger_delete(dhd->osh, wlfc->hanger);

	/* free top structure */
	MFREE(dhd->osh, dhd->wlfc_state, sizeof(athost_wl_status_info_t));
	dhd->wlfc_state = NULL;

	dhd_os_wlfc_unblock(dhd);

	if (dhd->plat_deinit)
		dhd->plat_deinit((void *)dhd);
	return BCME_OK;
}

int dhd_wlfc_interface_event(dhd_pub_t *dhdp,
	ewlfc_mac_entry_action_t action, uint8 ifid, uint8 iftype, uint8* ea)
{
	athost_wl_status_info_t* wlfc;
	int rc;

	if (dhdp == NULL) {
		WLFC_DBGMESG(("Error: %s():%d\n", __FUNCTION__, __LINE__));
		return BCME_BADARG;
	}

	dhd_os_wlfc_block(dhdp);

	wlfc = (athost_wl_status_info_t*)dhdp->wlfc_state;
	if (!wlfc || (wlfc->proptxstatus_mode == WLFC_FCMODE_NONE)) {
		dhd_os_wlfc_unblock(dhdp);
		return WLFC_UNSUPPORTED;
	}

	rc = _dhd_wlfc_interface_entry_update(wlfc, action, ifid, iftype, ea);

	dhd_os_wlfc_unblock(dhdp);
	return rc;
}

int dhd_wlfc_FIFOcreditmap_event(dhd_pub_t *dhdp, uint8* event_data)
{
	athost_wl_status_info_t* wlfc;
	int rc;

	if (dhdp == NULL) {
		WLFC_DBGMESG(("Error: %s():%d\n", __FUNCTION__, __LINE__));
		return BCME_BADARG;
	}

	dhd_os_wlfc_block(dhdp);

	wlfc = (athost_wl_status_info_t*)dhdp->wlfc_state;
	if (!wlfc || (wlfc->proptxstatus_mode == WLFC_FCMODE_NONE)) {
		dhd_os_wlfc_unblock(dhdp);
		return WLFC_UNSUPPORTED;
	}

	rc = _dhd_wlfc_FIFOcreditmap_update(wlfc, event_data);

	dhd_os_wlfc_unblock(dhdp);
	return rc;
}

int dhd_wlfc_BCMCCredit_support_event(dhd_pub_t *dhdp)
{
	athost_wl_status_info_t* wlfc;
	int rc;

	if (dhdp == NULL) {
		WLFC_DBGMESG(("Error: %s():%d\n", __FUNCTION__, __LINE__));
		return BCME_BADARG;
	}

	dhd_os_wlfc_block(dhdp);

	wlfc = (athost_wl_status_info_t*)dhdp->wlfc_state;
	if (!wlfc || (wlfc->proptxstatus_mode == WLFC_FCMODE_NONE)) {
		dhd_os_wlfc_unblock(dhdp);
		return WLFC_UNSUPPORTED;
	}

	rc = _dhd_wlfc_BCMCCredit_support_update(wlfc);

	dhd_os_wlfc_unblock(dhdp);
	return rc;
}

int dhd_wlfc_event(dhd_pub_t *dhdp)
{
	int rc;

	if (dhdp == NULL) {
		WLFC_DBGMESG(("Error: %s():%d\n", __FUNCTION__, __LINE__));
		return BCME_BADARG;
	}

	dhd_os_wlfc_block(dhdp);
	rc = _dhd_wlfc_enable(dhdp);
	dhd_os_wlfc_unblock(dhdp);

	return rc;
}

int
dhd_wlfc_dump(dhd_pub_t *dhdp, struct bcmstrbuf *strbuf)
{
	int i;
	uint8* ea;
	athost_wl_status_info_t* wlfc;
	wlfc_hanger_t* h;
	wlfc_mac_descriptor_t* mac_table;
	wlfc_mac_descriptor_t* interfaces;
	char* iftypes[] = {"STA", "AP", "WDS", "p2pGO", "p2pCL"};

	if (!dhdp || !strbuf) {
		WLFC_DBGMESG(("Error: %s():%d\n", __FUNCTION__, __LINE__));
		return BCME_BADARG;
	}

	dhd_os_wlfc_block(dhdp);

	wlfc = (athost_wl_status_info_t*)dhdp->wlfc_state;
	if (!wlfc) {
		dhd_os_wlfc_unblock(dhdp);
		bcm_bprintf(strbuf, "wlfc not initialized yet\n");
		return WLFC_UNSUPPORTED;
	}

	h = (wlfc_hanger_t*)wlfc->hanger;
	if (h == NULL) {
		bcm_bprintf(strbuf, "wlfc-hanger not initialized yet\n");
	}

	mac_table = wlfc->destination_entries.nodes;
	interfaces = wlfc->destination_entries.interfaces;
	bcm_bprintf(strbuf, "---- wlfc stats ----\n");
	if (h) {
		bcm_bprintf(strbuf, "wlfc hanger (pushed,popped,f_push,"
			"f_pop,f_slot, pending) = (%d,%d,%d,%d,%d,%d)\n",
			h->pushed,
			h->popped,
			h->failed_to_push,
			h->failed_to_pop,
			h->failed_slotfind,
			(h->pushed - h->popped));
	}

	bcm_bprintf(strbuf, "wlfc fail(tlv,credit_rqst,mac_update,psmode_update), "
		"(dq_full,rollback_fail) = (%d,%d,%d,%d), (%d,%d)\n",
		wlfc->stats.tlv_parse_failed,
		wlfc->stats.credit_request_failed,
		wlfc->stats.mac_update_failed,
		wlfc->stats.psmode_update_failed,
		wlfc->stats.delayq_full_error,
		wlfc->stats.rollback_failed);

	bcm_bprintf(strbuf, "PKTS (credit,sent,drop) "
		"(AC0[%d,%d,%d,%d],AC1[%d,%d,%d,%d],AC2[%d,%d,%d,%d],"
		"AC3[%d,%d,%d,%d],BC_MC[%d,%d,%d,%d])\n",
		wlfc->FIFO_credit[0], wlfc->stats.send_pkts[0],
		wlfc->stats.drop_pkts[0], wlfc->stats.drop_pkts[1],
		wlfc->FIFO_credit[1], wlfc->stats.send_pkts[1],
		wlfc->stats.drop_pkts[2], wlfc->stats.drop_pkts[3],
		wlfc->FIFO_credit[2], wlfc->stats.send_pkts[2],
		wlfc->stats.drop_pkts[4], wlfc->stats.drop_pkts[5],
		wlfc->FIFO_credit[3], wlfc->stats.send_pkts[3],
		wlfc->stats.drop_pkts[6], wlfc->stats.drop_pkts[7],
		wlfc->FIFO_credit[4], wlfc->stats.send_pkts[4],
		wlfc->stats.drop_pkts[8], wlfc->stats.drop_pkts[9]);

	bcm_bprintf(strbuf, "\n");
	for (i = 0; i < WLFC_MAX_IFNUM; i++) {
		if (interfaces[i].occupied) {
			char* iftype_desc;

			if (interfaces[i].iftype > WLC_E_IF_ROLE_P2P_CLIENT)
				iftype_desc = "<Unknown";
			else
				iftype_desc = iftypes[interfaces[i].iftype];

			ea = interfaces[i].ea;
			bcm_bprintf(strbuf, "INTERFACE[%d].ea = "
				"[%02x:%02x:%02x:%02x:%02x:%02x], if:%d, type: %s "
				"netif_flow_control:%s\n", i,
				ea[0], ea[1], ea[2], ea[3], ea[4], ea[5],
				interfaces[i].interface_id,
				iftype_desc, ((wlfc->hostif_flow_state[i] == OFF)
				? " OFF":" ON"));

			bcm_bprintf(strbuf, "INTERFACE[%d].PSQ(len,state,credit),(trans,supp_trans)"
				"= (%d,%s,%d),(%d,%d)\n",
				i,
				interfaces[i].psq.len,
				((interfaces[i].state ==
				WLFC_STATE_OPEN) ? "OPEN":"CLOSE"),
				interfaces[i].requested_credit,
				interfaces[i].transit_count, interfaces[i].suppr_transit_count);

			bcm_bprintf(strbuf, "INTERFACE[%d].PSQ"
				"(ac0,sup0),(ac1,sup1),(ac2,sup2),(ac3,sup3) = "
				"(%d,%d),(%d,%d),(%d,%d),(%d,%d)\n",
				i,
				interfaces[i].psq.q[0].len,
				interfaces[i].psq.q[1].len,
				interfaces[i].psq.q[2].len,
				interfaces[i].psq.q[3].len,
				interfaces[i].psq.q[4].len,
				interfaces[i].psq.q[5].len,
				interfaces[i].psq.q[6].len,
				interfaces[i].psq.q[7].len);
		}
	}

	bcm_bprintf(strbuf, "\n");
	for (i = 0; i < WLFC_MAC_DESC_TABLE_SIZE; i++) {
		if (mac_table[i].occupied) {
			ea = mac_table[i].ea;
			bcm_bprintf(strbuf, "MAC_table[%d].ea = "
				"[%02x:%02x:%02x:%02x:%02x:%02x], if:%d \n", i,
				ea[0], ea[1], ea[2], ea[3], ea[4], ea[5],
				mac_table[i].interface_id);

			bcm_bprintf(strbuf, "MAC_table[%d].PSQ(len,state,credit),(trans,supp_trans)"
				"= (%d,%s,%d),(%d,%d)\n",
				i,
				mac_table[i].psq.len,
				((mac_table[i].state ==
				WLFC_STATE_OPEN) ? " OPEN":"CLOSE"),
				mac_table[i].requested_credit,
				mac_table[i].transit_count, mac_table[i].suppr_transit_count);
#ifdef PROP_TXSTATUS_DEBUG
			bcm_bprintf(strbuf, "MAC_table[%d]: (opened, closed) = (%d, %d)\n",
				i, mac_table[i].opened_ct, mac_table[i].closed_ct);
#endif
			bcm_bprintf(strbuf, "MAC_table[%d].PSQ"
				"(ac0,sup0),(ac1,sup1),(ac2,sup2),(ac3,sup3) = "
				"(%d,%d),(%d,%d),(%d,%d),(%d,%d)\n",
				i,
				mac_table[i].psq.q[0].len,
				mac_table[i].psq.q[1].len,
				mac_table[i].psq.q[2].len,
				mac_table[i].psq.q[3].len,
				mac_table[i].psq.q[4].len,
				mac_table[i].psq.q[5].len,
				mac_table[i].psq.q[6].len,
				mac_table[i].psq.q[7].len);
		}
	}

#ifdef PROP_TXSTATUS_DEBUG
	{
		int avg;
		int moving_avg = 0;
		int moving_samples;

		if (wlfc->stats.latency_sample_count) {
			moving_samples = sizeof(wlfc->stats.deltas)/sizeof(uint32);

			for (i = 0; i < moving_samples; i++)
				moving_avg += wlfc->stats.deltas[i];
			moving_avg /= moving_samples;

			avg = (100 * wlfc->stats.total_status_latency) /
				wlfc->stats.latency_sample_count;
			bcm_bprintf(strbuf, "txstatus latency (average, last, moving[%d]) = "
				"(%d.%d, %03d, %03d)\n",
				moving_samples, avg/100, (avg - (avg/100)*100),
				wlfc->stats.latency_most_recent,
				moving_avg);
		}
	}

	bcm_bprintf(strbuf, "wlfc- fifo[0-5] credit stats: sent = (%d,%d,%d,%d,%d,%d), "
		"back = (%d,%d,%d,%d,%d,%d)\n",
		wlfc->stats.fifo_credits_sent[0],
		wlfc->stats.fifo_credits_sent[1],
		wlfc->stats.fifo_credits_sent[2],
		wlfc->stats.fifo_credits_sent[3],
		wlfc->stats.fifo_credits_sent[4],
		wlfc->stats.fifo_credits_sent[5],

		wlfc->stats.fifo_credits_back[0],
		wlfc->stats.fifo_credits_back[1],
		wlfc->stats.fifo_credits_back[2],
		wlfc->stats.fifo_credits_back[3],
		wlfc->stats.fifo_credits_back[4],
		wlfc->stats.fifo_credits_back[5]);
	{
		uint32 fifo_cr_sent = 0;
		uint32 fifo_cr_acked = 0;
		uint32 request_cr_sent = 0;
		uint32 request_cr_ack = 0;
		uint32 bc_mc_cr_ack = 0;

		for (i = 0; i < sizeof(wlfc->stats.fifo_credits_sent)/sizeof(uint32); i++) {
			fifo_cr_sent += wlfc->stats.fifo_credits_sent[i];
		}

		for (i = 0; i < sizeof(wlfc->stats.fifo_credits_back)/sizeof(uint32); i++) {
			fifo_cr_acked += wlfc->stats.fifo_credits_back[i];
		}

		for (i = 0; i < WLFC_MAC_DESC_TABLE_SIZE; i++) {
			if (wlfc->destination_entries.nodes[i].occupied) {
				request_cr_sent +=
					wlfc->destination_entries.nodes[i].dstncredit_sent_packets;
			}
		}
		for (i = 0; i < WLFC_MAX_IFNUM; i++) {
			if (wlfc->destination_entries.interfaces[i].occupied) {
				request_cr_sent +=
				wlfc->destination_entries.interfaces[i].dstncredit_sent_packets;
			}
		}
		for (i = 0; i < WLFC_MAC_DESC_TABLE_SIZE; i++) {
			if (wlfc->destination_entries.nodes[i].occupied) {
				request_cr_ack +=
					wlfc->destination_entries.nodes[i].dstncredit_acks;
			}
		}
		for (i = 0; i < WLFC_MAX_IFNUM; i++) {
			if (wlfc->destination_entries.interfaces[i].occupied) {
				request_cr_ack +=
					wlfc->destination_entries.interfaces[i].dstncredit_acks;
			}
		}
		bcm_bprintf(strbuf, "wlfc- (sent, status) => pq(%d,%d), vq(%d,%d),"
			"other:%d, bc_mc:%d, signal-only, (sent,freed): (%d,%d)",
			fifo_cr_sent, fifo_cr_acked,
			request_cr_sent, request_cr_ack,
			wlfc->destination_entries.other.dstncredit_acks,
			bc_mc_cr_ack,
			wlfc->stats.signal_only_pkts_sent, wlfc->stats.signal_only_pkts_freed);
	}
#endif /* PROP_TXSTATUS_DEBUG */
	bcm_bprintf(strbuf, "\n");
	bcm_bprintf(strbuf, "wlfc- pkt((in,2bus,txstats,hdrpull),(dropped,hdr_only,wlc_tossed)"
		"(freed,free_err,rollback)) = "
		"((%d,%d,%d,%d),(%d,%d,%d),(%d,%d,%d))\n",
		wlfc->stats.pktin,
		wlfc->stats.pkt2bus,
		wlfc->stats.txstatus_in,
		wlfc->stats.dhd_hdrpulls,

		wlfc->stats.pktdropped,
		wlfc->stats.wlfc_header_only_pkt,
		wlfc->stats.wlc_tossed_pkts,

		wlfc->stats.pkt_freed,
		wlfc->stats.pkt_free_err, wlfc->stats.rollback);

	bcm_bprintf(strbuf, "wlfc- suppress((d11,wlc,err),enq(d11,wl,hq,mac?),retx(d11,wlc,hq)) = "
		"((%d,%d,%d),(%d,%d,%d,%d),(%d,%d,%d))\n",

		wlfc->stats.d11_suppress,
		wlfc->stats.wl_suppress,
		wlfc->stats.bad_suppress,

		wlfc->stats.psq_d11sup_enq,
		wlfc->stats.psq_wlsup_enq,
		wlfc->stats.psq_hostq_enq,
		wlfc->stats.mac_handle_notfound,

		wlfc->stats.psq_d11sup_retx,
		wlfc->stats.psq_wlsup_retx,
		wlfc->stats.psq_hostq_retx);
	bcm_bprintf(strbuf, "wlfc- generic error: %d\n", wlfc->stats.generic_error);

	for (i = 0; i < WLFC_MAX_IFNUM; i++) {
		bcm_bprintf(strbuf, "wlfc- if[%d], pkt_cnt_in_q/AC[0-4] = (%d,%d,%d,%d,%d)\n", i,
			wlfc->pkt_cnt_in_q[i][0],
			wlfc->pkt_cnt_in_q[i][1],
			wlfc->pkt_cnt_in_q[i][2],
			wlfc->pkt_cnt_in_q[i][3],
			wlfc->pkt_cnt_in_q[i][4]);
	}
	bcm_bprintf(strbuf, "\n");

	dhd_os_wlfc_unblock(dhdp);
	return BCME_OK;
}

int dhd_wlfc_clear_counts(dhd_pub_t *dhd)
{
	athost_wl_status_info_t* wlfc;
	wlfc_hanger_t* hanger;

	if (dhd == NULL) {
		WLFC_DBGMESG(("Error: %s():%d\n", __FUNCTION__, __LINE__));
		return BCME_BADARG;
	}

	dhd_os_wlfc_block(dhd);

	wlfc = (athost_wl_status_info_t*)dhd->wlfc_state;
	if (!wlfc || (wlfc->proptxstatus_mode == WLFC_FCMODE_NONE)) {
		dhd_os_wlfc_unblock(dhd);
		return WLFC_UNSUPPORTED;
	}

	memset(&wlfc->stats, 0, sizeof(athost_wl_stat_counters_t));

	hanger = (wlfc_hanger_t*)wlfc->hanger;

	hanger->pushed = 0;
	hanger->popped = 0;
	hanger->failed_slotfind = 0;
	hanger->failed_to_pop = 0;
	hanger->failed_to_push = 0;

	dhd_os_wlfc_unblock(dhd);

	return BCME_OK;
}

int dhd_wlfc_get_enable(dhd_pub_t *dhd, int *val)
{
	if (!dhd || !val) {
		WLFC_DBGMESG(("Error: %s():%d\n", __FUNCTION__, __LINE__));
		return BCME_BADARG;
	}

	dhd_os_wlfc_block(dhd);

	*val = dhd->wlfc_enabled ? 1 : 0;

	dhd_os_wlfc_unblock(dhd);

	return BCME_OK;
}

int dhd_wlfc_set_enable(dhd_pub_t *dhd, int val)
{
	if (!dhd) {
		WLFC_DBGMESG(("Error: %s():%d\n", __FUNCTION__, __LINE__));
		return BCME_BADARG;
	}

	dhd_os_wlfc_block(dhd);

	dhd->wlfc_enabled = val ? 1 : 0;

	dhd_os_wlfc_unblock(dhd);

	return BCME_OK;
}

int dhd_wlfc_get_mode(dhd_pub_t *dhd, int *val)
{
	athost_wl_status_info_t* wlfc;

	if (!dhd || !val) {
		WLFC_DBGMESG(("Error: %s():%d\n", __FUNCTION__, __LINE__));
		return BCME_BADARG;
	}

	dhd_os_wlfc_block(dhd);

	wlfc = (athost_wl_status_info_t*)dhd->wlfc_state;
	*val = wlfc ? wlfc->proptxstatus_mode : 0;

	dhd_os_wlfc_unblock(dhd);

	return BCME_OK;
}

int dhd_wlfc_set_mode(dhd_pub_t *dhd, int val)
{
	athost_wl_status_info_t* wlfc;

	if (!dhd) {
		WLFC_DBGMESG(("Error: %s():%d\n", __FUNCTION__, __LINE__));
		return BCME_BADARG;
	}

	dhd_os_wlfc_block(dhd);

	wlfc = (athost_wl_status_info_t*)dhd->wlfc_state;
	if (wlfc) {
		wlfc->proptxstatus_mode = val & 0xff;
	}

	dhd_os_wlfc_unblock(dhd);

	return BCME_OK;
}

bool dhd_wlfc_is_supported(dhd_pub_t *dhd)
{
	athost_wl_status_info_t* wlfc;

	if (dhd == NULL) {
		WLFC_DBGMESG(("Error: %s():%d\n", __FUNCTION__, __LINE__));
		return FALSE;
	}

	dhd_os_wlfc_block(dhd);

	wlfc = (athost_wl_status_info_t*)dhd->wlfc_state;
	if (!wlfc || (wlfc->proptxstatus_mode == WLFC_FCMODE_NONE)) {
		dhd_os_wlfc_unblock(dhd);
		return FALSE;
	}
	dhd_os_wlfc_unblock(dhd);

	return TRUE;
}

bool dhd_wlfc_is_header_only_pkt(dhd_pub_t * dhd, void *pktbuf)
{
	athost_wl_status_info_t* wlfc;
	bool rc = FALSE;

	if (dhd == NULL) {
		WLFC_DBGMESG(("Error: %s():%d\n", __FUNCTION__, __LINE__));
		return FALSE;
	}

	dhd_os_wlfc_block(dhd);

	wlfc = (athost_wl_status_info_t*)dhd->wlfc_state;
	if (!wlfc || (wlfc->proptxstatus_mode == WLFC_FCMODE_NONE)) {
		dhd_os_wlfc_unblock(dhd);
		return FALSE;
	}

	if (PKTLEN(wl->sh.osh, pktbuf) == 0) {
		wlfc->stats.wlfc_header_only_pkt++;
		rc = TRUE;
	}

	dhd_os_wlfc_unblock(dhd);

	return rc;
}
#endif /* PROP_TXSTATUS */
