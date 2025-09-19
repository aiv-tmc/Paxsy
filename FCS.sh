#! /bin/bash
gcc src/preprocessor/preprocessor.c src/lexer/lexer.c src/parser/parser.c src/semantic/semantic.c src/error_manager/error_manager.c src/main.c -o paxsy && cp paxsy /usr/local/bin
