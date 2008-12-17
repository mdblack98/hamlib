/*
 * hamlib - (C) Frank Singleton 2000 (javabear at users.sourceforge.net)
 *
 * ft450.c - (C) Nate Bargmann 2007 (n0nb at arrl.net)
 *           (C) Stephane Fillod 2008
 *
 * This shared library provides an API for communicating
 * via serial interface to an FT-450 using the "CAT" interface
 *
 *
 * $Id: ft450.c,v 1.6 2008-12-17 22:57:04 mrtembry Exp $
 *
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307  USA
 *
 */


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "hamlib/rig.h"
#include "bandplan.h"
#include "serial.h"
#include "misc.h"
#include "yaesu.h"
#include "newcat.h"
#include "ft450.h"
#include "idx_builtin.h"

/*
 * ft450 rigs capabilities.
 * Also this struct is READONLY!
 *
 */

const struct rig_caps ft450_caps = {
    .rig_model =          RIG_MODEL_FT450,
    .model_name =         "FT-450",
    .mfg_name =           "Yaesu",
    .version =            NEWCAT_VER ".1",
    .copyright =          "LGPL",
    .status =             RIG_STATUS_BETA,
    .rig_type =           RIG_TYPE_TRANSCEIVER,
    .ptt_type =           RIG_PTT_RIG,
    .dcd_type =           RIG_DCD_NONE,
    .port_type =          RIG_PORT_SERIAL,
    .serial_rate_min =    4800,         /* Default rate per manual */
    .serial_rate_max =    38400,
    .serial_data_bits =   8,
    .serial_stop_bits =   1,            /* Assumed since manual makes no mention */
    .serial_parity =      RIG_PARITY_NONE,
    .serial_handshake =   RIG_HANDSHAKE_NONE,
    .write_delay =        FT450_WRITE_DELAY,
    .post_write_delay =   FT450_POST_WRITE_DELAY,
    .timeout =            2000,
    .retry =              0,
    .has_get_func =       FT450_FUNCS,
    .has_set_func =       FT450_FUNCS,
    .has_get_level =      FT450_LEVELS,
    .has_set_level =      RIG_LEVEL_SET(FT450_LEVELS),
    .has_get_parm =       RIG_PARM_NONE,
    .has_set_parm =       RIG_PARM_NONE,
    .level_gran = {
	[LVL_RAWSTR] = { .min = { .i = 0 }, .max = { .i = 255 } },
	[LVL_CWPITCH] = { .min = { .i = 400 }, .max = { .i = 800 }, .step = { .i = 100 } },
    },
    .ctcss_list =         NULL,	/* TODO */
    .dcs_list =           NULL,
    .preamp =             { 10, RIG_DBLST_END, }, /* TBC */
    .attenuator =         { 18, RIG_DBLST_END, }, /* TBC */
    .max_rit =            Hz(9999),
    .max_xit =            Hz(0),
    .max_ifshift =        Hz(1000),
    .vfo_ops =            FT450_VFO_OPS,
    .targetable_vfo =     RIG_TARGETABLE_FREQ,
    .transceive =         RIG_TRN_OFF,        /* May enable later as the 450 has an Auto Info command */
    .bank_qty =           0,
    .chan_desc_sz =       0,
    .str_cal =            FT450_STR_CAL,
    .chan_list =          {
		{   1, 500, RIG_MTYPE_MEM,  NEWCAT_MEM_CAP },
		{ 501, 504, RIG_MTYPE_EDGE, NEWCAT_MEM_CAP },    /* two by two */
		RIG_CHAN_END,
	    		  },

    .rx_range_list1 =     {
        {kHz(30), MHz(60), FT450_ALL_RX_MODES, -1, -1, FT450_VFO_ALL, FT450_ANTS},   /* General coverage + ham */
        RIG_FRNG_END,
    }, /* FIXME:  Are these the correct Region 1 values? */

    .tx_range_list1 =     {
        FRQ_RNG_HF(1, FT450_OTHER_TX_MODES, W(5), W(100), FT450_VFO_ALL, FT450_ANTS),
        FRQ_RNG_HF(1, FT450_AM_TX_MODES, W(2), W(25), FT450_VFO_ALL, FT450_ANTS),	/* AM class */
        FRQ_RNG_6m(1, FT450_OTHER_TX_MODES, W(5), W(100), FT450_VFO_ALL, FT450_ANTS),
        FRQ_RNG_6m(1, FT450_AM_TX_MODES, W(2), W(25), FT450_VFO_ALL, FT450_ANTS),	/* AM class */

        RIG_FRNG_END,
    },

    .rx_range_list2 =     {
        {kHz(30), MHz(56), FT450_ALL_RX_MODES, -1, -1, FT450_VFO_ALL, FT450_ANTS},
        RIG_FRNG_END,
    },

    .tx_range_list2 =     {
        FRQ_RNG_HF(2, FT450_OTHER_TX_MODES, W(5), W(100), FT450_VFO_ALL, FT450_ANTS),
        FRQ_RNG_HF(2, FT450_AM_TX_MODES, W(2), W(25), FT450_VFO_ALL, FT450_ANTS),	/* AM class */
        FRQ_RNG_6m(2, FT450_OTHER_TX_MODES, W(5), W(100), FT450_VFO_ALL, FT450_ANTS),
        FRQ_RNG_6m(2, FT450_AM_TX_MODES, W(2), W(25), FT450_VFO_ALL, FT450_ANTS),	/* AM class */

        RIG_FRNG_END,
    },

    .tuning_steps =       {
        {FT450_SSB_CW_RX_MODES, Hz(10)},    /* Normal */
        {FT450_SSB_CW_RX_MODES, Hz(100)},   /* Fast */

        {FT450_AM_RX_MODES,     Hz(100)},   /* Normal */
        {FT450_AM_RX_MODES,     kHz(1)},    /* Fast */

        {FT450_FM_RX_MODES,     Hz(100)},   /* Normal */
        {FT450_FM_RX_MODES,     kHz(1)},    /* Fast */

        RIG_TS_END,

    },

    /* mode/filter list, .remember =  order matters! */
    .filters =            {
        {FT450_CW_RTTY_PKT_RX_MODES,  kHz(1.8)},  /* Normal must be first */
        {FT450_CW_RTTY_PKT_RX_MODES,  kHz(0.5)},  /* Narrow filter bandwidth */
        {FT450_CW_RTTY_PKT_RX_MODES,  kHz(2.4)},  /* Wide   filter bandwidth */
        {RIG_MODE_SSB,                kHz(2.4)},  /* Normal SSB filter bandwidth */
        {RIG_MODE_SSB,                kHz(1.8)},  /* Narrow SSB filter bandwidth */
        {RIG_MODE_SSB,                kHz(3.0)},  /* Wide   SSB filter bandwidth */
        {RIG_MODE_AM,                 kHz(6)},    /* normal AM filter */
        {RIG_MODE_AM,                 kHz(3)},
        {RIG_MODE_AM,                 kHz(9)},
        {FT450_FM_RX_MODES,           kHz(12)},   /* FM  */
        {RIG_MODE_FM,                 kHz(8)},    /* narrow FM */

        RIG_FLT_END,
    },


    .priv =               NULL,           /* private data FIXME: */

    .rig_init =           newcat_init,
    .rig_cleanup =        newcat_cleanup,
    .rig_open =           newcat_open,     /* port opened */
    .rig_close =          newcat_close,    /* port closed */

    .set_freq =           newcat_set_freq,
    .get_freq =           newcat_get_freq,
    .set_mode =           newcat_set_mode,
    .get_mode =           newcat_get_mode,
    .set_vfo =            newcat_set_vfo,
    .get_vfo =            newcat_get_vfo,
    .set_ptt =            newcat_set_ptt,
    .get_ptt =            newcat_get_ptt,
    .set_split_vfo =      newcat_set_split_vfo,
    .get_split_vfo =      newcat_get_split_vfo,
    .set_rit =            newcat_set_rit,
    .get_rit =            newcat_get_rit,
    .get_func =           newcat_get_func,
    .set_func =           newcat_set_func,
    .get_level =          newcat_get_level,
    .set_level =          newcat_set_level,
    .get_mem =            newcat_get_mem,
    .set_mem =            newcat_set_mem,
    .vfo_op =             newcat_vfo_op,
    .get_info =           newcat_get_info,
};

