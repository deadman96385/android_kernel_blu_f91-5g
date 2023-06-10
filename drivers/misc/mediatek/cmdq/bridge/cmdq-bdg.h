/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __CMDQ_BDG_H__
#define __CMDQ_BDG_H__


inline u32 spi_read_reg(const u32 addr);
inline s32 spi_write_reg(const u32 addr, const u32 val);

s32 cmdq_bdg_irq_handler(void);

struct cmdq_client;
/* shutdown loop */
void cmdq_bdg_client_shutdown(struct cmdq_client *cl);
void cmdq_bdg_client_get_irq(struct cmdq_client *cl, u32 *irq);

#if IS_ENABLED(CONFIG_MTK_CMDQ_V3)
struct cmdqRecStruct;
void cmdq_bdg_dump_handle(struct cmdqRecStruct *rec, const char *tag);
#endif
#if IS_ENABLED(CONFIG_MTK_CMDQ_MBOX_EXT)
struct cmdq_pkt;
void cmdq_bdg_dump_summary(struct cmdq_client *client, struct cmdq_pkt *pkt);
#endif

#endif
