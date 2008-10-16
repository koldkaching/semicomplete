%{ 
#include <stdio.h>
#include <string.h>

#include "conf.tab.h"
#include "grok_config.h"
#include "grok_input.h"
#include "grok_matchconf.h"

int yylineno;
void yyerror (YYLTYPE *loc, struct config *conf, char const *s) {
  fprintf (stderr, "some error: %s\n", s);
  fprintf (stderr, "Line: %d\n", yylineno);
}

#define DEBUGMASK(val) (printf("VAL: %d\n", val), (val > 0) ? ~0 : 0)
%}

%union{
  char *str;
  int num;
}

%token <str> QUOTEDSTRING
%token <num> INTEGER
%token CONF_DEBUG "debug"

%token PROGRAM "program"
%token PROG_FILE "file"
%token PROG_EXEC "exec"
%token PROG_MATCH "match"
%token PROG_LOADPATTERNS "load-patterns"

%token FILE_FOLLOW "follow"

%token EXEC_RESTARTONFAIL "restart-on-failure"
%token EXEC_MINRESTARTDELAY "minimum-restart-delay"
%token EXEC_RUNINTERVAL "run-interval"

%token MATCH_PATTERN "pattern"
%token MATCH_REACTION "reaction"

%token '{' '}' ';' ':' '\n'

%pure-parser
%parse-param {struct config *conf}
%locations

%start config

%%

config: config root 
      | root 
      | error { printf("Error: %d\n", yylloc.first_line); }

root: root_program
    | "debug" ':' INTEGER { conf->logmask = DEBUGMASK($3); }

root_program: PROGRAM '{' 
            { conf_new_program(conf);
              SETLOGMASK(*conf, CURPROGRAM); 
            }
                program_block 
              '}' 
       
program_block: program_block program_block_statement 
             | program_block_statement

program_block_statement: program_file 
                 | program_exec
                 | program_match
                 | program_load_patterns
                 | "debug" ':' INTEGER { CURPROGRAM.logmask = DEBUGMASK($3); }

program_load_patterns: "load-patterns" ':' QUOTEDSTRING 
                     { conf_new_patternfile(conf); CURPATTERNFILE = $3; }

program_file: "file" QUOTEDSTRING '{' 
              { conf_new_input(conf);
                SETLOGMASK(CURPROGRAM, CURINPUT);
                CURINPUT.type = I_FILE;
                CURINPUT.source.file.filename = $2;
                printf("curinput: %x\n", &CURINPUT);
              }
              file_block 
              '}' 

program_exec: "exec" QUOTEDSTRING '{'
              { conf_new_input(conf);
                SETLOGMASK(CURPROGRAM, CURINPUT);
                CURINPUT.type = I_PROCESS;
                CURINPUT.source.process.cmd = $2;
              }
              exec_block
              '}' 

program_match: "match" '{' 
             { conf_new_matchconf(conf);
               SETLOGMASK(CURPROGRAM, CURMATCH.grok);
             }
               match_block
               '}' 

file_block: file_block file_block_statement
          | file_block_statement

file_block_statement: /*empty*/
          | "follow" ':' INTEGER { CURINPUT.source.file.follow = $3; }
          | "debug" ':' INTEGER { CURINPUT.logmask = DEBUGMASK($3); }

match_block: match_block match_block_statement
           | match_block_statement

           
match_block_statement: /* empty */
           | "pattern" ':' QUOTEDSTRING { grok_compile(&CURMATCH.grok, $3); }
           | "reaction" ':' QUOTEDSTRING { CURMATCH.reaction.cmd = $3; }
           | "debug" ':' INTEGER { CURMATCH.grok.logmask = DEBUGMASK($3); }

exec_block: exec_block exec_block_statement
          | exec_block_statement
          
exec_block_statement: /* empty */
          | "restart-on-failure" ':'  INTEGER 
             { CURINPUT.source.process.restart_on_death = $3 }
          | "minimum-restart-delay" ':' INTEGER
             { CURINPUT.source.process.min_restart_delay = $3 }
          | "run-interval" ':' INTEGER
             { CURINPUT.source.process.run_interval = $3 }
          | "debug" ':' INTEGER { CURINPUT.logmask = DEBUGMASK($3); }