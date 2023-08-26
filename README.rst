HDQL
====

This is a readme file for *hierarchical data query language* (HDQL).

HDQL is domain-specific language designed to be embeddable and reasonably
performant add-on to C/C++ projects dealing with streamed data of complex
object model. Typical applications are various scientific tasks, converters,
etc.

Usage niche is in between of low-level data processing applications usually
done with FPGA and ASM-based low-level software and high level analyzis means
like R, or Python ecosystem. For instance, in high energy physics (HEP) experiments
before analyzing a physical event one has to *reconstruct* it based on data
recorded from detectors. Detector digits corresponding to a single event are
low-level, raw measurements that need to be calibrated and explored before
they provide information on physics. Concerning complexity of the usual event
object model (multiple digits can be produced by various detector types, be
multivariate, etc) exploring them becomes a tedious task, especially regarding
that this kind applications must be performant ones even on modern computers.

HDQL provides a flexible and extensible tool to construct queries for
hierarchical data in a laconic manner (way more laconic than imperative
languages and even more expressive in its niche than SQL or GraphQL).

Disclaimer
----------

This is prototype still, yet the project is capable to perform useful
operations. This file provides specification for basic grammar and usage
example, though both can slighty change in future versions.

Basic queries
-------------

The language queries hierarchical data objects and is designed to operate
with streams (long sequences) of objects with known topology. It facilitates
basic filtering and value arithmetics, but is not capable to alterate the
topology of the data (so, this is one of many deliberate simplifications in
comparison to SQL, GraphQL, etc).

Language model relies on presence of a certain root item (a root, usually read
from stream-like sources), whose attributes can be retrieved by string
identifiers (i.e. root object has named attributes). For instance, considering
HEP physical event model, those events are statistically-independed and usually
streamed in a sequential manner. Retrieving, say, timestamp from each event
must be an easy task:

.. code-block:: hdql

    .eventTimestamp

means that one would like to retrieve an attribute of root item named
``eventTimestamp``.

Root item can (and practically always does) have sub-items within associated
collections. Those collections can be addressed using same syntax, stackign
attribute dereference operator "``.``", e.g.:

.. code-block:: hdql

    .hits.energyDeposition

This expression will retrieve ``energyDeposition`` of every item within ``hits``
collection of a root object.

Practically, collections are indexed most of the time, providing certain
key/value mapping. For instance, a simple linear array is
an ordered collection of items and each item has an index within a collection,
so one can retrieve an item with ``[]`` operator.

.. code-block:: hdql

    .hits[23].energyDeposition

will retrieve energy deposition only from item #23 if ``hits`` is the linear
array.

Changing values
---------------

.. warning::

   Changing values is foreseen, but currently not implemented feature.

One can perform various arithmetic operations with selected data, for instance:

.. code-block:: hdql

    .hits.energyDeposition *= 100

will change the ``energyDeposition`` values of all the ``.hits`` by multiplying
it by 100.

Querying collections by key
---------------------------

Collections can be indexed in different ways, so selectors in ``[]`` operator
can imply various syntax, depending on the key type. HDQL deliberately permits
for third-party grammars for such a needs. So, for instance

.. code-block:: hdql

    .hits[kin == ECAL].energyDeposition

can be a valid expression when user code provides selection for expressions
like ``kin == ECAL``.

One has to note, that queries always returns vectorised results, so for
instance both

.. code-block:: hdql

    .hits.energyDeposition

and

.. code-block:: hdql

    .hits[kin == ECAL].energyDeposition

results in a list of values. It is possible to apply arithmetical operations
on query results:

.. code-block:: hdql

    .hits[kin == HCAL].energyDeposition / 100

Such an expression will yield a sequence of energy deposition values divided by
100 and indexed values with keys selected by expression in ``[]`` (for certain
detector kin only).

Scopes and Compound Types
-------------------------

Query result can be a list of numerical values or compound objects. For instance,

.. code-block:: hdql

    .hits.distance

results in a (trivial) table of a single column and rows indexed similar to
``.hits`` attribute, while

.. code-block:: hdql

    .hits

will result of table containign all the data every entry in ``.hits``
provides (with rows indexed by hits). In case of *compound types* one can
modify a query by injecting new attributes using a *scope operator* ``{}`` and
assignment operator ``:=``:

.. code-block:: hdql

    .hits{halfDistance := .distance/2}

Resulting table will contain what usual ``hits`` entry provides plus new
attribute named ``halfDistance``.

Filtering collections
---------------------

Besides of by-key selection with ``[]`` operator one can filter compound query
results by attribute values using scope operator and *filtering* suffix
condition that should be placed into *scope* operator, after
colon (``:``) marker:

.. code-block:: hdql

    .hits{: .energyDeposition > 10}

Of course, within a scope operator creation of new attributes and filter
expressions can be combined. For instance, following expression:

.. code-block:: hdql

   .tracks{chi2ToNDF := .chi2/.ndf : .chi2ToNDF < 10}.hits[kin == MM || kin == GM]

formulated in natural language should means something like "take only those
tacks with chi square over number of degrees of freedom and retrieve tracks
provided by MicroMega or GEM detectors only".

Aggregate methods
-----------------

.. warning::

   Foreseen, but not yet supported.

Some basic aggregate methods are available: ``max()``, ``min()``, ``sum()``,
``average()``, ``median()``, ``variance()``, ``rms()``, ``unique()``,
``arbitrary()``.

.. code-block:: hdql

    sum(.hits.energyDeposition)

    max(.tracks{:.pValue > 0.05}.hits[kin == MM]{sqrt(.x**2 + .y**2 + .z**2)})

To use aggregate methods on groups, use scope operator. For instance:

.. code-block:: hdql

    .tracks{average(.hits.momentum)}

will result in momentum estimation for every track, averaged by hits.

Boolean aggregate methods: ``any()``, ``every()``, ``none()``:

.. code-block:: hdql

    any(.hits.eDep > 100)

Contrary to classic definition of aggregate methods, these functions operates
on the entire query result set, rather than various grouped ones.

TODO Lists
==========

Language Level
--------------

#. (done) basic query syntax and static compounds C API
#. (done) key selection (``[]`` operator)
#. (done) virtual compounds and scope operator
#. (done) scalar arithmetic
#. (done) scalar arithmetic on static values
#. (done) value filtering
#. (in progress) collection-and-scalar arithmetic
#. Constants (``pi``, ``exp``) and user-defined static values (external
   variables for units, like ``cm``, ``GeV``, and calibration values etc)
#. Support for scalar functions and implicit type casting (say, ``sin()`` is
   defined for floating point type, but shall accept also integers)
#. Support for aggregate functions (``sum()``, ``average()``, ``any()``, etc)
#. Standard mathematical functions (``log()``, ``atan2()``, etc)
#. Assignment and value modification operators
#. Ternary operator (``?:``) and in-range comparison (``a < b <= c``)

API Level
---------

#. (done) Compounds C API
#. (done) Query API
#. (done) Flat collection key views
#. (done) Basic type cast helpers
#. Page-aligned and pool allocators
#. C++ wrappers API
   * Template-based interface implementation generators for structs
   * Laconic query representation

Project Level
-------------

#. (done) Basic UTs (using gtest?)
#. Build system (CMake, autotools, whatever)
#. Sphinx documentation page

...and more UTs!

