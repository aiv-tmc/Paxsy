#! /bin/bash
gcc src/lexer/lexer.c src/parser/parser.c src/error_manager/error_manager.c src/main.c -o paxsi && cp paxsi /usr/local/bin
