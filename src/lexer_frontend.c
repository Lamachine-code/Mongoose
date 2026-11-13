#include "mongoose.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Lexer: token slices borrow the source buffer. */
typedef struct { const char*s; size_t p,n,cap; int line,col; Token*v; Diagnostic*e; } Lexer;
static char ch(Lexer*l){return l->s[l->p];}
static char step(Lexer*l){char c=l->s[l->p++];if(c=='\n'){l->line++;l->col=1;}else l->col++;return c;}
static bool emit(Lexer*l,TokenType t,size_t start,int line,int col){if(l->n==l->cap){size_t c=l->cap?l->cap*2:64;Token*q=realloc(l->v,c*sizeof*q);if(!q)return false;l->v=q;l->cap=c;}l->v[l->n++]=(Token){t,l->s+start,l->p-start,line,col};return true;}
static bool lexerr(Diagnostic*e,int line,int col,const char*m){if(e){snprintf(e->message,sizeof e->message,"%s",m);e->line=line;e->column=col;}return false;}
static TokenType kw(const char*s,size_t n){const char*words[]={"let","function","if","then","else","stop","true","false","return","and","or","not"};TokenType types[]={T_LET,T_FUNCTION,T_IF,T_THEN,T_ELSE,T_STOP,T_TRUE,T_FALSE,T_RETURN,T_AND,T_OR,T_NOT};for(size_t i=0;i<12;i++)if(strlen(words[i])==n&&!memcmp(s,words[i],n))return types[i];return T_IDENT;}
bool lex_source(const char*s,TokenArray*out,Diagnostic*e){if(!s||!out)return false;memset(out,0,sizeof*out);Lexer l={.s=s,.line=1,.col=1,.e=e};while(ch(&l)){char c=ch(&l);if(c==' '||c=='\t'||c=='\r'){step(&l);continue;}int line=l.line,col=l.col;size_t b=l.p;bool line_comment=false;if(c=='/'&&s[l.p+1]=='/'){size_t q=l.p;while(q>0&&s[q-1]!='\n'&&s[q-1]!='\r')q--;line_comment=true;for(size_t j=q;j<l.p;j++)if(s[j]!=' '&&s[j]!='\t'){line_comment=false;break;}}if(c=='#'||line_comment){while(ch(&l)&&ch(&l)!='\n')step(&l);continue;}if(c=='\n'){step(&l);if(!emit(&l,T_NEWLINE,b,line,col))goto oom;continue;}if(isalpha((unsigned char)c)||c=='_'){step(&l);while(isalnum((unsigned char)ch(&l))||ch(&l)=='_')step(&l);if(!emit(&l,kw(s+b,l.p-b),b,line,col))goto oom;continue;}if(isdigit((unsigned char)c)){step(&l);while(isdigit((unsigned char)ch(&l)))step(&l);if(ch(&l)=='.'&&isdigit((unsigned char)s[l.p+1])){step(&l);while(isdigit((unsigned char)ch(&l)))step(&l);}if(!emit(&l,T_NUMBER,b,line,col))goto oom;continue;}step(&l);TokenType t=T_ERROR;switch(c){case '+':t=T_PLUS;break;case '-':t=T_MINUS;break;case '*':if(ch(&l)=='*'){step(&l);t=T_POWER;}else t=T_STAR;break;case '/':if(ch(&l)=='/'){step(&l);t=T_FLOOR_DIV;}else t=T_SLASH;break;case '%':t=T_PERCENT;break;case '^':t=T_POWER;break;case '=':if(ch(&l)=='='){step(&l);t=T_EQ;}else t=T_ASSIGN;break;case '!':if(ch(&l)=='='){step(&l);t=T_NE;}break;case '<':if(ch(&l)=='='){step(&l);t=T_LE;}else t=T_LT;break;case '>':if(ch(&l)=='='){step(&l);t=T_GE;}else t=T_GT;break;case '(':t=T_LPAREN;break;case ')':t=T_RPAREN;break;case '[':t=T_LBRACKET;break;case ']':t=T_RBRACKET;break;case ',':t=T_COMMA;break;}if(t==T_ERROR){free(l.v);return lexerr(e,line,col,"invalid character");}if(!emit(&l,t,b,line,col))goto oom;}if(!emit(&l,T_EOF,l.p,l.line,l.col))goto oom;out->items=l.v;out->count=l.n;return true;oom:free(l.v);return lexerr(e,l.line,l.col,"out of memory in lexer");}
void free_tokens(TokenArray*t){if(t){free(t->items);memset(t,0,sizeof*t);}}

