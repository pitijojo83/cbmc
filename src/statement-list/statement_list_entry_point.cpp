/*******************************************************************\

Module: Statement List Language Entry Point

Author: Matthias Weiss, matthias.weiss@diffblue.com

\*******************************************************************/

/// \file
/// Statement List Language Entry Point

#include "statement_list_entry_point.h"
#include "statement_list_typecheck.h"

#include <goto-programs/goto_functions.h>
#include <util/config.h>

#define INITIALIZE_FUNCTION CPROVER_PREFIX "initialize"
#define DB_ENTRY_POINT_POSTFIX "_entry_point"
#define CPROVER_HIDE CPROVER_PREFIX "HIDE"

/// Creates a call to __CPROVER_initialize and adds it to the start function's
/// body.
/// \param [out] function_body: Body of the start function.
/// \param symbol_table: Symbol table, containing the symbol for
///   __CPROVER_initialize.
/// \param main_symbol_location: Source location of the main symbol.
static void add_initialize_call(
  code_blockt &function_body,
  const symbol_tablet &symbol_table,
  const source_locationt &main_symbol_location)
{
  symbolt init = symbol_table.lookup_ref(INITIALIZE_FUNCTION);
  code_function_callt call_init{init.symbol_expr()};
  call_init.add_source_location() = main_symbol_location;
  function_body.add(call_init);
}

/// Creates a call to the main function block and adds it to the start
/// function's body.
/// \param [out] function_body: Body of the start function.
/// \param symbol_table: Symbol table, containing the main symbol.
/// \param main_function_block: Main symbol of this application.
static void add_main_function_block_call(
  code_blockt &function_body,
  symbol_tablet &symbol_table,
  const symbolt &main_function_block)
{
  const code_typet &function_type = to_code_type(main_function_block.type);
  PRECONDITION(1u == function_type.parameters().size());
  const code_typet::parametert &data_block_interface =
    function_type.parameters().front();
  symbolt instance_data_block;
  instance_data_block.name =
    id2string(data_block_interface.get_base_name()) + DB_ENTRY_POINT_POSTFIX;
  instance_data_block.type = data_block_interface.type().subtype();
  instance_data_block.is_static_lifetime = true;
  instance_data_block.mode = STATEMENT_LIST_MODE;
  symbol_table.add(instance_data_block);
  const address_of_exprt data_block_ref{instance_data_block.symbol_expr()};

  code_function_callt::argumentst args{data_block_ref};
  code_function_callt call_main{main_function_block.symbol_expr(), args};
  call_main.add_source_location() = main_function_block.location;
  function_body.add(call_main);
}

/// Creates __CPROVER_initialize and adds it to the symbol table.
/// \param [out] symbol_table: Symbol table that should contain the function.
static void generate_statement_list_init_function(symbol_tablet &symbol_table)
{
  symbolt init;
  init.name = INITIALIZE_FUNCTION;
  init.type = code_typet({}, empty_typet{});

  code_blockt dest;
  dest.add(code_labelt(CPROVER_HIDE, code_skipt()));
  init.value = dest;
  symbol_table.add(init);
}

/// Creates a start function and adds it to the symbol table. This start
/// function contains calls to __CPROVER_initialize and the main symbol.
/// \param main: Main symbol of this application.
/// \param [out] symbol_table: Symbol table that should contain the function.
/// \param message_handler: Handler that is responsible for error messages.
bool generate_statement_list_start_function(
  const symbolt &main,
  symbol_tablet &symbol_table,
  message_handlert &message_handler)
{
  PRECONDITION(!main.value.is_nil());
  code_blockt start_function_body;
  start_function_body.add(code_labelt(CPROVER_HIDE, code_skipt()));

  add_initialize_call(start_function_body, symbol_table, main.location);
  // TODO: Support calls to STL functions.
  // Since STL function calls do not involve a data block, pass all arguments
  // as normal parameters.
  add_main_function_block_call(start_function_body, symbol_table, main);

  // Add the start symbol.
  symbolt start_symbol;
  start_symbol.name = goto_functionst::entry_point();
  start_symbol.type = code_typet({}, empty_typet{});
  start_symbol.value.swap(start_function_body);
  start_symbol.mode = main.mode;

  if(!symbol_table.insert(std::move(start_symbol)).second)
  {
    messaget message(message_handler);
    message.error() << "failed to insert start symbol" << messaget::eom;
    return true;
  }

  return false;
}

bool statement_list_entry_point(
  symbol_tablet &symbol_table,
  message_handlert &message_handler)
{
  // Check if the entry point is already present and return if it is.
  if(
    symbol_table.symbols.find(goto_functionst::entry_point()) !=
    symbol_table.symbols.end())
    return false;

  irep_idt main_symbol_name;

  // Find main symbol, if any is given.
  if(config.main.has_value())
  {
    std::list<irep_idt> matches;

    forall_symbol_base_map(
      it, symbol_table.symbol_base_map, config.main.value())
    {
      symbol_tablet::symbolst::const_iterator s_it =
        symbol_table.symbols.find(it->second);

      if(s_it == symbol_table.symbols.end())
        continue;

      if(s_it->second.type.id() == ID_code)
        matches.push_back(it->second);
    }

    if(matches.empty())
    {
      messaget message(message_handler);
      message.error() << "main symbol `" << config.main.value() << "' not found"
                      << messaget::eom;
      return true;
    }

    if(matches.size() > 1)
    {
      messaget message(message_handler);
      message.error() << "main symbol `" << config.main.value()
                      << "' is ambiguous" << messaget::eom;
      return true;
    }

    main_symbol_name = matches.front();
  }
  else
  {
    // TODO: Support the standard entry point of STL (organisation blocks).
    // This also requires to expand the grammar and typecheck.
    // For now, return false to let the typecheck itself pass (vital for
    // --show-symbol-table). The missing entry symbol will be caught later.
    return false;
  }

  const symbolt &main = symbol_table.lookup_ref(main_symbol_name);

  // Check if the symbol has a body.
  if(main.value.is_nil())
  {
    messaget message(message_handler);
    message.error() << "main symbol `" << id2string(main_symbol_name)
                    << "' has no body" << messaget::eom;
    return true;
  }

  generate_statement_list_init_function(symbol_table);
  return generate_statement_list_start_function(
    main, symbol_table, message_handler);
}