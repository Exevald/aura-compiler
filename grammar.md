```
program = module_decl top_level_list | top_level_list ;

module_decl = "module" qualified_id ";" ;

top_level_list = top_level_node top_level_list | EPSILON ;
top_level_node = import_decl
               | export_decl
               | declaration
               | effect_def
               | actor_decl
               | statement ;

import_decl = "import" qualified_id import_as_opt ";" ;
import_as_opt = "as" identifier | EPSILON ;

export_decl = "export" exportable ";" ;
exportable = declaration_no_semi | effect_def_no_semi | actor_decl_no_semi | identifier ;

qualified_id = identifier qualified_id_tail ;
qualified_id_tail = "." identifier qualified_id_tail | EPSILON ;

declaration = var_decl
            | const_decl
            | type_alias
            | func_decl
            | struct_decl
            | enum_decl
            | interface_decl ;

declaration_no_semi = var_decl_no_semi | const_decl_no_semi | func_decl | type_alias_no_semi | struct_decl_no_semi | enum_decl_no_semi | interface_decl_no_semi ;

var_decl = var_decl_no_semi ";" ;
var_decl_no_semi = "var" identifier type_guide_opt assign_expr_opt
                 | "shared" "var" identifier type_guide_opt assign_expr_opt
                 | "thread_local" "var" identifier type_guide_opt assign_expr_opt ;

type_guide_opt = ":" dataType | EPSILON ;
assign_expr_opt = "=" expression | EPSILON ;

const_decl = const_decl_no_semi ";" ;
const_decl_no_semi = "const" identifier type_guide_opt "=" expression ;

func_decl = "fn" identifier type_params_opt "(" param_list_opt ")" type_guide_opt context_req_opt effect_spec_opt contract_list block_stmt
          | "comptime" "fn" identifier type_params_opt "(" param_list_opt ")" type_guide_opt context_req_opt effect_spec_opt contract_list block_stmt ;

type_alias = type_alias_no_semi ";" ;
type_alias_no_semi = "type" identifier type_params_opt "=" dataType ;

struct_decl = struct_decl_no_semi ;
struct_decl_no_semi = "struct" identifier type_params_opt implements_opt "{" struct_member_list "}" contract_list ;
implements_opt = "implements" interface_type_list | EPSILON ;
interface_type_list = qualified_id interface_type_list_tail ;
interface_type_list_tail = "," qualified_id interface_type_list_tail | EPSILON ;
struct_member_list = struct_member struct_member_list | EPSILON ;
struct_member = field_decl | struct_method ;
struct_method = "fn" identifier "(" param_list_opt ")" type_guide_opt effect_spec_opt contract_list block_stmt ;
field_decl = identifier ":" dataType ";" ;

enum_decl = enum_decl_no_semi ;
enum_decl_no_semi = "enum" identifier type_params_opt "{" variant_list "}" ;
variant_list = variant_decl variant_list_tail ;
variant_list_tail = "|" variant_decl variant_list_tail | EPSILON ;
variant_decl = identifier variant_args_opt ;
variant_args_opt = "(" dataType_list ")" | EPSILON ;

interface_decl = interface_decl_no_semi ;
interface_decl_no_semi = "interface" identifier type_params_opt "{" method_sig_list "}" ;
method_sig_list = method_sig method_sig_list | EPSILON ;
method_sig = "fn" identifier "(" param_list_opt ")" type_guide_opt effect_spec_opt ";" ;

dataType_list = dataType dataType_list_tail ;
dataType_list_tail = "," dataType dataType_list_tail | EPSILON ;
dataType = base_dataType dataType_tail ;
dataType_tail = "->" dataType | EPSILON ;

base_dataType = "int" | "float" | "bool" | "string" | "void" | "never"
          | identifier type_args_opt
          | "[" dataType "]"
          | "ptr" "<" dataType ">"
          | "ref" "<" dataType ">"
          | "(" dataType ")" ;

type_args_opt = "<" dataType_list ">" | EPSILON ;
type_params_opt = "<" type_params ">" | EPSILON ;
type_params = type_param type_params_tail ;
type_params_tail = "," type_param type_params_tail | EPSILON ;
type_param = identifier type_constraint_opt ;
type_constraint_opt = ":" type_constraint | EPSILON ;
type_constraint = dataType type_constraint_tail ;
type_constraint_tail = "+" dataType type_constraint_tail | EPSILON ;

expression = assignment_expr ;
assignment_expr = logic_or assignment_tail ;
assignment_tail = "=" assignment_expr | EPSILON ;

logic_or = logic_and logic_or_tail ;
logic_or_tail = "or" logic_and logic_or_tail | "||" logic_and logic_or_tail | EPSILON ;

logic_and = equality logic_and_tail ;
logic_and_tail = "and" equality logic_and_tail | "&&" equality logic_and_tail | EPSILON ;

equality = relational equality_tail ;
equality_tail = "==" relational equality_tail | "!=" relational equality_tail | EPSILON ;

relational = additive relational_tail ;
relational_tail = "<" additive relational_tail | ">" additive relational_tail | "<=" additive relational_tail | ">=" additive relational_tail | EPSILON ;

additive = multiplicative additive_tail ;
additive_tail = "+" multiplicative additive_tail | "-" multiplicative additive_tail | EPSILON ;

multiplicative = unary multiplicative_tail ;
multiplicative_tail = "*" unary multiplicative_tail | "/" unary multiplicative_tail | "mod" unary multiplicative_tail | "div" unary multiplicative_tail | EPSILON ;

unary = primary | unary_op unary ;
unary_op = "+" | "-" | "not" | "!" | "*" | "&" ;

primary = "(" expression ")" | "true" | "false" | "null" | "return" | integer_literal | float_literal | string_literal | identifier_expr | arrow_func | array_lit | "comptime" block_stmt ;

identifier_expr = identifier trailer_list ;
trailer_list = trailer trailer_list | EPSILON ;
trailer = "." identifier | "(" arg_list_opt ")" | "[" expression "]" ;

arg_list_opt = arg_list | EPSILON ;
arg_list = expression arg_list_tail ;
arg_list_tail = "," expression arg_list_tail | EPSILON ;

param_list_opt = param_list | EPSILON ;
param_list = parameter param_list_tail ;
param_list_tail = "," parameter param_list_tail | EPSILON ;
parameter = identifier type_guide_opt assign_expr_opt ;

statement = statement_no_semi
          | statement_with_semi ";"
          | ";" ;

statement_no_semi = block_stmt
                  | if_stmt
                  | while_stmt
                  | iter_stmt
                  | handle_stmt
                  | transaction_stmt
                  | unsafe_stmt ;

statement_with_semi = var_decl_no_semi
                    | const_decl_no_semi
                    | return_stmt
                    | print_stmt
                    | expression ;

block_stmt = "{" statement_list "}" ;
statement_list = statement statement_list | EPSILON ;

print_stmt = "print" expression ;

return_stmt = "return" expression_opt ;
expression_opt = expression | EPSILON ;

if_stmt = "if" "(" expression ")" block_stmt else_opt ;
else_opt = "else" block_or_if | EPSILON ;
block_or_if = block_stmt | if_stmt ;

while_stmt = "while" "(" expression ")" block_stmt ;

iter_stmt = "iter" "(" identifier "of" expression adapter_chain_opt ")" block_stmt ;
adapter_chain_opt = "with" "[" adapter_list "]" | EPSILON ;
adapter_list = adapter adapter_list_tail ;
adapter_list_tail = "," adapter adapter_list_tail | EPSILON ;
adapter = "drop" "(" expression ")" | "take" "(" expression ")" | "reverse" | "filter" "(" expression ")" | "transform" "(" expression ")" ;

handle_stmt = "handle" expression "with" "{" handler_list "}" ;
handler_list = handler handler_list | EPSILON ;
handler = "effect" identifier "(" param_list_opt ")" "->" block_stmt ;

transaction_stmt = "transaction" "(" region_expr ")" block_stmt ;
region_expr = identifier | "shared" identifier ;

unsafe_stmt = "unsafe" block_stmt ;

context_req_opt = "with" "{" context_binding_list "}" | EPSILON ;
context_binding_list = context_binding context_binding_list_tail ;
context_binding_list_tail = "|" context_binding context_binding_list_tail | EPSILON ;
context_binding = identifier ":" dataType ;

effect_spec_opt = "raises" "{" effect_type_list "}" | EPSILON ;
effect_type_list = effect_type effect_type_list_tail ;
effect_type_list_tail = "|" effect_type effect_type_list_tail | EPSILON ;
effect_type = identifier type_args_opt ;

contract_list = contract contract_list | EPSILON ;
contract = "requires" "(" expression ")" | "ensures" "(" expression ")" | "invariant" "(" expression ")" ;

effect_def = effect_def_no_semi ;
effect_def_no_semi = "effect" identifier effect_body_opt ;
effect_body_opt = "{" method_sig_list "}" | EPSILON ;

actor_decl = actor_decl_no_semi ;
actor_decl_no_semi = "actor" identifier type_params_opt "{" actor_body "}" ;
actor_body = actor_field_list actor_method_list ;
actor_field_list = actor_field actor_field_list | EPSILON ;
actor_field = "state" identifier ":" dataType assign_expr_opt ";" ;
actor_method_list = actor_method actor_method_list | EPSILON ;
actor_method = msg_or_query identifier "(" param_list_opt ")" type_guide_opt effect_spec_opt contract_list block_stmt ;
msg_or_query = "msg" | "query" ;

arrow_func = "fn" "(" param_list_opt ")" effect_spec_opt "->" arrow_body ;
arrow_body = expression | block_stmt ;
array_lit = "[" arg_list_opt "]" ;
```