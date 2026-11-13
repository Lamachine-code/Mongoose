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
static const char *vm_error(VMResult result) {
 switch(result) {
  case VM_STACK_OVERFLOW:return "runtime stack overflow";
  case VM_STACK_UNDERFLOW:return "runtime stack underflow";
  case VM_INVALID_BYTECODE:return "invalid bytecode";
  case VM_TYPE_ERROR:return "runtime type error";
  case VM_DIVISION_BY_ZERO:return "division by zero";
  case VM_UNDEFINED_GLOBAL:return "undefined global variable";
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
 if(ok) ok=parse_tokens(&tokens,&ast,&diagnostic);
 if(ok) ok=analyze_ast(&ast,&diagnostic);
 if(ok) ok=compile_ast(&ast,&chunk,&diagnostic);
 if(!ok) fprintf(stderr,"%s:%d:%d: %s\n",argv[1],diagnostic.line,diagnostic.column,
                 diagnostic.message[0]?diagnostic.message:"compilation failed");
 else {
  VMResult result=vm_run(&vm,&chunk);
  if(result!=VM_OK){fprintf(stderr,"%s: runtime error: %s\n",argv[1],vm_error(result));ok=false;}
 }
 chunk_free(&chunk); free_ast(&ast); free_tokens(&tokens); free(source);
 return ok?0:1;
}
