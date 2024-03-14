#include "../util/fixpoint_util.h"
#include "api.h"
#include "asm-flatware.h"

enum RO_TABLE_ID
{
  InputROTable,          // Encode tree
  FileSystemBaseROTable, // File system base
  ArgsROTable,           // Args
  EnvROTable,            // Env
  ScratchROTable,        // Scratch space for c-flatware
  ScratchFileROTable,    // Scratch space for file system
  File0ROTable,          // File 0
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
  StdOutRWMem,   // Stdout
  StdErrRWMem,   // Stderr
  StdTraceRWMem, // Trace
  File0RWMem,    // File 0
};

enum RO_MEM_ID
{
  ScratchROMem, // Scratch space
  StdInROMem,   // Stdin
  File0ROMem,   // File 0
};

enum FLATWARE_INPUT_ENCODE
{
  INPUT_RESOURCE_LIMITS = 0,
  INPUT_PROGRAM = 1,
  INPUT_FILESYSTEM = 2,
  INPUT_ARGS = 3,
  INPUT_STDIN = 4,
  INPUT_ENV = 5,
};

enum FLATWARE_OUTPUT_ENCODE
{
  OUTPUT_EXIT_CODE = 0,
  OUTPUT_FILESYSTEM = 1,
  OUTPUT_STDOUT = 2,
  OUTPUT_STDERR = 3,
  OUTPUT_TRACE = 4,
};
