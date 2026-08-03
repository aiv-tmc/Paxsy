" Vim syntax file
" Language:     Paxsy
" Maintainer:   Your Name <email>
" Last Change:  2026-07-24
" Description:  Syntax highlighting for the Paxsy programming language
"               Based on the official language specification.

if exists("b:current_syntax")
  finish
endif

" ---------------------------------------------------------------------
" Keywords
" ---------------------------------------------------------------------

" Main control structures, declarations, operators
syn keyword paxsyKeyword    func let comptime use module spawn defer
syn keyword paxsyKeyword    return if else match case while do for
syn keyword paxsyKeyword    break continue pass volatile asm alloc free
syn keyword paxsyKeyword    extern null none error try catch
syn keyword paxsyKeyword    true false and or not proc self

" Modifiers
syn keyword paxsyStorageClass  extern volatile

" Conditionals and loops (for more precise grouping)
syn keyword paxsyConditional   if else match case
syn keyword paxsyRepeat        while do for
syn keyword paxsyStatement     return defer break continue pass spawn
syn keyword paxsyException     try catch error

" ---------------------------------------------------------------------
" Data types
" ---------------------------------------------------------------------
syn keyword paxsyType      Int8 Int16 Int32 Int64 Int128 Int256
syn keyword paxsyType      UInt8 UInt16 UInt32 UInt64 UInt128 UInt256
syn keyword paxsyType      Real32 Real64 UTF8 UTF16 UTF32
syn keyword paxsyType      Bool Void Auto Char Int UInt Real Thread

" Composite types (Struct, Enum, Union)
syn keyword paxsyStructure Struct Enum Union

" ---------------------------------------------------------------------
" Operators and punctuation
" ---------------------------------------------------------------------
" Two- and three-character operators
syn match paxsyOperator /==/
syn match paxsyOperator /<>/
syn match paxsyOperator />=/
syn match paxsyOperator /<=/
syn match paxsyOperator />>/
syn match paxsyOperator /<</
syn match paxsyOperator />>>/
syn match paxsyOperator /<<</
syn match paxsyOperator /+=/
syn match paxsyOperator /-=/
syn match paxsyOperator /\*=/
syn match paxsyOperator /\/=/
syn match paxsyOperator /%=/
syn match paxsyOperator /\$=/
syn match paxsyOperator /&=/
syn match paxsyOperator /\^=/
syn match paxsyOperator />>=/
syn match paxsyOperator /<<=/
syn match paxsyOperator />>>=/
syn match paxsyOperator /<<<=/
syn match paxsyOperator /\.\./
syn match paxsyOperator /::/
syn match paxsyOperator /<-/

" Single operators and separators
" (order matters: specific first, then general)
syn match paxsyOperator /[+\-*/%]/    " arithmetic
syn match paxsyOperator /[=<>]/       " comparison and assignment (additional)
syn match paxsyOperator /[~$&^]/      " bitwise
syn match paxsyOperator /[()\[\]{};,]/
syn match paxsyOperator /|/
syn match paxsyOperator /&/           " address-of / bitwise AND
syn match paxsyOperator /@/           " dereference
syn match paxsyOperator /\./          " field access (does not capture numbers)
syn match paxsyOperator /!/
syn match paxsyOperator /?/

" ---------------------------------------------------------------------
" Numeric literals
" ---------------------------------------------------------------------
" Integers: decimal, binary, octal, hexadecimal
" Separators _ allowed, suffixes U (unsigned) and f (Real32)
syn match paxsyNumber /\<[0-9][0-9_]*\([uU]\)\?\>/
syn match paxsyNumber /\<0b[01][01_]*\([uU]\)\?\>/
syn match paxsyNumber /\<0o[0-7][0-7_]*\([uU]\)\?\>/
syn match paxsyNumber /\<0x[0-9A-Fa-f][0-9A-Fa-f_]*\([uU]\)\?\>/

" Floating-point numbers (decimal and hexadecimal)
syn match paxsyNumber /\<[0-9][0-9_]*\.[0-9][0-9_]*\([eE][+-]\?[0-9][0-9_]*\)\?\([fF]\)\?\>/
syn match paxsyNumber /\<[0-9][0-9_]*[eE][+-]\?[0-9][0-9_]*\([fF]\)\?\>/
syn match paxsyNumber /\<0x[0-9A-Fa-f][0-9A-Fa-f_]*\.[0-9A-Fa-f][0-9A-Fa-f_]*\([pP][+-]\?[0-9][0-9_]*\)\?\([fF]\)\?\>/

" ---------------------------------------------------------------------
" Strings and interpolation
" ---------------------------------------------------------------------
" Regular string with possible interpolation {expression}
syn region paxsyString start=+"+ end=+"+ contains=paxsyInterpolation

" Interpolation: expression inside { } highlighted as Special
syn match paxsyInterpolation /{[^}]*}/ contained

" ---------------------------------------------------------------------
" Comments
" ---------------------------------------------------------------------
" Single-line //
syn match paxsyComment /\/\/.*$/ contains=paxsyTodo

" Multi-line /* ... */
syn region paxsyComment start="\/\*" end="\*\/" contains=paxsyTodo

" Special markers TODO, FIXME, XXX
syn keyword paxsyTodo contained TODO FIXME XXX

" ---------------------------------------------------------------------
" Folding
" ---------------------------------------------------------------------
" Blocks { } are foldable
syn region paxsyBrace matchgroup=paxsyOperator start="{" end="}" contains=TOP fold

" For module and comptime you could add folding, but they use the same
" braces, so this is enough.

" ---------------------------------------------------------------------
" Linking groups to highlighting
" ---------------------------------------------------------------------
hi def link paxsyKeyword       Keyword
hi def link paxsyStorageClass  StorageClass
hi def link paxsyConditional   Conditional
hi def link paxsyRepeat        Repeat
hi def link paxsyStatement     Statement
hi def link paxsyException     Exception
hi def link paxsyType          Type
hi def link paxsyStructure     Structure
hi def link paxsyOperator      Operator
hi def link paxsyNumber        Number
hi def link paxsyString        String
hi def link paxsyInterpolation Special
hi def link paxsyComment       Comment
hi def link paxsyTodo          Todo

let b:current_syntax = "paxsy"
