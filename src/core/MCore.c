/**
 * @file
 * The core-related functions
 */
#include "MCore.h"

/**
 * @brief The Welcome message
 */
void Welcome() {
  Message(MESSAGE_INFO, "This is Mechanic, v.%s.%s.%s\n",
      PACKAGE_VERSION_MAJOR, PACKAGE_VERSION_MINOR, PACKAGE_VERSION_PATCH);
  Message(MESSAGE_INFO, "Author: %s\n", PACKAGE_AUTHOR);
  Message(MESSAGE_INFO, "Bugs/features/support: %s\n", PACKAGE_BUGREPORT);
  Message(MESSAGE_INFO, "%s\n", PACKAGE_URL);
  Message(MESSAGE_OUTPUT, "\n");
}

/**
 * @brief The Prepare hook
 *
 * @param m The module pointer
 *
 * @return 0 on success, error code otherwise
 */
int Prepare(module *m) {
  int mstat = SUCCESS;
  setup *s = &(m->layer.setup);
  query *q;

  q = LoadSym(m, "Prepare", LOAD_DEFAULT);
  if (q) mstat = q(m->node, m->filename, s);
  CheckStatus(mstat);

  return mstat;
}

/**
 * @brief The Process hook
 *
 * @param m The module pointer
 *
 * @return 0 on success, error code otherwise
 */
int Process(module *m, pool **p) {
  int mstat = SUCCESS;
  setup *s = &(m->layer.setup);
  query *q;

  q = LoadSym(m, "Process", LOAD_DEFAULT);
  if (q) mstat = q(m->node, m->filename, p, s);
  CheckStatus(mstat);

  return mstat;
}

