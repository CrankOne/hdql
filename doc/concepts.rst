Concepts cheatsheet
===================

HDQL is a domain-specific language designed for querying deeply structured,
hierarchical data through a C/C++ interface. It is built around the
concept of attribute definitions (ADs) that encode both the structural and
access properties of data elements, forming explicit schemata. Queries
in HDQL are composable and stateful objects, supporting selection,
and filtering. The definition of transient attributes at runtime is
anticipated for special kind of transient compound structures.
Each query yields not only values but also associated hierarchical keys,
enabling fine-grained identification within complex data. The C API provides
a fairly efficient, zero-copy interface via opaque handles and offers mechanisms
for key handling. Oriented towards streaming access model in mind,
HDQL facilitates data introspection, event processing, and analysis
across diverse structured datasets.

Type system
-----------

 * HDQL is stronly typed, meaning that types are always known and explicit
   after expression is interpreted;
 * HDQL considers relation ("compound data to attribute datum");
 * This relation is defined by structure called attribute definition (AD)
 * Orthogonal features of AD defined by appropriate interfaces are:
    - type composition property: compound or atomic;
    - arity property: collection or scalar.
 * Compounds are fully (only) defined as a set of ADs and name;
 * Atomics are defined by named ``ValueInterface`` permitting for four types:
    - logic (bool);
    - integer;
    - floating point;
    - string.
 * Collection access is controlled by collection interface defining iteration
   and (optionally) selection routines (from C API -- via iterator);
 * Scalar access is controlled by collection interface.

This way, everything starts with defining compound as a collection
of named ADs, each AD is defined by one of two interface implementations,
defining how attribute has to be iterated. Compounds form the schema for a
data domain.


Query object
------------

Queries are the core construct of the language. They support chaining,
collection element selection, and value-based filtering.

 * A simple 1st-level query is defined by referencing AD with respect to a
   compound, delimited with period. For instance ``.foo`` refers to AD named
   "foo" in root object;
 * First-level queries always relies on some implicit root compound. Other
   level queries are chained from it as ``.foo.bar``, etc.
 * A query is a stateful generator that supports two core operations:
   "reset" (which reinitializes the internal state and is idempotent), and
   "yield" (which returns the current value and advances the query’s
   internal iterator);
 * Queries can be chained resulting in the right-to-left properties iterated
   by depth-first scheme. Lexically, it is expressed as concatenation (chaining)
   of properties with period (``.``) delimiter: ``.foo.bar``,
   ``.hits.rawData.samples``;
 * The chaining performs a reset/get for the leftmost query, then uses the
   result as the root for the next simple query, and so on;
 * Query may have "selection" assigned. Lexically it corresponds to ``.foo[23]``
   or ``.foo[2:3]``, etc. Content of selection expression is delegated to
   external interpreters -- expressions inside ``[...]`` are delibirately made
   to NOT be part of HDQL. HDQL interpreter forwards those expression to
   third-party parser via collection interface;
 * Query may have filtering expression evaluating logic expressions to
   restrict next state in "yield". Lexically it is written as
   a sub-query in curly brackets after column, e.g. ``.foo{:.a > 0}``.

All the rest lexics is evolving around the query concept formulated this way.

Virtual compounds and transient properties
------------------------------------------

Virtual compounds provides means for aliasing and re-using the queries.

 * Query may create new attributes within transient compounds. Lexically it is
   written as an expression with ``:=`` operator inside curly
   brackets: ``.foo{ c := .a+.b }``. This
   query results in a new (virtual) compound with property named ``c``;
 * It is possible to define filtering expression on newly defined transient
   compounds: ``.foo{ newAttr := a + b : .newAttr > 100 }``;
 * Multiple transient properties in a virtual compound can be defined using
   comma: ``.foo{a01 := .a0/.a1, a12 := .a1/.a2}``;
 * Technically, transient properties are created in a *virtual compound* which
   is defined at the stage when query is interpreted, contrary to usual
   compounds which are defined before and are immutable;
 * Transient properties are merely aliases for queries. This way they support
   chaining, indexing and selection features, as ordinary ADs;
 * Virtual compound AD refers to a synthetic interfaces with state
   corresponding to sub-queries;
 * Selection, defining transient properties and value filtering can be
   combined in one query expression: ``.foo[1-10]{check:=.chi2/.ndf:.check<30}``.

Virtual compounds always have a parent compound they are based on. Inheritance
chain of the copmpounds eventually ends with some non-virtual compound defined
by user's schema.

.. todo::

   It is possible that we will introduce ``as`` keyword alias for ``:=``
   operator as some user seemed to prefer keywords
   notation: ``{.uno as one, .dos.tres as three}``.

Binding queries and bound virtual compounds
-------------------------------------------

 * A virtual compound can be *bound* to a query within it, resulting in a
   collection. The ``*`` operator prefixing the query expression: ``{h := *.hits}``;
 * Bound virtual compound is a collection of compounds, resulting each compound
   instance defined for each element.

.. todo::

   In keyword-based syntax, we will probably add ``each from`` sentence instead
   of ``*`` operator: ``{each hit from .hits}``.

Query keys
----------

 * Every "yield" operation results in not only the query result itself,
   but also key or a list of "keys", where each "key" item corresponds
   to a simple query (a dereferenced AD, including scalars);
 * Transient properties create branches in the keys list, at the memory level
   expressed as nested lists. For instance ``.one{foo := .two}.foo``
   will result in one nested list to resolve transient property ``foo`` in the
   main list;
 * From C API perspective "keys" are rather complex structure including nested
   keys lists and trivial placeholders corresponding to scalar attribute
   dereferencing (since every simple query operation results in a key);
 * Because of its complexity, dealing with keys list directly for the purpose
   of obtaining their values is rather discouraged. Instead HDQL C API provides
   "key view" structure (``hdql_KeyView``) which maps meaningful keys from
   their sparse computation-efficient branching structure into
   flat C array of structures with resolved value interfaces;
 * Upon dereferencing a collection it is possible to tag a collection
   key for further handling in user API. Lexically it is expressed as
   arbitrary token given after ``->`` delimiter in square brackets. For instance
   ``.foo[->uno].bar[->dos]`` will annotated 1st and 2nd key with labels "uno"
   and "dos" correspondingly.

User code is recommended to prefer ``hdql_KeyView`` and utilize query-time labels
for meaningful access instead of direct usage of keys. The raw keys structure
remains accessible at the API level, though in practice ``hdql_KeyView`` should
be preferred. Skipping key materialization entirely may offer a minor
performance gain in specific scenarios (anticipated by the evaluation API).

C API
-----

 * All the definitions (atomic types, compounds, functions, etc) are stored
   in the structure referred as "context": ``hdql_Context``. It is opaque for
   user code and exposes limited API;
 * Context is used to interpret query expression into C data structure called
   ``hdql_Query``. It is opaque for user code and exposes limited API;
 * After query is interpreted, it is possible to inspect the AD returned by
   rightmost simple query in query chain. This AD is called "top AD" and
   can be obtained by ``hdql_query_top_attr()`` function;
 * A query can be applied to the data structure of type corresponding to the
   root compound in order to iterate over values defined by query expression
   (which are of type defined by top AD);
 * Even if the top AD is a collection, the query engine flattens this
   via iteration -- so user code should treat the result as single values
   per every yielding ("evaluate query") call;
 * Query engine avoids copying of the values as much as possible. Everything
   related to particular data (access, dereferencing, forwarding) is done
   via user-defined interfaces and is hidden behind opaque
   pointer ``hdql_Datum_t``. This pointer is similar to ``void *`` in spirit.
   All data interactions in queries (input, output) use the opaque pointer
   type ``hdql_Datum_t``.
 * Checking/guaranteeing type consistency for provided and yielded pointers
   is the user's code responsibility. HDQL engine is built to guarantee
   type integrity as long as user calls follows the rules defined by AD
   interfaces;
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

