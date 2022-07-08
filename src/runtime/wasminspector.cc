#include "wasminspector.hh"

#include <string>

using namespace std;

namespace wasminspector {

using namespace wabt;

WasmInspector::WasmInspector( Module* module, Errors* errors )
  : errors_( errors )
  , current_module_( module )
  , visitor_( this )
  , imported_functions_()
  , exported_ro_mem_()
  , exported_ro_mem_idx_()
  , exported_rw_mem_()
  , exported_rw_mem_idx_()
  , exported_ro_table_()
  , exported_ro_table_idx_()
  , exported_rw_table_()
  , exported_rw_table_idx_()
{
  for ( Export* export_ : module->exports ) {
    if ( export_->kind == ExternalKind::Memory ) {
      if ( export_->name.find( "ro_mem" ) != string::npos ) {
        exported_ro_mem_.push_back( module->GetMemoryIndex( export_->var ) );
        exported_ro_mem_idx_.push_back( (uint32_t)atoi( export_->name.substr( 7 ).data() ) );
      }
      if ( export_->name.find( "rw_mem" ) != string::npos ) {
        exported_rw_mem_.push_back( module->GetMemoryIndex( export_->var ) );
        exported_rw_mem_idx_.push_back( (uint32_t)atoi( export_->name.substr( 7 ).data() ) );
      }
    }

    if ( export_->kind == ExternalKind::Table ) {
      if ( export_->name.find( "ro_table" ) != string::npos ) {
        exported_ro_table_.push_back( module->GetTableIndex( export_->var ) );
        exported_ro_table_idx_.push_back( (uint32_t)atoi( export_->name.substr( 9 ).data() ) );
      }
      if ( export_->name.find( "rw_table" ) != string::npos ) {
        exported_rw_table_.push_back( module->GetTableIndex( export_->var ) );
        exported_rw_table_idx_.push_back( (uint32_t)atoi( export_->name.substr( 9 ).data() ) );
      }
    }
  }

  for ( Import* import_ : module->imports ) {
    if ( import_->kind() == ExternalKind::Func ) {
      imported_functions_.insert( import_->field_name );
    }
  }
}

Result WasmInspector::OnMemoryCopyExpr( MemoryCopyExpr* expr )
{
  return CheckMemoryAccess( &expr->destmemidx );
}

Result WasmInspector::OnMemoryFillExpr( MemoryFillExpr* expr )
{
  return CheckMemoryAccess( &expr->memidx );
}

Result WasmInspector::OnMemoryGrowExpr( MemoryGrowExpr* expr )
{
  return CheckMemoryAccess( &expr->memidx );
}

Result WasmInspector::OnMemoryInitExpr( MemoryInitExpr* expr )
{
  return CheckMemoryAccess( &expr->memidx );
}

Result WasmInspector::OnStoreExpr( StoreExpr* expr )
{
  return CheckMemoryAccess( &expr->memidx );
}

Result WasmInspector::OnTableSetExpr( TableSetExpr* expr )
{
  return CheckTableAccess( &expr->var );
}

Result WasmInspector::OnTableCopyExpr( TableCopyExpr* expr )
{
  return CheckTableAccess( &expr->dst_table );
}

Result WasmInspector::OnTableGrowExpr( TableGrowExpr* expr )
{
  return CheckTableAccess( &expr->var );
}

Result WasmInspector::OnTableFillExpr( TableFillExpr* expr )
{
  return CheckTableAccess( &expr->var );
}

Result WasmInspector::OnTableInitExpr( TableInitExpr* expr )
{
  return CheckTableAccess( &expr->table_index );
}

void WasmInspector::VisitFunc( Func* func )
{
  current_func_ = func;
  visitor_.VisitFunc( func );
  current_func_ = nullptr;
}

void WasmInspector::VisitExport( Export* export_ )
{
  if ( export_->name.find( "rw_mem" ) != string::npos ) {
    result_ = CheckMemoryAccess( &export_->var );
  }
  return;
}

void WasmInspector::VisitGlobal( Global* global )
{
  visitor_.VisitExprList( global->init_expr );
}

void WasmInspector::VisitElemSegment( ElemSegment* segment )
{
  visitor_.VisitExprList( segment->offset );
}

void WasmInspector::VisitDataSegment( DataSegment* segment )
{
  visitor_.VisitExprList( segment->offset );
}

Result WasmInspector::ValidateAccess()
{
  for ( Func* func : current_module_->funcs )
    VisitFunc( func );
  for ( Export* export_ : current_module_->exports )
    VisitExport( export_ );
  for ( Global* global : current_module_->globals )
    VisitGlobal( global );
  for ( ElemSegment* elem_segment : current_module_->elem_segments )
    VisitElemSegment( elem_segment );
  for ( DataSegment* data_segment : current_module_->data_segments )
    VisitDataSegment( data_segment );
  return result_;
}

Result WasmInspector::ValidateImports()
{
  // Module does not contain imports from other modules
  for ( Import* import : current_module_->imports ) {
    switch ( import->kind() ) {
      case ExternalKind::Global:
      case ExternalKind::Table: {
        break;
      }

      case ExternalKind::Memory:
      case ExternalKind::Func: {
        if ( import->module_name != "fixpoint" ) {
          return Result::Error;
        }
        break;
      }

      default:
        return Result::Error;
    }
  }

  // Only rw memory can have nonzero initial size
  for ( auto index : this->exported_ro_mem_ ) {
    Memory* memory = current_module_->memories[index];
    if ( memory->page_limits.initial > 0 ) {
      return Result::Error;
    }
  }

  // Only rw table can have nonzero initial size
  for ( auto index : this->exported_ro_table_ ) {
    Table* table = current_module_->tables[index];
    if ( table->elem_limits.initial > 0 ) {
      return Result::Error;
    }
  }

  return Result::Ok;
}

Result WasmInspector::Validate()
{
  auto result = ValidateAccess();
  result |= ValidateImports();
  return result;
}

Result WasmInspector::CheckMemoryAccess( Var* memidx )
{
  if ( find( exported_ro_mem_.begin(), exported_ro_mem_.end(), current_module_->GetMemoryIndex( *memidx ) )
       == exported_ro_mem_.end() ) {
    return Result::Ok;
  } else {
    return Result::Error;
  }
}

Result WasmInspector::CheckTableAccess( Var* tableidx )
{
  if ( find( exported_ro_table_.begin(), exported_ro_table_.end(), current_module_->GetTableIndex( *tableidx ) )
       == exported_ro_table_.end() ) {
    return Result::Ok;
  } else {
    return Result::Error;
  }
}
} // namespace wasminspector
