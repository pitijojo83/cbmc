/*******************************************************************\

Module: A special command line object for the gcc-like options

Author: CM Wintersteiger

Date: June 2006

\*******************************************************************/

#ifndef CPROVER_GOTO_CC_GCC_CMDLINE_H
#define CPROVER_GOTO_CC_GCC_CMDLINE_H

#include "goto_cc_cmdline.h"

class gcc_cmdlinet:public goto_cc_cmdlinet
{
public:
  // overload
  virtual bool parse(int, const char**);

  gcc_cmdlinet()
  {
  }
};

#endif // CPROVER_GOTO_CC_GCC_CMDLINE_H
