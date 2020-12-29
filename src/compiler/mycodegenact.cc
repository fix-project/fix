#include <clang/CodeGen/BackendUtil.h> 

#include "mycodegenact.hh"

using namespace llvm;
using namespace clang;

void EmitObjToMemory::anchor() {}

void EmitObjToMemory::ExecuteAction() {}

EmitObjToMemory::EmitObjToMemory( std::unique_ptr<raw_pwrite_stream> OS,
                                  llvm::LLVMContext *_VMContext ) 
                                  : CodeGenAction( Backend_EmitObj, _VMContext ),
                                    OS_( std::move( OS ) ) {}

