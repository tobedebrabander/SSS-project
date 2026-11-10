"""
guardlib - a small helper layer for writing guardcc (Part 2).

This module hides tree-sitter behind a handful of plain functions so you can
focus on the control-flow-integrity logic, not on parsing C. You never touch a
syntax tree node directly: every function here takes a `Source` and gives you
back plain data - names, booleans, byte offsets - and a `Rewrite` object lets
you insert and replace text by offset without worrying about edits shifting
each other.

Typical use in guardcc:

    from guardlib import Source, Rewrite

    def instrument(code: bytes) -> bytes:
        src = Source(code)
        rw  = Rewrite(code)
        # ... call the queries below and record edits on rw ...
        return rw.result()

The queries you will need:

    src.functions()                 -> list[Func]
    src.calls()                     -> list[Call]
    src.returns_in(func)            -> list[Return]
    src.address_taken_functions()   -> list[str]
    src.has_include("guard.h")      -> bool

A Func has:   .name, .body_open (offset just after '{'),
              .body_close (offset of the closing '}'),
              .is_void, .is_main, .falls_off_end
A Call has:   .is_indirect, .callee_start, .callee_end, .callee_text
A Return has: .start, .end, .has_value, .value_start, .value_end

A Rewrite has:  .insert(at, text)   and   .replace(start, end, text)
                .result()           applies every edit and returns bytes.

Everything about awkward C declarators (for example telling a function pointer
variable apart from a function that returns a function pointer) is handled for
you inside this module.
"""
from dataclasses import dataclass

import tree_sitter_c as _tsc
from tree_sitter import Language as _Language, Parser as _Parser

_C = _Language(_tsc.language())
_PARSER = _Parser(_C)


# ==========================================================================
# Plain-data views handed to student code
# ==========================================================================
@dataclass
class Func:
    name: str
    body_open: int       # byte offset just AFTER the opening '{'
    body_close: int      # byte offset OF the closing '}'
    is_void: bool
    is_main: bool
    falls_off_end: bool  # True if control can run off the end (no final return)


@dataclass
class Call:
    is_indirect: bool
    callee_start: int    # byte offset where the callee expression begins
    callee_end: int      # byte offset just after the callee expression
    callee_text: str


@dataclass
class Return:
    start: int           # offset of the 'return' keyword
    end: int             # offset just after the ';'
    has_value: bool
    value_start: int     # valid only if has_value
    value_end: int       # valid only if has_value


# ==========================================================================
# Edit buffer
# ==========================================================================
class Rewrite:
    """Collects text edits by byte offset and applies them safely.

    Record as many inserts/replaces as you like, in any order; result()
    applies them from right to left so offsets never shift under you. Edits
    recorded at the same offset keep the order you added them.
    """
    def __init__(self, code: bytes):
        self._code = code
        self._ops = []
        self._seq = 0

    def insert(self, at: int, text: str):
        self._ops.append((at, at, text.encode(), self._seq)); self._seq += 1

    def replace(self, start: int, end: int, text: str):
        self._ops.append((start, end, text.encode(), self._seq)); self._seq += 1

    def result(self) -> bytes:
        out = self._code
        for start, end, rep, _ in sorted(
                self._ops, key=lambda o: (o[0], o[1], o[3]), reverse=True):
            out = out[:start] + rep + out[end:]
        return out


# ==========================================================================
# Internal AST helpers (students do not need to read past here)
# ==========================================================================
def _walk(node):
    yield node
    for c in node.children:
        yield from c and _walk(c)


def _text(code, node):
    return code[node.start_byte:node.end_byte].decode(errors="replace")


def _core_identifier(node):
    """Descend a declarator chain to the name it declares; return that node."""
    n = node
    while n is not None and n.type != "identifier":
        nxt = n.child_by_field_name("declarator")
        if nxt is None:
            inner = [c for c in n.children
                     if c.type.endswith("declarator") or c.type == "identifier"]
            nxt = inner[0] if inner else None
        n = nxt
    return n


def _declares_function(declarator):
    """True if this declarator declares a function, not a variable.

    Decided by the declarator directly containing the name: a
    function_declarator immediately around the identifier means a function
    (even one returning a function pointer, e.g. int (*f(void))(int)); a
    pointer/array declarator around the identifier means a variable.
    """
    n = declarator
    parent_type = None
    while n is not None and n.type != "identifier":
        nxt = n.child_by_field_name("declarator")
        if nxt is None:
            inner = [c for c in n.children
                     if c.type.endswith("declarator") or c.type == "identifier"]
            nxt = inner[0] if inner else None
        if nxt is None:
            return False
        parent_type = n.type
        n = nxt
    return parent_type == "function_declarator"


def _func_declarator(defn):
    d = defn.child_by_field_name("declarator")
    while d is not None and d.type != "function_declarator":
        d = d.child_by_field_name("declarator")
    return d


# ==========================================================================
# Source: the query surface students use
# ==========================================================================
class Source:
    def __init__(self, code: bytes):
        self._code = code
        self._root = _PARSER.parse(code).root_node
        self._defined = self._collect_defined()

    # ---- includes -------------------------------------------------------
    def has_include(self, header: str) -> bool:
        needle_q = f'"{header}"'.encode()
        needle_a = f"<{header}>".encode()
        return needle_q in self._code or needle_a in self._code

    # ---- functions ------------------------------------------------------
    def _collect_defined(self):
        defined = {}
        for n in _walk(self._root):
            if n.type == "function_definition":
                fd = _func_declarator(n)
                if fd is None:
                    continue
                ident = _core_identifier(fd.child_by_field_name("declarator"))
                if ident is not None:
                    defined[_text(self._code, ident)] = n
        return defined

    def functions(self):
        out = []
        for name, n in self._defined.items():
            body = n.child_by_field_name("body")
            if body is None or body.type != "compound_statement":
                continue
            ret_type = n.child_by_field_name("type")
            is_void = ret_type is not None and \
                _text(self._code, ret_type).strip() == "void"
            stmts = [c for c in body.children
                     if c.is_named and c.type != "comment"]
            last = stmts[-1] if stmts else None
            falls = last is None or last.type != "return_statement"
            out.append(Func(
                name=name,
                body_open=body.start_byte + 1,
                body_close=body.end_byte - 1,
                is_void=is_void,
                is_main=(name == "main"),
                falls_off_end=falls,
            ))
        return out

    # ---- calls ----------------------------------------------------------
    def calls(self):
        """Every call in the file, each tagged as direct or indirect.

        A call is indirect when the thing being called is not a plain named
        function: a struct member, a subscript, a dereference, the result of
        another call, or a plain identifier that is a variable/parameter in
        scope (a function pointer) rather than a declared function.
        """
        var_scopes = _ScopeTracker(self._code, self._root, self._defined)
        out = []
        for n in _walk(self._root):
            if n.type != "call_expression":
                continue
            fn = n.child_by_field_name("function")
            if fn is None:
                continue
            if fn.type == "identifier":
                name = _text(self._code, fn)
                indirect = var_scopes.is_variable_at(name, fn.start_byte)
            else:
                indirect = True
            out.append(Call(
                is_indirect=indirect,
                callee_start=fn.start_byte,
                callee_end=fn.end_byte,
                callee_text=_text(self._code, fn),
            ))
        return out

    # ---- returns --------------------------------------------------------
    def returns_in(self, func: Func):
        """Every return statement inside the given function."""
        defn = self._defined[func.name]
        body = defn.child_by_field_name("body")
        out = []

        def rec(n):
            if n.type == "function_definition" and n is not defn:
                return  # do not descend into a nested function
            if n.type == "return_statement":
                value = next((c for c in n.children
                              if c.is_named and c.type != "comment"), None)
                out.append(Return(
                    start=n.start_byte,
                    end=n.end_byte,
                    has_value=value is not None,
                    value_start=value.start_byte if value else -1,
                    value_end=value.end_byte if value else -1,
                ))
            for c in n.children:
                rec(c)

        rec(body)
        return out

    # ---- address-taken analysis ----------------------------------------
    def address_taken_functions(self):
        """Names of defined functions whose address escapes.

        A function's address is taken whenever its name appears anywhere
        other than as the callee of a direct call: in an assignment, an
        initializer (including a table of function pointers), an argument, a
        return value, or after '&'.
        """
        taken = set()
        for n in _walk(self._root):
            if n.type != "identifier":
                continue
            name = _text(self._code, n)
            if name not in self._defined:
                continue
            p = n.parent
            if p is not None and p.type == "call_expression":
                callee = p.child_by_field_name("function")
                # Compare by node id, not `is`: tree-sitter hands back a fresh
                # Python object for the same tree position each time, so `is`
                # would be False even for the very same node.
                if callee is not None and callee.id == n.id:
                    continue  # direct call
            if p is not None and (p.type == "function_declarator"
                                  or p.type.endswith("declarator")):
                continue  # its own definition/prototype
            taken.add(name)
        return sorted(taken)


# --------------------------------------------------------------------------
# Scope tracking for the indirect-call decision (internal)
# --------------------------------------------------------------------------
class _ScopeTracker:
    """Answers: at byte offset X, is `name` a variable/parameter in scope?

    Built once per file. We record, for every variable/parameter declaration,
    the byte range of the scope it belongs to; a name is a variable at X if any
    such range contains X. Functions declared (not called) never count.
    """
    def __init__(self, code, root, defined):
        self._spans = {}  # name -> list of (lo, hi) scope byte ranges
        self._scan(code, root, defined)

    def _add(self, name, lo, hi):
        if name:
            self._spans.setdefault(name, []).append((lo, hi))

    def _declared_names(self, code, decl):
        names = []
        for d in decl.children:
            if d.type == "init_declarator":
                names.append(_core_identifier(d.child_by_field_name("declarator")))
            elif d.type.endswith("declarator"):
                if not _declares_function(d):
                    names.append(_core_identifier(d))
        return [n for n in names if n is not None]

    def _scan(self, code, root, defined):
        # file-scope variables
        for n in root.children:
            if n.type == "declaration":
                for ident in self._declared_names(code, n):
                    self._add(_text(code, ident), root.start_byte, root.end_byte)
        # per-function: parameters + locals, scoped to the function body
        for n in _walk(root):
            if n.type == "function_definition":
                body = n.child_by_field_name("body")
                if body is None:
                    continue
                lo, hi = body.start_byte, body.end_byte
                d = n.child_by_field_name("declarator")
                for p in _walk(d):
                    if p.type == "parameter_declaration":
                        ident = _core_identifier(p.child_by_field_name("declarator"))
                        if ident is not None:
                            self._add(_text(code, ident), lo, hi)
                for stmt in _walk(body):
                    if stmt.type == "declaration":
                        for ident in self._declared_names(code, stmt):
                            self._add(_text(code, ident), lo, hi)

    def is_variable_at(self, name, offset):
        for lo, hi in self._spans.get(name, ()):
            if lo <= offset < hi:
                return True
        return False
