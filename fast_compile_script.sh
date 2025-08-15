#! /bin/bash
gcc lexer/lexer.c parser/parser.c error_manager/error_manager.c main.c -o ../paxsi
sudo cp ../paxsi /usr/local/bin

