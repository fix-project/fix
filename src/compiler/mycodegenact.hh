#include <clang/CodeGen/CodeGenAction.h>

class EmitObjToMemory : public clang::CodeGenAction
{
    virtual void anchor();
    
    std::unique_ptr<llvm::raw_pwrite_stream> OS_;

  public:
    EmitObjToMemory( std::unique_ptr<llvm::raw_pwrite_stream> OS, llvm::LLVMContext *_VMContext = nullptr );

  protected:
    void ExecuteAction() override;
};
