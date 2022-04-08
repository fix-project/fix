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
  , exported_ro_()
  , exported_rw_()
{
  for ( Export* export_ : module->exports ) {
    if ( export_->kind == ExternalKind::Memory ) {
      if ( export_->name.find( "ro_mem" ) != string::npos ) {
        exported_ro_.push_back( module->GetMemoryIndex( export_->var ) );
      }
      if ( export_->name.find( "rw_mem" ) != string::npos ) {
        exported_rw_.push_back( module->GetMemoryIndex( export_->var ) );
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

Result WasmInspector::ValidateMemAccess()
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
        if ( import->module_name != "env" ) {
          return Result::Error;
        }
        break;
      }

      default:
        return Result::Error;
    }
  }

  // All ro and rw memories are imported
  for ( auto index : this->exported_ro_ ) {
    if ( index >= current_module_->num_memory_imports ) {
      return Result::Error;
    }
  }
  
  for ( auto index : this->exported_rw_ ) {
    if ( index >= current_module_->num_memory_imports ) {
      return Result::Error;
    }
  }

  // Only rw memory can have nonzero initial size
  for ( auto index : this->exported_ro_ ) {
    Memory* memory = current_module_->memories[index];
    if ( memory->page_limits.initial > 0 ) {
      return Result::Error;
    }
  }
  return Result::Ok;
}

Result WasmInspector::CheckMemoryAccess( Var* memidx )
{
  if ( find( exported_ro_.begin(), exported_ro_.end(), current_module_->GetMemoryIndex( *memidx ) )
       == exported_ro_.end() ) {
    return Result::Ok;
  } else {
    return Result::Error;
  }
}

const set<string>& WasmInspector::GetImportedFunctions()
{
  return imported_functions_;
}

std::vector<uint32_t> WasmInspector::GetExportedRO()
{
  std::vector<uint32_t> result;
  for ( Export* export_ : current_module_->exports ) {
    if ( export_->kind == ExternalKind::Memory ) {
      if ( export_->name.find( "ro_mem" ) != string::npos ) {
        result.push_back( (uint32_t)atoi( export_->name.substr( 7 ).data() ) );
      }
    }
  }
  return result;
}

std::vector<uint32_t> WasmInspector::GetExportedRW()
{
  std::vector<uint32_t> result;
  for ( Export* export_ : current_module_->exports ) {
    if ( export_->kind == ExternalKind::Memory ) {
      if ( export_->name.find( "rw_mem" ) != string::npos ) {
        result.push_back( (uint32_t)atoi( export_->name.substr( 7 ).data() ) );
      }
    }
  }
  return result;
}

std::map<uint32_t, uint64_t> WasmInspector::GetNonZeroRW()
{
  std::map<uint32_t, uint64_t> result;
  for ( Import* import_ : current_module_->imports ) {
    if ( import_->kind() == ExternalKind::Memory ) {
      if ( import_->field_name.find( "rw_mem" ) != string::npos && cast<MemoryImport>(import_)->memory.page_limits.initial != 0) {
        result.insert( {(uint32_t)atoi( import_->field_name.substr( 7 ).data() ), cast<MemoryImport>(import_)->memory.page_limits.initial} );
      }
    }
  }
  return result;
} 
} // namespace wasminspector
