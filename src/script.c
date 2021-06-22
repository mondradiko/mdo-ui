#include "script.h"

#include <stdio.h>
#include <string.h> /* for strlen, strncmp */
#include <wasmtime.h>

#include "panel-api.h"

struct canary_script_s
{
  const mdo_allocator_t *alloc;
  mdo_result_t wasm_error;
  mdo_result_t wasmtime_error;
  mdo_result_t wasm_trap_error;

  wasm_engine_t *engine;
  wasmtime_store_t *store;
  wasmtime_context_t *context;

  wasmtime_linker_t *linker;

  wasmtime_module_t *module;
  wasmtime_instance_t instance;

  /* TODO(marceline-cramer): use mdo-utils vector */
  struct
  {
    canary_panel_t **vals;
    size_t size;
    size_t capacity;
  } panels;
};

static mdo_result_t
log_wasmtime_error (canary_script_t *ui_script, wasmtime_error_t *error)
{
  wasm_byte_vec_t error_message;
  wasmtime_error_message (error, &error_message);
  wasmtime_error_delete (error);

  mdo_result_t result = ui_script->wasmtime_error;
  mdo_result_t logged = LOG_RESULT (result, error_message.data);
  wasm_byte_vec_delete (&error_message);

  return logged;
}

static mdo_result_t
log_wasm_trap (canary_script_t *ui_script, wasm_trap_t *trap)
{
  wasm_byte_vec_t trap_message;
  wasm_trap_message (trap, &trap_message);
  wasm_trap_delete (trap);

  mdo_result_t result = ui_script->wasm_trap_error;
  mdo_result_t logged = LOG_RESULT (result, trap_message.data);
  wasm_byte_vec_delete (&trap_message);

  return logged;
}

static wasm_trap_t *
env_abort_cb (void *env, wasmtime_caller_t *caller, const wasmtime_val_t *args,
              size_t arg_num, wasmtime_val_t *results, size_t result_num)
{
  return NULL;
}

static void
finalizer_cb (void *env)
{
}

static void
link_function (canary_script_t *ui_script, const char *module,
               const char *symbol, wasm_functype_t *functype,
               wasmtime_func_callback_t cb)
{
  /* TODO(marceline-cramer): collect created funcs with vector and delete */
  wasmtime_extern_t func_extern;
  func_extern.kind = WASMTIME_EXTERN_FUNC;
  wasmtime_func_new (ui_script->context, functype, cb, ui_script, finalizer_cb,
                     &func_extern.of.func);

  wasmtime_error_t *error
      = wasmtime_linker_define (ui_script->linker, module, strlen (module),
                                symbol, strlen (symbol), &func_extern);
  if (error)
    log_wasmtime_error (ui_script, error);
}

mdo_result_t
canary_script_create (canary_script_t **ui_script,
                      const mdo_allocator_t *alloc)
{
  canary_script_t *new_ui_script
      = mdo_allocator_malloc (alloc, sizeof (canary_script_t));
  *ui_script = new_ui_script;

  new_ui_script->alloc = alloc;
  new_ui_script->module = NULL;

  new_ui_script->panels.capacity = 16;
  new_ui_script->panels.vals = mdo_allocator_calloc (
      alloc, new_ui_script->panels.capacity, sizeof (canary_panel_t *));
  new_ui_script->panels.size = 0;

  mdo_result_t wasm_error
      = mdo_result_create (MDO_LOG_ERROR, "wasm error: %s", 1, false);
  new_ui_script->wasm_error = wasm_error;

  mdo_result_t wasmtime_error
      = mdo_result_create (MDO_LOG_ERROR, "wasmtime error: %s", 1, false);
  new_ui_script->wasmtime_error = wasmtime_error;

  mdo_result_t wasm_trap_error
      = mdo_result_create (MDO_LOG_ERROR, "wasm trap thrown: %s", 1, false);
  new_ui_script->wasm_trap_error = wasm_trap_error;

  new_ui_script->engine = wasm_engine_new ();
  if (!new_ui_script->engine)
    return LOG_RESULT (wasm_error, "failed to create engine");

  new_ui_script->store
      = wasmtime_store_new (new_ui_script->engine, NULL, finalizer_cb);
  if (!new_ui_script->store)
    return LOG_RESULT (wasm_error, "failed to create store");

  new_ui_script->context = wasmtime_store_context (new_ui_script->store);

  new_ui_script->linker = wasmtime_linker_new (new_ui_script->engine);
  if (!new_ui_script->linker)
    return LOG_RESULT (wasmtime_error, "failed to create linker");

  {
    wasm_valtype_vec_t params;
    wasm_valtype_vec_new_uninitialized (&params, 4);

    for (size_t i = 0; i < params.size; i++)
      params.data[i] = wasm_valtype_new_i32 ();

    wasm_valtype_vec_t results;
    wasm_valtype_vec_new_empty (&results);

    /* TODO(marceline-cramer): collect with vector and delete */
    wasm_functype_t *functype = wasm_functype_new (&params, &results);

    link_function (new_ui_script, "env", "abort", functype, env_abort_cb);
  }

  {
    wasm_valtype_vec_t params;
    wasm_valtype_vec_new_uninitialized (&params, 5);

    params.data[0] = wasm_valtype_new_i32 ();

    for (size_t i = 1; i < params.size; i++)
      params.data[i] = wasm_valtype_new_f32 ();

    wasm_valtype_vec_t results;
    wasm_valtype_vec_new_empty (&results);

    /* TODO(marceline-cramer): collect with vector and delete */
    wasm_functype_t *functype = wasm_functype_new (&params, &results);

    link_function (new_ui_script, "", "UiPanel_setColor", functype,
                   canary_panel_set_color_cb);
  }

  {
    wasm_valtype_vec_t params;
    wasm_valtype_vec_new_uninitialized (&params, 11);

    params.data[0] = wasm_valtype_new_i32 ();

    for (size_t i = 1; i < params.size; i++)
      params.data[i] = wasm_valtype_new_f32 ();

    wasm_valtype_vec_t results;
    wasm_valtype_vec_new_empty (&results);

    /* TODO(marceline-cramer): collect with vector and delete */
    wasm_functype_t *functype = wasm_functype_new (&params, &results);

    link_function (new_ui_script, "", "UiPanel_drawTriangle", functype,
                   canary_panel_draw_triangle_cb);
  }

  return MDO_SUCCESS;
}

mdo_result_t
canary_script_load (canary_script_t *ui_script, const char *filename)
{
  const mdo_allocator_t *alloc = ui_script->alloc;
  mdo_result_t wasm_error = ui_script->wasm_error;

  FILE *f = fopen (filename, "rb");
  if (!f)
    return LOG_RESULT (wasm_error, "failed to open UI script file");

  wasm_byte_vec_t file_contents;

  fseek (f, 0, SEEK_END);
  file_contents.size = ftell (f);
  fseek (f, 0, SEEK_SET);
  file_contents.data = mdo_allocator_malloc (alloc, file_contents.size);
  fread (file_contents.data, 1, file_contents.size, f);
  fclose (f);

  wasmtime_error_t *wasmtime_error = wasmtime_module_new (
      ui_script->engine, (const uint8_t *)file_contents.data,
      file_contents.size, &ui_script->module);
  mdo_allocator_free (alloc, file_contents.data);

  if (wasmtime_error)
    return log_wasmtime_error (ui_script, wasmtime_error);

  if (!ui_script->module)
    return LOG_RESULT (wasm_error, "failed to compile UI script");

  wasm_trap_t *trap = NULL;
  wasmtime_error = wasmtime_linker_instantiate (
      ui_script->linker, ui_script->context, ui_script->module,
      &ui_script->instance, &trap);

  if (wasmtime_error)
    return log_wasmtime_error (ui_script, wasmtime_error);

  if (trap)
    return log_wasm_trap (ui_script, trap);

  return MDO_SUCCESS;
}

void
canary_script_delete (canary_script_t *ui_script)
{

  const mdo_allocator_t *alloc = ui_script->alloc;

  if (ui_script->panels.vals)
    mdo_allocator_free (alloc, ui_script->panels.vals);

  if (ui_script->module)
    wasmtime_module_delete (ui_script->module);

  if (ui_script->linker)
    wasmtime_linker_delete (ui_script->linker);

  if (ui_script->store)
    wasmtime_store_delete (ui_script->store);

  if (ui_script->engine)
    wasm_engine_delete (ui_script->engine);

  mdo_allocator_free (alloc, ui_script);
}

wasm_trap_t *
canary_script_new_trap (canary_script_t *ui_script, const char *message)
{
  return wasmtime_trap_new (message, strlen (message));
}

static bool
get_callback (canary_script_t *ui_script, const char *symbol,
              wasmtime_func_t *func)
{
  size_t symbol_len = strlen (symbol);

  wasmtime_extern_t exported;

  if (!wasmtime_instance_export_get (ui_script->context, &ui_script->instance,
                                     symbol, symbol_len, &exported))
    {

      LOG_ERR ("could not find callback '%s'", symbol);
      return false;
    }

  if (exported.kind != WASMTIME_EXTERN_FUNC)
    {
      LOG_ERR ("export '%s' is not a function", symbol);
      return false;
    }

  *func = exported.of.func;

  return true;
}

mdo_result_t
canary_script_bind_panel (canary_script_t *ui_script, canary_panel_t *ui_panel,
                          canary_panel_key_t *panel_key)
{
  /* TODO(marceline-cramer): bounds checking, reallocation */
  *panel_key = ui_script->panels.size++;
  ui_script->panels.vals[*panel_key] = ui_panel;

  wasmtime_func_t bind_panel_cb;
  if (!get_callback (ui_script, "bind_panel", &bind_panel_cb))
    return LOG_RESULT (ui_script->wasmtime_error,
                       "couldn't get bind_panel callback");

  wasmtime_val_t args[]
      = { { .kind = WASM_I32, .of = { .i32 = *panel_key } } };

  wasmtime_val_t results[1];

  wasm_trap_t *trap = NULL;
  wasmtime_func_call (ui_script->context, &bind_panel_cb, args, 1, results, 1,
                      &trap);

  if (trap)
    return log_wasm_trap (ui_script, trap);

  return MDO_SUCCESS;
}

void
canary_script_unbind_panel (canary_script_t *ui_script,
                            canary_panel_key_t panel_key)
{
  /* TODO(marceline-cramer): bounds checking, reallocation */
  ui_script->panels.vals[panel_key] = NULL;
}

canary_panel_t *
canary_script_lookup_panel (canary_script_t *ui_script,
                            canary_panel_key_t panel_key)
{
  /* TODO(marceline-cramer): bounds checking */
  return ui_script->panels.vals[panel_key];
}
