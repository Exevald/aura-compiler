```
program          = [ module_decl ], { top_level_node } ;
top_level_node   = import_decl, ";" 
                 | export_decl, ";" 
                 | declaration, ";" 
                 | effect_def, ";" 
                 | actor_decl, ";" ;

module_decl      = "module", qualified_id, ";" ;
import_decl      = "import", qualified_id, [ "as", identifier ] ;
export_decl      = "export", ( declaration | effect_def | actor_decl | identifier ) ;
qualified_id     = identifier, { ".", identifier } ;

declaration      = var_decl 
                 | const_decl 
                 | func_decl 
                 | type_alias 
                 | struct_decl 
                 | enum_decl 
                 | interface_decl ;

type_alias       = "type", identifier, [ type_params ], "=", type ;

struct_decl      = "struct", identifier, [ type_params ], 
                   "{", { field_decl }, "}", [ contract_list ] ;
field_decl       = identifier, type_guide, ";" ;

enum_decl        = "enum", identifier, [ type_params ], 
                   "{", variant_decl, { "|", variant_decl }, "}" ;
variant_decl     = identifier, [ "(", type, { ",", type }, ")" ] ;

interface_decl   = "interface", identifier, [ type_params ], 
                   "{", { method_sig }, "}" ;
method_sig       = "fn", identifier, "(", [ param_list ], ")", type_guide, [ effect_spec ], ";" ;

type             = func_type | array_type | ptr_type | ref_type | base_type ;
func_type        = base_type, "->", type ;
array_type       = "[", type, "]" ;
ptr_type         = "ptr", "<", type, ">" ;
ref_type         = "ref", "<", type, ">" ;
base_type        = primitive_type 
                 | identifier, [ type_args ] 
                 | "(", type, ")" ;
type_args        = "<", type, { ",", type }, ">" ;
primitive_type   = "int" | "float" | "bool" | "string" | "void" | "never" ;

type_params      = "<", type_param, { ",", type_param }, ">" ;
type_param       = identifier, [ ":", type_constraint ] ;
type_constraint  = type, { "+", type } ;

type_guide       = ":", type ;

var_decl         = [ storage_modifier ], "var", identifier, [ type_guide ], [ "=", expression ] ;
const_decl       = "const", identifier, [ type_guide ], "=", expression ;
storage_modifier = "shared" | "thread_local" ;

func_decl        = [ comptime_mod ], "fn", identifier, [ type_params ], 
                   "(", [ param_list ], ")", type_guide, 
                   [ context_req ], [ effect_spec ], [ contract_list ], 
                   block_stmt ;

comptime_mod     = "comptime" ;
param_list       = parameter, { ",", parameter } ;
parameter        = identifier, [ type_guide ], [ "=", expression ] ;

context_req      = "with", "{", context_binding, { "|", context_binding }, "}" ;
context_binding  = identifier, ":", type ;

effect_spec      = "raises", "{", effect_type, { "|", effect_type }, "}" ;
effect_type      = identifier, [ type_args ] ;

contract_list    = { contract } ;
contract         = "requires", "(", expression, ")" 
                 | "ensures", "(", expression, ")" 
                 | "invariant", "(", expression, ")" ;

effect_def       = "effect", identifier, "{", { effect_op }, "}" ;
effect_op        = "fn", identifier, "(", [ param_list ], ")", [ type_guide ], ";" ;

actor_decl       = "actor", identifier, [ type_params ], 
                   "{", { actor_field }, { actor_method }, "}" ;
actor_field      = "state", identifier, type_guide, [ "=", expression ], ";" ;
actor_method     = ("msg" | "query"), identifier, "(", [ param_list ], ")", [ type_guide ], 
                   [ effect_spec ], [ contract_list ], block_stmt ;

expression       = logic_expr ;
logic_expr       = rel_expr, [ logic_expr_tail ] ;
logic_expr_tail  = ( "&&" | "||" ), rel_expr, [ logic_expr_tail ] ;

rel_expr         = arith_expr, [ rel_expr_tail ] ;
rel_expr_tail    = ( "<" | ">" | "<=" | ">=" | "==" | "!=" ), arith_expr, [ rel_expr_tail ] ;

arith_expr       = term, [ arith_expr_tail ] ;
arith_expr_tail  = ( "+" | "-" ), term, [ arith_expr_tail ] ;

term             = factor, [ term_tail ] ;
term_tail        = ( "*" | "/" | "mod" | "div" ), factor, [ term_tail ] ;

factor           = unary_op, factor 
                 | primary_expr ;

primary_expr     = "(", expression, ")" 
                 | "true" | "false" | "null" 
                 | integer_literal | float_literal | string_literal
                 | ident_expr
                 | arrow_func
                 | array_lit
                 | range_lit
                 | comptime_block ;

ident_expr       = identifier, [ type_args ], [ member_chain ], [ call_chain ], [ index_chain ] ;
member_chain     = ".", identifier, [ member_chain ] ;
call_chain       = "(", [ arg_list ], ")", [ call_chain ] ;
index_chain      = "[", expression, "]", [ index_chain ] ;

arrow_func       = "(", [ param_list ], ")", [ effect_spec ], "->", ( expression | block_stmt ) ;

array_lit        = "[", [ array_items ], "]" ;
array_items      = expression, { ",", expression } ;

range_lit        = "[", expression, "..", expression, "]" ;

comptime_block   = "comptime", block_stmt ;

unary_op         = "+" | "-" | "not" | "!" ;
arg_list         = expression, { ",", expression } ;

statement        = block_stmt
                 | var_decl, ";"
                 | assignment_stmt, ";"
                 | return_stmt, ";"
                 | if_stmt
                 | while_stmt
                 | iter_stmt
                 | handle_stmt
                 | transaction_stmt
                 | unsafe_stmt
                 | expression, ";" ;

assignment_stmt  = lvalue, "=", expression ;
lvalue           = identifier, [ member_chain ], [ index_chain ] ;

block_stmt       = "{", { statement }, "}" ;

return_stmt      = "return", [ expression ] ;

if_stmt          = "if", "(", expression, ")", block_stmt, [ else_part ] ;
else_part        = "else", block_stmt | "else", if_stmt ;

while_stmt       = "while", "(", expression, ")", block_stmt ;

iter_stmt        = "iter", "(", identifier, "of", expression, [ adapter_chain ], ")", block_stmt ;
adapter_chain    = "<", adapter, { ",", adapter }, ">" ;
adapter          = "drop", "(", expression, ")" 
                 | "take", "(", expression, ")" 
                 | "reverse" 
                 | "filter", "(", expression, ")" 
                 | "transform", "(", expression, ")" ;

handle_stmt      = "handle", expression, "with", "{", { handler }, "}" ;
handler          = "effect", identifier, "(", [ param_list ], ")", "->", block_stmt ;

transaction_stmt = "transaction", "(", region_expr, ")", block_stmt ;
region_expr      = identifier | "shared", identifier ;

unsafe_stmt      = "unsafe", block_stmt ;

context_access   = "context", ".", identifier ;

identifier       = letter, { letter | digit | "_" } ;
letter           = "A" | "B" | "C" | "D" | "E" | "F" | "G" | "H" | "I" | "J" | "K" | "L" | "M" 
                 | "N" | "O" | "P" | "Q" | "R" | "S" | "T" | "U" | "V" | "W" | "X" | "Y" | "Z" 
                 | "a" | "b" | "c" | "d" | "e" | "f" | "g" | "h" | "i" | "j" | "k" | "l" | "m" 
                 | "n" | "o" | "p" | "q" | "r" | "s" | "t" | "u" | "v" | "w" | "x" | "y" | "z" ;
digit            = "0" | "1" | "2" | "3" | "4" | "5" | "6" | "7" | "8" | "9" ;

integer_literal  = digit, { digit } ;
float_literal    = digit, { digit }, ".", digit, { digit } ;
string_literal   = '"', { any_char_except_quote }, '"' ;

eof              = "#" ;
```