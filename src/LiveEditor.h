#pragma once
#include "Core.h"
#include <TextEditor.h>
namespace cycode::reader {
bool pullLiveDocument(TextEditor &,const LiveDocument &,uint64_t &appliedRevision,size_t &undoVersion);
}
