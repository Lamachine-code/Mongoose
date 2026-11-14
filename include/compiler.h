#ifndef MONGOOSE_COMPILER_H
#define MONGOOSE_COMPILER_H

#include "mongoose.h"
#include "chunk.h"

bool compile_ast(const Ast *ast, Chunk *chunk, Diagnostic *error);

#endif
