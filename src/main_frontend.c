#include "mongoose.h"
#include "compiler.h"
#include "vm.h"
#include <stdio.h>
#include <stdlib.h>

static char *read_file(const char *path) {
 FILE *file=fopen(path,"rb");
 if(!file) return NULL;
 if(fseek(file,0,SEEK_END)!=0){fclose(file);return NULL;}
 long size=ftell(file);
 if(size<0){fclose(file);return NULL;}
 rewind(file);
 char *source=malloc((size_t)size+1);
 if(!source){fclose(file);return NULL;}
 size_t count=fread(source,1,(size_t)size,file);
 if(ferror(file)){free(source);fclose(file);return NULL;}
 fclose(file); source[count]='\0'; return source;
}
static const char *diagnostic_phase_name(DiagnosticPhase phase) {
 switch(phase) {
  case DIAGNOSTIC_LEXICAL:return "lexical error";
  case DIAGNOSTIC_SYNTAX:return "syntax error";
  case DIAGNOSTIC_SEMANTIC:return "semantic error";
  case DIAGNOSTIC_COMPILE:return "compile error";
  case DIAGNOSTIC_NONE:break;
 }
 return "compile error";
}
static const char *vm_error(VMResult result) {
 switch(result) {
  case VM_STACK_OVERFLOW:return "runtime stack overflow";
  case VM_STACK_UNDERFLOW:return "runtime stack underflow";
  case VM_INVALID_BYTECODE:return "invalid bytecode";
  case VM_TYPE_ERROR:return "runtime type error";
  case VM_DIVISION_BY_ZERO:return "division by zero";
  case VM_UNDEFINED_GLOBAL:return "undefined global variable";
  case VM_CALL_OVERFLOW:return "maximum call depth exceeded";
  case VM_ARITY_ERROR:return "incorrect function argument count";
  case VM_ARRAY_REQUIRED:return "operation requires an array";
  case VM_DICTIONARY_REQUIRED:return "operation requires a dictionary";
  case VM_STRING_REQUIRED:return "dictionary key must be a string";
  case VM_INDEXABLE_REQUIRED:return "indexed value must be an array, dictionary or string";
  case VM_INDEX_ERROR:return "invalid or out-of-range index";
  case VM_OUT_OF_MEMORY:return "out of memory";
  case VM_UTF8_ERROR:return "invalid UTF-8 string";
  case VM_OK:return "success";
 }
 return "unknown runtime error";
}
int main(int argc,char **argv) {
 if(argc!=2){fprintf(stderr,"usage: mongoose <source.snake>\n");return 2;}
 char *source=read_file(argv[1]);
 if(!source){perror(argv[1]);return 2;}
 Diagnostic diagnostic={0}; TokenArray tokens={0}; Ast ast={0}; Chunk chunk;
 chunk_init(&chunk); VM vm; vm_init(&vm);
 bool ok=lex_source(source,&tokens,&diagnostic);
 if(!ok) diagnostic.phase=DIAGNOSTIC_LEXICAL;
 if(ok) { ok=parse_tokens(&tokens,&ast,&diagnostic); if(!ok) diagnostic.phase=DIAGNOSTIC_SYNTAX; }
 if(ok) { ok=analyze_ast(&ast,&diagnostic); if(!ok) diagnostic.phase=DIAGNOSTIC_SEMANTIC; }
 if(ok) { ok=compile_ast(&ast,&chunk,&diagnostic); if(!ok) diagnostic.phase=DIAGNOSTIC_COMPILE; }
 if(!ok) fprintf(stderr,"%s:%d:%d: %s: %s\n",argv[1],diagnostic.line,diagnostic.column,
                 diagnostic_phase_name(diagnostic.phase),diagnostic.message[0]?diagnostic.message:"compilation failed");
 else {
  VMResult result=vm_run(&vm,&chunk);
  if(result!=VM_OK){
   fprintf(stderr,"%s:%d: runtime error: %s\n",argv[1],vm.error_line,vm_error(result));
   for(size_t i=vm.frame_count;i>0;--i){
    CallFrame frame=vm.frames[i-1];
    FunctionInfo function=chunk.functions[frame.function_index];
    int line=(i==vm.frame_count)?vm.error_line:vm.frames[i].call_line;
    fprintf(stderr,"  at %.*s (%s:%d)\n",(int)function.name_length,function.name,argv[1],line);
   }
   if(vm.frame_count) fprintf(stderr,"  at <main> (%s:%d)\n",argv[1],vm.frames[0].call_line);
   ok=false;
  }
 }
 vm_free(&vm); chunk_free(&chunk); free_ast(&ast); free_tokens(&tokens); free(source);
 return ok?0:1;
}
