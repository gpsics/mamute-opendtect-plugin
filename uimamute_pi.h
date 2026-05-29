#pragma once

#include "uiodmainmod.h"

#if defined(uiMamuteModeling_EXPORTS) || defined(UIMAMUTEMODELING_EXPORTS)
#define do_export_uiMamuteModeling
#else
#define do_import_uiMamuteModeling
#endif

#if defined(do_export_uiMamuteModeling)
#define Export_uiMamuteModeling mExp(uiODMain)
#define Extern_uiMamuteModeling extern mExp(uiODMain)
#elif defined(do_import_uiMamuteModeling)
#define Export_uiMamuteModeling mImp(uiODMain)
#define Extern_uiMamuteModeling extern mImp(uiODMain)
#else
#define Export_uiMamuteModeling
#define Extern_uiMamuteModeling extern
#endif