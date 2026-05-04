Note about binding operator
===========================

For simple case of single query like ``{h:=.hits}`` and ``{h:=*.hits}`` the
difference may be not obvious at first glance: the first results in a scalar
item of virtual compound type defined for each root one, the second results
in a collection of items defined for every "hit" in ``.hits``. However, for
more complex case like ``{t:=.tracks, h:=.hits}`` and
``{t:=*.tracks, h:=*.hits}`` the difference is very important as the second
one results in a Cartesian product -- new item of virtual compound type is
returned by a query for each unique pair of hit and track from the collections.

This binding operator transcends some fundamental limitations of maintaining
single chain of iterators within a single query. For instance, if each ``hits``
compound element defines ``amplitude`` and ``time`` scalar atomic properties
and root object defines ``masterTime`` scalar atomic, it is not otherwise
possible to iterate simultaneously over pairs of amplitude and time minus
master time. Query like ``{dt := hits.time - .masterTime, a := hits.amplitude}``
will result in ``dt`` and ``a`` being independent properties, while in
the compound defined on top of ``.hits`` (considering each hit from collection)
there is no means to request ``masterTime`` from parent compound.

Generally, it seeemed that three operators are somewhat equivalent in the
expressive capabilities and the impact on evaluation model they can offer:

- Binding operator ``{h:=*.hits, mt:=.masterTime}{dt:=h.time - .mt, amp:=h.amplitude}``
- Cartesian product, like ``{p:=product(.hits, .masterTime)}{p.t - p.masterTime, p.amplitude}``
- Access to previous query in chain ``.hits{dt := .t - parent.masterTime, amp:=.amplitude}``

While parent query access introduce complications to the existing query state
model, first option was choosen over Cartesian product function because it is
fairly explicit and is seemingly more natural with respect to existing syntax.

