#include "git_revision.h"

#define EVAL(X) X
#define STRINGIFY_(X) #X
#define STRINGIFY(X) STRINGIFY_(X)

#if STATIC_BUILD_SETTINGS
const char *revision = "";
const int revision_number = 0;
#else
const char *revision = STRINGIFY(GIT_REVISION);
const int revision_number = GIT_VERSION_NUMBER;
#endif
