/** @file ui_script.h
 */

#pragma once

#include <stdint.h> /* for uint32_t */

#include <mdo-utils/allocator.h>
#include <mdo-utils/result.h>

#include "ui_panel.h"

/** @typedef mdo_ui_script_t
 */
typedef struct mdo_ui_script_s mdo_ui_script_t;

/** @typedef mdo_ui_panel_key_t
 */
typedef uint32_t mdo_ui_panel_key_t;

/** @function mdo_ui_script_create
 * @param ui_script
 * @param alloc
 * @return #mdo_result_t.
 */
mdo_result_t mdo_ui_script_create (mdo_ui_script_t **,
                                   const mdo_allocator_t *);

/** @function mdo_ui_script_delete
 * @param ui_script
 */
void mdo_ui_script_delete (mdo_ui_script_t *);

/** @function mdo_ui_script_load
 * @param ui_script
 * @param filename
 * @return #mdo_result_t.
 */
mdo_result_t mdo_ui_script_load (mdo_ui_script_t *, const char *);

/** @function mdo_ui_script_new_trap
 * @param ui_script
 * @param message
 * @return #wasm_trap_t.
 */
wasm_trap_t *mdo_ui_script_new_trap (mdo_ui_script_t *, const char *);

/** @function mdo_ui_script_bind_panel
 * @param ui_script
 * @param ui_panel
 * @param panel_key
 * @return #mdo_result_t.
 */
mdo_result_t mdo_ui_script_bind_panel (mdo_ui_script_t *, mdo_ui_panel_t *,
                                       mdo_ui_panel_key_t *);

/** @function mdo_ui_script_unbind_panel
 * @param ui_script
 * @param panel_key
 */
void mdo_ui_script_unbind_panel (mdo_ui_script_t *, mdo_ui_panel_key_t);

/** @function mdo_ui_script_lookup_panel
 * @param ui_script
 * @param panel_key
 * @return A pointer to the panel, or NULL on error.
 */
mdo_ui_panel_t *mdo_ui_script_lookup_panel (mdo_ui_script_t *,
                                            mdo_ui_panel_key_t);
