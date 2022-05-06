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

  wabt::Result Validate();
  const std::set<std::string>& GetImportedFunctions() { return imported_functions_; }
  std::vector<uint32_t> GetExportedROMems() { return exported_ro_mem_idx_; }
  std::vector<uint32_t> GetExportedRWMems() { return exported_rw_mem_idx_; }
  std::vector<uint32_t> GetExportedROTables() { return exported_ro_table_idx_; }
  std::vector<uint32_t> GetExportedRWTables() { return exported_rw_table_idx_; }
  std::vector<wabt::Index> GetExportedROMemIndex() { return exported_ro_mem_; }
  std::vector<wabt::Index> GetExportedRWMemIndex() { return exported_rw_mem_; }

  // Implementation of ExprVisitor::DelegateNop.
  wabt::Result OnMemoryCopyExpr( wabt::MemoryCopyExpr* ) override;
  wabt::Result OnMemoryFillExpr( wabt::MemoryFillExpr* ) override;
  wabt::Result OnMemoryGrowExpr( wabt::MemoryGrowExpr* ) override;
  wabt::Result OnMemoryInitExpr( wabt::MemoryInitExpr* ) override;
  wabt::Result OnTableSetExpr( wabt::TableSetExpr* ) override;
  wabt::Result OnTableCopyExpr( wabt::TableCopyExpr* ) override;
  wabt::Result OnTableGrowExpr( wabt::TableGrowExpr* ) override;
  wabt::Result OnTableFillExpr( wabt::TableFillExpr* ) override;
  wabt::Result OnTableInitExpr( wabt::TableInitExpr* ) override;
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
  wabt::Result CheckTableAccess( wabt::Var* tableidx );
  wabt::Result ValidateAccess();
  wabt::Result ValidateImports();

  wabt::Errors* errors_ = nullptr;
  wabt::Module* current_module_ = nullptr;
  wabt::Func* current_func_ = nullptr;
  wabt::ExprVisitor visitor_;
  wabt::Result result_ = wabt::Result::Ok;

  std::set<std::string> imported_functions_;
  std::vector<wabt::Index> exported_ro_mem_;
  std::vector<uint32_t> exported_ro_mem_idx_;
  std::vector<wabt::Index> exported_rw_mem_;
  std::vector<uint32_t> exported_rw_mem_idx_;
  std::vector<wabt::Index> exported_ro_table_;
  std::vector<uint32_t> exported_ro_table_idx_;
  std::vector<wabt::Index> exported_rw_table_;
  std::vector<uint32_t> exported_rw_table_idx_;
};

} // namespace wasminspector
