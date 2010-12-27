/*
 * gamekiller.c - Cartridge handling, Game Killer cart.
 *
 * Written by
 *  Groepaz <groepaz@gmx.net>
 *
 * This file is part of VICE, the Versatile Commodore Emulator.
 * See README for copyright notice.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 *  02111-1307  USA.
 *
 */

#include "vice.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CARTRIDGE_INCLUDE_SLOTMAIN_API
#include "c64cartsystem.h"
#undef CARTRIDGE_INCLUDE_SLOTMAIN_API
#include "c64export.h"
#include "c64io.h"
#include "c64mem.h"
#include "cartridge.h"
#include "gamekiller.h"
#include "snapshot.h"
#include "types.h"
#include "util.h"

/* #define GKDEBUG */

#ifdef GKDEBUG
#define DBG(x) printf x
#else
#define DBG(x)
#endif

/*
    ROBTEK "Game Killer"

    - 1 8k ROM
    - when cartridge is active, ultimax is enabled when addr>=e000, so the
      rom is visible at e000, below is normal c64 ram
    - the code writes 0 to both de00 and df00 to disable the cartridge. we
      assume the cart uses the full io1 and io2 range
    - when the freezer button is pressed the cartridge will be enabled and
      an NMI will be triggered
*/

#define GAME_KILLER_CART_SIZE (8*0x400)

/* ---------------------------------------------------------------------*/

static int cartridge_disable_flag;

static void REGPARM2 gamekiller_io1_store(WORD addr, BYTE value)
{
    DBG(("io1 %04x %02x\n",addr,value));
    cartridge_disable_flag++;
    if (cartridge_disable_flag > 1) {
        cart_config_changed_slotmain(2, 2, CMODE_READ);
        DBG(("Game Killer disabled\n"));
    }
}

static void REGPARM2 gamekiller_io2_store(WORD addr, BYTE value)
{
    DBG(("io2 %04x %02x\n",addr,value));
    cartridge_disable_flag++;
    if (cartridge_disable_flag > 1) {
        cart_config_changed_slotmain(2, 2, CMODE_READ);
        DBG(("Game Killer disabled\n"));
    }
}

static io_source_t gamekiller_io1_device = {
    CARTRIDGE_NAME_GAME_KILLER,
    IO_DETACH_CART,
    NULL,
    0xde00, 0xdeff, 0xff,
    0, /* read is never valid */
    gamekiller_io1_store,
    NULL,
    NULL, /* TODO: peek */
    NULL, /* TODO: dump */
    CARTRIDGE_GAME_KILLER
};

static io_source_t gamekiller_io2_device = {
    CARTRIDGE_NAME_GAME_KILLER,
    IO_DETACH_CART,
    NULL,
    0xdf00, 0xdfff, 0xff,
    0, /* read is never valid */
    gamekiller_io2_store,
    NULL,
    NULL, /* TODO: peek */
    NULL, /* TODO: dump */
    CARTRIDGE_GAME_KILLER
};

static io_source_list_t *gamekiller_io1_list_item = NULL;
static io_source_list_t *gamekiller_io2_list_item = NULL;

static const c64export_resource_t export_res = {
    CARTRIDGE_NAME_GAME_KILLER, 1, 1, &gamekiller_io1_device, &gamekiller_io2_device, CARTRIDGE_GAME_KILLER
};

/* ---------------------------------------------------------------------*/

void gamekiller_freeze(void)
{
    DBG(("Game Killer freeze\n"));
    cart_config_changed_slotmain(3, 3, CMODE_READ | CMODE_RELEASE_FREEZE);
    cartridge_disable_flag = 0;
}


void gamekiller_config_init(void)
{
    cart_config_changed_slotmain(3, 3, CMODE_READ);
    cartridge_disable_flag = 0;
}

void gamekiller_config_setup(BYTE *rawcart)
{
    memcpy(romh_banks, rawcart, GAME_KILLER_CART_SIZE);
    cart_config_changed_slotmain(3, 3, CMODE_READ);
    cartridge_disable_flag = 0;
}

/* ---------------------------------------------------------------------*/

static int gamekiller_common_attach(void)
{
    if (c64export_add(&export_res) < 0) {
        return -1;
    }

    gamekiller_io1_list_item = c64io_register(&gamekiller_io1_device);
    gamekiller_io2_list_item = c64io_register(&gamekiller_io2_device);

    return 0;
}

int gamekiller_bin_attach(const char *filename, BYTE *rawcart)
{
    if (util_file_load(filename, rawcart, GAME_KILLER_CART_SIZE, UTIL_FILE_LOAD_SKIP_ADDRESS) < 0) {
        return -1;
    }

    return gamekiller_common_attach();
}

int gamekiller_crt_attach(FILE *fd, BYTE *rawcart)
{
    BYTE chipheader[0x10];

    if (fread(chipheader, 0x10, 1, fd) < 1) {
        return -1;
    }

    if (chipheader[0xb] > 0) {
        return -1;
    }

    if (fread(rawcart, GAME_KILLER_CART_SIZE, 1, fd) < 1) {
        return -1;
    }

    return gamekiller_common_attach();
}

void gamekiller_detach(void)
{
    c64export_remove(&export_res);
    c64io_unregister(gamekiller_io1_list_item);
    c64io_unregister(gamekiller_io2_list_item);
    gamekiller_io1_list_item = NULL;
    gamekiller_io2_list_item = NULL;
}

/* ---------------------------------------------------------------------*/

#define CART_DUMP_VER_MAJOR   0
#define CART_DUMP_VER_MINOR   0
#define SNAP_MODULE_NAME  "CARTGK"

int gamekiller_snapshot_write_module(snapshot_t *s)
{
    snapshot_module_t *m;

    m = snapshot_module_create(s, SNAP_MODULE_NAME,
                          CART_DUMP_VER_MAJOR, CART_DUMP_VER_MINOR);
    if (m == NULL) {
        return -1;
    }

    if (0
        || (SMW_B(m, (BYTE)cartridge_disable_flag) < 0)
        || (SMW_BA(m, romh_banks, GAME_KILLER_CART_SIZE) < 0)) {
        snapshot_module_close(m);
        return -1;
    }

    snapshot_module_close(m);
    return 0;
}

int gamekiller_snapshot_read_module(snapshot_t *s)
{
    BYTE vmajor, vminor;
    snapshot_module_t *m;

    m = snapshot_module_open(s, SNAP_MODULE_NAME, &vmajor, &vminor);
    if (m == NULL) {
        return -1;
    }

    if ((vmajor != CART_DUMP_VER_MAJOR) || (vminor != CART_DUMP_VER_MINOR)) {
        snapshot_module_close(m);
        return -1;
    }

    if (0
        || (SMR_B_INT(m, &cartridge_disable_flag) < 0)
        || (SMR_BA(m, romh_banks, GAME_KILLER_CART_SIZE) < 0)) {
        snapshot_module_close(m);
        return -1;
    }

    snapshot_module_close(m);

    return gamekiller_common_attach();
}
