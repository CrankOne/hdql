Concepts cheatsheet
===================

HDQL is a domain-specific language designed for querying deeply structured,
hierarchical data through a unified C/C++ interface. It centers around the
concept of attribute definitions (ADs) that encode both the structural and
access properties of data elements, forming explicit compound schemas. Queries
in HDQL are composable and stateful, supporting operations like selection,
filtering, and the definition of transient (virtual) attributes at runtime.
Each query yields not only values but also associated hierarchical keys,
enabling fine-grained identification within complex data. The C API provides
an efficient, zero-copy interface via opaque handles and offers mechanisms
for safe key handling. Designed with flexibility and streaming in mind,
HDQL facilitates dynamic data introspection, event processing, and analysis
across diverse structured datasets.

Type system
-----------

 * HDQL considers relation ("compound data to attribute datum")
 * This relation is defined by structure called attribute definition (AD)
 * Main features of AD defined by appropriate interfaces are:
    - arity property: collection/scalar
    - type property: compound/atomic
 * Compounds are fully defined as set of ADs -- not less, not more.
 * Atomics are defined by ``ValueInterface``
 * Collection access is controlled by collection interface
 * Scalar access is controlled by collection interface

This way, everything starts with defining compound as a collection
of named ADs, each AD is defined by two interface implementations.
Compounds (defined as a set of named ADs) form the schema for a data domain.
Each AD encodes both the structure (arity/type) and access semantics of a datum.


Query object
------------

 * A simple query is defined by referencing AD with respect to a compound,
   for instance ``.foo`` refers to AD named "foo" in root object.
 * Queries always relies on some implicit root compound
 * A query is a stateful generator that supports two core operations:
   "reset" (which reinitializes the internal state and is idempotent), and
   "yield" (which returns the current value and advances the query’s
   internal iterator).
 * Queries can be chained resulting in the rightmost property iterated
   by depth-first scheme. Lexically, it is expressed as concatenation
   of properties with period (``.``) delimiter: ``.foo.bar``,
   ``.hits.rawData.samples``
 * The chaining performs a reset/get for the leftmost query, then uses the
   result as the root for the next simple query, and so on.
 * Query may have "selection" assigned. Lexically it corresponds to ``.foo[23]``
   or ``.foo[2:3]``, etc. Content of selection expression is NOT part of HDQL,
   HDQL just forwards expression in square brackets to third-party parser
   via collection interface
 * Query may have filtering expression evaluating logic expressions to
   restrict next state in "yield". Lexically it is written as
   a sub-query in curly brackets after column, e.g. ``.foo{:.a > 0}``
 * Query may create transient attributes. Lexically it is written as
   an expression with ``:=`` operator inside curly brackets: ``.foo{ c := .a+.b }``
 * It is possible to define filtering expression on newly defined transient
   ADs: ``.foo{ newAttr := a + b : .newAttr > 100 }``
 * Multiple transient properties can be defined using
   comma: ``.foo{a01 := .a0/.a1, a12 := .a1/.a2}``
 * Technically, transient properties are created in a *virtual compound* which
   is defined at the stage when query is interpreted, contrary to usual
   compounds which are defined before and are immutable
 * Transient properties are defined as full-fledged queries supporting chaining,
   indexing and selection features.
 * Virtual compound AD refers to a synthetic interfaces with state
   corresponding to sub-queries
 * Selection, defining transient properties and value filtering can be
   combined in one query expression: ``.foo[1-10]{check:=.chi2/.ndf:.check<30}``

Queries are the core construct of the language. They support chaining,
collection element selection, and value-based filtering.


Query keys
----------

 * Every "yield" operation results in not only the query result itself,
   but also key or a list of "keys", where each "key" item corresponds
   to a simple query (a dereferenced AD, including scalars)
 * Transient properties create branches in the keys list, at the memory level
   expressed as nested lists. For instance ``.one{foo := .two}.foo``
   will result in one nested list to resolve transient property ``foo`` in the
   main list.
 * From C API perspective "keys" are rather complex structure including nested
   keys lists and trivial placeholders corresponding to scalar attribute
   dereferencing (since every simple query operation results in a key)
 * Because of its complexity, dealing with keys list directly for the purpose
   of obtaining their values is rather discouraged. Instead HDQL C API provides
   "key view" structure (``hdql_KeyView``) which maps meaningful keys into
   flat C array of structures with resolved value interfaces.
 * Upon dereferencing a collection it is possible to tag a collection
   key for further handling in user API. Lexically it is expressed as
   arbitrary token given after ``->`` delimiter in square brackets. For instance
   ``.foo[->uno].bar[->dos]`` will annotated 1st and 2nd key with labels "uno"
   and "dos" correspondingly.

User code is recommended to prefer ``hdql_KeyView`` and utilize query-time labels
for meaningful access instead of direct usage of keys. The raw keys structure
remains accessible at the API level, though in practice ``hdql_KeyView`` should
be preferred. Skipping key materialization entirely may offer a minor
performance gain in specific scenarios.


C API
-----

 * All the definitions (atomic types, compounds, functions, etc) are stored
   in the structure referred as "context": ``hdql_Context``. It is opaque for
   user code and exposes limited API.
 * Context is used to interpret query expression into C data structure called
   ``hdql_Query``. It is opaque for user code and exposes limited API.
 * After query is interpreted, it is possible to inspect the AD returned by
   rightmost simple query in query chain. This AD is called "top AD" and
   can be obtained by ``hdql_query_top_attr()`` function
 * A query can be applied to the data structure of type corresponding to the
   root compound in order to iterate over values defined by query expression
   (which are of type defined by top AD)
 * Even if the top AD is a collection, the query engine flattens this
   via iteration -- so user code should treat the result as single values
   per every "yield" call.
 * Query engine avoids copying of the values as much as possible. Everything
   related to particular data (access, dereferencing, forwarding) is done
   via user-defined interfaces and is hidden behind opaque
   pointer ``hdql_Datum_t``. This pointer is similar to ``void *`` in spirit.
   All data interactions in queries (input, output) use the opaque pointer
   type ``hdql_Datum_t``.
 * Checking/guaranteeing type consistency for provided and yielded pointers
   is the user's code responsibility. HDQL engine is built to guarantee
   type integrity according to rules defined by AD interfaces.
 * A compiled query (``hdql_Query`` instance) maintains internal state and is
   *not thread-safe*. It should only be used with one root datum at a
   time. However, due to its low memory footprint, multiple instances of the
   same query can be evaluated in parallel for concurrent processing.

This way, practically, the user code:
    1. Maintains context with definitions (see C context API defined in
       ``hdql_context_...()`` functions)
    2. Uses context to interpret user-defined query expression into
       query object, knowing root compound type: ``hdql_compile_query()``
    3. Uses query object to apply it on the datum subject (of root
       compound type) -- a query "reset" operation: ``hdql_query_reset()``
    4. Invokes 1+ times ``hdql_query_get()`` to perform "yield"
       operation. Each time expecting result to be in agreement with
       what is returned by ``hdql_query_top_attr()``.

It is common to inspect the top AD after query compilation and emit an error if
the result type can not be handled or not expected.

Note that query result can be both -- of atomic and of a compound type.
Both cases are generally valid from HDQL perspective, but user might
want to restrict (and assure) specific returned type.

