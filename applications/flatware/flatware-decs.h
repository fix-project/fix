#include "api.h"
#include "asm-flatware.h"
#include "fixpoint_util.h"

enum RO_TABLE_ID
{
  InputROTable,          // Encode tree
  ScratchROTable,        // Scratch space for c-flatware
};

enum RW_TABLE_ID
{
  FileSystemRWTable, // File system
  OutputRWTable,     // Encode tree
  ScratchRWTable,    // Scratch space
};

enum RW_MEM_ID
{
  ScratchRWMem,  // Scratch space
};

enum RO_MEM_ID
{
  ScratchROMem, // Scratch space
};

enum FLATWARE_INPUT_ENCODE
{
  INPUT_RESOURCE_LIMITS = 0,
  INPUT_PROGRAM = 1,
  INPUT_FILESYSTEM = 2,
  INPUT_ARGS = 3,
  INPUT_STDIN = 4,
  INPUT_ENV = 5,
  INPUT_RANDOM_SEED = 6,
};

enum FLATWARE_OUTPUT_ENCODE
{
  OUTPUT_EXIT_CODE = 0,
  OUTPUT_FILESYSTEM = 1,
  OUTPUT_STDOUT = 2,
  OUTPUT_STDERR = 3,
};
