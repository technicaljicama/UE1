/*------------------------------------------------------------------------------------
	Dependencies.
------------------------------------------------------------------------------------*/

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <vitasdk.h>
#include <vrtld.h>

#define CORE_API DLL_IMPORT
#include "UnBuild.h"
#include "UnGcc.h"

/*------------------------------------------------------------------------------------
	Common definitions.
------------------------------------------------------------------------------------*/

#define MAX_PATH 1024
#define MAX_ARGV_NUM 8
#define DATA_PATH "data/unreal/System/"

extern char GRootPath[];
extern const vrtld_export_t GAuxExports[];
extern const size_t GNumAuxExports;
