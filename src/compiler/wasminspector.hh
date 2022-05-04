#pragma once

#include <map>
#include <set>

#include "src/cast.h"
#include "src/common.h"
#include "src/error.h"
#include "src/expr-visitor.h"
#include "src/ir.h"
#include "src/wast-lexer.h"

namespace wasminspector {

class WasmInspector : public wabt::ExprVisitor::DelegateNop
{
public:
  WasmInspector( wabt::Module* module, wabt::Errors* errors );
  WasmInspector( const WasmInspector& ) = default;
  WasmInspector& operator=( const WasmInspector& ) = default;

  wabt::Result ValidateMemAccess();
  wabt::Result ValidateImports();
  const std::set<std::string>& GetImportedFunctions();
  std::map<uint32_t, uint64_t> GetRWSizes();
  std::vector<uint32_t> GetExportedRO();
  std::vector<uint32_t> GetExportedRW();
  std::vector<wabt::Index> GetExportedROIndex() { return exported_ro_; }
  std::vector<wabt::Index> GetExportedRWIndex() { return exported_rw_; }
  std::vector<uint32_t> GetImportedROTables();
  std::vector<uint32_t> GetImportedRWTables();
  std::map<uint32_t, uint64_t> GetRWTableSizes();

  // Implementation of ExprVisitor::DelegateNop.
  wabt::Result OnMemoryCopyExpr( wabt::MemoryCopyExpr* ) override;
  wabt::Result OnMemoryFillExpr( wabt::MemoryFillExpr* ) override;
  wabt::Result OnMemoryGrowExpr( wabt::MemoryGrowExpr* ) override;
  wabt::Result OnMemoryInitExpr( wabt::MemoryInitExpr* ) override;
  wabt::Result OnStoreExpr( wabt::StoreExpr* ) override;

private:
  void VisitFunc( wabt::Func* func );
  void VisitExport( wabt::Export* export_ );
  void VisitGlobal( wabt::Global* global );
  void VisitElemSegment( wabt::ElemSegment* segment );
  void VisitDataSegment( wabt::DataSegment* segment );
  void VisitScriptModule( wabt::ScriptModule* script_module );
  void VisitCommand( wabt::Command* command );
  wabt::Result CheckMemoryAccess( wabt::Var* memidx );

  wabt::Errors* errors_ = nullptr;
  wabt::Module* current_module_ = nullptr;
  wabt::Func* current_func_ = nullptr;
  wabt::ExprVisitor visitor_;
  wabt::Result result_ = wabt::Result::Ok;

  std::set<std::string> imported_functions_;
  std::vector<wabt::Index> exported_ro_;
  std::vector<wabt::Index> exported_rw_;
};

} // namespace wasminspector
