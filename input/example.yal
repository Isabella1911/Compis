(* pico.yal — YALex specification for the PICO language *)

{
(* Generated lexer for PICO language *)
}

(* ── Character classes ──────────────────────────────── *)

let digit      = ['0'-'9']
let nonzero    = ['1'-'9']
let letter     = ['a'-'z' 'A'-'Z']
let alphanum   = letter | digit | '_'

(* ── Numeric literals ───────────────────────────────── *)

let int_lit    = '0' | nonzero digit*
let float_lit  = int_lit '.' digit+

(* ── String literal ─────────────────────────────────── *)

let str_char   = [^ '"' '\n' '\\'] | '\\' _
let string_lit = '"' str_char* '"'

(* ── Identifiers ────────────────────────────────────── *)

let ident      = letter alphanum*

(* ── Line comment ───────────────────────────────────── *)

let line_cmt   = '-' '-' [^ '\n']* '\n'

(* ══════════════════════════════════════════════════════ *)

rule gettoken =

    [' ' '\t' '\n' '\r']          { return lexbuf }
  | line_cmt                      { return lexbuf }

  | float_lit                     { return FLOAT_LIT }
  | int_lit                       { return INT_LIT }
  | string_lit                    { return STRING_LIT }

  | "true"                        { return BOOL_LIT }
  | "false"                       { return BOOL_LIT }

  | "macro"                       { return KW_MACRO }
  | "emit"                        { return KW_EMIT }
  | "when"                        { return KW_WHEN }
  | "otherwise"                   { return KW_OTHERWISE }
  | "repeat"                      { return KW_REPEAT }
  | "until"                       { return KW_UNTIL }
  | "let"                         { return KW_LET }

  | ident                         { return IDENT }

  | "<-"                          { return ASSIGN }
  | "=="                          { return EQ }
  | "!="                          { return NEQ }
  | "<="                          { return LEQ }
  | ">="                          { return GEQ }
  | "&&"                          { return AND }
  | "||"                          { return OR }

  | '+'                           { return PLUS }
  | '-'                           { return MINUS }
  | '*'                           { return TIMES }
  | '/'                           { return DIV }
  | '<'                           { return LT }
  | '>'                           { return GT }
  | '!'                           { return NOT }

  | '('                           { return LPAREN }
  | ')'                           { return RPAREN }
  | '{'                           { return LBRACE }
  | '}'                           { return RBRACE }
  | ';'                           { return SEMICOLON }
  | ','                           { return COMMA }

  | eof                           { raise( 'End of input' ) }