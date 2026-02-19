```
program         = [ module_decl ], { top_level_node } ;
top_level_node  = import_decl ";" 
                | export_decl ";" 
                | declaration ";" 
                | effect_def ";" 
                | actor_decl ";" ;

module_decl     = "module" qualified_id ";" ;
import_decl     = "import" qualified_id [ "as" identifier ] ;
export_decl     = "export" exportable ;
exportable      = declaration 
                | effect_def 
                | actor_decl 
                | identifier ;

qualified_id    = identifier { "." identifier } ;

declaration     = var_decl 
                | const_decl 
                | func_decl 
                | type_alias 
                | struct_decl 
                | enum_decl 
                | interface_decl ;

var_decl        = [ storage_modifier ] "var" identifier [ type_guide ] [ "=" expression ] ;
const_decl      = "const" identifier [ type_guide ] "=" expression ;
storage_modifier = "shared" | "thread_local" ;

func_decl       = [ "comptime" ] "fn" identifier [ type_params ] 
                  "(" [ param_list ] ")" type_guide 
                  [ context_req ] [ effect_spec ] [ contract_list ] 
                  block_stmt ;

type_alias      = "type" identifier [ type_params ] "=" type ;

struct_decl     = "struct" identifier [ type_params ] 
                  "{" { field_decl } "}" [ contract_list ] ;
field_decl      = identifier type_guide ";" ;

enum_decl       = "enum" identifier [ type_params ] 
                  "{" variant_decl { "|" variant_decl } "}" ;
variant_decl    = identifier [ "(" [ type { "," type } ] ")" ] ;

interface_decl  = "interface" identifier [ type_params ] 
                  "{" { method_sig } "}" ;
method_sig      = "fn" identifier "(" [ param_list ] ")" type_guide 
                  [ effect_spec ] ";" ;

type            = func_type | array_type | ptr_type | ref_type | base_type ;
func_type       = base_type "->" type ;
array_type      = "[" type "]" ;
ptr_type        = "ptr" "<" type ">" ;
ref_type        = "ref" "<" type ">" ;
base_type       = primitive_type 
                | identifier [ type_args ] 
                | "(" type ")" ;
type_args       = "<" type { "," type } ">" ;
primitive_type  = "int" | "float" | "bool" | "string" | "void" | "never" ;

type_params     = "<" type_param { "," type_param } ">" ;
type_param      = identifier [ ":" type_constraint ] ;
type_constraint = type { "+" type } ;

type_guide      = ":" type ;

expression      = logic_or_expr ;

logic_or_expr   = logic_and_expr { "||" logic_and_expr } ;
logic_and_expr  = equality_expr { "&&" equality_expr } ;
equality_expr   = relational_expr { ( "==" | "!=" ) relational_expr } ;
relational_expr = additive_expr { ( "<" | ">" | "<=" | ">=" ) additive_expr } ;
additive_expr   = multiplicative_expr { ( "+" | "-" ) multiplicative_expr } ;
multiplicative_expr = unary_expr { ( "*" | "/" | "mod" | "div" ) unary_expr } ;

unary_expr      = unary_op unary_expr 
                | primary_expr ;
unary_op        = "+" | "-" | "not" | "!" ;

primary_expr    = "(" expression ")" 
                | "true" 
                | "false" 
                | "null" 
                | integer_literal 
                | float_literal 
                | string_literal
                | identifier_expr
                | arrow_func
                | array_lit
                | "comptime" block_stmt ;

identifier_expr = identifier [ type_args ] { member_access | call | index_access } ;
member_access   = "." identifier ;
call            = "(" [ arg_list ] ")" ;
index_access    = "[" expression "]" ;

arrow_func      = "(" [ param_list ] ")" [ effect_spec ] "->" ( expression | block_stmt ) ;

array_lit       = "[" [ expression { "," expression } ] "]" ;
arg_list        = expression { "," expression } ;
param_list      = parameter { "," parameter } ;
parameter       = identifier [ type_guide ] [ "=" expression ] ;

statement       = block_stmt
                | simple_stmt ";" ;

simple_stmt     = var_decl
                | const_decl
                | assignment_stmt
                | return_stmt
                | if_stmt
                | while_stmt
                | iter_stmt
                | handle_stmt
                | transaction_stmt
                | unsafe_stmt
                | expression ;

assignment_stmt = lvalue "=" expression ;
lvalue          = identifier { member_access | index_access } ;

block_stmt      = "{" { statement } "}" ;

return_stmt     = "return" [ expression ] ;

if_stmt         = "if" "(" expression ")" block_stmt [ "else" ( block_stmt | if_stmt ) ] ;
while_stmt      = "while" "(" expression ")" block_stmt ;

iter_stmt       = "iter" "(" identifier "of" expression [ adapter_chain ] ")" block_stmt ;
adapter_chain   = "<" adapter { "," adapter } ">" ;
adapter         = "drop" "(" expression ")" 
                | "take" "(" expression ")" 
                | "reverse" 
                | "filter" "(" expression ")" 
                | "transform" "(" expression ")" ;

handle_stmt     = "handle" expression "with" "{" { handler } "}" ;
handler         = "effect" identifier "(" [ param_list ] ")" "->" block_stmt ;

transaction_stmt = "transaction" "(" region_expr ")" block_stmt ;
region_expr     = identifier | "shared" identifier ;

unsafe_stmt     = "unsafe" block_stmt ;

context_req     = "with" "{" context_binding { "|" context_binding } "}" ;
context_binding = identifier ":" type ;

effect_spec     = "raises" "{" effect_type { "|" effect_type } "}" ;
effect_type     = identifier [ type_args ] ;

contract_list   = { contract } ;
contract        = "requires" "(" expression ")" 
                | "ensures" "(" expression ")" 
                | "invariant" "(" expression ")" ;

effect_def      = "effect" identifier "{" { effect_op } "}" ;
effect_op       = "fn" identifier "(" [ param_list ] ")" [ type_guide ] ";" ;

actor_decl      = "actor" identifier [ type_params ] 
                  "{" { actor_field } { actor_method } "}" ;
actor_field     = "state" identifier type_guide [ "=" expression ] ";" ;
actor_method    = ( "msg" | "query" ) identifier "(" [ param_list ] ")" 
                  [ type_guide ] [ effect_spec ] [ contract_list ] block_stmt ;

identifier      = letter { letter | digit | "_" } ;
integer_literal = digit { digit } ;
float_literal   = digit { digit } "." digit { digit } ;
string_literal  = '"' { any_char_except_quote } '"' ;
```