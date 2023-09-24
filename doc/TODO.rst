
TODO list
=========

Points here are marked in a notation defining certain order and rough topic.
For instance ``FIX2`` means 2nd item of issue to fix. Implemented items got
deleted description, but their mark and theme is left in the document to keep
brief changelog.

Improvements and augmentation
-----------------------------

Items done (changelog)

 - LNG1. [done] basic query syntax and static compounds C API
 - LNG2. [done] key selection (``[]`` operator)
 - LNG3. [done] virtual compounds and scope operator
 - LNG4. [done] scalar arithmetic
 - LNG5. [done] scalar arithmetic on static values
 - LNG6. [done] value filtering
 - LNG7. [done] collection-and-scalar arithmetic
 - LNG9. [done] Support for scalar functions and implicit type casting
 - API2. [done] CMake/autotools-based build configuration system
 - LNG11. [done, 444d554] Standard mathematical functions (``log()``, ``atan2()``, etc)

LNG12. Support for scalar arithmetics attribute
~~~~

Namely, one needs to implement treatment of expressions like this:

 .. code-block:: hdql

    .event.tracks{chi2prob(.chi2, .ndf) : size > 3}

This must yield list indexed by keys from `.tracks` collection attribute with
values corresponding to `chi2prob(.chi2, .ndf)` of every track. Currently,
this can be achieved by

 .. code-block:: hdql

    .event.tracks{prob := chi2prob(.chi2, .ndf) : size > 3}.prob

i.e. via virtual compound. This way one create redundant query instances
loosing performance while the case when we are interested in only one
calculated value is pretty frequent in applications.


LNG13. Implement standard functions/aggreagate methods
~~~~

 - ``all()``/``any()``/``none()`` -- logic concatenation
 - ``max()``/``min()``/``arb()`` -- element selection (augmented with support compounds
   with custom filters? sort()?)
 - ``mean()``/``stddev()``/``median()``/``standardize()``... -- statistics

LNG14. External constants
~~~~

    Mathematical/physics/user defined constants as both dynamic and static
    expressions: exp/pi/cm/mm/etc, connection with external dictionaries, etc.

LNG15. Assignment/data modification operators
~~~~

``=``, ``+=`` etc, properly account for "read-only" property. Probably will
require changes in query evaluation machine as violates functional paradigm in
some way...

LNG16. Ternary operator (`?:`)
~~~~

On first look seems to be useful, but might be redundant. Practical
experience needed.

LNG17. Ternary comparison (`a < b < c`)
~~~~

Can be done at the parser level by concatenating with AND, perhaps no need
for dedicated interface.

LNG18. Page-alignmed memory allocator for context
~~~~

Should bring some benefits on performance.

LNG19. Scope inheritance for types, functions, conversions, etc
~~~~

A must for applications: for certain package user code should be capable to
inherit context with predefined assets to avoid frequent re-initialization and
duplication of what is considered "standard" for their package.

API1. Support for compounds in auto-function helper
~~~~

Currently ``hdql::helpers::AutoFunction<>`` does not support user-defined
compound types. To implement this one has to think on closer cooperation of
this helper and ``hdql::helpers::Compounds`` which at the first look is bad
decision as it makes two optional parts of the API dependant.

API2. Additional data associated/retrieved within context
~~~

Context is an object shared between various user's routines. That *can* be
useful for tagged user's data, like:

.. code-block:: cpp

    // in user code
    hdql_context_associate("one", (void*) one);

    // in iface code
    int one = *((int *) hdql_get("one"));

A flaw here is that it is user's responsibility to bother on type validity...

API3. Support for variadic-sized types
~~~~

Types with variadic sizes should be of use in certain applications, e.g. a
string key in collections.

DOC1. Sphinx/Doxygen-based documentation pages
~~~~

Usual setup of Doxygen XML output + Sphinx-based page rendering seems decent.

DOC2. Introductory tutorial, cheatsheet
~~~~

On a simple example, shall explain how to cope this thing to C++ structs,
XML/whatever. Would be nice also to split it onto basic (C-only) API part
and part with C++ helpers.

DOC3. API doc
~~~~

Current doxygen-based comments are very crude, we should provide a better
structure at some point.

LNG20. Key variables (``coll[foo:=bar]``)
~~~~

Required in apps. Probably will require additional lexical features.

Fixes
-----

FIX1. Fix double call to `dereference()` method
~~~~

Happens with scalar value acces interface at least in some circumustances.

So far it does not create much trouble, but may lead to performance losses.


