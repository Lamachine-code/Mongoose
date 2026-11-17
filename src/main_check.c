#include "mongoose.h"
#include <stdio.h>
#include <stdlib.h>
static const char *diagnostic_phase_name(DiagnosticPhase phase) {
 switch(phase) {
  case DIAGNOSTIC_LEXICAL:return "lexical error";
  case DIAGNOSTIC_SYNTAX:return "syntax error";
  case DIAGNOSTIC_SEMANTIC:return "semantic error";
  case DIAGNOSTIC_COMPILE:return "compile error";
  case DIAGNOSTIC_NONE:break;
 }
 return "frontend error";
}
int main(int argc,char **argv) {
 if(argc!=2) return 2;
 FILE *f=fopen(argv[1],"rb"); if(!f) return 2;
 if(fseek(f,0,SEEK_END)!=0){fclose(f);return 2;}
 long n=ftell(f); if(n<0){fclose(f);return 2;} rewind(f);
 char *s=malloc((size_t)n+1); if(!s){fclose(f);return 2;}
 size_t got=fread(s,1,(size_t)n,f); bool io_error=ferror(f)!=0; fclose(f);
 if(io_error){free(s);return 2;} s[got]='\0';
 Diagnostic d={0}; TokenArray t={0}; Ast a={0};
 bool ok=lex_source(s,&t,&d);
 if(!ok)d.phase=DIAGNOSTIC_LEXICAL;
 if(ok){ok=parse_tokens(&t,&a,&d);if(!ok)d.phase=DIAGNOSTIC_SYNTAX;}
 if(ok){ok=analyze_ast(&a,&d);if(!ok)d.phase=DIAGNOSTIC_SEMANTIC;}
 if(!ok)fprintf(stderr,"%s:%d:%d: %s: %s\n",argv[1],d.line,d.column,diagnostic_phase_name(d.phase),d.message[0]?d.message:"frontend error");
 free_ast(&a);free_tokens(&t);free(s);return ok?0:1;
}
