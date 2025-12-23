# DIANA REFERENCE MANUAL

**Draft Revision 4 - May 1986**

**Authors:**
- Kathryn L. McKinley
- Carl F. Schaefer

**Organization:** Intermetrics, Inc.

**Prepared For:** Naval Research Laboratory, Washington, D.C. 20375

**Contract:** N00014_84-C_2445

---

## TABLE OF CONTENTS

## CHAPTER 1 INTRODUCTION

1.1 THE DESIGN OF DIANA ............ ................... ............ 1-2
1.2 THE DEFINITION OF THE DIANA OPERATIONS .I.............. 1-4
1.3 THE DEFINITION OF A DIANA USER ............ .............. ............ 1-5
1.4 THE STRUCTURE OF THIS DOCUMENT ............ .............. ............ 1-7
1.4.1 NOTATION ............ ......................... 1-7
## CHAPTER 2 IDL SPECIFICATION

## CHAPTER 3 SEMANTIC SPECIFICATION

* 3.1 ALL DECL ............ ......................... 3-3
3.1.1 ITEM ............ ........................ ............ 3-3
3.1.1.1 DSCRMTPARAM DECL ............ ................. ............ 3-4
3.1.1.1.1 PARAM ............ ....................... ............ 3-4
3.1.1.2 SUBUNITBODY ............ .................... ............ 3-4
3.1.1.3 DECL ............ ....................... ............ 3-5
3.1.1.3.1 USEPRAGMA ............ ..................... 3-5
3.1.1.3.2 REP ............ ........................ ............ 3-5
3.1.1.3.2.1 NAMEDREP ............ ..................... 3-5
3.1.1.3.3 ID OECL....................... ............ ............ ............ 3-6
3.1.1.3.3.1 SIMPLE RENAMEDECL ............ ..................... .3-7
3.1.1.3.3.2 UNIT DECL ............ ............ ............ 3-8
3.1.1.3.3.2.1 NON GENERIC DECL ............ .................. ............ 3-8
3.1.1.3.4 ID SDECL ............ ............ ............ 3-9
3.1.1.3.4.1 EXP DECL ............ ...................... 3-9
3.1.1.3.4.1.1 OBJECTDECL ............ .................... .3-10
3.2 DEFNAME ............ ......................... ............ 3-13
3.2.1 PREDEF NAME ............ .................... 3-14
3.2.2 SOURCE NAME ............ .................... .3-14
3.2.2.1 LABEL NAME ............ ..................... ............ 3-14
3.2.2.2 TYPE NAME ............ ..................... 3-15
3.2.2.3 OBJECT NAME ............ .................... ............ 3-16
3.2.2.3.1 ENUM_LITERAL ............ .................... ............ 3-16
3.2.2.3.2 INIT_OBJECTNAME ............ .................. ............ 3-17
3.2.2.3.2.1 VC NAME ............ ............ ............ .. 3-17
3.2.2.3.2.2 COMPNAME ............ ..................... 3-18
3.2.2.3.2.3 PARAM NAME . . . . . . . . . . . . . . . . . . . . . 3-18
3.2.2.4 UNIT NAME ............ ..................... 3-19
3.2.2.4.1 NON TASK NAME ............ ................... .3-19
3.2.2.4.1.1 SUBPROG PACKNAME ............ ................. 3-20
3.2.2.4.1.1.1 SUBPROGNAME ............ .................... 3-20
3.3 TYPESPEC ............ ........................ ............ 3-25
3.3.1 DERIVABLE_SPEC ............ ................... .3-25
3.3.1.1 PRIVATE SPEC ............ .................... ............ 3-26
3.3.1.2 FULL TYPE SPEC ............ ................... ............ 3-27
3.3.1.2.1 NON TASK ............ ....... ............ .. 3-27
3.3.1.2.1.1 SCALAR ............ ....................... ............ 3-28
3.3.1.2.1.1.1 REAL
3.3.1.2.1.2 UNCONSTRAINED. ............ 3-29
3.3.1.2.1.2.1 UNCONSTRAINED COMPOSITE. ............ 3-29
3.3.1.2.1.3 CONSTRAINED ............ .................... ............ 3-30
3.4 TYPE DEF ............ ......................... 3-33
3.4.1 CONSTRAINED DEF. ............ 3-33
3.4.2 ARRACCDER_DEF ............ 3-34
3.5 CONSTRAINT ............ ........................ 3-36
3.5.1 DISCRETE RANGE
3.5.1.1 RANGE ............ ........................ ............ 3-36
3.5.2 REALCONSTRAINT ............ ................... ............ 3-37
3.6 UNIT OESC ............ ........................ 3-39
3.6.1 UNIT KIND ............ ...................... ............ 3-39
3.6.1.1 RENAME INSTANT ............ .................... 3-40
3.6.1.2 GENERICPARAM ............ .................... 3-42
3.6.2 BODY ............ ......................... 3-42
3.7 HEADER ............ .......................... ............ 3-44
3.7.1 SUBP ENTRYHEADER ............ ................... ............ 3-44
3.8- GENERAL ASSOC ............ ...................... ............ 3-46
3.8.1 NAMEDASSOC ............ .................... 3-46
3.8.2 EXP ............ ........................ 3-47
3.8.2.1 NAME ............ ........................ ............ 3-47
3.8.2.1.1 DESIGNATOR ............ ............ ............ 3-47
3.8.2.1.1.1 USED OBJECT ............ ..... ............ 3-47
3.8.2.1.1.2 USED NAME ............ ..................... 3-48
3.8.2.1.2 NAME EXP ............ ...................... ............ 3-49
3.8.2.1.2.1 NAME VAL ............ ...................... ............ 3-49
3.8.2.2 EXP EXP ............ ...................... ............ 3-50
3.8.2.2.1 AGG_EXP ............ ...................... ............ 3-51
3.8.2.2.2 EXP VAL ............ ..................... ............ 3-52
3.8.2.2.2.1 EXP_VAL EXP ............ .................... 3-53
3.8.2.2.2.1.1 MEMBERSHIP ............ ..................... ............ 3-53
3.8.2.2.2.1.2 QUAL_CONV ............ ..................... ............ 3-53
3.9 STM ELEM ............ ......................... ............ 3-56
3.9.1 STM ............ ........................ 3-56
3.9.1.1 BLOCK LOOP ............ ..................... ............ 3-57
3.9.1.2 ENTRY_STM ............ ..................... ............ 3-57
3.9.1.3 CLAUSES STM ............ .................... 3-57
3.9.1.4 STM WITH EXP ............ .................... 3-58
3.9.1.4.1 STM WITH EXP NAME ............ ................. ............ 3-58
3.9.1.5 STM WITHNAME ............ ................... ............ 3-58
3.9.1.5.1 CALLSTM.. ............ ..................... 3-59
3.10 MISCELLANEOUS NODES AND CLASSES ............ ............. ............ 3-61
3.10.1 CHOICE ............ ....................... ............ 3-61
3.10.2 ITERATION ............ ..................... ............ 3-61
I
I
- 3.10.2.1 FOR REV ............ ...................... ............ 3-62
3.10.3 MEMBERSHIP OP ............ ................... ............ 3-62
3.10.4 SHORT CIRCUIT OP ............ .................. ............ 3-62
3.10.5 ALIGNMENT CLAUSE ............ .................. ............ 3-62
3.10.6 VARIANT PART ............ .................... ............ 3-62
3.10,7 TEST CLAUSE ELEM ............ .................. ............ 3-63
3.10.7.1 TEST CLAUSE ............ .................... ............ 3-63
3.10.8 ALTERNATIVE ELEM ............ .................. ............ 3-63
3.10.9 COMP REP ELEM
3.10.10 CONTEXT ELEM ............ .................... ............ 3-64
3.10.11 VARIANT_ELEM ............ .................... ............ 3-64
3.10.12 compilation ............ .................... ............ 3-64
3.10.13 compilationunit ............ .................. ............ 3-65
3.10.14 comp_list . . . . . . . . . . . . . . . . . . . . . 3-65
3.10.15 index ............ ....................... .3-65
## CHAPTER 4 RATIONALE

### 4.1 DESIGN DECISIONS
* 4.1.1 INDEPENDENCE OF REPRESENTATION ............ ........... ............ 4-2
4.1.1.1 SEPARATE COMPILATION ............ .. 4-3
4.1.2 EFFICIENT IMPLEMENTATION AND SUITABLITY FOR
VARIOUS KINDS OF PROCESSING ............ ............. 4-3
4.1.2.1 STATIC SEMANTIC INFORMATION ............ ............. 4-4
4.1.2.2 WHAT IS 'EASY TO RECOMPUTE'? ............ ............ 4-5
4.1.3 REGULARITY OF DESCRIPTION ............ .............. .4-5
### 4.2 DECLARATIONS
4.2-1 MULTIPLE ENTITY DECLARATION ............ 47
4.2.1.1 OBJECT DECLARATIONS AND COMPONENT DECLARATIONS . . 4-7
4.2.2 SINGLE ENTITY DECLARATIONS ............ .............. ..4-8
4.2.2.1 PROGRAM UNIT DECLARATIONS AND ENTRY DECLARATIONS
4.2.3 REPRESENTATION CLAUSES, USE CLAUSES, AND PRAGMAS . 4-11
4.2.4 GENERIC FORMAL PARAMETER DECLARATIONS ............ .. 4-11
4.2.5 IMPLICIT DECLARATIONS ............ ............... ............ 4-12
4.3 SIMPLE NAMES ............ ....................... .4-13
4.3.1 DEFININ' OCCURRENCES OF PREDE7INED ENTITIES 4-1.'
4.3.2 MULTIPLE DEFINING OCCURRENCES
4.3.2.1 MULTIPLE DEFINING OCCURRENCES OF TYPE NAMES 4-16
4.3.2.2 MULTIPLE DEFINING OCCURRENCES OF TASK NAMES
4.3.3 USED OCCURRENCES ............ .................. ............ 4-17
4.4 TYPES AND SUBTYPES ............ ............ 4-19
4.4.1 CONSTRAINED AND UNCONSTRAINED TYPES AND SUBTYPES 4-22
4.4.2 UNIVERSAL TYPES ............ .................. ............ 4-23
4.4.3 DERIVED TYPES ............ ................... ............ 4-23
4.4.4 PRIVATE, LIMITED PRIVATE, AND LIMITED TYPES ............ ............ 4-24
4.4.5 INCOMPLETE TYPES ............ .................. ............ 4-27
4.4.6 GENERIC FORMAL TYPES ............ ................ ............ 4-28
4.4.7 REPRESENTATION INFORMATION ............ ............. ............ 4-29
4.5 CONSTRAINTS ............ ....................... ............ 4-31
4.6 EXPRESSIONS ............ ... ............ 4-35
4.6.1 EXPRESSIONS WHICH INTRODUCE ANONYMOUS SUBTYPES . . 4-36
4.6.2 FUNCTION CALLS AND OPERATORS ............ ............ 4-37
4.6.3 IMPLICIT CONVERSIONS ............ ................ 4-37
4.6.4 PARENTHESIZED EXPRESSIONS ............ ............. ............ 4-39
4.6.5 ALLOCATORS
4.6.6 AGGREGATES AND STRING LITERALS
4.7 PROGRAM UNITS. ............ ............ ............ ............ ............ ............ ............ ............ ............ ............ ............ ............ ............ ............ ............ ............ ............ ............ ............ .. 4-43
4.7.1 RENAMED UNITS . . . . . . . . . . . . . . . . . . . 4-43
4.7.2 GENERIC INSTANTIATIONS ............ ............... .4-45
4.7.3 TASKS ............ ....................... 4-48
4.7.4 USER_DEFINED OPERATORS. ............ ............... .4-49
4.7.5 DERIVED SUBPROGRAMS ............ ................ 4-50
4.8 PRAGMAS ............ ......................... 4-52
## CHAPTER 5 EXAMPLES

## CHAPTER 6 EXTERNAL REPRESENTATION OF DIANA

## CHAPTER 7 THE DIANA PACKAGE IN ADA

APPENDIX A DIANA CROSS_REFERENCE GUIDE ............ ............... ............ A_I
APPENDIX 8 REFERENCES ............ ........................ ............ B_I
Acknowledgements for the First Edition
DIANA is based on two earlier proposals for intermediate forms for Ada
programs: TCOL and AIDA. It could not have been designed without the efforts
of the two groups that designed these previous schemes. Thus we are deeply
grateful to:
- AIDA: Manfred Dausmann, Guido Persch, Sophia Drossopoulou, Gerhard
7 Goos, and Georg Winterstein -- all from the University of Karlsruhe.
- TCOL: Benjamin Brosgol (Intermetrics), Joseph Newcomer
(Carnegie-Mellon University), David Lamb (CMU), David Levine
- (Intermetrics), Mary Van Deusen (Prime), and Wm. Wulf (CMU).
The actual design of DIANA was conducted by teams from Karlsruhe,
Carnegie-Mellon, Intermetrics and Softech. Those involved were Benjamin
Brosgol, Manfred Dausmann, Gerhard Goos, David Lamb, John Nestor, Richard
Simpson, Michael Tighe, Larry Weissman, Georg Winterstein, and Wm. Wulf.
Assistance in creation of the document was provided by Jeff Baird, Dan Johnston,
Paul Knueven, Glenn Marcy, and Aaron Wohl -- all from CMU.
We are grateful for the encouragement and support provided for this effort
by Horst Clausen (IABG), Larry Druffel (DARPA), and Marty Wolfe (CENTACS) as
well as our various funding agencies.
Finally, the design of DIANA was conducted at Eglin Air Force Base with
substantial support from Lt. Col. W. Whitaker. We could not have done it
without his aid.
DIANA's original design was funded by Defense Advanced Research Projects
Agency (DARPA), the Air Force Avionics Laboratory, the Department of the Army,
Communication Research and Development Command, and the Bundesamt fuer
Wehrtechnik und Beschaffung.
Gerhard Goos
Wm. A. Wulf
Editors, First Edition
-J
Acknowledgements For The Second Edition
Subsequent to DIANA's original design, the Ada Joint Program Office of the
United States Department of Defense has supported at Tartan Laboratories,
Incorporated a continuing effort at revision. This revision has been performed
by Arthur Evans, Jr., and Kenneth J. Butler, with considerable assistance from
John R. Nestor and Wm. A. Wulf, all of Tartan.
We are grateful to the following for their many useful comments and
suggestions.
o Georg Winterstein, Manfred Dausmann, Sophia Droussopoulou, Guido
Persch, and Jergen Uhl, all of the Karlsruhe Ada Implementation Group;
o Julie Sussman and Rich Shapiro of Bolt Beranek and Newman, Inc.; and to
o Charles Wetherell and Peggy Quinn of Bell Telephone Laboratories.
Additional comments and suggestions have been offered by Grady Booch, Benjamin
Brosgol, Gil Hanson, Jeremy Holden, Bernd Krieg-Brueckner, David Lamb, H.-H.
Nageli, Teri Payton, and Richard Simpson.
We thank the Ada Joint Program Office (AJPO) for supporting DIANA's
revision, and in particular Lt. Colonel Larry Druffel, the director of AJPO.
Valuable assistance as Contracting Officer's Technical Representative was
provided first by Lt. Commander Jack Kramer and later by Lt. Commander Brian
Schaar; we are pleased to acknowledge them.
DIANA is being maintained and revised by Tartan Laboratories Inc. for the
Ada Joint Program Office of the Department of Defense under contract number
MOAg03-82-C_0148 (expiration date: 28 February 1983). The Project Director of
DIANA Maintenance for Tartan is Arthur Evans, Jr.
Acknowledgements For This Edition
The United States Department of Defense has continued to support DIANA
through a DIANA maintenance program and revision effort at Intermetrics, Inc.
This draft revision of the Diana Reference Manual has been prepared by Carl
Schaefer and Kathryn McKinley.
The authors are grateful to David Lamb (Queen's University) for his
excellent recommendations on restructuring the classes of DIANA, to Rudy Krutar
(Naval Research Laboratory) for his valuable assistance as Contracting Officer's
Technical Representative, and to Ben Hyde and Gary Bray (Intermetrics) for their
valuable suggestions. Kellye Sheehan (MCC) contributed to the early stages of
the maintenance effort.
DIANA is being maintained and revised by Intermetrics, Inc. for the Ada
Joint Program Office of the Department of Defense under contract number
N00014_84-C_2455, administered by the Naval Research Laboratory. The contract
expires 30 September 1986. The Project Director of DIANA Maintenance for
Intermetrics is Carl Schaefer.
Preface to the First Edition
This document defines DIANA, an intermediate form of Ada [7] programs that
is especially suitable for communication between the Front and Back Ends of Ada
compilers. It is based on the Formal Definition of Ada (6] and resulted from
the merger of the best aspects of two previous proposals: AIDA [4, 10] and TCOL
[2]. Although DIANA is primarily intended as an interface between the parts of
a comoiler, it is also suitable for other programming support tools and
carefully retains the structure of the original source program.
The definition of DIANA given here is expressed in another notation, IOL,
that is formally defined in a separate document [9]. The present document is,
however, completely self-contained; those aspects of IDL that are needed for the
DIANA definition are informally described before they are used. Interested
readers should consult the IDL formal description either if they are concerned
with either a more precise definition of the notation or if they need to define
other data structures in an Ada support environment. In particular, implemen-
tors may need to extend DIANA in various ways for use with the tools in a
specific environment and the IDL document provides information on how this may
be done.
This version of DIANA has been "frozen" to meet the needs of several groups
who require a stable definition in a very short timeframe. We invite comments
and criticisms for a longer-term review. We expect to re-evaluate DIANA after
some practical experience with using it has been accumulated.
Preface To The Second Edition
Since first publication of the DIANA Reference Manual in March, 1981,
further developments in connection with Ada and DIANA have required revision of
DIANA. These developments include the following:
o The original DIANA design was based on Ada as defined in the July 1980
Ada Language Reference Manual (7], referred to hereafter as Ada-80; the
present revision is based on Ada as defined in the July 1982 Ada LRM
[81, referred to hereafter as Ada-82.
o Experience with use of DIANA has revealed errors and flaws in the
original design; these have been corrected.
This publication reflects our best efforts to cope with the conflicting
pressures on us both to impact minimally on existing implementations and to
create a logically defensible design.
Tartan Laboratories Inc. invites any further comments and criticisms c-
DIANA in general, and this version of the reference manual in particular. Ar,
correspondence may be sent via ARPANet mail to Diana-Query@USC_ECLB. Pape, mail
may be sent to
DIANA Manual
Tartan Laboratories Inc.
477 Melwood Avenue
Pittsburgh PA 15213
We believe the changes made to DIANA make no undue constraint on any OIANý
users or potential DIANA users, and we wish to hear from those who perceive any
of these changes to be a problem.
Preface To This Edition
This is a draft rcvision of the DIANA Reference Manual.
Experienco .ch DIANA has revealed weaknesses both in the definition of
DIANA and in the DIANA Reference Manual. This draft revision incorporates
changes in both areas.
Changes to the definition of DIANA include:
o Overhauling the representation of types and subtypes to accord better
with the definition of subtypes in Ada.
o "Partitioning" the DIANA so that any node or class (except the node
void) is directly a member of no more than one class.
o "Hoisting" attributes to the highest appropriate class.
o Otherwise regularizing the nomenclature of classes, ncces, and
attributes.
Changes to the DIANA Reference Manual include:
o Separation of semantic specification from rationale.
o Systematic coverage of static semantics of DIANA.
o Inclusion of hierarchical diagrams providing a pictorial representation
of class-membership relations.
o Inclusion of several substantial examples.
o Inclusion of a cross-reference index of nodes and attributes.
Chapter 7, External Representation of DIANA, and Chapter 8, The DTNI.
Package in Ada, are incomplete in this draft.
Intermetrics, Inc. invites any further comments and criticisms on DIANA in
general, and this draft version of the reference manual in particular. Any
correspondence may be sent via ARPANet mail to DIANA_QUERY@USC_ISIF. Paper mail
may be sent to
DIANA Maintenance
Intermetrics, Inc.
4733 Bethesda Ave.
Bethesda, MD 20814
CHAPTER I
The purpose of standardization is
to aid the creative craftsman, not .o
enforce the common mediocrity [11].
1.1 THE DESIGN OF DIANA
In a programming environmer such as that envisioned for Ada(1), there will
be a number of tools -- formatters (pretty printers), language-oriented editors,
cross-reference generators, test-case generators, etc. In general, the input
and output of these tools is NOT the source text of the program being developed;
instead it is some intermediate form that has been produced by another tool in
the environment. This document defines DIANA, Descriptive Intermediate
Attributed Notation for Ada. DIANA is an intermediate form of Ada programs
which has been designed to be especially suitable for communication between two
essential tools -- the Front and Back Ends of a compiler -- but also to be
suitable for use by other tools in an Ada support environment. DIANA encodes
the results of lexical, syntactic and STATIC semantic analysis, but it does NOT
include the results of DYNAMIC semantic analysis, of optimizat'on, or of code
generation.
DIANA is an abstract data type. The DIANA representation of a particular
Ada program is an instance of this abstract type. As with all abstract types,
PIANA defines a set of operations that provide the only way in which instances
of the type can be examined or modified. The actual data or file structures
used to represent the type are hidden by these operations, in the sense that the
implementation of a private type in Ada is hidden.
References may be made to a DIANA "tree", "abstract syntax tree", or
"attributed parse tree"; similarly, references may be made to "nodes" in these
trees. In the context of DIANA as an abstract data type, it is important to
appreciate the implications of such terms. This terminology does NOT imply that
the data structure used to implement DIANA is necessarily a tree using pointers,
etc. Rather, the notion. of attributed trees serves as the abstract model for
the definition of DIANA.
The following princ Ales governed the original design of DIANA:
o DIANA should be representation-independent. An effort was mace tc
avoid implying any particular implementation for the DIANA abstract
type. Implementation-specific information (such as a value on the
target machine) is represented in DIANA by other abstract types which
must be supplied by each implementation. In addition, DIANA may be
extended or contracted to cater to implementation-specific purposes.
o DIANA should be suitable for various kinds of processing. Although the
primary purpose of DIANA is communication between the Front and Back
Ends of compilers, other environment tools should be able to use it as
well. The needs of such programs were considered carefully.
o DIANA should be efficiently implementable. DIANA is intended to be
used; hence it was necessary to consider issues such as size and
processing speed.
o The DIANA description and notation should be regular. Consistency in
these areas is essential to both understanding and processing.
(1) Ada is a registered trademark of the U.S. Department of Defense.
Although DIANA is representation-independent, there must be at least one
form of the DIANA representation that can be communicated between computing
systems. Chapter 6 defines an externally visible ASCII form of the DIANA
representation of an Ada program. In this form, the DIANA representation can be
communicated between arbitrary environment tools and even between arbitrary
computing systems. The form may also be useful during the development of the
tools themselves.
I -
I-.
1.2 THE DEFINITION OF THE DIANA OPERATIONS
Every object of type DIANA is the representation of some specific Ada
program (or portion of an Ada program). A minimum set of operations on the
DIANA type must provide the ability to:
o determine the type of a given object (in DIANA terms, the object's node
type).
o obtain the value of a specific attribute of a node.
o build a node from its constituent parts.
o determine whether or not a given pair of instances of a DIANA type are
in fact the same instance, as opposed to equivalent ones. For the
scalar types (Integer and Boolean), no distinction is drawn between
equality and equivalence.
o assign a specific node to a variable, or a specific scalar value to a
scalar variable.
o set the value of an attribute of a given node.
The sequence type Seq Of can be considered as a built-in type that has a
few special operators. The operators defined for a sequence type allow an
implementation to:
o create a sequence of a given type
o determine whether or not a sequence is empty
o select an element of a sequence
o add an element to a sequence
o remove an element from a sequence
o compare two sequences to see if they are the same sequence
o assign a sequence to a variable of a sequence type
1.3 THE DEFINITION OF A DIANA USER
Inasmuch as DIANA is an abstract data type, there is no need that it be
implemented in any particular way. Additionally, because DIANA is extendable, a
particular implementation may choose to use a superset of the DIANA defined in
this reference manual. In the face of innumerable variations on the same theme,
it is appropriate to offer a definition of what it means to "use" DIANA. Since
it makes sense to consider DIANA only at the interfaces, two types of DIANA
users are considered: those which "produce" DIANA, and those that "consume" it.
These aspects are considered in turn:
o producer
In order for a program to be considered a DIANA producer, it must
produce as output a structure that includes all of the information
contained in DIANA as defined in this document. Every attribute
defined herein must be present, and each attribute must have the value
defined for correct DIANA and may not have any other value. This
requirement means, for example, that additional values, such as the
evaluation of non-static expressions, may not be represented using the
"DIANA-defined attributes. An implementation is not prevented from
defining additional attributes, and in fact it is expected that most
DIANA producers will also produce additional attributes.
There is an additional requirement on a DIANA producer: The DIANA
structure must have the property that it could have been produced from
a legal Ada program. This requirement is likely to impinge most
strongly on a tool other than a compiler Front End that produces DIANA.
As an example of this requirement, in an arithmetic expression, an
offspring of a mutiplication could not be an addition but would instead
have to be a parenthesized node whose offspring was the addition, since
Ada's parsing rules require the parentheses. The motivation for this
requirement is to ease the construction of a DIANA consumer, since the
task of designing a consumer is completely open-ended unless it can
make some reasonable assumptions about its input.
o consumer
In order for a program to be considered a DIANA consumer, it must
depend on no more than DIANA as defined herein for the representation
of an Ada program. This definition does not prevent a consumer from
requiring other kinds of input (such as information about the library,
which is not represented in DIANA); however, the DIANA structure must
be the only form of representation for an Ada program. This
restriction does not prevent a consumer from being able to take
advantage of additional attributes that may be defined in an
implementation; however, the consumer must also be able to accept input
that does not have these additional attributes. It is also incorrect
for a program to expect attributes defined herein to have values that
are not here specified. For example, it is wrong for a program to
expect the attribute sm value to contain values of expressions that are
not static.
There are twc attributes that are defined herein that are NOT required to
be supported by a DIANA user: Ix comments and lx srcpos. These attributes are
too implementation-specific to be required for all DIANA users.
It should be noted that the definition of a producer and that of a consumer
are not mutually exclusive; for example, a compiler Front End that produces
DIANA may also read DIANA for separate compilation purposes.
Having defined a DIANA producer and a DIANA consumer, it is now possible to
specify the requirements for a DIANA user. It is not proper to claim that a
given implementation uses DIANA unless EITHER it meets the following two
criteria:
o It must be able to read and/or write (as appropriate) the external form
of DIANA defined in Chapter 6 of this document.
o The DIANA that is read/written must be either the output of a DIANA
producer or suitable input for a DIANA consumer, as specified in this
section.
OR it meets this criterion:
o The implementation provides a package equivalent to that described in
Chapter 7.
1.4 THE STRUCTURE OF THIS DOCUMENT
As previously stated, DIANA is an abstract data type that can be modeled as
an attributed tree. This document defines both the domain and the operations of
this abstract type. The domain of the DIANA type is a subset of the
(mathematical) domain known as attributed trees. In order to specify this
subset precisely a subset of a notation called IDL [91 is used. A knowledge of
IDL is necessary to read or understand this document. Chapter 2 consists of the
IDL description of the DIANA domain, organized in the same manner as the Ada
Reference Manual. The DIANA operations are described in section 1.2.
Though the IDL description of DIANA may suffice to describe the structure
of DIANA, it does not convey the full semantics of that structure. For example,
in certain cases the set of allowed values of an attribute may be a subset of
the values belonging to the type of the attribute (although the IDL language
would permit the definition of a subclass in such cases, to do so would
undoubtedly disrupt the hierarchy and cause such a proliferation of subclasses
that DIANA would be almost impossible to understand). In addition, the IDL does
not specify the instances in which two attributes must denote the same noce.
Restrictions such as those described above are given in the semantic
specification of DIANA, the third chapter of this document.
Chapter 4 is a rationale for the design of DIANA. While the semantic
specification is organized according to the class structure of DIANA, the
rationale is composed of sections dealing with different semantic concepts which
are not necessarily applicable to any one DIANA class.
Chapter 5 contains examples of various kinds of DIANA structures. Each
example contains a segment of Ada code and an illustration of the resulting
DIANA structure.
Chapter 6 describes the external form of DIANA, an ASCII representation
suitable for communication between different computing systems.
Chapter 7 consists of a package specification for the CIANA interface,
written in Ada.
Appendix A is a cross-reference guide for the nodes, classes and attributes
of DIANA.
Appendix B is a list of references.
1.4.1 NOTATION
To assist the reader in understanding 'his material, certain typographic
and notational conventions are followed consistently throughout this document,
as illustrated in Figure 1-1.
DECL IDL class name
constant Id IDL.node name
sm__exp type IDL attribute
Type IDL reserved word"is" Ada reserved word
Figure 1-1. Typographic and Notational Conventions Used in this Document.
These conventions include:
o The appearance of class names, node names, and attribute names IN THE
TEXT are distinguished by the following typographic conventions: class
and node names are bold-faced, and attribute names are underlined.
These conventions are not followed in the IOL specification, the
diagrams, or the cross-reference guide.
o Ada reserved words appear in quotes.
o IDL reserved words appear in lower-case letters, except for the first
letter, which is capitalized.
o Class names appear in all upper-case letters.
o node names appear in all lower-case letters.
o attribute names appear in all lower case letters, with one of the
prefixes defined below.
o There are four kinds of attributes defined in DIANA: structural,
lexical, semantic, and code. The names of these attributes aTe
lexically distinguished in the definition by the following prefixes:
"+ as
Structural attributes define the abstract syntax tree of an Ada
program.
"I x
Lexical attributes provide information about the source form of the
program, such as the spelling of identifiers, or position in tne
source file.
"+ S.
Semantic attributes encode the results of semantic analysis -- type
and overload resolution, for example.
"+ cd
Code attributes provide information from representation
specifications that must be observed by the Back End.
o A class name or node name ending in ' s' is always a sequence of what
comes before the ' ' (if the prefix is extremely long it may be
slightly shortened in the sequence name). Thus the reader can be sure
on seeing exps that the definition
I
exps => as_list: Seq Of EXP;
appears somewhere.
o A class name ending in '-ELEM' contains both the node or class denoted
by the prefix of the class name and a node representing a pragma. The
name of the node representing the pragma consists of the prefix of the
class name and the suffix '_pragma'. Hence the reader knows that for
"the class name. STMELEM the following definition exists

STMELEM ::= STM I stmpragma;

Throughout the remainder of this document all references to the Ada
Reference Manual (ANSI/MIL_STO-1815A-1983) will have the following form: [ARM,
section number].
## CHAPTER 2
IOL SPECIFICATION
This chapter contains the IOL description of DIANA. It is organized in a
manner that parallels the Ada Reference Manual -- each section contains the
corresponding segment of Ada syntax along with the related IDL definitions. In
some cases a ¢etion does not contain any IDL definitions because that
particular construct is represented by a node or class which also represents
another construct, and the IOL definitions were included in the section
pertaining to the other construct. For example, the section covering operators
(section 4.5) does not contain IDL definitions because operators in DIANA are
represented as function calls, and the related IDL definitions are included in
the section on subprogram calls (section 6.4).
Structure Diana
Root compilation Is
-- Private Type Definitions
Type source_position;
Type comments;
Type symbol_rep;
Type value;
Type operator;
Type numberrep;
-- 2. Lexical Elements
-- Syntax 2.0
-- has no equivalent in concrete syntax
void =>
-- 2.3 Identifiers, 2.4 Numeric Literals, 2.6 String Literals
-- Syntax 2.3
-- not of interest for Diana
DEF NAME SOURCE NAME I PREDEF NAME;

DEFNAME => lx_symrep : symbol_rep;

SOURCENAME OBJECTNAME I TYPENAME I UNITNAME I LABELNAME;
OBJECT_NAME => sm_obj_type : TYPESPEC;

UNITNAME => sm_first : DEF NAME;
-- 2.8 Pragmas
-- Syntax 2.8.A
-- pragma ::= 
-- pragma identifier ((argumentassociation f, argumentassociationl)];

pragma => as_used_name_id : used_name_id,
as general_assocs : generaT_assocs;
-- seq of EXP and/or assoc
generalassocs => aslist : Seq Of GENERAL_ASSOC;
-- Syntax 2.8.B
-- argument association
-- [argument-identifier =>] name
I [argument-identifier =>] expression
-- 3. Declarations and Types
-- 3.1 Declarations
-- Syntax 3.1
-- declaration
-- object declaration number declaration
-- I type declaration I subtype_declaration
-- I subprogramdeclaration I packagedeclaration
-- I task declaration I generic declaration
-- I exception declaration generic-instantiation
-- I renamingdeclaration I deferred constantdeclaration

DECL ::= ID SDECL I IDDECL;

IDDECL type_decl
I subtypedecl
I task decl
I UNITOECL;

IDDECL => as_source name SOURCE NAME;

ID S DECL EXPDECL
- exception decl
I deferredconstant decl;

IDSDECL => as_source_name s : source name s;

EXPDECL OBJECTDECL
- number-decl;

EXPDECL => as_exp : EXP;
-- 3.2 Objects and Named Numbers
-- Syntax 3.2.A
-- object declaration
-- identifier list [constant] subtype indication [:= expression];
-- I identifier-list [constant] constraTned array definition [:= expression
EXP ::= void;

CONSTRAINEDDEF subtypeindication;

OBJECT DECL ::= constant decl I variable decl;
OBJECT DECL => astype_def : TYPEDEF;

constant decl => 
variable decl =>
OBJECT_NAME ::= INITOBJECTNAME;
INIT OBJECT NAME VC NAME;

INIT OBJECTNAME => sm_init_exp : EXP;

VC NAME variable id I constant id;
VC_NAME -> sm renamesobj : Boolean,
sm address : EXP; -- EXP or void
variable id => sm_is shdred : Boolean;
constantid => sm_first : DEF NAME;
-- Syntax 3.2.8
-- number declaration
-- identifierlist constant := universal static expression;
number decl => ;
INITOBJECT NAME number id;
number id 2)
-- Syntax 3.2.C
-- identifier list identifier {, identifieri
source name s => as_list : Seq Of SOURCE NAME;
-- 3.3 Types and Subtypes
-- 3.3.1 Type Declarations
-- Syntax 3.3.1.A
-- type_declaration ::= full_type_declaration
-- I incomplete_type_declaration I private_typedeclaration
-- full_typedeclaration
-- type identifier (discriminant_part] is type-definition;
typedecl => as_dscrmt decl s : dscrmt decl_s,
as_typedef : TYPEDEF;
TYPE NAME type_id;
TYPE_NAME => sm_type_spec : TYPE_SPEC;
type_id => sm_first : DEFNAME;
-- Syntax 3.3.1.8
-- type_definition
-- enumeration_type_definition I integer typedefinition
-- I real type definition I arraytype_aefinition
-- I record typedefinition I accesstypedefinition
-- I derived type_definition
TYPE DEF enumeration def
CONSTRAINED DEF
I ARR ACC DER DEF
I record def;
CONSTRAINEDDEF integerdef
float def
I fixed-def;
ARR_ACCDERDEF constrained_array_def
unconstrained_arraydef
access def
derived_def;

ARRACCDERDEF => as_subtypeindication : subtypeindication;

TYPESPEC :- DERIVABLE_SPEC;
DERIVABLE SPEC FULL TYPE SPEC I PRIVATESPEC;
DERIVABLE_SPEC => smiderived : TYPE_SPEC,
smiisanonymous : Boolean;
FULLTYPESPEC taskspec I NONTASK;

NON TASK ::= SCALAR I UNCONSTRAINED I CONSTRAINED;
NONTASK => sm_base_type :TYPESPEC;
SCALAR ::= enumeration I integer I REAL;
SCALAR => sm_range : RANGE;
SCALAR => cd_impl_size : Integer;
REAL ::= float I fixed;
REAL => smiaccuracy : value;

UNCONSTRAINED UNCONSTRAINED COMPOSITE access;

UNCONSTRAINED => sm_size : EXP; -- EXP or void
UNCONSTRAINED COMPOSITE ::= array I record;

UNCONSTRAINED_COMPOSITE => sm_is limited Boolean,
smis_packed Boolean;
CONSTRAINED constrainedarray
I constrained record
I constrained-access;

CONSTRAINED => smidepends_on_dscrmt : Boolean;
-- 3.3.2 Subtype Declarations

SDIANAReference Manual Draft Revision 4 Page 2-7
-- Syntax 3.3.2.A
-- subtypedeclaration subtype identifier is subtypeindication;
subtypedecl => as_subtype indication : subtype_indication;

TYPE NAME ::= subtype_id;

subtype id => 
-- Syntax 3.3.2.8
-- subtype indication type-mark (constraint]
-- type-mark type-name I subtypename
CONSTRAINT void;

CONSTRAINEDDEF => as_constraint : CONSTRAINT;

subtypeindication => as_name : NAME;
-- Syntax 3.3.2.C
-- constraint
-- range constraint I floatingpointconstraint I fixed_ppintconstraint
*- I index_constraint I discriminant_constraint
CONSTRAINT DISCRETE RANGE
REAL CONSTRAINT
I index_constraint
I dscrmt_constraint;
-- 3.4 Derived Type Definitions
-- Syntax 3.4
-- derived_typedefinition ::= new subtypeindication
derived def =>
-- 3.5 Scalar Types
-- Syntax 3.5
. .- -- rangeconstraint ::= range range
-- range ::= range attribute
S-- I simpleexpression ............ simple expression
DISCRETERANGE RANGE
I discrete_subtype;
RANGE range I rangeattribute I void;

RANGE => sm_type_spec : TYPE_SPEC;

range => as_exp1 : EXP,
as_exp2 : EXP;
rangeattribute => as_name : NAME,
as_used_name_id : used_name_id,
as_exp : EXP; -- EXP or void
-- 3.5.1 Enumeration Types
-- Syntax 3.5.1.A
-- enumerationtypedefinition
-- (enumerationliteralspecification {, enumerationliteral specification}
enumeration def => as_enum_literal_s : enum_literal_s;
enum literals => as_list : Seq Of ENUM_LITERAL;
enumeration => sm_literals : enumliteral s;
-- Syntax 3.5.1.8
-- enumeration literalspecification ::= enumerationliteral
-- enumeration literal ::= identifier I character literal
OBJECT_NAME ENUMLITERAL;
ENUMLITERAL enumeration_id I character id;

ENUMLITERAL => sm_oos : Integer,

sm rep Integer;
enumeration id => •
character_id => ;
-- 3.5.4 Integer Types
-- Syntax 3.5.4
-- integertypedefinition ::= rangeconstraint
integer_def =>
integer => ;
-- 3.5.6 Real Types
-- Syntax 3.5.6
-- real type definition
S--fToating_pointconstraint I fixedpoint constraint
REAL CONSTRAINT float constraint
I fixed_constraint;
REAL_CONSTRAINT => sm_type_spec : TYPE_SPEC;
,* -- 3.5.7 Floating Point Types
-- Syntax 3.5.7
-- floatingpoint constraint
-- floatingaccuracy_definition [range_constraint]
-- floating accuracy definition ::= digits static simple expression
float def =>
REAL_CONSTRAINT = as_exp : EXP,
as_range RANGE;
float constraint =>
float =>
-- 3.5.9 Fixed Point Types
-- Syntax 3.5.9
-- fixed_point constraint
-- fixedaccuracy definition [range-constraint]
-- fixed accuracy definition ::= delta static simpleexpression
fixed def => ;
fixed constraint =>
fixed => cd_impl_small : value;
-- 3.6 Array Types
-- Syntax 3.6.A
-- arraytype definition ::=
S-- unconstrainedarray_definition I constrained_array_definition
-- unconstrained_array_definition :-n
-- array(indexsubtype definition {, index_subtype_definition}) of
S--component_subtypeindication
-- constrained_array definition
-- array index_constraint of component_subtype indication
constrained_array_def => as_constraint :CONSTRAINT;
index_constraint => as_discrete_range_s :discrete_range_s;
discrete range_s => as_list :Seq Of DISCRETERANGE;
unconstrained_array_def => as_index_s :index_s;
scalar_s => as_list :Seq Of SCALAR;
array => sm_indexs : index_s,
sm_comp_type :TYPESPEC;
constrained_array => sm_index_subtype_s :scalar_s;
-- Syntax 3.6.8
-- index-subtype definition :=type_mark range <>
index => as_name :NAME,
sm_type_spec :TYPESPEC;
index_s => as_list :.Seq Of index;
-- Syntax 3.6.C
-- index_constraint :=(discrete range [, discrete_rangel)
-- discrete range :=discrete subtype indication Irange
discrete_subtype => as_subtype_indication subtype_indication;
-- 3.7 Record Types
-- Syntax 3.7.A
-- record -type definition
-- record
-- component_list
-- end record

REP ::= void;

record_def => as_comp_list :comp_list;
record => sm_discriminant_s :dscrmt_decl_s,
sm_comp_list comp_list,
sm_representation : REP; -- REP or void
constrained_record => sm_normalized dscrmts : exp_s;
-- Syntax 3.7.B
-- component list::
-- component-declaration (component declarationi
-- I[component-declaration) variant part
-- I null;
-- component declaration
-- identifier-list :component subtype definition [:= expression];
-- component subtype_definition :=subtype_indication
DECL null_comp_decl;
INIT_OBJECT_NAME COMP_NAME;
COMPNAME component_id I discriminant_id;

COMP_NAME => sm_comp_rep :COMP_REP_ELEM;

component_id => 
-- 3.7.1 Discriminants
-- Syntax 3.7.1
-- discriminant part
-- (discriminant_specification f; discriminant-specificationi)
-- discrimtinant -specification:=
-- identifier-list type-mark [:= expression]
ITEM DSCRMTPARAMJECL;
DSCRMTPARAMOECL dscrmt_decl;
DSCRMT_PARAM_DECL => as_source -name -s :source-name_s.,
as_name :NAME,
as_exp :EXP;
dscrmt deci s => as_list :Seq Of dscrmt_decl;
dscrmt_decl =>
discriminant_id = sm first DEFNAME;
-- 3.7.2 Discriminant Constraints
DIANA Reference ainual Draft Revision 4 Page 2-12
IOL SPECIFICATION
-- Syntax 3.7.2
-- discriminant_constraint
-- (discriminant association f, discriminant-association})
-- discriminant association ::=
-- [discriminantsimplename [idiscriminantsimplenamel =>1 expression
dscrmt_constraint => asgeneralassoc_s : general assoc s;
-- 3.7.3 Variant Parts
-- Syntax 3.7.3.A
-- variant_part
-- case discriminant_simple_name is
-- variant
-- {variant}
-- end case;
-- variant ::=
-- when choice {1 choicel =>
-- component_list

VARIANTPART ::= variant_part I void;

variant_part => as_name : NAME,
as_variants : variant_s;
variant s => as_list : Seq Of VARIANTELEM;
VARIANTELEM variant I variantpragma;
variant => as_choice s choices,
as_complist comp_list;
choice s => as_list : Seq Of CHOICE;
comp_list => as_decl s : decl s,
asvariant_part : VARIANT_PART,
aspragmas : pragma s;
variant pragma => aspragma : pragma;
-- Syntax 3.7.3.8
-- choice ::= simple expression
-- I discreterange I others I component_simple_name

CHOICE ::= choice exp I choice_range I choiceothers;

choice exp => as_exp : EXP;
"DIANA Reference Manual Draft Revision 4 Page 2-13
IOL SPECIFICATION
choice_range => as_discreterange DISCRETE_RANGE;
choice others =>
"--3.8 Access Types
-- Syntax 3.8
-- accesstype definition ::= access subtypeindication
access def => ;
access => sm_storage size : EXP, -- EXP or void
sm is controlled : Boolean,
sm_desig_type : TYPESPEC,
sm master : ALLDECL;
* constrainedaccess => sm_desig_type : TYPE_SPEC;
-- 3.8.1 Incomplete Type Declarations
-- Syntax 3.8.1
-- incompletetypedeclaration ::= type identifier (discriminant part];
TYPEDEF void;
TYPESPEC incomplete;
incomplete => sm_discriminant s dscrmt decls;
TYPE_SPEC void;
-- 3.9 Declarative Parts
-- Syntax 3.9.A
-- declarativepart
-- {basic declarativeitem) flaterdeclarativeiteml
-- basic declarative item ::= basic declaration
-- I representation-clause I use-clause

DECL ::= REP;
DECL ::= USEPRAGMA;
USEPRAGMA ::= use I pragma;
-- Syntax 3.9.B
-- later declarative item ::= body
-- I subprogramdeclaration I packagedeclaration
-- i task declaration i generic declaration
-- I use-clause I generic_instantiation
-- body ::= properbody I stub
-- properbody ::= subprogrambody I packagebody I taskbody
ITEM ::= DECL I SUBUNIT BODY;

item s => as_list : Seq Of ITEM;
UNITDECL generic decl
I NONGENERICDECL;

UNITDECL => as_header : HEADER;

NONGENERICDECL subprogentrydecl
I packagedecl;

NONGENERICDECL => as_unit kind : UNIT_KIND;
-- 4. Names and Expressions
-- 4.1 Names
-- Syntax 4.1.A
-- name ::= simple name
-- I character literal I operator_symbol
-- I indexed component I slice
-- I selected_component I attribute
-- simple name ::= identifier

NAME DESIGNATOR
I NAMEEXP;
NAMEEXP NAME VAL
indexed
slice
all;

NAME EXP => as_name : NAME;
NAMEEXP => smexp_type : TYPE_SPEC;

NAME VAL attribute
I selected;

NAME VAL => sm_value : value;
DESIGNATOR ::= USED OBJECT I USEDNAME;
DESIGNATOR => sm_defn : DEFNAME,

lx_symrep : symbol_rep;
USEDNAME used op I used_nameid;
A used op =>
used_name_id
USED OBJECT used char I used object id;
USED_OBJECT => sm_exp_type : TYPE_SPEC,
sm value : value;
used char ->
used-object_id -;
a
-- Syntax 4.1.B
-- prefix ::= name I function call

NAME VAL ::= function_call;
-- 4.1.1 Indexed Components
-- Syntax 4.1.1
-- indexedcomponent prefix(expression {, expression))

exps => aslist Seq Of EXP;
indexed => as_exp_s exps;
-- 4.1.2 Slices
-- Syntax 4.1.2
-- slice ::= prefix(discreterange)
slice => as_discreterange DISCRETE_RANGE;
-- 4.1.3 Selected Components
-- Syntax 4.1.3
-- selected component ::= prefix.selector
-- selector ::= simple name
-- I character-literal I operator symbol I all
selected => asdesignator : DESIGNATOR;
all => ;
-- 4.1.4 Attributes
-- Syntax 4.1.4
-- attribute ::= prefix'attribute designator
-- attribute designator ::= simplename [(universal static expression)]
attribute => asusednameid : used_name_id,
as_exp EXP;
-- 4.2 Literals
-- 4.3 Aggregates
-- Syntax 4.3.A
-- aggregate ::=
-- (componentassociation J, componentassociationi)
aggregate -> asgeneralassoc-s : generalassocs;
aggregate => smnormalized_comp_s : generalassocs;
-- Syntax 4.3.B
-- component association
-- (choice Hi choice) => I expression
GENERALASSOC NAMED_ASSOC I EXP;

NAMED ASSOC ::= named;
NAMEDASSOC => as_exp : EXP;

named => aschoices choices;
-- 4.4 Expressions
-- Syntax 4.4.A
-- expression ::=
-- relation land relation} I relation land then relationi
-- I relation for relationi I relation for else relation)
-- I relation fxor relation}

EXPVAL ::= short-circuit;

shortcircuit => asexpl : EXP,
as short-circuitop : SHORT CIRCUITOP,
asexp2 : EXP;
SHORT CIRCUIT OP and then I or else;
and then =>
or else =>
-- Syntax 4.4.B
-- relation ::=
-- simple expression [relational-operator simple_expression]
IL SPECIFICATION
-- I simpleexpression (not] in range
-- I simpleexpression (not] in type-mark
EXP VALEXP MEMBERSHIP;
MEMBERSHIP rangemembership I typemembership;

MEMBERSHIP => as_membership_op : MEMBERSHIPOP;

range_membership => asrange RANGE;
type_membership => as_name NAME;
MEMBERSHIPOP in_op I not-in;
inop =>
not in =>
-- Syntax 4.4.C
-- simpleexpression
-- [unaryoperator] term {binary_adding_operator terml
-- term ::= factor {multiplyingoperator factor]
-- factor primary [ primary] I abs primary I not primary
-- Syntax 4.4.D
-- primary ::=
-- numeric literal I null 1 aggregate I stringliteral I name I allocator
-- I function-call I type_conversion I qualifiedexpression I (expression)
EXP NAME
I EXP_EXP;
EXP EXP EXP VAL
I AGG EXP
I quaTifiedallocator
I subtype_allocator;

EXPEXP => sm_exp_type : TYPE_SPEC;

EXP VAL numeric literdl
I null access
I EXP VAL EXP;

EXP VAL => sm_value : value;
EXP VAL EXP ::= QUALCONV

I parenthesized;

EXP VALEXP => as_exp : EXP;

AGG EXP aggregate
I stringliteral;
IOL SPECIFICATION
AGGEXP -> smdiscrete_range : DISCRETE_RANGE;
parenthesized -> ;
numeric-literal => Ix numrep : number_rep;
string-literal => lx_symrep : symbolrep;
null access -> ;
-- 4.5 Operators and Expression Evaluation
-- Syntax 4.5
-- logical-operator ::= and I or I xor
-- relationaloperator ::= = I I <I I > I>=
-- addingoperator + I -I &
-- unary_operator + I -
-- multiplyingoperato- ::= I / I mod I rem
-- highestprecedence_operator ** I abs I not
-- 4.6 Type Conversions
-- Syntax 4.6
-- typeconversion-::= type_mark(expression)

QUALCONV ::= conversion

I qualified;

QUALCONV => as_name : NAME;

conversion => ;
-- 4.7 Qualified Expressions
-- Syntax 4.7
-- qualified expression
-- type mark'(expression) I typemark'aggregate
aualified => ;
-- 4.8 Allocators
-- Syntax 4.8
-- allocator
-- new subtype_indication I new qualifiedexpression
qualified allocator => asqualified : qualified;
subtype_allocator => assubtypeindication : subtype_indication,
sm_desigtype : TYPESPEC;
-- 5. Statements
5.1 Simple and Compound Statements - Sequences of Statements
-- Syntax 5.1.A
-- sequence of statements ::= statement [statement}
STMELEM STM I stmpragma;
stm_s => aslist : Seq Of STM_ELEM;
stm-pragma => aspragma : pragma;
-- Syntax 5.1.B
-- statement ::=
-- flabel} simple_statement I flabel} compound statement

STM ::= labeled;

labeled => a.ssource name s : source_names,
as_pragma_s : pragma_s,
as stm : STM;
-- Syntax 5.1.C
-- simple statement ::= null _statement
-- I assignmentstatement I procedure call _statement
-- I exit statement I return-statement
-- I goto_statement entry_call _statement
-- I delay_statement I abort statement
-- I raise-statement code statement
STM null stm
I abort;
STM STMWITHEXP;
STM WITH EXP return
I delay;
STMWITHEXP STMWITHEXPNAME;

STMWITHEXP => as_exp EXP;

STMWITHEXP NAME assign
IOL SPECIFICATION
exit
I code;

STM WITH EXP NAME => as_name : NAME;
STM ::= STMWITHNAME;

STM WITH NAME goto
I raise;
STM WITH NAME CALL STM;
CALLSTM-::= entrycall
I procedure-call;

STMWITHNAME => as_name : NAME;
-- Syntax 5.1.D
-- compoundstatement
-- if statement I case statement
-- I loop_statement I block statement
-- accept_statement I selectstatement

STM accept
I BLOCK LOOP
I ENTRY STM;
STMWITHEXP case;
STM CLAUSESSTM;
CLAUSESSTM if
I selective wait;

CLAUSESSTM => astestclauseelems test clauseelems,

as_stm_s stm s;
-- Syntax 5.1.E
-- laoel ::= <<«abel simplename-

LABEL NAME ::= label id;

LABEL_NAME => sm_stm : STM;
label id =>
-- Syntax 5.1.F
-- null statement ::= null
null stm => ;
-- 5.2 Assignment Statement
-- Syntax 5.2
-- assignment statement
-- variable name := expression;
assign =>
-- 5.3 If Statements
-- Syntax 5.3.A
-- if statement
-- if condition then
-- sequenceofstatements
-- felsif condition then
-- sequenceof_statementsl
-- [else
-- sequence_of_statements]
-- end if;
if => ;
TEST CLAUSE cond clause;
TEST_CLAUSE => as_exp : EXP,
as stm s : stm_s;
cond clause =>
-- Syntax 5.3.B
-- condition ::= booleanexpression
-- 5.4 Case Statements
-- Syntax 5.4
-- case statement
-- case expression is
-- case statement alternative
-- {caseStatementalternative}
-- end case;
-- case statement alternative ::=
-- when choice-{0 choice I =>
-- sequenceofstatementsl
ALTERNATIVE ELEM alternative I alternative pragma;
case => asalternatives : alternatives;
alternative s => as_list : Seq Of ALTERNATIVEELEM;
alternative => aschoice s : choices,
as stm s stm s;
!-
IOl SPECIFICATION
alternativepragma => aspragma : pragma;
-- 5.5 Loop Statements
-- Syntax 5.5.A
-- loop statement
-- [loop simple name:]
riteratTon -scheme) loop
-- sequence of statements
-- end loop [loopsimple name];

BLOCKLOOP ::= loop;
BLOCK LOOP => as_source name : SOURCENAME;
SOURCE NAME ::= void;
LABELNAME ::= blockloopid;

blockloop id => 

ITERATION ::= void;

loop => as_iteration : ITERATION,
as stms : stm-s;
-- Syntax 5.5.B
-- iteration scheme ::= while condition
-- I for Toop parameter_specification
-- loopparameter specification
identifier in [reverse] discrete_range

ITERATION ::= FOR REV:
FOR REV ::= for I reverse;

FOR_REV => as_source name : SOURCE NAME,
as discreterange : DISCRETE_RANGE;
for => ;
reverse =>

OBJECT NAME ::= iterationid;

iteration id => 

ITERATION ::= while;

while => as_exp : EXP;
-- 5.6 Block Statements
SDIANAReference Manual Draft Revision 4 Page 2-23
IOL SPECIFICATION
-- Syntax 5.6
-- block statement
-- (bTock simple name:]
S -- [aeclare
S-- declarativepart]
-- begin
-- sequence_ofstatements
-- [exception
-- exception handler
-- [exceptionhandler)]
-- end (block simplename];

BLOCK LOOP ::= block;

,4 block - as block body : blockbody;
blockbody => as_item s : items,
as stm s: stm_5,
as alternative s : alternative_s;
-- 5.7 Exit Statements
-- Syntax 5.7
-- exit statement
-- exit [loopname] (when condition];
NAME void;-
exit => sm_stm : STM;
-- 5.8 Return Statements
-- Syntax 5.8
-- return-statement ::= return [expression];
return =>
-- 5.9 Goto Statemelits
-- Syntax 5.9
-- goto statement ::= goto label_name;
goto =>
-- 6. Subprograms
-- 6.1 Subprogram Declarations
I-
I-
-- Syntax 6.1.A
-- subprogramdeclaration : subprogramspecification;
subprogentrydecl =>

UNIT NAME ::= NON TASK NAME;

NON TASK NAME SUBPROG PACK NAME;

NON TASK NAME => smspec-: HEADER;
SUBPROG PACK NAME ::= SUBPROG NAME;

SUBPROG_PACK_NAME => sm_unit_desc : UNITDESC,
sm_address : EXP;
SUBPROG NAME procedure id I function id I operatorid;

SUBPROG NAME => sm_is iniTne : Boolean,-

sminterface : PREDEFNAME;
UNITDESC UNIT KIND I BODY
I implicitnot_eq I derived subprog;
UNIT KIND void;
derivedsubprog => sm_derivable : SOURCENAME;
implicit_not_eq => sm_equal : SOURCE_NAME;
procedure fd =>
functionTd =>
operator_id =>
-- Syntax 6.1.B
-- subprogramspecification
-- procedure identifier [formal _part]
-- I function designator (formal-_part] return typemark
-- designator ::= identifier I operator_symbol
-- operatorsymbol ::= string_literal

HEADER ::= SUBP ENTRY HEADER;

SUBPENTRYHEADER procedure_spec I functionspec;

SUBPENTRYHEADER => asparams : param_s;

procedurespec => 
function_spec => as_name : NAME;
DJIANA Reference Manual Draft Revision 4 Page 2-25
IOL SPECIFICATION
-- Syntax 6.1.C
-- formal-part
* -- (parameter specification f; parameter specification})
-- parameterspecification ::=
-- identifier-list mode typemark [:= expression]
-- mode ::= (in] I in out I out
param_s => as_list : Seq Of PARAM;
OSCRMTPARAMDECL PARAM;
PARAM in I out I in-out;
in -> Ix default : Boolean;
in out =>
out =>
INITOBJECTNAME PARAM NAME;

PARAM NAME ::= in id I in out id I out_id;
PARAMNAME => sm_first :DEFNAME;

in id => 
in out id =>
out id-=>
-- 6.3 Subprogram Bodies
-- Syntax 6.3
-- subprogram-body
-- subprogramspecification is
-- [declarative-part]
-- begin
-- sequenceofstatements
-- [exception
-- exceptionhandler
-- [exception-handler)]
-- end (designator];

BODY ::= block-body I stub I void;

subprogram-body => as_header HEADER;
-- 6.4 Subprogram Calls
-- Syntax 6.4
-- procedurecallstatement
-- procedure-name [actual_parameter part);
-- function call ::=
- - functTon-name (actualparameterpart]
-- actual parameterpart ::=
-- (parameter-association 1, parameter_associationi)
-- parameter association ::=
-- (formaTlparameter =>] actual-parameter
-- formal-parameter ::= parametersimplename
-- actual _parameter ( a m
-- expression I variable-name I type-mark(variable name)

CALL STM => asgeneral assoc s : general _assoc_s;
CALL STM => smnormalized param_s :exp_s;

procedure call => 
function call => asgeneral assoc s : general _assocs;
function call => smnormalized params : exp_s;
function-call 2> lx_prefix : Boolean;
NAMED ASSOC ::2 assoc;
assoc => asusedname : USEDNAME;
-- 7. Packages
-- 7.1 Package Structure
-- Syntax 7.1.A
-- package_declaration ::= packagespecification;
package_decl => ;

SUBPROGPACKNAME ::= packageid;

packageid => 
-- Syntax 7.1.8
-- packagespecification
-- package identifier is
-- {basic declarative-item)
-- [private
-- (basic declarative iteml]
-- end [packagesimplename]
package spec => as_decl sl : decl!s,
as_decls2 : declis;
decl -s as_list : Seq Of DECL;
-- Syntax 7.1.C
-- packagebody
-- package body package simple name is
-- [declarativepart]
" -- (begin
-- sequenceof statements
S-- (exception
-- exception handler
-- fexception-handler}l]
-- end (package_simplename];
packagebody =>
-- 7.4 Private Type and Deferred Constant Declarations
-- Syntax 7.4.A
-- privatetype declaration
-- type identifier (discriminant part] is (limited] private;
TYPEDEF private_def I 1_privatedef;
private def => ;
lprivate_def =>

TYPENAME ::= private_type_id I 1_privatetypeid;

S.- privatetypeid => 
l_private_type_id =>

PRIVATE SPEC ::= private I 1_private;

PRIVATE_SPEC => sm_discriminant s : dscrmt decls,
sm_typespec : TYPE_SPEC;
private => ;
l_private =>
-- Syntax 7.4.8
-- deferred constant declaration
-- identTfierlist : constant type_mark;
deferred constant decl => as_name : NAME;
-- 8. Visibility Rules
-- 8.4 Use Clauses
-- Syntax 8.4
-- use-clause ::= use packagename {, packagename};
use => asname s name s;
-- 8.5 Renaming Declarations
-- Syntax 8.5
-- renaming_declaration
-- identifier : type mark renames object_name;
-- I identifier : exception renames exceptionname;
-- I package identifier renames packagename;
-- I subprogramspecification renames subprogramorentry name;

IDDECL ::= SIMPLERENAMEDECL;

SIMPLERENAMEDECL renamesobj_decl
I renames-exc decl;

SIMPLERENAMEDECL => as_name : NAME;

renames objdecl => astypemark name NAME;
renames exc decl =>
UNITKIND RENAMEINSTANT;
RENAME INSTANT renamesunit;
RENAME_INSTANT => as_name : NAME;
renames unit =>
-- 9. Tasks
-- 9.1 Task Specifications and Task Bodies
-- Syntax 9.1.A
-- task-declaration ::= task_specification;
-- taskspecification
-- task (type] identifier (is
-- fentrydeclaration)
-- {representation clause}
-- end (task_simplename]]
I1L SPECIFICATION
task decl - as_decl-s : dec1_s;
task-spec = sm decl s : declis,
smbody-: BODY,
msm address : EXP,
sm size : EXP,
sm storage size EXP;
-- Syntax 9.1.3
-- taskbody
•-- task body task simple name is
-- (declarative_partT
*... begin
S-- sequence_of statements
-- (exception
-- exceptionhandler
,-- [exception handler)]
-- end (task_simpTe_name);
task-body =>
UNIT NAME taskbody_id;
- taskbodyid => smtype_spec TYPESPEC,
sm_body : BODY;
-- 9.4 Task Dependence -. Termination of Tasks
ALLDECL block-master;
block-master => smnstm : STM;
-- 9.5 Entries, Entry Calls and Accept Statements
*... Syntax 9.5.A
-- entry_declaration
-j -- entry identifier ((discrete_range)] [formal_part];
SUBP ENTRY HEADER entry;
entry => as_discreterange DISCRETERANGE;

SOURCENAME ::= entryid;

entry-id = sm_spec : HEADER,
sm address : EXP;
-- Syntax 9.5.B
[DL SPECIFICATION
-- entrycall statement ::= entry name [actual-_parameter_part];
entry-call =>
-- Syntax 9.5.C
-- acceptstatement
-- accept entry simple name [(entryindex)] (formal_part) [do
-- sequence of statements
-- end (entrysimple name]l;
-- entryindex ::= expression
accept > as_name : NAME,
as param s : param s,
as_stm-s : stms;
-- 9.6 Delay Statements, Duration and Time
-- Syntax 9.6
-- delaystatement ::= delay simpleexpression;
delay => ;
-- 9.7 Select Statements
-- Syntax 9.7
-- select statement ::= selective wait
-- I conditional entrycall I timed_entry_call
-- 9.7.1 Selective Waits
-- Syntax 9.7.1.A
-- selective wait
-- select
-- select-alternative
-- for
-- select alternative}
-- [else
-- sequenceof statements]
-- end select;
selective wait =>
-- Syntax 9.7.1.B
-- selective alternative
-- [when condition =>]
-- selective wait alternative
-- selective wait alternative ::= acceptalternative
-- I delay_alternative I terminate alternative
-- acceptalternative accept_statement (sequenceofstatements]
-- delayalternative delay-statement [sequenceofstatements]
-- terminate alternative ::= terminate;
TESTCLAUSEELEM TESTCLAUSE I select_altpragma;

TEST CLAUSE ::= select-alternative;

test clause elem s => as_list : Seq Of TESTCLAUSEELEM;
select-alternative => ;
selectaltpragma => aspragma pragma;
STM terminate;
terminate => ;
-- 9.7.2 Conditional Entry Calls
-- Syntax 9.7.2
-- conditional _entry_call
-- select
-- entry_call _statement
-- [sequence_of_statementi]
-- else
-- sequenceofstatements
-- end select;
ENTRY STM .- condentry 1 timed entry;
ENTRY_STM => as_stm sl : stms,
-asstms2 : stms;
cond_entry =>
-- 9.7.3 Timed Entry Calls
-- Syntax 9.7.3
-- timed_entrycall
-- select
-- entry_call _statement
-- [sequence_of_s'-tements]
-- or
-- delay_alternative
-- end select;
timed-entry => ;
-- 9.10 Abort Statements
-- Syntax 9.10
-- abort statement ::= abort task name 1, task name);
name s => as_list : Seq Of NAME;
abort -> as_name s : names;
10. Program Structure and Compilation Issues
-- 10.1 Compilation Units - Library Units
-- Syntax 10.1.A
-- compilation ::= {compilationuniti
compilation => as_compltn-unit_s : compltnunits;
compltnunits => aslist : Seq Of compilation unit;
-- Syntax 10.1.8
-- compilation unit
-- contextclause libraryunit I contextclause secondaryunit
-- libraryunit ::=
- - subprogramdeclarationi packagedeclaration
-- generic declaration I generic instantiation
-- I subprogrambody
-- secondaryunit ::= libraryunit body I subunit
-- libraryunitbody ::= subprogrambody I packagebody

ALL DECL ::= void;

pragmas => as_list : Seq Of pragma;
compilation-unit => as_context elem s : context-elems,
as_all decT : ALL DECL,
as_pragmas : pragma s;
CONTEXTELEM contextpragma;
context_pragma =) aspragma : pragma;
-- Context Clauses - With Clauses
-- Syntax IO.1.1.A
-- context-clause (withclause {use clause}l
context elems > as list : Seq Of CONTEXTELEM;
-- Syntax 10.1.1.8
-- withclause ::= with unitsimplename {, unitsimple_namel;
CONTEXT ELEM ::- with;
with => as_name s : names,
as_use_pragmas : usepragmas;
usepragmas => aslist : Seq Of USE_PRAGMA;
-- 10.2 Subunits of Compilation Units
-- Syntax 10.2.A
-- subunit ::=
-- separate (parent_unitname) proper body
subunit => as_name : NAME,
as_subunitbody SUBUNIT_BODY;
SUBUNIT BODY subprogram body I packagebody I task_body;
SUBUNIT_BODY => as_source name SOURCENAME,
asbody :-BODY;
-- Syntax 10.2.B
-- bodystub ::=
-- subprogramspecification is separate;
-- I package body packagesimplename is separate;
-- I task body tasksimple_name is separate;
stub => ;
-- 11. Exceptions
-- 11.1 Exception Declarations
-- Syntax 11.1
-- exceptionjdeclaration ::= identifier.list : exception;
exception decl ->

SOURCE NAME ::= exception id;

exceptionid => smrenamesexc : NAME;
-- 11.2 Exception Handlers
-- Syntax 11.2
-- exception handler
-- when exception choice I1exceptionchoice} =>
-- sequence of statements
-- exception-choice ::- exception-name I others
-- 11.3 Raise Statements
-- Syntax 11.3
-- raise-statement ::= raise [exception_name];
raise => ;
-- 12. Generic Program Units
-- 12.1 Generic Declarations
-- Syntax 12.1.A
-- generic_declaration ::= genericspecification;
-- generic_specification ::=
-- generic formal part subprogramspecification
-- ( generic formal part package specification

HEADER ::= package spec;

genericdecl => asitem s : items;
NONTASKNAME genericId;
generic id => smgeneric_param_s : items,
smbody : BODY,
sm is inline : Boolean;
-- Syntax 12.1.B
-- generic formalpart generic {generic_parameter_declarationi
I
"IDL SPECIFICATION
-- Syntax 12.1.C
-- genericparameter declaration
-- identifier list : (in [out]) type mark [:= expression);
-- I type identTfier is generic_type_definition;
S-- I privatetypedeclaration
-- I with subprogramspecification [is name];
S-- I with subprogramspecification [is <>];
UNIT KIND :: GENERICPARAM;

GENERICPARAM ::= name_default

I box default
I no default;
name default => as_name : NAME;
"box default =>
no-default =>
-- Syntax 12.1.0
-- generic typedefinition
-- (<>) I range <> I digits <> I delta <>
-- I array_typedefinition I accesstypedefinition
TYPEDEF formal dscrt def
I formal integerdef
I formal fixed def
I formalfloat-def;
formal dscrt def =>
formal fixed def =>
formal float def =>
formal integerdef =>
-- 12.3 Generic Instantiation
-- Syntax 12.3.A
-- generic_instantiation
-- package identifier is
-- new generic_packagename [genericactualpart];
-- I procedure identifier is
-- new generic_procedurename (genericactual_part];
-- I function identifier is
-- new genericfunctionname [genericactual_part];
-- generic actual part ::=
-- (generic-association f, generic-association})
0

RENAME INSTANT ::= instantiation;

instantiation => as_general assoc s : generalassocs;
instantiation sm_decl-s : declIs;
-- Syntax 12.3.8
-- generic association ::=
-- [generic formalparameter =>] genericactualparameter
-- genericformal_parameter parametersimplename I operatorsymbol
-- Syntax 12.3.C
-- generic actual _parameter expression I variablejname
-- I subprogramname I entry_name I typemark
-- 13. Representation Clauses and
-- Implementation Dependent Features
-- 13.1 Representation Clauses
-- Syntax 13.1
-- representation clause
-- typerepresentation clause I addressclause
-- typejrepresentation clause ::z length clause
-- f enumerationrepresentationclauseI recordrepresentationclause

REP ::= NAMEDREP I recordrep;
REP => as_name NAME;
NAMEDREP => as_exp EXP;
-- 13.2 Length Clause
-- 13.3 Enumeration Representation Clauses
-- Syntax 13.2
-- length-clause ::= for attribute use simpleexpression;
-- Syntax 13.3
-- enumeration representationclause
-- for typesimple_name use aggregate;
NAMED REP ::= lengthenum_rep;

length enumrep => ;
-- 13.4 Record Representation Clauses
-- Syntax 13.4.A
record representation clause
for type simple name use
-- record alITgnmentclause]
-- {component clause}
-- end record;
-- alignmentclause ::= at mod static simple_expression;
ALIGNMENT_CLAUSE alignment I void;
alignment => aspragmas pragmas,
as_exp : EXP;
record_rep => asalignment clause : ALIGNMENT_CLAUSE,
as_comp_rep_s : compreps;
-- Syntax 13.4.B
-- component clause
-- component_simple_name at staticsimple_expression range staticrange;
COMP REP ELEM comprep I void;
COMP REP ELEM compreppragma;
compreps => aslist : Seq Of COMPREPELEM;
comprep => asname : NAME,
as_exp : EXP,
asrange RANGE;
compreppragma => as_pragma pragma;
-- 13.5 Address Clauses
-- Syntax 13.5
-- address clause ::= for simplename use at simpleexpression;
NAMED REP := address;
address =>
-- 13.8 Machine Code Insertions
-- Syntax 13.8
-- code-statement type mark'record aggregate;
code =>
-J
[OL SPECIFICATION
-- 14.0 Input-Output
-- I/O procedure calls are not specially handled. They are
-- represented by procedure or function calls (see 6.4).
-- Predefined Diana Environment

PREDEF NAME ::= attribute id

I pragma id
I argument id
I bltn operatorid
I void;
attribute id => ;
TYPE_SPEC ::= universalinteger I universal-fixed I universal-real;
universal integer =>
universal fixed =>
universal-real =>
argument_id => ;
bltnoperator id => smoperator : operator;
pragma_id => sm_argument_id_s : argumentids;
argumentids => asltst : Seq Of argumentid;
ALL SOURCE DEFNAME I ALL DECL I TYP" DEF I SEQUENCES
I STM ELEM I GENERAL ASSOC i-CONSTRAINT I CHOICE
I HEADER I UNIT DESC_I TEST CLAUSE ELEM
I MEMBERSHIP OP_I SHORT CIRCUIT OP_I ITERATION
I ALTERNATIVE ELEM I COMP REP ELEM I CONTEX T ELEM
I VARIANT ELEM I ALIGNMENT_CLAUSE I VARIANT PART
comp_list I compilation T compilation unit I index;
SEQUENCES alternative s I argument id s I choice s
comp_rep_s I compltn unit s I context elem s
I decls I dscrmt decl s I general assoc-s
discrete ranges I enum_literal_s I exps I item s
index s T name s I params I pragmas I scalar_s-
source name s T stm-s I test clause elem s
usepragmas I variant_s;

ALL SOURCE => lx_srcpos : sourceposition,

Ix comments : comments;
ALL DECL ITEM I subunit;
End
'-I
## CHAPTER 3
-d
This chapter describes the semantics of DIANA. The structure of this
chapter parallels the DIANA class hierarchy. Each section corresponds to a
class in the DIANA class hierarchy, and each subsection corresponds to a
subclass of that class in the hierarchy. Each node is discussed in the section
corresponding to the class which directly contains it.
Since the class structure of DIANA is a hierarchy, it was possible to
construct hierarchy diagrams to illustrate pictorially the relationships between
the various nodes and classes. At the end of each major section is a hierarchy
diagram which depicts the nodes and classes discussed in that section, along
with the attributes which are defined for those nodes and classes. Beneath each
class or node name is a list of attribute names corresponding to the attributes
which are defined at that level. Hence an attribute appearing immediately below
a class name is defined for all classes and nodes which are below that class in
the diagram.
It should be noted that the classes ALL SOURCE and SEQUENCES have been
omitted from this chapter due to the simple nature of their structure and the
fact that they represent optional features of DIANA. All nodes which may
represent source text have the attributes lx srcpos and lx comments; however,
DIANA does not require that these attributes be represented in a DIANA
structure. The sole purpose of class ALL SOURCE is to define these two
attributes for its constituents; the only nodeJ which do not inherit these
attributes are those belonging to class TYPE SPEC. In order to be consistent,
the IDL specification of DIANA defines a sequence node (or header) for each
sequence; however, an implementation is not required to represent the sequence
node itself. Class SEQUENCES is a set of sequence nodes, all of which have a
single attribute (other than lx srcpos and Ix comments) called as list which
denotes the actual sequence.
The folloing conventions are observed throughout this chapter:
(a) All attributes which are inherited by the node void are undefined. In
addition, no operations are defined for the attributes inherited by the
node void.
(b) Although a class may contain the node void, an attribute which has that
class as its type cannot be void unless the semantic specification
explicitly states that the attribute may be void.
(c) The attributes lx srcpos and Ix comments are undefined for any nodes
which do not represent source coe.? For certain nodes, such as those
in class PREDEF NAME, these attributes will never be defined.
(d) Unless otherwise specified, all nodes represent source code.
(e) A sequence cannot be empty unless the semantic specification explicitly
allows it.
(f) If the manual specifies that the copying of a node is optional, and an
implementation chooses to copy that node, then the copying of any nodes
denoted by structural attributes of the copied node is also optional.
.1
Section 3.1
ALL DECL
3.1 ALL DECL
The four immediate offspring of class ALL OECL are void, subunit,
"block-master, and ITEM.
The subunit node represents a subunit, and has two non-lexical attributes
-- as_name and as subunit body. The attribute as_name denotes the name of the
parent unit (a selected, used name Id, or used op node); as subunit body
designates the node corresponding to the proper body.
The block master node represents a block statement that may be a master
because it contains immediately within its declarative part the definition of an
* access type which designates a task. Its only non-lexical attribute, sm stm,
denotes a blockbody node. The block master node can only be referenced by the
sm master attribute of the access node, thereby serving as an intermediate node
between the access type definition and the block statement. The block master
node does not represent source code.
3.1.1 ITEM
In general, the nodes in class ITEM correspond to explicit declarations;
i.e. declarative items that can be found in formal parts, declarative parts,
component lists, and program unit specifications. Certain declarative nodes
(subtype decl, constant decl, renames obj decl, and subprog entry decl) may also
appear in another context -- as a part of e sequence of decTarations constructed
for the instantiation of a generic unit. When used in this special context
these declarative nodes are not accessible through structural attributes, and do
not correspond to source code.
Certain implicit declarations described in the Ada manual are not
represented in DIANA: the predefined operations associated with type
definitions; label, loop, and block names; anonymous base types created by a
constrained array or scalar type definition; anonymous task types; derived
subprograms. Although the entities themselves have explicit representations
(i.e. defining occurrences), their declarations do not.
-J
With the exception of the node null comp decl and the nodes in classes REP
and USE PRAGMA, all of the nodes in class ITEM have a child representing the
identifier(s) or symbol(s) used to name the newly defined entity (or entities).
These nodes, members of class SOURCE NAME, are termed the "defining occurrence"
of their respective identifiers; they carry all of the information that
I-
describes the associated entity.
The classes DSCRMT PARAM_DECL, SUBUNIT BODY, and DECL comprise ITEM.
3.1.1.1 DSCRMT PARAI4DECL
The DSCRNT PARAM DECL class is composed of nodes representing either a
discriminant specifiCation or a formal parameter specification. The as_name
attribute defined on this class denotes a selected or used name i- node
corresponding to the type mark given in the specification.
The dscrmt dec node represents a discriminant specification. The
as source name s attribute denotes a sequence of dscrut id nodes, and the as exp
attribute references a node corresponding to the default initial value; if there
is no initial value given, as exp is void.
3.1.1.1.1 PARAM
A node in class PARAM may represent either a formal parameter specification
contained in a formal part, or a generic formal object declaration. The
as source name s attribute denotes a sequence of in Id, in out id, and out id
nodes, unless the PARAN node corresponds to a generTc formal object declaration,
in which case only in id and in out Id nodes are permitted.
The in node represents a formal parameter declaration of mode in. Its
as exp attribute denotes the default value of the parameter, and is void if none
is given. The in node also has an lx default attribute which indicates whether
or not the mode is specified explicitly.
The in out and out nodes represent formal parameter declarations of mode in
out and out, respectively. The as exp attribute of these nodes is always void.
3.1.1.2 SUBUNIT BODY
The class SUBUNIT BODY is composed of nodes representing declarations of
subunit bodies. The as body attribute defined on this class may denote either a
block body or a stub node, depending on whether the declaration corresponds to a
proper body or a body stub.
The subprogram-body node represents the declaration of a subprogram body.
The as source name attribute denotes a node in class SUBPROGONAME, and the
as header attribute references a procedurespec or a functionspec node.
The package body node represents the declaration of a package body; its
as source name attribute refers to a packageId node. If the package body is
empty (i.e. it contains no declarative part, no sequence of statements, and no
exception handlers) then as body still denotes a blockbody node; however, all
of the sequences in the bloc--l body node are empty.
The task body node represents the declaration of a task body; its
as source name attribute denotes a task-bodyId.
3.1.1.3 DECL
The class DECL contains the nodes associated with basic declarative items,
record component declarations, and entry declarations.
The node null comp deci represents a record component list defined by the
word "null". It gas no attributes other than lexical ones, and appears only as
the first member of a sequence denoted by the as decl s attribute of a comp_list
node; the only kind of node which can succeed it in the sequence is a pragma
node.
3.1.1.3.1 USE PRAGMA
The class USE PRAGMA contains the nodes pragma and use.
The pragma node represents a pragma. The as_used_name_id attribute denotes
the name of the pragma, and the asgeneral assocs attribute references a
possibly empty sequence of argument associations (the sequence may contain a
mixture of assoc and EXP nodes).
The use node represents a use clause. The as_name s attribute represents
the list of package names given in the use clause. If the use clause appears as
, a basic declarative item, the sequence can contain both used_name_id and
selected nodes; if it is a part of a context clause, it will-contain
used_name_id nodes only.
3.1.1.3.2 REP
The nodes in class REP correspond to representation clauses which may
appear as declarative items (i.e. address clauses, length clauses, record
representation clauses, and enumeration representation clauses).
The node record_rep represents a record representation clause. The
attribute as_name references a used name Id corresponding to the record type
name; as ali ment clause and as comp reps -denote the alignment clause and
component clauses, respectively. The attribute as alinment clause is void if
the representation clause does not contain an alignment clause, and
as comp rep s may be empty if no component clauses or pragmas are present.
3.1.1.3.2.1 NAMED REP
The nodes length enum rep and address comprise the class NAMED REP, a group
of representation clauses which consist of a name and an expression.
-,J
The length enum rep node may represent either a length clause or an
enumeration representation clause. In the former case the as_name attribute
denotes an attribute node and the as exp attribute corresponds to the simple
expression. In the case of an enumeration representation clause the as_name
attribute denotes a used_name_id corresponding to the enumeration type, and
as exp references an aggregate node.
The address node represents an address clause. Its as_name attribute
references a node from the class USED SOURCE NAME correspondTngto the name of
the entity for which the address is being specified. The as exp attribute
records the address expression.
3.1.1.3.3 ID DECL
The ID DECL class represents those declarations which define a single
entity ratger than a sequence of entities (i.e. declarations defining an
identifier, not an identifier list). Included in this class are the type decl,
subtype decl, and task decl nodes, as well as the UNIT DECE and
SIMPLE RENAME DECL classes, representing unit declarations and - rendming
declarations, respectively.
The type decl node represents a type declaration -- incomplete, private,
generic, derived, or full. The only type declaration that is not represented by
this node is that of a task type, which is denoted by a task decl node instead.
The type decl node has three non-lexical attributes: as source name,
as dscrmt decl s, and as type def.
The as source name attribute of a type decl node denotes a node
representing a new defining occurrence of the type name, the kind of node
depending on the kind of type declaration. Certain type names will have more
than one declaration point -- those corresponding to incomplete types or
(limited) private types. The as source name attribute of the typedecl node
associated with a (limited) private type declaration references a
privatetype id or I privatetype_id node; for all other type declarations
as source name will designate a type id node. The subsequent full type
declaration for an incomplete or (limited) private type is treated as an
ordinary full type declaration; hence the as source name attribute of the full
type declaration corresDonding to a (limited) private type will denote a typeid
rather than a private type_id or 1_private type_id.
The as dscrmt decl s attribute of a type decl node is a possibly empty
sequence containing the discriminant declarations which appear in the type
declaration; for declarations of derived types and generic formal types which
are not private this sequence is always empty.
The as type def attribute associated with a typedecl node designates a
node representing the portion of source code following the reserved word "is";
hence the as type-def attribute for an incomplete type definition is void, anc
may not be void for any other kind of type declaration. The permitted values of
the as type def attribute for the remainder of the type declarations are as
follows: for a (limited) privdte type declaration -- a private def or
iprivatedef node; for a generic type declaration -- a TYPE DEF node having the
prefix "formal-", an unconstrained array def node, a constrained array def node,
or an access def-node; for a derivid type declaration -- a derTved node; and
finally, foý a full type declaration -- an enumeration def, integer def,
float def, fixed def, unconstrained arraydef, constrainedarray def,
recoradef, or access def node.
The subtype decl node represents a subtype declaration; it defines two
attributes: as source name and as subtype indication. The former denotes a
subtype Id node and the latter a subtype indication node. The subtype id
represents the defining occurrence oT the subtype name, and the
subtypeindication node records the type mark and constraint appearing in the
subtype declaration.
The second context in which a subtypedecl node may appear is as a part of
a normalized parameter list for a generic instantiation, in which case the
subtypedecl node does not represent actual source code. This case is discussed
in more detail in section 3.6.1.1.
The task decl node represents the declaration of either a task type or a
single task object with an anonymous type, depending on whether or not the
reserved word "type" is included in the specification. The difference is
indicated by the value of the as source name attribute -- a type id node in the
former case, a variable id node in the latter. The as decl s attribute is a
possibly empty sequenEe of nodes representing the- entry declarations and
representation clauses given in the task specification (subprog entrydecl and
REP nodes). The declaration of a task object (or objects) of a named type is
represented by a variable decl node rather than a task decl node.
3.1.1.3.3.1 SIMPLE RENAME DECL
The class SIMPLE RENAME DECL contains nodes representing the renaming of an
object or an exception. The renaming of an entity as a subprogram or a package
is represented by a subprogentry decl node or a packagedecl node,
respectively.
A renaming declaration for an object is represented by a renamesobjdecl
node. The as source name attribute denotes a variable id or a constant id,
depending on the kind of object renamed. A constant object is -eoresented b) a
constant id; constant objects include constants, discriminants, parameters of
mode in, loop parameters, and components of constant objects. An object that
does not belong to any of the previous categories is represented by a
variable id (this includes objects of a limited type). The as_name attribute of
a renamesobj_decl node denotes a node of type NAME which represents the object
being renamed. The as type mark name attribute references a selected or
used_name_id node corresponding to the type mark appearing in the renaming
declaration.
The renames objdecl node may also appear in a normalized parameter list
for a generic instantiation. This case does not correspond to source code, and
is discussed in detail in section 3.6.1.1.
The renaming of an exception is represented by a renames exc decl node, for
which the as source name attribute always designates an-exception Id. The
as_name attribute can be either a selected node or a used name Id node
corresponding to the exception being renamed.
3.1.1.3.3.2 UNIT DECL
The class UNIT DECL represents the declaration of a subprogram, package,
generic unit, or entry. The as header attribute which is defined on the class
references a HEADER node, the type of which is determined by the reserved word
appearing in the declaration (i.e. "procedure", "function", "package", or
"entry").
The generic decl node corresponds to the declaration of a generic unit.
The as source name attribute references a generic id representing the name of
the generic unit. The as header attribute may denote a procedure spec, a
function spec, or a packagespec. The attribute as item s is a possibly empty
sequence of generic formal parameter declarations -- a list of nodes of type in,
In out, typejdecl, or subprogentryjdeci.
3.1.1.3.3.2.1 NON GENERIC OECL
The class NON GENERIC DECL encompasses subprogram, package, and entry
declarations. The as unTt kind attribute that is defined on the class
determines the kind of declaration the subprog entry decl or package decl node
represents: a renaming declaration, an instantiation, a generic formal
parameter declaration, or an "ordinary" declaration.
An entry (family) declaration is represented by a subprogentrydecl node
for which the as source name attribute is an entry id, the as header attribute
is an entry node, and the as unit kind attribute is void. The renaming of an
entry as a procedure is treated as a procedure declaration (i.e. the
as source name attribute is a procedure id, not an entryId).
The as source name attribute of a package decl node will always designate a
package id, and the sm header attribute -- a package spec. However, the
as unit kind attribute may have one of three values: renames unit (representing
the name of the unit being renamed), instantiation (representTng the name of the
generic unit and the generic actual part), or void (if the declaration is an
"ordinary" one).
The declaration of a procedure, a function, or an operator is represented
by a subprogentrydecl node, for which the as header attribute can be either a
procedure spec or a function spec. In addition to the three values of
as unit kTnd described in the previous paragraph, the as unit kind attribute of
a subprog entry decl node may designate a node from class GENERIC PARAM if the
subprogram in tee declaration is a generic formal parameter..
The as source name attribute for a subprogram declaration is a node from
class SUBPOG NE, with one exception. A declaration renaming an enumeration
literal as a function will have an ENUN LITERAL node as its as source name
attribute (the function spec node denoted by the as header attribute will
contain an empty parameter list). For all other declarations the type of node
designated by the as source name attribute is determined by the kind of
declaration introducing the new name (i.e. a declaration renaming an attribute
as a function will have a function id as its as source name attribute).
A subprogentrydeci node may also appear in a normalized parameter list
for a generic instantiation. In this case the declaration will always be a
renaming declaration which does not correspond to source code (see section
3.6.1.1 for details).
3.1.1.3.4 IO S DECL
The IDSDECL class contains nodes corresponding to declarations which may
define more than one entity -- variable declarations, (deferred) constant
declarations, record component declarations, number declarations, and exception
declarations. Although any of these declarations may introduce a single
identifier, a node from class ID S DECL will always be used to represent the
declaration, never a node from cTas [O DECL.
An exception decl node represents an exception declaration; the
as source name s attribute designates a sequence of exception id nodes.
A deferred constant decl node denotes a deferred constant declaration. The
as source name s attribute refers to a sequence of constant id nodes; each
constant id node represents the first defining occurrence of the associated
identifier. The as_name attribute of the deferred constant decl node is a
used_name_id or selected node representing the type mark given in tne
declaration. The subsequent full declaration of the deferred constant(s) will
be represented by a constant decl node.
3.1.1.3.4.1 EXP DECL
The EXP DECL class represents multiple object declarations that can include
an initial value -- number declarations, variable declarations, and constant
declarations.
A number declaration is denoted by a number decl node for which the
as source name s attribute is a sequence of number id nodes, and the as exp
attribute references a node corresponding to the static expression given in the
declaration.
3.1.1.3.4.1.1 OBJECT DECL
Class OBJECT DECL represents variable, constant, and component
declarations.
A variable decl node represents either a variable declaration in a
declarative part or a component declaration in a record type definition;
as source name s is a sequence of variable id nodes or component_id nodes,
respectively. - The as ex attribute denotes-the (default) initial vaTue, and is
void if none is given. oFr a variable declaration, as type def may denote
either a subtype indication node or a constrained array def node; for a
component declaration as type def refers to a subtype indicatTon node.
A constant decl node represents a full constant declaration. The attribute
as source name s is a sequence of constant Id nodes; as exp represents the
initial value. The as type def attribute may denote either a subtypeindication
node or a constrained arraydef node.
A constant decl node may also appear in a special normalized parameter
sequence for a generic instantiation, in which case it does not represent source
code (see section 3.6.1.1 for details).
Unlike other object declarations, which contain named types only, the
declarations in class OBJECT DECL may introduce anonymous subtypes via a
constrained array definition or the inclusion of a constraint in the subtype
indication. If the object(s) being declared are of a named type, then the
sm obj type attribute of each defining occurrence node in the as source name s
sequence denotes the same entity -- the TYPE SPEC node referenced-in the
defining occurrence node corresponding to the type mark.
If the object declaration contains an anonymous subtype (i.e. as type def
denotes a constrained array def node or a subtype indication node with a
non-void as constraint attribute) then a different TYPE SPEC node will be
created for the sm obj type attribute of each defining occurrence node in the
as source name s sequence. The sm is anonymous attribute of each will have the
vaTue TRUE. If the constraint is non-static, then each TYPE SPEC node
references its own copy of the CONSTRAINT node corresponding to the new
constraint; if the constraint is static then each TYPE SPEC may or may not
reference its own copy. DIANA does not require that the node referenced by the
TYPE DEF attribute of the OBJECT DECL node have a unique node representing the
constraint, even if the constrainf is non-static; the OBJECT DECL node is
allowed to share the CONSTRAINT node with one of the TYPE SPEC nodes.
I
I
V
a-
II II•l
:CI I
6. IwoV
SI i I.a US
W - - - -
~_IV
• IU.1;I I L
CVlI -I-- - - - -0
• i Z 4 I
UdA
I- VIVI 0,
I 01 I l. I II a
VI I I/I
0 I I VI
SIvi
SI iI II
I C
411
9L
0•1 I--
I L II . "
0. II6
V i I 1i.
oU I- -"
U1 Ia ,
I I
I I I
0 L
I
I C
4) • 9L
I I C
Ii iU
I0 I--
-u .91
1 m
XI C1 -
w II l
UCC
CLC
-~ ;w1 -Jto
Section 3.2
DEF NAME
3.2 OEF NAME
The appearances of identifiers, operators, and enumeration characters in a
DIANA tree are divided into defining and used occurrences; the class DEF NAME
contains all of the nodes representing defining occurrences. Each entity o? an
Ada program has a defining occurrence; uses of the name or symbol denoting the
entity always refer to this definition. The defining occurrence contains the
semantic information pertaining to the associated entity; none of the nodes in
class DEF NAME have any structural attributes.
* The names represented by this class fall into two principal categories:
predefined names and user-defined names. Defining occurrences corresponding to
user-defined entities are introduced by the as source name or as source name s
attribute of nodes in class ITEM, BLOCK LOOP, LABELETDand FOR REV. Defining
occurrences associated with predefined -entities are not aEcessable via
structural attributes since they do not have a declaration point.
Each node in class DEF NAME has an lx symrep attribute to retain the source
representation of the identifier or character literal associated with the
defintng occurrence. Those nodes in class SOURCENAME generally have lx srcpos
and Ix comments attributes for which the values are defined; the values of these
attributes are undefined for nodes in class PREDEF NAME. Certain nodes in class
* SOURCE NAME may be used to represent both predefined and user-defined names
(nodes-such as exceptionid); however, lx srcpos and lx comments for these nodes
are undefined when representing a predefined name.
The names associated with certain entities may have more than one point of
definition; in particular, those corresponding to:
(a) deferred constants
(b) incomplete types
(c) non-generic (limited) private types
(d) discriminants
S(e) non-generic formal parameters
(f) program units
I-.-
For these names, the first defining occurrence (which is indicated by the
sm first attribute) is treated as THE definition. In general, all references to
the entity refer to the first defining occurrence, and the multiple defining
occurrences of an entity all have the same attribute values. Types and deferred
constants present special cases which are discussed in subsequent sections.
3.2.1 PREDEF NAME
The nodes in class PREDEF NAME correspond to the names of entities for
which the Ada language does not provide a means of declaration; consequently a
node from class PREDEF NAME will NEVER be designated by a structural attribute.
The nodes attribute id, argument id, pragma id, bltn operator id, and void
comprise this class. The nodes argument_id (The name of a pragma argument or
argument value) and attribute id (the name of an Ada attribute) have no
attributes other than lx symre . The pragma_id represents the name of a pragma.
The sm argument id s attribute denotes a sequence of argument identifiers
associated with the pragma (i.e. the sequence for pragma LIST contains nodes
denoting the argument identifiers ON and OFF); if a particular pragma has no
argument identifiers the sequence is empty. The node bltn operator id
corresponds to a predefined operator; the different operators are distinguished
by the sm operator attribute.
3.2.2 SOURCE NAME
The SOURCE NAME class is composed of those nodes corresponding to defining
occurrences of intities which may be declared by the user.
The exception id node represents an exception name. If the exception id is
a renaming then the sm renames attribute is a used_name_id or a selected node
denoting the original exception name (the node which is designated by the
as_name attribute of the renames exc decl node). If the exception name is not
introduced by a renaming declaration then sm renames is void.
An entry (family) name is denoted by an entry id node which has two
non-lexical attributes : sm spec and sm address. The sm spec attribute
references the entry node (which contains the discrete range and formal part)
designated by the as header attribute of the subprog_entry_decl node. The
sm address attribute denotes the expression given in an address clause; if no
address clause is applicable this attribute is void.
3.2.2.1 LABEL NAME
The class LABEL NANE represents those identifiers associated with
statements; the sm str attribute defined on this class denotes the statement to
which the name corresponds. A label id node represents the name of a statement
label and is introduced by a labeled node; sm stm can reference any node in
class STM. A block loop_id represents the name of a block or a loop; sm stm
A SEMANTIC SPECIFICATION
denotes the block or loop node which introduces the block loop Id.
3.2.2.2 TYPE MANE
STheclass TYPE NAME contains nodes associated with the names of types or
subtypes; it has an sm type spec attribute attribute defined on it. Certain
type names may have more than one defining occurrence; in particular, those
corresponding to private and limited private types which are not generic formal
types, and those associated with incomplete types.
j A private typelid or I privatetype Id node represents the defining
occurrence of a type name introduced by i (limited) private type declaration;
the type may or may not be a generic formal type. A privatetypeid or
1_prIvate typeId node has an sm first attribute that references itself, and an
I sm type spec attribute denoting a private or 1_private node.
If the (limited) private type is not a generic formal type then its name
has a second defining occurrence corresponding to the subsequent full type
declaration. The second defining occurrence is represented by a type id node;
the sm first attribute references the private type id or I private type id node
of the corresponding (limited) private type declaration, and the sm type spec
I attribute denotes the full type specification, a node belonging to class
I FULL TYPE SPEC.
Used occurrences of a (limited) private type name will reference the
private-typejid or 1_private typeid as the definition.
Each defining occurrence of the name of an incomplete type is represented
by a typeid node, the sm first attribute of which denotes the typeid node
corresponding to the incomplete type declaration. Ordinarily, the sm type spec
attribute of the type_id nodes for both the incomplete and the full type
declaration refer to the full type specification -- a node from class
I FULL TYPE SPEC. The single exception occurs when the incomplete type is
declared Timmediately within the private part of a package" [ARM, section 3.8.1]
and the package body containing the full type declaration is a separate
compilation unit, in which case the sm type spec attribute of the type id
corresponding to the incomplete type declaration denotes an incomplete node.
The defining occurrences of all other kinds of type names are represented
by typejid nodes. The sm first attribute references the node which contains it,
and the sm type spec attribute denotes a node belonging to the class
FULL TYPESP.
A new TYPE SPEC node is created for the sm type spec attribute of a type id
node unless the type id corresponds to an incomplete type declaration and the
full type declaration iý in the same compilation unit. A new private or
1 private node is always created for the sm type spec attribute of a
prlvate type Id or 1 private typeId node.
A subtype id node represents the defining occurrence of a subtype name; its
only non-lexical attribute is sm type spec, which references the appropriate
subtype specification. If the subtypeid is introduced by a subtype declaration
In which the subtype indication contains a constraint then a new TYPE SPEC node
is created to represent the subtype specification. If the subtype declaration
does not impose a new constraint then the sm type spec attribute references the
TYPE_SPEC node associated with the type mark appearing in the declaration.
A subtypeid may also be introduced by a declarative node in a normalized
parameter list for a generic instantiation, in which case the subtype id does
not correspond to source code. The correct values for its attributes Tn this
instance are defined in section 3.6.1.1.
3.2.2.3 OBJECT NAME
The class OBJECT NAME contains nodes representing defining occurrences of
entities having a value and a type; it is composed of iteration id,
ENUM_LITERAL, and INIT OBJECT NAME. The smob_ y attribute which is defTred
on the class denotes the subtype of the object or literal.
An iteration id represents the defining occurrence of a loop parameter, and
is introduced by an iteration node. The sm obj type attribute references tne
enumeration or integer node denoted by the sm base type attribute of tne
DISCRETE RANGE node associated with the iteration scheme.
3.2.2.3.1 ENUM_LITERAL
The class ENUM_LITERAL is composed of nodes representing the defining
occurrences of literals associated with an enumeration type. The nodes
enumeration id and character Id comprise this class -- enumeration id
corresponds to an identifier, character id to a character literal.
ENUM_LITERAL defines the attributes sm pos and sm rep, both of which are of
type Integer. The attribute sm pos contains the value of the predefined Ada
attribute POS, i.e. the universal integer corresponding to the actual position
number of the enumeration literal. The sm rep attribute contains the value of
the predefined Ada attribute VAL; the user may set this value with an
enumeration representation clause. If no such clause is in effect, the value of
sm rep will be the same as that of sm pos. The sm ooj type attribute references
the enumeration node corresponding to the enumeration type to which the literal
belongs.
An ENUM_LITERAL node may be introduced by either an enumeration def node or
a subprogentrydecl node. The latter corresponds to the renaming of an
enumeration literal as a function, in which case the semantic attributes of the
ENUM_LITERAL node will have the same values as those of the node corresponding
to t_e original literal.
An ENUM_LITERAL node may be introduced by a declarative node in a special
normalized parameter list for a generic instantiation; in this instance the
ENUM_LITERAL node does not correspond to source code. This case is discussed in
detaTil in section 3.6.1.1.
3.2.2.3.2 INIT OBJECT NAME
The class INIT OBJECT NAME contains nodes corresponding to defining
occurrences of objects whict may have an initial value; it defines an attribute
sm init exp to record this value. This attribute represents those (default)
initial values which are explicitly given; i.e. the default value NULL for an
access object is not represented by sm init exp unless it is explicitly
specified in the source code. The objects denoted by the nodes of this class
include named numbers, variables, constants, record components, and formal
parameters.
The node number id represents the definition of a named number. The
sm obj type attribute denotes a universal integer or universal real node, and
the sm init exp attribute references the node denoted by the as exp attribute of
the corresponding numberdecl node.
3.2.2.3.2.1 VC NAME
The class VC NAME is composed of the nodes variable id and constant id,
denoting the names of variables and constants, respectTvely. The attribtutes
sm renames obj and sm address are defined for the nodes in this class.
The sm renames obj attribute is of type Boolean, and indicates whether or
not the name of the object is a renaming; the value of this attribute determines
the meaning of the sm init exp attribute for nodes in this class. If the name
is introduced by a renaming declaration then sm init exp denotes the NAME node
referenced by the as_name attribute of the renames objdecl node. Otherwise,
sm init exp is the XP node designated by the as exp attribute of the associated
iBJECTDEC[ node, and consequently may be void.
The sm address attribute denotes the expression for the address of the
object as given in an address clause; if no such clause is applicable sm address
is void. In the case of a renaming, the value of the sm address attribute is
determined by the original object; if the original object cannot be named in an
address clause then sm address is void.
For a VC NAME node corresponding to an ordinary object deciaration the
sm obj type attribute denotes either the TYPE SPEC node corresoonding to the
* type mark in the declaration, or an anonymous TYPE SPEC node if the declaration
contains a constrained array definition or a constraint in the subtype
indication. If the variable id or constant id is introduced by a
renames obj decl node, then sm obj ty e is the TYPE SPEC node corresponding to
the subtype of the original object (hence this TYPE_SPEC node does not
necessarily correspond to the type mark in the renaming declaration, although it
will have the same base type).
A constant id represents the name of a constant object. A constant object
may be either a (deferred) constant or the renaming of one of the following: a
(deferred) constant, a discriminant, a loop parameter, a (generic) formal
parameter of mode in, or a component of a constant object. The sm first
attribute references the constant id node corresponding to the first defining
occurrence of the associated name. For a constant id node associated with the
i
OIANA Reference Manual Draft Revision 4 Page 3-18
full declaration of a deferred constant this attribute will reference the
constant id corresponding to the deferred declaration; for all other constant id
nodes the sm first attribute will contain a self-reference.
The attributes of the constant id nodes representing the defining
occurrences of a deferred constant -have the same values. The sm obj type
attribute designates a private or l_private node, and sm init exp denotes the
initialization expression given in the full constant declaration. Used
occurrences of a deferred constant name reference the constant id of the
deferred declaration.
The variable id node represents the name of an object which is declared in
an object declaration or a renaming declaration but is not a constant object.
The sm is shared attribute has a Boolean value indicating whether or not a
SHARED pragma has been applied to the variable. If the variable Id represents a
renaming then sm is shared indicates whether or not the origTnal object is
shared.
Both the constant id and the variable i% nodes nmay be introduced by
declarative nodes in a normalized parameter lTst for a generic instantiation, in
which case they do not represent source code. The appropriate values for the
attributes of each are discussed in section 3.6.1.1.
3.2.2.3.2.2 COMP_NAME
The nodes component_id and discriminant_id comprise the class COMP_NAME,
which represents the defining occurrences ol identifiers associated with record
components and record discriminarts. The attribute sm comp rep is defined for
the nodes in this class; it references the node corresponding to the applicable
component representation clause, and is void if no such clause exists. The
attribute smcomp_rep can never denote a compreppragma node.
The sm init exp attribute represents the default initial value, referencing
the EXP node designated by the as exp attribute of the variable decl or
dscrmt decl node (hence sm init exp can be void).
Unlike component names, discriminant names may have multiple defining
occurrences, therefore an sm first attribute is defined for the discriminant_id
node (the instance of a component name in a component representation clause is
considered to be a used occurrence rather than a defining occurrence). If an
incomplete or non-generic (limited) private declaration contains a discriminant
part, the discriminants will have a second definition point at the full type
declaration; the sm first attribute of both discriminant_id nodes will reference
the discriminant_id node corresponding to the earlier-incomplete or (limited)
private declaration.
3.2.2.3.2.3 PARAM NAME
The class PARAM NAME contains nodes corresponding to the names of formal
parameters declared in the formal parts of subprograms, entries, accept
-' statements, and generic units. The nodes in Id, in out id, and out id comprise
PARAN NAME," representing parameters of mo_e in, Tn out, and out,-respectively
"- (an otid node can never be used to represent a generic formal object).
The attribute sm initexp records the initial value; it denotes the EXP
node referenced by the as e p attribute of the corresponding in, in out, or out
node. The attribute sm '-it exp is void for in out id and out id nodes.
Formal parameters associated with subprogram declarations, entry
declarations, and accept statements may have more than one defining occurrence.
The sm first attribute for a PARAN NAME node belonging to an entry declaration
"or an accept statement will always reference the PARAM NAME node of the entry
declaration. The sm first attribute of a PARA1KNAME node corresponding to a
subprogram name denotes the PARAJM NAME node of-the subprogram declaration, body
declaration, or stub declaration which first introduces the identifier.
3.2.2.4 UNIT NAME
The class UNIT NAME represents the defining occurrences of those
identifiers and symbols associated with program units; it contains the nodes
taskbody_id, genericid, and package id, as well as the class SUBPROGNAME.
The task body id node denotes a task unit name introduced by the
declaration oT a body or a stub. The sm first attribute references the type id
or variable id node (depending on whether or not the task type is anonymous) of
the task specification. The sm type spec attribute denotes the taskspec node
denoted by either the sm.type spec attribute of the typeid node or the
sm obj type attribute of the variable-id node.
If the body of the task is in the same compilation unit then the bm body
attribute of the task body_id references the body (: black body node). If the
body is in another compTlation unit, but the stub is not, then sm body denotes
the stub (a stub node). Otherwise sm body is void.
3.2.2.4.1 NON TASK NAME
The nodes in class NON TASK NAME correspond to the names of program units
which are not tasks. T_e nude generic_id and the class SUBPROG PACK NAME
comprise this class.
The generic Id node corresponds to the defining occurrence of the name of a
generic unit (the name of an instantiated unit is represented by a member cf
class SUBPROG PACK_NAME). The sm first attribute of a generic id always
I references the generic id of the generic specification. The sm spec attribute
denotes the procedurespec, function spec, or packagespec associated with the
subprogram or package specification. The attribute sm generic param s
represents the formal part of the generic specification, and references the same
sequence as the as item s attribute of the corresponding generic decl node. The
sm is inline attribute indicates whether or not an INLINE pragma-has been given
for the generic unit. The value of the sm body attribute is determined in the
same manner as the sm body attribute of the task bodyid node (discussed in the
previous section).
3.2.2.4.1.1 SUBPROG PACK NAME
Defining occurrences of packages and subprograms are represented by members
of class SUBPROG PACK NAME. The attributes sm address and sm unit desc are
defined on this class. -The sm address attribute records the expression given in
an address clause for the unit, if such a clause does not exist then sm address
is void. The sm unit desc attribute is a multi-purpose attribute; in some cases
it is used to indicate that a particular unit is a special case (such as a
renaming), in others it is used as a "shortcut" to another node (such as the
unit body).
The node package_id represents the defining occurrence of a package; its
sm spec attribute denotes a packagespec node. If the package id does not
correspond to a renaming or an instantiation then sm spec references the
package spec designated by the as header attribute of the packagedecl node,
sm first references the package id of the package specification, and
m 'unitdesc denotes a node from class BODY (the value of this attribute is
determined in the same manner as the value of the sm body attribute of the
taskbody id, which is discussed in section 3.2.2.4).
If the packageId corresponds to a renaming then the sm unit desc attribute
references a renames unit node which provides access to the original unit. The
sm first attribute of the packagejid contains a self-reference, while the
sm spec and sm address attributes have the same values as those of the original
package.
If the package id is introduced by an instantiation then sm unit desc
designates an instantiation node containing the generic actual part as well as a
normalized parameter list. The sm first attribute of the package_id contains a
self-reference; the value of sm address is determined by the existence of an
address clause for the instantiated package, consequently it may be void. The
sm spec attribute references a new packagespec node that is created by copying
the specification of the generic unit and replacing every occurrence of a formal
parameter by a re_erence to a an entity in the normalized parameter !is*. The
construction of the new specification is discussed in further detail in section
3.6.1.1.
3.2.2.4.1.1.1 SUBPROG NAME
The class SUBPROG NAME represents defining occurrences of subprograms; it
comprises the nodes procedure-id, function id, and operator id. The attributes
sm is inline and sm interface are defined for the nodes in this class. The
sm is inline attribute has a boolean value which indicates whether or not an
INLINE pragma has been given for the subprogram. If an INTERFACE pragma is
given for the subprogram then sm interface denotes the pragma, otherwise it is
void.
The procedure id node corresponds to a defining occurrence of a procedure
or an entry renamed as a procedure; its sm spec attribute references a
procedurespec node. In addition to representing a function, a function id may
represent an attribute or operator renamed as a function. An operator_id may
denote an operator or a function renamed as as operator. The sm spec attribute
of a function id or ar operatorId designates a function spec node.
If the SUBPROG NAME node is introduced by an "ordinary" declaration then
sm unit desc denotes a member of class BODY, and sm spec references the HEADER
node denoted by the as header attribute of the associated subprog entry decl
node. The appropriate BODY node is selected in the manner described for the
sm body attribute of the taskbodyId node in section 3.2.2.4.
Like a package_id, a SUBPROG NAME node may be introduced by a renaming
declaration, in which case sm unit desc denotes a renames unit node and sm first
contains a self-reference. However, the sm spec attribute of the SUBPROG NAME
node does not denote the specificationoF -theoriginal unit, but that OT the
renaming declaration. The values of the attributes sm address, sm is inline,
and sm interface are the same as those of the original unit.
The instantiation of a subprogram is treated in the same manner as the
instantiation of a package. The sm unit desc attribute denotes an instantiation
node, sm first contains a self-reference, and a new procedure spec or
function spec is constructed in the manner described in section 3.6.1.1. The
values oT sm address and sm interface are determined by the presence of an
associated address clause or INTERFACE pragma; either may be void. The
sm is inline attribute is true if an INLINE pragma is given for the generic unit
OR the instantiated unit.
A SUBPROG NAME node may also represent a generic formal parameter, in which
case sm unit desc denotes a node belonging to class GENERIC PARAM, the sm first
ettribute contains a self-reference, and sm spec denotes the HEADER node
introduced by the generic parameter declarat'io7n.o The attributes sm address and
sm interface are void, and sm is inline is false.
The sm unit desc attribute of an operator id may reference an
implicit not eq node, which indicates that the inequality operator has been
declared imoTicitly by the user through the declaration of an equality operator.
The inequality operator is not the predefined operator, but because it cannot be
explicitly declared it has no corresponding body, hence that of the
corresponding equality operator must be utilized. Access to the equality
operator is provided by the implicitnoteq node. An operator id which contains
a reference to an implicit not eq node can be denoted only by semantic
attributes.
The sm spec attribute of an operator id representing an implicitly declared
inequality operator may reference either-the function spec of the corresponding
equality operator or a copy of it. The attribute sm address is void,
sm interface has the same value as the sm interface attribute of the
corresponding equality operator, and the value of sm is inline is determined by
whether or not an INLINE pragma is given for the implicitly declared inequality
operator.
The sm unit desc attribute of a SUBPROG NAME node may reference a
derived subprog node, in which case the procedure or function is a derived
subprogram. The specification of the derived subprogram is obtained by copying
that of the corresponding subprogram of the parent type, and making the
following substitutions:
(a) each reference to the parent type is replaced by a reference to the
derived type
(b) each reference to a subtype of the parent type is replaced by a
reference to the derived type
(c) each expression of the parent type is replaced by a type conversion
which has the expression as the operand and the derived type as the
target type.
The remaining attributes have the following values: sm address is void,
sm interface has the same value as the sm interface attribute of the
corresponding derivable subprogram, and the value of sm is inline is determined
by the existence of an INLINE pragma for the derived subprogram.
The nodes in class SUBPROG NAME may be introduced by declarative nodes in a
normalized parameter list constructed for a generic instantiation, in which case
they do not correspond to source code. A more detailed discussion may be found
in section 3.6.1.1.
I
I
SENANTIC SPECIFICATION
0 C
-cc,
ai - 11
S 0.
6.0 CL
I K
.0
6z Iin
SI 3 I lI
I 4U C
I Q 1o - --..
cc a)
XE0
I -0
I I. i26
I I S I 0
a 10
2k,
II- I I J I
n II t o
I.I I hS I_S
F a
C I 0 "" -
-- 6 I I_I UccI 2
6 • Eli d
I~~ z 411 i
61 -. - :
I 4
2 E
6.6
_. - WL
j_,, a•
III -
.mI It I ui j- C
0 I 0
'00I to t
I
I 0 I Uw- l •
• m m_mm m n mni I I•mm '
K
Section 3.3
TYPESPEC
3.3 TYPE_SPEC
The classes TYPE SPEC and TYPE DEF are complementary -- the former
represents the semantic concept of an Aja type or subtype, the latter represents
the syntax of the declaration of an Ada type or subtype. A TYPE SPEC noae does
not represent source code; it has no lexical or structural attributes, only
semantic attributes and code attributes. A TYPE DEF node has no other purpose
than to record source rode, containing only le_ical and structural attributes.
A node from class TYPE SPfC will NEVER be designated by a structural attribute,
a node from class TYPEDEF node will NEVER be designated by a semantic
attribute.
Each distinct type or subtype is represented by a distinct node from class
TYPE SPEC; furthermore, there are never two TYPE SPEC nodes for the same entity.
Although anonymous type and subtype declarations-are not represented in DIANA,
the anonymous types and subtypes are themselves represented by nodes from class
TYPE SPEC.
The nodes universal integer, universal real, and universal float represent
the universal types; they have no attributes.
The incomplete node represents a special kind of incomplete type.
Ordinarily incomplete types are not represented by TYPE SPEC nodes, all
references are to the full type specification. The sole exception occurs when
an incomplete type declaration is given in the private part of the package, and
the subsequent full type declaration appears in the package body, which is in a
separate compilation unit. In this case the incomplete type is represented by
an incomplete node, and all references denote this node rather than the full
type specification. The incomplete node defines an sm discriminant s attribute
which aenotes the sequence of discriminant declarations designated by tne
as dscrmt decl s attribute of the type decl node introducing the incomplete
type. This sequence may be empty.
3.3.1 DERIVABLE SPEC
The class DERIVABLE SPEC consists of nodes representing types which may be
derived. The attribute sm derived which is defined on this class refers to the
parent type if the type is derived; otherwise it is void. The sm aerived
attribute is void for a subtype of a derived type. The nodes in this -asEs also
have a boolean attribute, sm is anonymous, which indicates whether or not the
type or subtype has a name.
A derived type is always represented by a new node corresponding to a new
base type (the node is the same kind as that of the parent type). If the
constraints on the parent type are not identical to those on the parent subtype
then the base type is anonymous, and a new node corresponding to a subtype of
that type is also created. The node associated with the subtype records the
additional constraint (the constraint may be given explicitly in the derived
type definition or implicitly by the type mark).
In addition to being created by a derived type definition, a derived type
may be introduced by a numeric type definition. The base type of a user-defined
numeric type is an anonymous derived type represented by the ,p-mp-itP.
integer, float, or fixed node. The sm derived attribute of the node for the
anonymous base type refers to the node corresponding to the appropriate
predefined type.
If a derived type is an enumeration type then a sequence of new enumeration
literals is created for the derived type, unless the parent type is a generic
formal type. The value of sm pos in each new ENUMLITERAL node is the same as
that in the corresponding node from the parent type; however, the sm obj type
attribute denotes the enumeration node for the derived type. The value of
sm rep depends on whether or not a representation clause is given for the
de'rived type; if not, the value is taken from the corresponding node from the
parent type.
If a derived type is a record type and a representation clause is given for
that derived type, then a sequence of new discriminants and a sequence of new
record components are created for the derived type. If a representation clause
is not given for the derived record type then construction of the new sequences
is optional. Excluding sm comp rep, the values of all of the attributes in each
new COMP_NAME node will be the same as those in the corresponding node from the
parent type; sm comp rep will be the same only if no representation clause is
given for the derived type.
Certain members of class DERIVABLE SPEC may represent generic formal types.
The attributes of these nodes reflect the properties of the generic formal
types, not those of the corresponding actual subtypes. For instance, the
sm size attribute of an array nodo corresponding to a generic formal array type
is always void, reflecting the fact that a representation clause cannot be given
for a generic type. The value of this attribute implies nothing about the va'ue
of this attribute in the array node of a corresponding actual subtype. The
values of attributes having a uniform value when corresponding to generic formal
types are discussed in the appropriate sections.
The sm derived attribute of a DERIVABLE SPEC node representing a generic
formal type is always void, and sm is anonymous is always false.
3.3.1.1 PRIVATE SPEC
The nodes in class PRIVATE SPEC -- private and I private -- represent
private and limited private types, respectiveTy. The attributes
sm discriminant s and sm type spec are defined for these nodes. The
sm_discriminant s attribute references the sequence of discriminant declarations
introduced by the (limited) private type declaration; hence it may be empty.
The sm_type_spec attribute designates the full type specification (a node from
class FUL•T_MEPEC) unless the node corresponds to a generic formal private
type, in which case the value of sm type spec is undefined.
A subtype or derived type declaration which imposes a new constraint on a
(limited) private type results in the creation of a constrained record node if
the declaration occurs in the visible part or outside of the package; if the
declaration occurs in the private part or th? package body then a node from
class CONSTRAINED or class SCALAR is created. The sm base type attribute of the
new node references the private or 1-private node associated with the type mark.
An attribute of type TYPE SPEC that denotes a (limited) private type always
references the private or 1-private node. Access to the associated full type
specification is provided by the sm type spec attribute of the PRIVATE_SPEC
node.
3.3.1.2 FULL TYPE SPEC
The class FULL TYPE SPEC represents types which are fully specified. The
node task_spec and the cTass NON_TASK comprise FULLTYPESPEC.
The task spec node represents a task type. A task type may be anonymous if
the reserved word "type" is omitted from the task specification, in which case
the task spec node will be introduced by the sm obj type attribute of a
variable id rather than the sm type spec attribute of a type id.
The task spec node defines five additional semantic attributes: sm decl s,
sm body, sm address, sm size, and sm storage size. The sm decl s attrioute
denotes the sequence of entry deLlarations and representation clauses designated
by the as decl s attribute of the associated task decl node. The attriDute
sm body denotes the block body node corresponding to the task body 'f it is in
the same compilation unit; if not, sm body refers to the stub node if the stub
is in the same compilation unit; if neither the body nor the stub is in the same
compilation unit, then sm body is void. Each of the remaining semantic
attributes (sm address, sm size, and raesize) denotes the EXP node of
the corresponding representation clause, if one exists; otherwise it is void.
3.3.1.2.1 NON TASK
Class NON TASK represents fully specified types which are not tasks. The
nodes in this class are used to denote both types and subtypes. The attribute
sm base type which is defined on this class references the base type -- a node
containing all of the representation information. The sm base type attribute of
a NON TASK node representing a generic formal type always contains a
self-reference. The classes SCALAR, UNCONSTRAINED, and CONSTRAINED comprise
NON_TASK.
3.3.1.2.1.1 SCALAR
The nodes in class SCALAR represent scalar types and subtypes. A scalar
subtype is denoted by the same kind of node as the type from which it is
constructed (unless it is constructed from a private type); however, a type may
always be distinguished from a subtype by the fact that the sm base type
attribute of a node corresponding to a type references itself.
The SCALAR class has an sm range attribute which references a node
corresponding to the applicable range constraint. In most cases this node
already exists (the source code has supplied a constraint, or the range from the
appropriate predefined type is applicable); however, in certain instances a new
range node must be constructed.
A new range node is created for an enumeration node introduced by either an
enumeration type definition or a derived type definition which does not impose a
constraint. The as expl and as exp2 attributes of the range node denote
USED OBJECT nodes corresponding to the first and last values of the enumeration
type. A new RANGE node is also created when more than one object is declared in
an object declaration containing an anonymous subtype with a non-static range
constraint. The subtypes of the objects do not share the same RANGE node in
this case; a new copy of the RANGE node is made for the new subtype of eacn
additional object in the declaration (if the constraint is static, the copy is
optional).
The attribute cd impl size which is defined on this class contains the
universal integer value of the Ada attribute SIZE; it may be less than a
user-defined size.
The nodes in class SCALAR may also represent generic formal scalar types.
The enumeration node represents a formal discrete type; the integer node a
formal integer type; the float node a formal floating point type; and the fixed
node a formal fixed point type. The sm range attribute for a generic formal
scalar type is undefined.
The node enumeration represents an enumeration type. If the type is not a
generic formal type then the sm literal s attribute references the sequence of
enumeration literals -- either the sequence denoted by the as enum_literal_s
attribute of the enumeration def node or a new sequence of literals createc for
a derived type. If the enumeration node represents a generic forma' type then
sm literal s denotes an empty sequence.
The integer node represents an integer type; it defines no attributes of
its own.
3.3.1.2.1.1.1 REAL
The nodes in class REAL -- float and fixed -- represent floating point
types and fixed point types, respectively. If the type is Dt a generic formal
type the sm_accuracy attribute contains the value of the accuracy definition:
digits for the float node, and delta for the fixed node. The value of
sm accuracy for a generic formal type is undefined. The fixed node defines an
"SEMANTIC SPECIFICATION
additional attribute, cd impl small, which has the value of the Ada attribute
SMALL.
3.3.1.2.1.2 UNCONSTRAINED
An unconstrained array, record, or access type is represented by a node
from class UNCONSTRAINED. The sm base type attribute of an array, record, or
access node always contains a self-reference. The sm size attribute which is
defined for this class references the EXP node give-niTna length clause for that
type; if no such clause is given then sm size is void.
-- The access node represents an unconstrained access type. An access type is
unconstrained if its designated type is an unconstrained array type, an
unconstrained record type, a discriminated private type, or an access type
having a designated type which is one of the above; otherwise, it is
constrained. A derived access type is unconstrained if its parent subtype is
unconstrained and the derived type definition does not contain an explicit
constraint.
The sm desig type attribute denotes the TYPE SPEC node corresponding to the
designated type -- an incomplete node, or a node from class UNCONSTRAINED or
class PRIVATE SPEC (if sm desig type denotes an access node, then the
sm desig type attribute of that access node cannot refer to another access
node). The TYPE_SPEC node referenced by the sm desig type attribute of an
access node is never anonymous.
The access node also defines the attributes sm storage size,
sm is controlled, and sm master. The sm storage size attribute denotes the EXP
node given in a length clause if one is applicable, otherwise it is void. The
attribute sm is controlled is of type Boolean, and indicates whether or not a
CONTROLLED pragma is in effect for that type.
The attribute sm master is defined only for those access types having a
task as a designated subtype. In those cases it references the master which
contains the corresponding access type definition. If the master is a proyrdm
unit then sm master denotes the declaration of the unit -- a task decl,
subprogentrydecl, or packagedecl node. If the master is a block then
sm master denotes a block master node, which contains a reference to the block
statement containing the acEess type definition.
The array and access nodes may represent generic formal types, in which
case the sm size attribute is void, sm storage size is void, sm is controlled is
false, and sm is packed is false.
3.3.1.2.1.2.1 UNCONSTRAINED_COMPOSITE
The class UNCONSTRAINED COMPOSITE represents unconstrained composite types;
it is composed of the nodes array and record. Two Boolean attributes are
defined on this class: sm is limited and sm is packed. The attribute
sm is limited indicates whether or not the type has any subcomponents which are
DIAKA Reference Manual Draft Revision 4 Page 3-30
of a limited type; sm is packed records the presence or absence of a PACK pragma
for that type.
The array node defines two attributes of its own: sm index s and
sm come type. The sm index s sequence represents the index subtypes (undefined
ranges) of the array. The attribute sm comp type references a TYPE SPEC node
corresponding to the component subtype; if the subtype indication representing
the component subtype imposes a new constraint then this TYPE SPEC node is an
anonymous subtype.
The node record defines the attributes sm discriminant s, sm comp_list, and
sm_reeresentation. The sm discriminant s attribute denotes the sequence of
discriminant declarations referenced by the as dscrmt decl s attribute of the
typedecl node introducing the record type; this sequence may be empty. The
sm comp_list attribute represents the component list, and the attribute
sm_representation designates the representation clause for that record type; if
none is applicable then sm representation is void.
3.3.1.2.1.3 CONSTRAINED
A constrained array, record, or access type is represented by a node from
class CONSTRAINED. The class CONSTRAINED defines the boolean attribute
sm depends on dscrmt, which is true for a record component subtype which depends
on a discriinnant, and false in all other cases. The sm derived attribute for a
constrained_array or constrained_record node is always void.
The constrained array node defines an sm index subtypes attribute which
denotes a sequence that does not correspond to source code. This sequence is a
semantic representation of the index_constraint, and is derived from the
as discrete_range_s sequence of the index_constraint node. The
sm index subtype s sequence consists of integer and/oF enumeration nodes, some
of which may be created solely for this sequence. If a particular discrete
range is given by a type mark then a new node is not created to represent that
discrete range, the enumeration or integer node associated with the type mark is
used. Otherwise, a new enumeration or integer node is created to represent the
new anonymous index subtype.
The sm base type attribute of a constrained array node always denotes an
array node. If the type is introduced by a constrained array definition then an
anoymous base type is created; i.e. the sm type spec attribute of the typeid
node or the sm obj type attribute of the VC NAME node denotes a
constrained array ncdl which has an anonymous array node as its base type. If
the constriined array definition is part of an object declaration then the
constrained array node will be anonymous as well. The array node representing
the base type does not correspond to source code; its sm index s attribute is a
sequence of undefined ranges which also are not derived from source code. The
array node incorporates the information in the constrained array type definition
pertaining to the component subtype, and the constrainedarray node retains the
constraint information.
A constrained record node has an sm normalized dscrmt s attribute which is
a normalized sequence of the expressions given in the discriminant_constraint.
"No new nodes must be created in order to construct this sequence. The
sm base type attribute of a constrained record node may denote a node of type
record,Tprivate, of l_private.
The constrained access node represents a constrained access type or
subtype. Its sm desiq type attribute denotes the designated subtype. If the
constrained access node is introduced by either a type declaration in which the
subtype ingication contains an explicit constraint, or a subtype declaration
that imposes a new constraint, then the designated subtype is a new anonymous
"subtype. The sm base tyoe attribute of a constrained access node references an
access, private, or 1_private node.
The constrained array and constrained access nodes may represent generic
formal types, in whiEh case the sm depends-on dscrmt attribute is false.
I-
SEMA:ITIC SPECIFICATION
* C Oil-
a U
C AIC-
"* I L
•1. I I
CL0 '-V w- 6
- ! r
- b
UU
E E .A m
C11 0a a0a
4 -
L -I ii 1.1 0 0l
0 I C--
L oz
t 0 I (m --
"I'I
SIn L9 E - - a-- - -
0. Il A 6-
7; ai o
Section 3.4
TYPEDEF
3.4 TYPE DEF
The nodes in class TYPEDEF represent the following constructs in the
source code:
(a) a subtype indication
(b) the portion of a type declaration following the reserved word "is"
(c) the subtype indication or constrained array definition in an object
declaration
With the exception of the nodes constrained arraydef and subtypeindication,
the nodes in this class may be designated only by the as type def attribute of
the typedecl node.
This class contains numerous nodes which do not define attributes of their
own, their purpose being to differentiate the various kinds of type definitions.
The nodes private def and 1_private def correspond to private and limited
private type definitions, respectively. The nodes formal dscrt def,
formal integer def, formal float def, and formal fixed def correspond to generic
formal scalar type definitTons. -- -
Th? node enumeration def corresponds to an enumeration type definition; the
attribute as enum literaT s denotes a sequence corresponding to the enumeration
literals given in the definition.
The node record def corresponds to a record type definition; as comp_list
is the component list given in the definition.
3.4.1 CONSTRAINED DEF
The class CONSTRAINED DEF consists of nodes representing source code
containing a constraint, hence the attribute as constraint is defined on this
class.
The nodes integer def, float def, and fixed def correspond to numeric type
definitions; the as constraint attribute references a node representing the
range constraint, floating point constraint, or fixed point constraint given in
the definition.
The subtypeindication node records the occurrence of a subtype indication
in the source code. It is never designated by the as type def attribute of a
type decl node; however, it may be referenced by the the as type def attribute
of in OBJECT DECL node; or by the as subtype indication attribute of a
subtypedecl, Uiscretesubtype, subtype allocator, or ARR ACC DER OEF node. The
as constraint attribute denotes the constraint given in the subtype indication
(if there is no constraint then this attribute is void), and as_name represents
the type mark.
3.4.2 ARR ACCOPER DEF
The class ARR ACC DER DEF is composed of those nodes associated with type
definitions containing -a subtype indication; in particular, array type
definitions, access type definitions, and derived type definitions. For an
array definition the as subtype indication attribute denotes a node
corresponding to the component subtype; for an access type definition
as subtype indication is the designated subtype; for a derived type definition
it is the parent subtype.
The nodes corresponding to array definitions each have an additional
attribute. The unconstrained arraydef node has an as index s attribute which
denotes a sequence representing the undefined ranges given in the unconstrained
array definition. A constrained array definition is represented by the
constrained_arraydef node, which has an as constraint attribute corresponding
to the sequence of discrete ranges given in the definition (an index_constraint
node).
| i N II
• _SEMANT IC SPECIFICATION
-. I
"I -I U
I -- U
I 0 II'
i_I I.i • I =
I &Ii I I
- .I -,, I, -I - - - - I
I U ..JI
"I 0 I 04
I -- eIl
Section 3.5
CONSTRAINT
3.5 CONSTRAINT
The members of class CONSTRAINT represent discrete ranges and the various
"kinds of constraints defined by the Ada programming language (this class is the
union of the Ada syntactic categories "discrete range" and "constraint"). This
class consists of the nodes index_constraint and dscrmt_constraint, as well as
the classes DISCRETE RANGE and REAL_CONSTRAINT.
The node index_constraint represents an array index_constraint. The
attribute as discrete_range_s denotes a sequence of nodes representing the
"discrete ranges.
A discriminant_constraint is represented by a dscrmt_constraint node. The
as general assoc s attribute corresponds to the sequence of discriminant
associations (a sequence of nodes of type named and/or EXP).
3.5.1 DISCRETE RANGE
The class DISCRETE_RANGE contains the node discrete-subtype and the class
RANGE.
A discrete subtype indication is represented by a discrete subtype node.
The as subtype indication attribute references a node representing the Subtype
indication itself.
3.5.1.1 RANGE
The nodes which comprise class RANGE -- range, rangeattribute, and void --
represent ranges and range constraints. The context determines whether a node
belonging to class RANGE represents a range or a range constraint. If the node
is introduced by an as constraint attribute then it represents a range
constraint; otherwise it is simply a range.
The context also determines the value of the sm type spec attribute. For a
RANGE node introduced by a subtype indication sm type spec refers to the SCALAR
node associated with the type mark. If the RAN node is introduced by a typE
definition or a derived type definition creating a new scalar type then
sm type spec denotes the specification ýf the new base type. Otherwise
sm type spec designates the node correspcnding to the appropriatE base , aS
specified by the Ada Reference Manual. For instance, sm base t pe of a RANGE
node corresponding to a slice denotes the specification f the index type.
The range node corresponds to a range given by two simple expressions,
which are denoted by the attributes as expl (the lower bound) and as exp2 (the
upper bound).
The rangeattribute node represents a range attribute. The as_name
attribute references the NAME node corresponding to the prefix, the attribute
as_used_name_id designates the attribute Id node for RANGE, and as exp denotes
the argument specifying the desired -dimension (if no argL -2nf is given then
as_exp is void).
3.5.2 REAL CONSTRAINT
The class REAL CONSTRAINT contains the nodes float constraint and
fixed constraint, representing floating point constraints and fixed point
constFaints, respectively. This class defines two structural attributes:
as exp and as range. The as exp attribute references the node representing the
Ml static expression for digits or delta. The attribute as ranae denotes
the range given in the constraint; it may be void for floating point constraints
and for fixed point constraints which do not correspond to fixed point type
definitions.
The nodes belonging to REAL CONSTRAINT also have an sm type spec attribute.
If the REAL CONSTRAINT node corresponds- to a subtype indication then
sm type spec of the REAL CONSTRAINT node and the corresponding RANGE node (if
there is one) denotes the type specification associated with the type mark., If
the constraint is introduced by a real type definition or a derived type
definition then sm type spec of the REAL CONSTRAINT node and the RANGE nocc (i
there is one) references the type specification of the new base type.
1
" .3 DIANA Reference Manual Draft Revision 4 Page 3-38
S ~ SEMANTIC SPECIFICATION
0Cc
AA
UI,- *1 I
C C
aZC
0I
ino
o 0
-- I .
4 i
3.6.1.1 RENAME INSTANT
The nodes in class RENAME INSTANT indicate that a subprogram or a package
has been renamed or instantiated. The meaning of the as_name attribute which is
defined on this class depends on whether the node is a renames unit node or an
instantiation node.
The node renames unit represents the renaming of an entity as a subprogram
or a package. The attribute as_name denotes the name of the original entity as
given in the renaming declaration.T-he valia values of as_name are determinec
by the kind of entity being renamed; they are as follows:
(a) package selected or used_name_id
(b) procedure selected or used_name_id
(c) function selected or used_name_id
(d) operator selected or usedop
(e) entry selected or used_name_id or indexed
(f) enumeration literal - selected or used-char or used objectid
(g) attribute - attribute
The instantiation node signifies the instantiation of a generic subprogram
or package. The as_name attribute designates a used_name_id or selected node
corresponding to the name of the generic unit, and the as general assoc s
attribute denotes a possibly empty sequence of parameter associations (nodes of
type EXP and assoc). The sm decl s attribute of the instantiation rcce Is
normalized list of the generic parameters, including entries for all cefault
parameters.
Declarative nodes are used to represent the actual parameters in the
sm decl s sequence. Each parameter has its own declarative node, arc eacm
declarative node introduces a new SOURCE NAME node. The Ix symreo attribute of
eacn SOURCE NAME node contains the symbcl representation of the gqereic n---'
parameter; however, the values of the semantic attributes are determired t) tb
actual parameter. None of the new nodes created during the prccess of
constructing the sm decl s sequence represent source code.
The declarations are constructed as follows:
(a) For every generic forma' in parameter, a constant declaraticn is
created. The as source name s sequence of the constant decl node
contains a single constant id node. The as type def attribute is
undefined, and the as exp attribute designates either the actual
expression or the default expression of the generic parameter
declaration.
The sm first attribute of the constant id node contains a
self-reference (it does not refer to the in id of the generic formal
object declaration), sm renames obj is false, and sm obj type denotes
the TYPE SPEC node of the actual parameter (or default expression).
The attrigute sm init exp designates the same node as the as exp
attribute of the constant decl node.
(b) For every generic formal in out parameter, a renaming declaration is
created. The as source name attribute of the renames obj decl node
"denotes a new variable id nod, and the as type mark name attribute is
undefined. The as_name attribute designates the name of the actual
parameter as given in the generic actual part.
The attribute values of the variable id are determined exactly as
if the declaration were a genuine renaming of the actual parameter as
the formal parameter (see section 3.2.2.3.2.1).
(c) For every generic formal type a subtype declaration is created. The
as source name attribute of the subtypedecl node designates a new
subtype id node which has an sm type spec attribute denoting the
TYPE SPEC node associated with the actual subtype. The
subtype indication node designated by the as subtype indication
attribute has a void as constraint attribute and an as_name attribute
which represents the type mark of the actual subtype.
(d) For every generic formal subprogram, a new subprogram declaration is
created. The subprogentrydecl node is a renaming declaration,
therefore the as unit kind attribute denotes a renames unit node which
references either the actual parameter or the appropriate default. The
as header attribute denotes the HEADER node of the generic actual
parameter.
The as source name attribute designates a new SUBPROG NAME or
ENUM_LITERAL node, depending on the actual (or deiault) parameter. The
kind of node and the values of its attributes (except for sm spec) are
determined precisely as if the declaration were an explicit renaming of
the actual entity as the formal subprogram (see sections 3.1.1.3.3.2.1
and 3.2.2.4.1.1.1). The sm spec attribute denotes the header of the
actual parameter rather than that of the ceneric formal •aramete-
declaration.
Once the normalized declaration list is constructed the specification part
of the generic unit is copied; however, every reference to a formal parameter in
the original generic specification is changed to a reference to the
corresponding newly created declaration. In addition, all references to the
discriminants of a formal type are changed to denote the corresponding
discriminants of the newly created subtype (i.e. the discriminants of the
actual type). All references to the formal parameters of a formal subprogram
are changed to denote the corresponding parameters of the newly created
subprogram (i.e. the formal parameters of the actual subprogram). The value of
the as_name attribute of a DSCRMT PARAM DECL node is undefined in this copy of
the specification, as is the vaue of the as type def attribute of an
OBJECTDECL node.
IJ
The sm spec attribute of the procedure id, function id, or package_id
corresponding to the instantiated unit designates this new specification.
3.6.1.2 GENERIC PARAM
The nodes in class GENERIC PARAM are used to indicate that a subprogram is Ia generic formal parameter. The nodes name-default, box-default, and no-default
comprise GENERIC PARAM.
The name default node signifies that a generic formal subprogram has an
explicitly gTven default. The as_name attribute represents the name of the
default as given -- a node from class- DOIGNATOR or an indexed node.
The node box default indicates that a box rather than a name is given for
the default; it defines no attributes of its own.
The no default node records the fact that no default is specified; it I
defines no attributes of its own.
3.6.2 BODY
The class BODY represent unit bodies; it contains the nodes stub,
blockbody, and void.
The stub node corresponds to a body stub; it defines no attributes of its
own.
The blockbody node represents the contents of either a proper body or a
block statement. It has three structural attributes -- as item s, as stm s, and
as alternative s -- corresponding to the declarative part,. the sequence of
statements, and the exception handlers, respectively.
-d OKAMA Reference Manual Oraft Rlevision 4 Page 3-423
, SEMANTIC SPECIFICATION
I U
I I
I 61-
I )
S I • Il
I ii.i i
I II
I I l-
I €-
I I
--I
Section 3.7
HEADER
3.7 HEADER
The nodes in class HEADER contain all of the information given in the
specification of a subprogram, entry, or package except for the name of the
entity. HEADER contains the node packagespec and the class SUBPENTRYHEADER.
A HEADER node corresponding to either the renaming of a package or an
instantiation will contain no information; i.e. any sequence attributes will
*, denote empty sequences and any class-valued attributes will be void.
The node packagespec represents the declarative parts of a package
specification. It has two semantic attributes -- as decl sl and as decl s2 --
corresponding to the visible and private parts of the specification,
respectively. Either or both of these sequences may be empty.
3.7.1 SUBP ENTRY HEADER
The nodes in class SUBP ENTRY HEADER record the information given in the
formal part of a subprogram or entry declaration. This class defines an
attribute as param s which denotes a possibly empty sequence of paramete-
specifications. The nodes procedurespec, functionspec, and entry comprise
SUBP ENTRY HEADER.
The node functionspec has an additional attribute, as_name, representing
the type mark given in the function specification. If the functionspec
corresponds to the instantiation of a generic function then as_name is void;
otherwise 't designates a used_name_id or a selected rode.
The entry node has the attribute as discrete range, denoting the discrete
range given in the entry declaration. If the declaration introduces a single
entry rather than an entry family then as discrete range is void.
u 0
Joi
I.
3I1
"a,66,
>
" I WIoI
S
'A
u
I--6II I
U~ I Q
L - I §
I S-
I-
0.
/--.
Section 3.8
GENERALASSOC
3.8 GENERAL_ASSOC
The class GENERAL ASSOC represents the following kinds of associations:
(a) parameter
(b) argument
. (c) generic
"(d) component
(e) discriminant
The classes NAMED ASSOC and EXP comprise GENERAL ASSOC. If the association
is given in named fori then it is represented by a nude from class NAMED_ASSOC;
otherwise it is denoted by a node from class EXP.
3.8.1 NAMED ASSOC
The NAMED ASSOC class contains two nodes -- named and assoc. It defines an
attribute as exp which records the expression given in the association.
The assoc node corresponds to associations which contain a single name;
i.e. parameter, argument, and generic associations. The as used name attribute
represents the argument identifier or (generic) formal parameter given in toe
association.
The node named represents associations that may contain more than one
"choice -- component associations (of an aggregate) and discriminant associations
(of a discriminant_constraint). It defines an as choice s attribute which
references a sequence of nodes representing the choices or discriminant names
given in the association. The simple names of components or discriminants that
occur within associations are represented by used_name_id nodes rather than
used object_id nodes.
3.8.2 EXP
The EXP class represents names and expressions; its three components are
NAME, EXP, and void.
Certain names and expressions may introduce anonymous subtypes; i.e.
slices, aggregates, string literals, and allocators. The anonymous subtype is
represented by a constrained_array or a constrained record node, and is
designated by the sm exp type attribute of the expression introducing it.
Anonymous index subtypes (for an anonymous array subtype) are introduced by
discrete ranges which are ,t given by type marks. Subsequent sections will
discuss in further detail the circumstances which produce an anonymous subtype,
as well as the representation of the subtype.
3.8.2.1 NAME
The class NAME represents used occurrences of names; it contains the
classes DESIGNATOR and NAMEEXP, and the node void.
3.8.2.1.1 DESIGNATOR
The nodes in class DESIGNATOR correspond to used occurrences of simple
names, character literals, and operator symbols. DIANA does not require that
each used occurrence of an identifier or symbol be represented by a distinct
node (although it does allow such a representation); hence it is possible for a
single instance of a node corresponding to a used occurrence to represent all of
the logical occurrences of the associated identifier. Used occurrences c* -am
numbers which occur in certain contexts are an exception to the previc-s
statement; see section 3.8.2.1.1.1.
DESIGNATOR consists of the classes USED OBJECT and USED NAME, and der!rzs
the attributes sm defn and lx symrep. The sm defn attribute references the
DEF NAME node corresponding to the defining occurrence of the entity (if tre
entity is predefined the DEF NAME node is not accessable through structural
attributes). The ly symreo att-Tbute 4s the string oerestaic" o• t+e -
of the entity.
3.8.2.1.1.1 USED OBJECT
The class USED OBJECT represents aopearances of enumeration lite-als.
objects, and named numbers. The sm defn attribute of a node from this class
denotes a node from class OBJECT NAME.
USED OBJECT defines the attributes sm exp type and sm value. The
sm exp type attribute denotes the subtype of the entity; i.e. the node
designated by the sm obj type attribute of the defining occurrence of the
entity. The sm value attribute records the static value of a constarn szala-
object; if the entity does not satisfy these conditions then sm value has a
, DIANA Reference Manual Draft Revision 4 Page 3-48
,, distinguished value indicating that it is not evaluated.
The nodes used char and used object id constitute this class; together they
represent the useU occurrences 3f all !he entities having defining occurrences
belonging to class OBJECT NAME. The used char node represents a used occurrence
of a character literal; a used object id node represents the use of an object,
an enumeration literal denoted by an identifier, or a named number. The sm defn
attribute of a used char node references a character id, the sm defn att7uta
of a used objectid miy designate any node from class OBJECTNAMET except for a
character id.
*• Although the names of objects most often occur in expressions, the names of
certain objects -- those of record components (including discriminants) and
parameters -- may also occur on the left-hand side of named associations; these
instances are represented by used_name_id nodes rather than usedobjectid
nodes.
The use of the new name of an enumeration literal renamed as a function is
represented by a used-char or usedobject_id node rather than a function-call
node.
If a used object id corresponds to a named number, and the use represented
by the used objectid occurs in a context requiring an implicit type conversior
of the named number, then the sm exp type attribute of the used object id
denotes the target type rather than a universal type. This means that-it is not
always possible for a single used occurrence of a named number to represent all
used occurrences of that named number; however, a single used occurrence having
a particular target type CAN represent all used occurrences of that named number
requiring that particular target type.
3.8.2.1.1.2 USEDNAME
The class USED NAME represents used occurrences of identifiers or sjmz;'s
corresponding to entities which do not have a value and a type. It contains the
*• node usedop and usedname id.
The roce used op -eoresents the use of an operator symbol, he-ce 4ts
sm dean aztr'bute denotes either an operator-id o, a bltnoperatorid.
A used_name_id node represents a use of the name of any of the remaining
kinds of entitTes. It may also record the occurrence of the simple name of a
discriminant, a component, or a parameter on the left-hand side of a named
association (however, it does not denote a used occurrence of such an object in
any other context). Excluding th4s special case, sm defn may re_e-ence ar.,
member of class DEF NAME except for an operatorTd, a bltn operator id, or a
member of class OBJECT NAME.
I-
3.8.2.1.2 NAME EXP
The nodes in class NAME EXP represent names which are not simple
identifiers or character symbols; i.e. function calls and names having a
prefix. The attributes as_name and sm exp type are defined for the nodes in
this class. The as_name attribute represents either the name of the function or
the prefix.
If the NAME EXP node corresponds to an expression then sm exp type
corresponds to t_e subtype of the entity, otherwise it is void. The o- 1 )
NAME EXP nodes which can possibly have a void sm exp type attribute are the
Indexed, attribute, and selected nodes.
The node all represents a dereferencing; i.e. a selected component formed
with the selector "all". The as_name attribute corresponds to the access
object, and sm exp type is the designated subtype.
The indexed node represents either an indexed component or a reference tc a
member of an entry family. For an indexed component the as exo s attribute
denotes a sequence of index expressions, as_name is the array prefix, and
sm exp type is the component subtype. The as exp s attribute of an entry family
member is a one-element sequence containing the entry index; as_name is t-e
entry name, and sm exp type is void.
A slice is represented by a slice node. The as_name attribute denotes the
array prefix and the as discrete range attribute is the discrete range.
The sm exp type attribute denotes the subtype of the slice. The subtype of
a slice is anonymous unless it can be determined statically that the bounds of
the slice are identical to the bounds of the array prefix, in which case the
sm exp type attribute of the slice node is permitted to reference tre
constrained array node associated with the array prefix. Otnerwise, ar
anonymous subtype is created for the slice node. The anonymous suntype is
represented by a constrained array node having the same base type as that of tre
array prefix; however, the Constraint is taken from the discrete range giver in
the slice.
3.8.2-1.2.1 NAME VAL
The class NAME VAL contains NAME EXP nodes which may have a static value,
consequently the sm value attribute Ts defined for the nodes in this class. If
the value is not static, sm value has a distinguished value indicating that the
expression is not evaluated. NAME VAL comprises the nodes attribute, selected.
and function call.
The node attribute corresponds to an Ada attribute other than a RANGE
attribute (which is represented by a rangeattribute node). The DIANA attribute
as_name denotes the prefix, as used name references the attribute id
corresponding to the given attribute name, and as exp is the universal static
expression. If no universal expression is present then as exp is void.
"SEMANTIC SPECIFICATION
The value of the sm exp type attribute of an attribute node depends on the
kind of Ada attribute it represents, as well as the context in which it occurs.
If the attribute node represents the BASE attribute then sm exp type is void.
If the Ada attribute returns a value of a universal type, and that value is the
object of an implicit type conversion (determined by the context), then
sm expype references the target type. Otherwise sm exp type denotes the
TPESPEC node corresponding to the type of the attribute as specified in the
Ada Reference Manual.
The node selected represents a selected component formed with any selector
other than the reserved word "all" (this includes an expanded name). The
as_name attribute denotes the prefix, and as designator corresponds to the
selector. If the selected node represents an object (i.e. an entity having a
value and a type, for instance a record component) then sm exp type is the
subtype of the object; otherwise it is void.
All function calls and operators are represented by function call nodes,
with the exception of the short circuit operators and the membershTp operators.
The as_name attribute denotes the name of the function or operator -- a
used_name_id, used op, or selected node. The lx prefix attribute records
whether the function call is given using infix or prjfix notation. The
as general assoc s attribute is a possibly empty sequence of paramete,
associations (nodes of type EXP and assoc); sm normalized param s is a
normalized list of actual parameters, including any expressions for default
parameters. The sm exp type attribute denotes the return type. If the function
call corresponds to a predefin-d operator then sm exp type references the
appropriate base type, as specified in section 4.5 of the Ada Reference Manual.
Although the use of an enumeration literal is considered to be equivalent
to a parameterless function call, it is represented by a used char or
usedobjectid node rather than a function call node (this includes the use of
an enumeration literal renamed as a finction). However, the use of an attribute
renamed as a function is represented by a function-call node, not an attribute
node.
3.8.2.2 EXPEXP
The class EXP EXP ro'o-sents evcress'ons wh'i a-e not na-p_. -n
attribute sm exp type which is defined on this Cidss denotes the TYPE SPEC noce
corresponding to the subtype of the expression. EXP EXP contains the nodUE
qualified allocator and subtypeallocator as well is the classes AGG EXP and
EXPVAL.
The nodes qualified allocator and subtype allocator reuresent the two forms
of allocators. Each node has the appropriate structural attribute --
as qualified or as subtype indication -- to retain the information given in the
allocator. The sm exp type attribute denotes the TYPE SPEC node corresponding
to the subtype of the access value to be returned, as determlnet from :he
context. The subtype allocator defines an additional attribute, sm desig type,
which denotes a TYPE SPEC node corresponding to the subtype of the object
created by the alTocatcr. If the subtype ind;cation cct'rs ar e':'4cit
constraint then sm desig type denotes a new TYPE_SPEC node corresponding to the
SEMANTIC 2?ECIFICATION
anonymous subtype of the object created by the allocator.
3.8.2.2.1 AGG EXP
The AGG EXP class represents aggregates and string literals; it is composed
of the nodes aggregate and string literal. The aggregate node may represent an
aggregate or a subaggregate. The string_literal node represents a string
literal (which may also be a subdggregate if it corresponds to the last
dimension of an aggregate corresponding to a multidimensional array Of
characters).
The class AGG EXP defines an sm discrete range attribute to represent the
bounds of a subaggregate; sm discrete range is void for a node representing an
aggregate. The sm exp type attribute of a node corresponding to an aggregdte
denotes the subtype of the aggregate; it is void for a subaggregate. This
implies that in an aggregate or stringliteral node exactly one of these two
attributes is void.
If sm exp type is not void, it designates a constrained_array or
constrained record node corresponding to the subtype. An aggregate or a str4,l
jiteral has an anonymous subtype unless it can be determined statically that tne
constraints on the aggregate are identical to those of the subtype obtained from
the context, in which case sm exp type may (but does not have to) reference the
node associated with that subtype.
If the aggregate has an anonymous subtype it is constructed from the base
type of the context type and the bounds as determined by the rules in the Ada
Reference Manual. If the bounds on the subaggregates for a particular dimension
of a multidimensional aggregate are not the same (a situation which V-ill result
in a CONSTRAINT ERROR during execution) DIANA does not specify the .dagg9re;3e
from which the bounds for the index_constraint are taken.
The string literal node defines only one additional attribute, 'x symre2,
which contains the string itself.
The aggregate node has two different representations of the secuence of
component associations; both may contain nodes of tyoe ncmcd arc EXP. -Te
as general assoc s attribute denotes the cequence zf cornocent lsscc:atcrs 35
given; sm normalized comp s is a sequence of normalized component associations
which are not necessarily in the same form as given, for the following reasons:
(a) Each named association having multiple choices is decomposed into
separate associations for the sm normalized comps sequence, one for
each choice in the given association; hence the as choice s secuenrce
a named node in the normalized list contains only one element. The
manner in which this decomposition is done is not specified, the or,
requirements being that the resulting associations be equivalent, and
that each association be either the component expression itself or a
named association with only one choice. Consider the array aggregate
( 1 2 I 3 => 0 )
The named association could be broken down in such a way that the
sm normalized comp s sequence appeared as if it came from any of the
following aggregates:
1 => 10, 2 => 10, 3 => 10 )
1(.. 3 => 10)
( 10, 10, 10
In the process of normalizing the component associations new named
nodes may be created, and duplication of the component expressions is
• optional. For the remainder of this section all named component
associations will be treated as if they had only one choice.
(b) For a record aggregate, if a choice is given by a component name then
the component expression rather than the named node is inserted in the
proper place in the sequence, hence the normalized sequence for a
record aggregate is actually a sequence of EXP nodes.
(c) In an array aggregate an association containing a choice which is a
simple expression may be replaced by the component expression if it can
be determined statically that the choice belongs to the appropriate
index subtype (this substitution is optional).
(d) A named association with an "others" choice is not allowed in the
sm normalized comp s sequence. For each component or range of
components denoted by the "others" either a component expression is
inserted in the proper spot in the sequence, or a new named node is
created containing the appropriate range.
Due to some of the changes mentioned above it is possible for the
sm normalized comp s sequence of an array aggregate to contain a mixture of EXP
and named nodes.
3.8.2.2.2 EXP VAL
The EXP VAL class contains nodes representing expressions which may have
static values, hence the sm value attribute is defined for the nodes in this
class. If the value is not static then sm value has a distinguished value which
indicates that the expression is not evaluated.
A numeric literal is represented by a numeric literal node. It has an
attribute lx numrep containing the numeric representation of the literal. If
the literal is the object of an implicit conversion then sm exp type denotes the
"target type rather than a universal type.
The null access node corresponds to the access value NULL; it defines no
_- attributes oT its own. Although a distinct null access node may be created for
each occurrence of the access value NULL, UIANA also permits a single
null access node to represent ail occurrences of the literal NULL for tnat
partTcular access type.
The node short circuit represents the use of a short circuit operator. The
as short circuit op attribute denotes the operator (andthen or or else);
as expl and as exp2 represent the expressions to the left and right of the
operator, respectively.
3.8.2.2.2.1 EXP VAL EXP
The class EXP VAL EXP defines an as exp attribute; it comprises the node
parenthesized and the Elasses MEMBERSHIP -and QUALCONV. 4
The parenthesized node represents a pair of parentheses enclosing an
expression. The as exp attribute denotes the enclosed expression, sm value is
the value of the expression if it is static, and sm exp type is the subtype of
the expression. A parenthesized node can NEVER be denoted by a semantic
attribute, nor can it be included directly in a sequence that is constructed
exclusively for a semantic attribute (such as a normalized sequence); the node
representing the actual expression is referenced instead.
3.8.2.2.2.1.1 MEMBERSHIP I
The class MEMBERSHIP represents the use of a membership operator. The
attribute as_ex records the simple expression, and the as membership attribute
denotes the applicable membership operator (in op or notin). MEMBERSHIP I
contains two nodes: rangemembership and type membership. Each contains the
appropriate structural attribute to retain the type or range given in the
expression. j
3.8.2.2.2.1.2 QUALCONV I
The nodes in class QUALCONV -- qualified and conversion -- correspond to
qualified expressions and explicit conversions, respectively. The as exp
attribute denotes the given expression or aggregate, and as_name references the
node associated with the type mark. The sm exp type attribute denotes tne
TYPE_SPEC node corresponding to the type mark. I
I
I
I
1
I
I
*L L
-,C 642
gI US
6w 10C46
SI. 50V 0 U -- J
CL 6
1A.
06 1 ur CC'
•I _CO -- OQ
0. E I r - ! I
,. i 6 V
E,E
I 'U • --
Z d .C S l L
-- I- U ,I 4 ,41
-61
-j 0.
UjU
U I
I I *C I I
S'. -- . I
I -I
41 "
-
I I I2I IIV 6 I"
aI
*6 N
- aaa
*ua 1" 4-
I- -- I
a CL' 42I
0 06L4,Oe
I I
SGI Ul
gW Q
I U LIC
C Im11oa
SI - I
- -us.a. I
Lj 00
I -" C I
I C I I I
00
0. 'a I I
I Ix I I
I I C
Section 3.9
STMELEM
3.9 STHNELEM
The class S4 ELEM contains nodes representing items which may appear in a
sequence of statements; i.e. nodes corresponding to statements or pragmas.
The node stm pragma represents a pragma which appears in a sequence of
statements. Its only non-lexical attribute, as pragma, designates a pragma
"*-node.
3.9.1 STM
A node from class ST represents an Ada statement. Some of the STH nodes
are grouped together because they are similar in their structure to other nodes
in the class; the manner in which these nodes are classified does not imply any
semantic similarity.
The node null stm represents a NULL statement; it defines no attributes of
its own.
The node labeled represents a labeled statement. The as source name s
attribute denotes a sequence of label names (label id nodes). These label names
are defining occurrences, hence the labeled node serves as a "declaration" for
the associated labels. The as pragma s attribute represents the pragmas
occurring between the label(s) and the statement itself; it designates a
- possibly empty sequence of pragma nodes. The as stm attribute denotes the
actual statement, it may reference any type of STM node other than another
labeled node.
The accept node represents an accept statement. The as_name attribute
records the entry simple name; it may denote either a used_name_id or an indexed
node, depending on whether or not the entry is a member of-an e7itry family. The
attr" .te as param s denotes a possibly empty sequence of nodes from class PARAM
corresponding to the formal part. The as stm s attribute is a possibly empty
sequence representing the statements to be executed during a rendezvous.
The abort node represents an abort statement. The as_name s attribute is a
sequence of nodes corresponding to the task names given in the abort statement.
The node terminate corresponds to a terminate statement; it defines no
attributes of its own.
The node goto represents a goto statement. The as_name attribute
corresponds to the label name given in the statement.
The raise node represents a raise statement. The attribute as_name denotes
the exception name, if specified; otherwise it is void.
3.9.1.5.1 CALL STM
The class CALL ST represents procedure calls and entry calls; it comprises
the nodes proceduFe call and entrycall. CALL ST defines two attributes:
as general-assoc s and sm normalized param s. The attribute as general assoc s
otes a possibly empty sequence containing a mixture of assoc and EXP nodes
representing the parameter associations. The sm normalized param s attribute
designates a possibly empty sequence corresponding to a normal-ized list of
actual parameters.
A call to an entry that has been renamed as a procedure is represented by a
procedure-call node rather than an entrycall node.
i
i
I
I
I
I
I
DIANA Reference lanual Draft Revision 4 Page 3-60
)a
SE
I II I IU
I I_I
I- ;
1- -
- 3I3--
>- - - -
CI I I U
I I
I60•
U•I-( / l l6-
S l, l I II
Jul W
I U --.
X •} U
I U I U"I
1.1 r18" -
Sia I-Ua
L |-
OI. 3 -=--
- -, -3•
Ia II ,
- I - I 0-
W I I ,II
Section 3.10
MISCELLANEOUS NODES AND CLASSES
3.10 MISCELLANEOUS NODES AND CLASSES
3.10.1 CHOICE
"Anode from class CHOICE represents either the use of a discriminant simple
name in a discriminant association, or a choice contained in one of the
following:
(a) a record variant
(b) a component association of an aggregate
(c) a case statement alternative
(d) an exception handler
Nodes in this class may appear only as a part of a sequence of choice nodes.
CHOICE comprises the nodes choiceexp, choicerange, and choiceothers.
The node choice-exp represents a choice that is a simple name or an
expression; it has a single structural attribute -- as exp. If the choiceexp
node corresponds to a simple name (that of a discriminant, a component, or an
exception) then as exp references a used_name_id node. Otherwise, choice exp
must represent a choice consisting of a simple expression, which is represented
by a node from class EXP.
A choice which is a discrete range is represented by a choice-range node.
The as discrete range attribute references the discrete range.
A choice others node corresponds to the choice "others"; it defines no
attributes of its own.
3.10.2 ITERATION
The members of class ITERATION -- while, FOR REV, and void -- represent the
iteration schemes of a loop (void corresponds-to the absence of an iteration
scheme). These nodes are introduced by the as iteration attribute of a loop
node.
I-
I_I
The while node represents a "while" iteration scheme. The as exp attribute
denotes a node representing the given condition.
3.10.2.1 FOR REV
The FOR REV class represents a "for" iteration scheme. If the reserved
word "reverse" appears in the loop parameter specification then the iteration is
represented by a reverse node; otherwise it is denoted by a for node. The
as source name attribute designates an iteration id corresponding to the
defining occurrence of the loop parameter. The as discrete range attribute
represents the discrete range.
3.10.3 MEMBERSHIP OP
The class MEMBERSHIP OP consists of the nodes in op and not in. These
nodes are introduced by the as membership op attribute of a MEMBERSHIP node,
their function being to indicate which operator is applicable.
3.10.4 SHORT CIRCUIT OP
The nodes in class SHORT CIRCUIT OP -- and then and or else -- serve to
distinguish the two types of-short.cTrcuit expFessions. They are introduced by
the as short circuit op attribute of the short-circuit node.
3.10.5 ALIGNMENT CLAUSE
The class ALIGNMENT CLAUSE represents the alignment clause portion of a
record representation clause. It is composed of the nodes alignment and void
(void corresponds to the absence of an alignment clause).
The alignment node contains the attributes as pragma s arc as eyp. The
former is a possibly empty secuerce of pragma nodes correspordira to the oragons
occurring between the reserved word "record" and the alignment clause. The
as exp attribute refers to the node associated with the static simple
expression.
3.10.6 VARIANT PART
The VARIANT PART class represents the variant part of a record type
definition; it contains the nodes variant part and void (void corresponds to tne
absence of a variant part).
The variant part node defines the attributes as_name and as variant s. The
as_name attribute references a used object id corresponding to the discriminant
"simple name; as variant s is a sequence containing at least one variant node and
possibly variantpragma nodes.
3.10.7 TESTCLAUSEELEM
The class TEST CLAUSE ELEM represents alternatives for an if statement or a
selective wait statement.- It contains the node select altpragma and the class
TEST_CLAUSE. These nodes may appear only in a test clause elem-s sequence.
The node selectaltpragma represents a pragma which occurs at a place
where a select alternative is allowed. It may appear only in a
test clause elem s sequence of a selective wait node. The as pragma attribute
denotes the-pragma itself.
3.10.7.1 TEST_CLAUSE
A TEST CLAUSE node (condclause or select alternative) represents a
condition and sequence of statements occurring in an if statement or a selective
wait statement. The as exp attribute corresponds to the condition, and the
as stm s attribute to the sequence of statements. The cond clause node may
appear only in a test clause elem s sequence of an if node, and the
select alternative node may oEcur -only in a test clause elem-s sequence of a
selective-wait node.
3.10.8 ALTERNATIVEELEM
The class ALTERNATIVE ELEM represents case statement alternatives,
exception handlers, and pragmas which occur at a place where either of the
previous items are allowed. The nodes alternative and alternativepragma
constitute ALTERNATIVEELEM; they may occur only as members of alternative s
sequences.
The alternativepragma node has a single structural attribute, as pragma,
which denotes the pragma.
The alternative node contains two non-lexical attributes: as choice s and
as stm s. For a case statement alternative the as choice s sequence may contain
any of the nodes belonging to class CHOICE: however, for an exception handler
the sequence is restricted to containing choice exp and choice others nodes.
The as stm s attribute represents the sequence of statements given in the
alternative or handler.
3.10.9 COMP REP ELEM
The class COMP REP ELEM consists of the nodes comp_rep and comp_reppragma,
which may appear onTy in the as comp rep s sequence of a record-rep node.
The comp rep node represents a component representation clause. The
as_name attribute references a used object id corresponding to the component
simple name, as_exp represents the static simple expression, and as range
denotes the static range.
A pragma that occurs at the place of a component clause is represented by a
comp_rep pragma node; as pragma denotes the pragma.
3.10.10 CONTEXTELEM 4
The nodes in class CONTEXT ELEM represent items which may appear at a place
where a context clause is allowed. They may occur only as members of the
context elem s sequence of a compilation unit node.
The with node represents a with clause and any subsequent use clauses and
pragmas. The as_name s attribute is a sequence of used_name_id nodes Icorresponding to the library unit names given in the with- claUse. The
as use pragma s attribute is a possibly empty sequence which can contain nodes
of type use and pragma.
The context pragma node has a single non-lexical attribute, as pragma,
which denotes the pragma.
3.10.11 VARIANT ELEM
The nodes in class VARIANT ELEM correspond to items which may apcear at a
spot where a variant is alTowed. These nodes are contained in the seauence
denoted by the as variant s attribute of the variantpart node.
The variant node has two structural attributes: as chch-e s ard
as comp_list. The as choice s attribute is a sequence reoresenting the choices
applicable to that particular variant; as comp_list corresponas ýo zne comporent
list.
The sole non-lexical attribute of the variantpragma node is as pragma,
denoting the pragma.
3.10.12 compilation
The node compilation corresponds to a compilation; it defines the attribute
as compltn unit s, a possibly empty sequence of compilation-unit nodes.
3.10.13 compilation-unit
A compilation unit node represents an item or items which may appear at a
place where a compilation unit is allowed; i.e. it may represent a compilatir
or a sequence of pragmas.
A compilation unit node represents a sequence of pragmas only when a
compilation consists of pragmas alone. In this case the as context elem s
sequence is empty, the as all decl attribute is void, and as pragma s denotes
the sequence of pragmas which constitute the compilation.
For a compilation unit node correspondir; to a compilation unit the
as context elem s attrTbute is a possibly empty sequence representing the
context clause and any pragmas preceding the compilation unit. The as all dec!
attribute denotes the library unit or the secondary unit, which may oe
represented by one of the following: a node belonging to class UNIT DECL, a
subunit, a subprogram body, or a packagebody. The as pragma s attribute
denotes the pragmas which follow the compilation unit and do not belong to a
subsequent compilation unit. The pragmas allowed to appear in this sequence
include INLINE, INTERFACE, LIST, and PAGE. LIST ana PAGE pragmas which occur
"between compilation units but after any INLINE or INTERFACE pragmas may appear
in either the as pragma s sequence of :ne preceding compilation unit or the
as context elem s sequence of the succeeding compilation unit.
3.10.14 comp_list
A record component list is represented by a comp_list node, which contains
three structural attributes: as decl s, as variant part, and as pragma s. The
as decl s attribute designates a sequence corresponding to either a series of
component declarations or the reserved word "null". The attribute
as variant part denotes the variant part of the record, if one exists. The
as pragma s attribute records the occurrence of pragmas between the variant part
and the end of the record declaration (i.e. pragmas appearing between 'end
case" and "end record").
If the record is a null record then as variant part is void, anc the
sequence denoted by as zragmas 4s empty. The as dec! s attribute is a sequence
I, having a nullcomp_decl noce as Its first elemenc, and any number of pragma
nodes after it.
If the record is not a null record then as decl s is a possibly empty
sequence which can contain nodes of type variable decl and pragma. If the
record type does not have a variant part then as variant part is void and
as pagma s is empty. It is not possible for as decl s to be empty an.l
as variant part to be void in the same ( nplist node.
3.10.15 index
The index node represents an undefined range, and appears only in sequences
associated with unconstrained array types and unconstrained array definitions
(such sequences are denoted by the as index s attribute of the the array and the
unconstrained array def nodes). The as_name attribute refers to the
used_name_id or selected node corresponding to the type mark given in the index
subtjpe Uefinition. The sm type spec attribute references the TYPE_SPEC node
associated with the type mark.
_ DIANA Reference Manual Draft Revision 4 Page 3-67
I..
0
SI V1
U(
C
C II
hi I_I 41 10V• C
I I 1.1 I I hI- - C I
I 1 -,- -.
m mi i C i i i i m m I- min C m mmmm 4mmm mi mmmm-I,, m - -
CIANA Reference Manual Draft Revision 4 Page 3-68
I i
I06
C m
i I I L C I
66I LI IA m
'- I"IS
kU I W - I t I
wu I I- U~ Ui I I
6 wI a, :I
w. I•q U1• I. I- l m •
I U: t . . I I
4_0 ! x.
uj *011
€z
61S -: 0 C m
1A I C IIC0
I SCLS
I1 II I
I - 't
uS
C
SDIANA Reference Manual Draft Revision 4 Page 3-69
i J SEMANTIC SPECIFICATION
II
I0
,I € -- - Q
CE
.II'*:
## CHAPTER 4
RATIONALE
Section 4.1
DESIGN DECISIONS
4.1 DESIGN DECISIONS
During the course of designing DIANA many design decisions were affected by
the need to adhere to the design principles set forth in the first chapter of
this document. This section discusses some of these decisions and the reasons
that they were made. Each subsection explains the design decisions pertaining
to a particular design principle.
4.1.1 INDEPENDENCE OF REPRESENTATION
One of the major design principles of DIANA requires that the definition of
DIANA be representation independent. Unfortunately, some of the information
which should be included in a DTANA structure is implementation-dependent. For
example, DIANA defines a source position attribute for each node which
represents source code. This attribute is'useful for reconstructing the source
program, for reporting errors, for source-level debuggers, and so on. It is
not, however, a type that should be defined as part of this standard sinte each
computer system has idiosyncratic notions of how a position in the source
program is encoded. For that matter, the concept of source position may not be
meaningful if the DIANA arises from a syntax editor. For these reasons,
attributes such as source position are merely defined to be private types.
A private type names an implementation-specific data structure that is
inappropriate to specify at the abstract structure level. DIANA-defines six
private types. Each of these corresponds to one of the kinds of information
which may be installation or target machine specific. They include types for
the source position of a node, the representation of identifiers, tre
representation of various values on the target system, and the representation of
comments from the source program. The DIANA user must supply an implementation
for each of these types.
As is explained in the Ada reference manual, a program is assumed to be
compiled in a 'standard environment'. An Ada program may explicitly or
implicitly reference entities defined in this environment, and the DIANA
"representation of the program must reflect this. The entities that may be
referenced include the predefined attributes and types. The DIANA definition of
these entities is not given in this document but is assumed to be available.
RATIONALE
4.1.1.1 SEPARATE COMPILATION
It would not be appropriate for DIANA to provide the library management
upon which separate compilation of Ada program units is based. Nonetheless, the
possibility of separate compilation affects the design of DIANA. The
intermediate representation of a previously compiled unit may need to be used
again. Furthermore, all of the information about a program unit may not be
known when it is first compiled.
The design of DIANA carefully avoids constraints on a separate compilation
system, aside from those implied directly by the Ada language. The design car
be extended to cover the full APSE requirements[3]. Special care has been takenr
that several versions of a unit body can exist corresponding to a single
specification, that simultaneous compilation within the same project is
possible, and that units of other libraries can be used effectively (51. The
basic decision which makes these facilities implementable is to forbid forwarc
references.
Certain entities may have more than one definition point in Ada. In such
cases, DIANA restricts the attribute values of all of the defining occurrences
to be identical. In the presence of separate compilation the requirement that
the values of the attributes at all defining occurrences are the same cannct
always be met. The forward references assumed by DIANA are void in these cases. IThe reasons for this approach are:
o A unit can be used even when the corresponding body is not yet
compiled. In this case, the forward reference must have the value void 1since the entity does7 not exist.
o Updating a DIANA representation would require write access to a file'
which may cause synchronization problems (see [5]).
o A library system may allow for several versions of bodies for the same
specification. If an attribute were updated its previous value would
be overwritten. Moreover, the maintenance of different versions Shoulc
be part of the library System and should not influence the intermeciate
representation.
4.1.2 EFFICIENT IMPLEMENTATION AND SUITABLITY FOR VARIOUS KINDS OF PROCESSING
The design goals of efficient realization and suitability for many kinds of
processing are interrelated. It was necessary to define a structure containing
the information needed to perform different kinds of processing without
overburdening any one kind of processing with the task of computing and
retaining a great deal oF extraneous information.
Since many tools will be manipulating the source text in some way, it was
decided that DIANA should retain the structure of the original source program.
In order to do this, structural attributes were defined. These attributes
define a tree representing the original source. It is always pcssible tc
regenerate the source text from its DIANA form (except for purely lexical
RATIONALE
issues, such as the placement of comments) by merely traversing the nodes
denoted by structural attributes.
Unfortunately, the structure of the original source program is not always
suitable for semantic processing. Hence, semantic attributes were added to
augment the structural ones. These attributes transform the DIANA structure
from a tree to a network. In some cases these attributes are merely "shortcuts"
to nodes which are already in the DIANA structure, but in other cases semantic
attributes denote nodes which do not correspond to source text at all.
In the process of adding semantic attributes to the definition of DIANA, it
was necessary to decide which information should be represented explicitly and
which should be recomputed from stored information. Obviously, storing as
little information as possible makes the DIANA representation smaller; however,
such an approach also increases the time required for semantic processing.
Storing all of the information required would improve processing speed at the
expense of storage requirements. The attribution principles of D!ANA are a
* compromise between these extremes.
In order to decide whether or not to include a particular attribute the
following criteria were considered:
o DIANA should contain only such information as would be typically
discovered via static (as opposed to dynamic) semantic analysis of the
original program.
o If information can be easily recomputed, it should be omitted.
These two points are discussed at length in the following two subsections.
4.1.2.1 STATIC SEMANTIC INFORMATION
DIANA includes only the information that is determined from STATIC semant'c
analysis, and excludes information whose determination requires DYNAMIC semantic
analysis.
This decision affects the evaluation of non-static excressic-s and
evaluation of exceptions. For examDle, the attribute sm value should not be
used to hold the value of an expression that is not static, ever if an
implementation's semantic analyzer is capable of eva•uating some sucn
expressions. Similarly, exceptions are part of the execution (i.e. dynamic)
semantics of Ada and should not be represented in DIANA. Thus the attribute
sm value is not used to represent an exception to be raised.
Of course, an implementation that does compute these additional values may
S_record the information by defining additional attributes. However, any DIANA
consumer that relies on these attributes cannot be considered a correct DIANA
Ifuser", as defined in this document.
I-
I
RATIONALE
4.1.2.2 WHAT IS 'EASY TO RECOMPUTE'? 4
Part of the criteria for including an attribute in DIANA is that it should
be omitted if it is easy to recompute from the stored information. It is
important to avoid such redundant encodings if DIANA is to remain an I
implementable internal representation. Of course, this guideline requires a
definition of this phrase.
An attribute is easily computed if:
o it requires visits to no more than three to four nodes; or
o it can be computed in one pass through the DIANA tree, and all nodes
with this attribute can be computed in the same pass.
The first criterion is clear; the second requires discussion.
Consider first an attribute that is needed to perform semantic analysis.
As an implementation is building a DIANA structure, it is free to create extra I(non-DIANA) attributes for its purposes. Thus the desired attributes can be
created by those implementations that need them. To require these attributes to
be represented by all DIANA users is an imposition on implementations which use Ialgorithms that do not require these particular attributes.
Consider now an attribute needed to perform code generation. As long as
the attribute can be determined in a single pass, the routine that reads in the
DIANA can readily add it as it reads in the DIANA. Again, some implementors may
not need the attribute, and it is inappropriate to burden all users with it.
It is for these reasons that suggestions for pointers to the enclosing
compilation unit, pointers to the enclosing namescope, and back pointers in
general have been rejected. These are attributes that are easily computed in
one pass through the DIANA tree and indeed may not be needed by all
implementations.
Of course, a DIANA producer can create a structure with extra attributes
beyond those specified for DIANA. Nevertheless, any DIANA consumer that relies
on these additional attributes is not a DIANA "user", as defined ;n t"is
document.
4.1.3 REGULARITY OF DESCRIPTION
In order to increase the clarity of the DIANA description, it was decided
that the class structure of DIANA should be a hierarchy. Unfortunately, some
nodes should belong in more than one class. To circumvent this problem, several
intermediate nodes were defined, nodes for which the only non-lexical attribute
denotes a node that already belongs to another class. These intermediate nodes
do not convey any structural or semantic information beyond the value of the
non-lexical attribute. DIANA contains the following intermediate nodes:
block master
r- RATIONALE
discrete subtype
integerBef
float def
fixed def
chol ceexp
choice-range
stm pragma
select alt_pragma
alternativepragma
comp_reppragma
context pragma
variant pragma
It should be noted that not all nodes containing a single non-lexical
attribute are intermediate nodes. For instance, the renames unit node has a
single non-lexical attribute as_name; however, the renames unit node is not an
intermediate node because it is used to convey the fact that a unit has been
* renamed, in addition to recording the name of the original unit via the as_name
attribute. On the other hand, the choiceexp node was introduced merely because
"the class EXP could not be included in both ASSOC and CHOICE. It contains no
more information than the EXP node denoted by its as exp attribute.
It is not the intention of DIANA to require that intermediate nodes be
represented as such; they are included in the definition of DIANA only to
maintain a hierarchy. This is a natural place for an implementation to optimize
its internal representation by excluding the intermediate nodes, and directly
referencing each node denoted by the non-lexical attribute of an intermediate
node.
In the DIANA description, attributes are defined at the highest possible
level; i.e. if all of the nodes of a class have the same attribute then the
attribute is defined on the class rather than on the individual nodes. In this
"way a node may "inherit" attributes from the class to which it belongs, and from
the class to which that class belongs, etc.
The node void receives a slightly different treatment than the other DIANA
nodes. It is the only node which violates the DIANA hierarchy, and it is the
only node which inherits attributes which cannot be used. The node void
represents "nothing". :t maj be thought of as a null pcinter, a1thc,:h it does
not have to be represented as such. Instead of requiring a different kind of
void node for each class to which void belongs, void was allowed to belong to
more than one class (thus constituting the only exception to the hierarchy).
Because void is a member of many classes, it inherits numerous attributes.
Rather than move the attribute definitions from the classes and put them on all
of the constituent nodes except for void, it was decided to allow void to
inherit attributes. Since it is meaningless for "nothing" to have attributes, a
"restriction was added to the semantic specification of DIANA. The attributes of
void may not be manipulated in any way; they cannot be set or examined, hence
they are in effect not represented in a DIANA structure.
V
I.
Section 4.2
DECLARATIONS
4.2 DECLARATIONS
Explicit declarations are represented in a DIANA structure by declarative
nodes. These nodes preserve the source text for source reconstruction and
conformance checking purposes. They do not record the results of semantic
analysis; that information is contained in the corresponding defining occurrence
nodes. All declarative nodes have a child that is a node or sequence of nodes
(of type SOURCENAME) representing the identifier(s) used to name the newly
defined entity or entities.
Declarative nodes are members of class ITEM. The nodes in class ITEM are
grouped according to similarities in the syntax of the code that they represent
and similarities in the context in which they can appear.
4.2.1 MULTIPLE ENTITY DECLARATIONS
Certain kinds of declarations -- object declarations, number declarations,
discrirminant declarations, compenent declarations, parameter declarations, and
exception declarations -- can introduce more than one entity. The nodes
corresponding to these declarations belong to two different classes,
DSCRMT PARAM DECL and [0_S DECL, both of which define an as source name s
attribute. -These classes are distinguished from each other because tney appear
in different contexts. Discriminant declarations can appear only in
discriminant parts, and parameter declarations can appear only in formal parts.
The remaining multiple entity declarations are basic declarative items, and can
appear in declarative parts. In addition, the basic declarative items can
appear in sequences containing pragmas, which cannot be given in discriminart
parts or formal parts.
ID S DECL is further subdivided into classes according to the syntactic
similarTtTes of the declarations it represents. For instance, object
declarations and number declarations may have (default) initial values, hence
constant decl, variable decl, and number decl belong to class EXPDECL, which
defines an as exp attribute to record that value.
4.2.1.1 OBJECT DECLARATIONS AND COMPONENT DECLARATIONS
The type portion of object declarations may be given in two different ways:
either by a subtype indication or a constrained array definition. The node
RATIONALE
constrained arrayjdef is already a member of class TYPEDEF, which represents
the syntax of type definitions. Rather than include constrained arraydef in
another class and disrupt the heirarchy, the node subtype indication was added
to class TYPE DEF. Thus the class OBJECT DECL, which comprises the nodes
constant decl and variable decl, defines the attribute as type def to represent
the type specification gTven in the object declaration. Obviously as type def
cannot denote any kind of TYPE DEF node other than subtypeindication and
constrained_array-def in this particular context.
The only other kind of declaration which introduces objects and does not
require the type specification to be a type mark is a component declaration, the
type portion of which is given by a subtype indication. Rather than define a
node exclusively for component declarations, they are represented by the same
kind of node as variable declarations, since the variable decl node allows
subtype indications for the as type def attribute. This is also conveniert
because component declarations may be interspersed with pragma•, and both pragma
and variable decl belong to class DECL (a sequence of componenr declarations is
represented Ey a decl s sequence). Component declaratiorns and variable
declarations may be distinguished by the fact that they appear in different
contexts, and by the type of nodes in the as source name s sequence. The former
contains componentid nodes and the latter contains variable id nodes.
A record component list may contain the reserved word "NULL" rather than
component declarations or a variant part. This is indicated by the insertion of
a nullcomp decl node instead of variable decl nodes in the sequence of
component declarations, Hence it was necessary to include the null comp decl
node in class DECL. It is convenient for the null comp decl node to be part of
a sequence because it may be followed by pragmas (a pragma can appear after a
semicolon delimiter). Although nullcomp_decl belongs to DECL, the ONLY place
that it can appear in a DIANA structure is as the first node in the as decl s
sequence of the comp_list node (this restriction is given in the semantic
specification of DIANA).
4.2.2 SINGLE ENTITY DECLARATIONS
The remaining kinds of declarations introduce single entities. They are
represented by the classes SUBUNIT BODY and ID DECL. Like the :IaSSES
DSCRMT PARAM DECL and TO S DECL. SUBUNTT BODY and TD DECL are distinouishec
because they represent- declarative items wnicn occur in different contexts.
SUBUNIT BODY represents body declarations, both proper body and stub
declarations. These declarations are separated from the declarations in IO DECL
because body declarations are exclusively later declarative items (the- few
members of ID DECL that are later declarative items are basic declarative items
as well). IO 5ECL contains basic declarative items and items that can appear ijr
task specifications.
Body declarations include subprogram body declarations, package body
declarations, task body declarations, and stub declarations. These declaraticrs
are represented by the nodes subprogram body, packagebody, and task body. The
difference between a proper body and a stub is indicated by the value of the
as body attribute, a blockbody in the former case, and a stub in the latter.
SDIANAReference Manual Draft Revision 4 Page 4-9
RATIONALE
4.2.2.1 PROGRAM UNIT DECLARATIONS AND ENTRY DECLARATIONS
Due to syntactic similarities, declarations of entries and program units
other than tasks are represented by nodes from class UNIT DECL, which contains
only three members: generic dec1, subprogentry-decl, and packagedecl. The
combination of the kind of node representing the declaration and the values of
the as header and as unit kind attributes uniquely determine the exact form of
* the declaration. The different kinds of declarations are listed with their
appropriate attribute values in Table 4.1.
The HEADER and UNIT KIND nodes also record information peculiar to that
"sort of declaration. For example, the renames unit node not only indicates that
a declaration is a renaming declaration, but retains the name of the original
"unit as well.
A task declaration can introduce either a task type, or a single task
* object with an anonymous type, depending on whether or not the declaration
contains the reserved word "type". The syntax of a task declaration differs
* from that of an ordinary type or object declaration, hence the type decl and
variable decl nodes are not suitable for representing a task decTaration.
• - Because the same information is given in the task declaration regardless of the
kind of entity it introduces, a task decl node represents both kinds of task
declarations. If the defining occurrence associated with the declaration is a
variable id then the declaration creates both an object and a type; if the
defining occurrence is a typeid then the declaration creates a type.
Since a task declaration always defines a new task type, a new task type
specification (a task node) is created for each declaration. If the type is
anonymous it is introduced by the sm obj.type attribute of the variable id:
otherwise the task node is introduced by the sm type spec attribute of the
type id.
It should be noted that a task object may also be declared with an ordinary
object declaration. Since declarative nodes record the syntax cf the
declaration, a variable decl node rather than a task decl node de-.ztes the
declaration in this Ease. This kind of declaration-does not introduce a new
task type, thus a new task type specification is not created for the task
object(s).
RATIONALE
declaration, at which point the context indicates whether or not the declaration
and its defining occurrence(s) are generic.
4.2.5 IMPLICIT DECLARATIONS
The Ada programming language defines different kinds of implicit
declarations. Certain operations are implicitly declared after a type
definition (including derived subprograms following a derived type definition).
Labels, loop names, and block names are implicitly declared at the end of the
corresponding declarative part. These declarations are not explicitly
represented in DIANA; to do so would interfere with source reconstruction.
Since a label, loop name, or block name can be associated with only one
statement, and the label or name precedes that statement in the source text, it
is natural for the defining occurrence of a label (a labelid) to be its
appearance in a labeled statement, and the defining occurrence of a bl-ck or
loop name (a blockloop_id) to be its appearance in a block or loop statement.
Implicitly declared operations and derived subprograms are associated with
a single type definition. Unfortunately, the names of these operations and
derived subprograms are not used in that type definition. As a result, there is
no appropriate structural attribute to introduce the defining occurrences of the
operations associated with a type. All appearances of the names of these
operations and derived subprograms are represented in the DIANA structure as
used occurrences. A defining occurrence still exists for each Such operation or
derived subprogram; however, it can only be referenced by semantic attributes.
Section 4.3
SIMPLE NAMES
4.3 SIMPLE NAMES
Simple names comprise identifiers, character literals, and operator
symbols.
The attributes lx srcpos and lx comments are defined for all DIANA nodes
that represent source code. An implementation has the option of including these
attributes or not; however, if an implementation does choose to include them
then it is necessary to have a distinct node for every occurrence of a simple
name in tht source code. Since it is not desirable to have to copy all of the
semantic attributes associated with the name of an entity every time the name is
used, the appearances of simple names in a DIANA tree are divided into defining
and used occurrences. The former are represented by class DEF NAME and the
latter by class DESIGNATOR.
In order to avoid constraining an implementation, DIANA does not REQUIRE
that a distinct used occurrence node be created for every use of a simple name.
A single used occurrence node may be-created for a particular name, and all
references to that entity in the source code may be represented by references to
that single used occurrence node in the DIANA structure.
The defining nodes for entities together with their attributes play the
same role as a dictionary or symbol table in conventional compiler strategy.
Unless there is interference from separate compilation, it is possible for all
information about an entity to be specified by attributes on its defining node.
The node for a used occurrence of an entity always refers back to this defining
occurrence via the sm defn attribute.
Defining occurrences are represented by different kinds of nodes rather
than a single construct, thereby allowing the dppropriate semantic attributes to
be attached to each. For instance, the defining occurrence of a discriminant is
represented by a discriminant_id, which has an attribute to record the
applicable component clause (if there is one); the definii.g occurrence of a
constant is represented by a constant id, which has an attribute that references
the applicable address clause (if theFe is one).
DIANA also distinguishes the kinds of usage depending on the properties of
the entity that is referenced. For example, a used occurrence of an object name
is represented by a usedobject_id, while that of an operator is represented by
a used-op.
DIANA Reference Nanual Draft Revision 4 Page 4-14
RATIONALE
DIANA has the following set of defining occurrences.
attribute id
argument Td
block loop id
bltnoperator id
character id
camponent-id
constant Id
discrimi;ant id
entry id
enumeration Id
exception ia
function Id
generic id
iteration id
in id
in-out id
laBei lid
lprivate typeid
number id
operat_r-id
out id
pacdage Id
pragmaTd
private typeId
procedure id
subtype iQ
taskbony_id
type_id -
variable id
and the following set of used occurrences:
used char
used-name id
used-object id
used op
4.3.1 DEFINING OCCURRENCES OF PREDEFINED ENTITIES
The consistency of this scheme requires the provision of a definition point
for predefined identifiers as well. Although these nodes will never be
introduced by a structural attribute because they do not have an explicit
declaration, they can be referenced by the sm defn attribute of a node
corresponding to a used occurrence.
Certain kinds of entities, such as exceptions, may be either predefined or
user-defined. Such an entity is represented by the same kind of node in either
case -- a node from class SOURCE NAME, which represents the defining occurrences
of all entities which can by declared by the user. If, however, a SOURCE NAME
node corresponds to a predefined entity then the lx jrcpos and lx comments
RATIONALE
attributes will be undefined since it does not correspond to source text.
Other entities can never be declared by the user; i.e. pragmas, pragma
argument identifiers, and attributes. These entities are represented by nodes
from class PREDEF NAME; the lx srcpos and lx comments attributes of nodes
belonging to thTs class are never defined. R FNAME also contains nodes
corresponding to defining occurrences of the predefined operators (these
operators cannot be declared by the user, although they may be overloaded). A
user-defined operator is represented by a node from class SOURCE NAME.
4.3.2 MULTIPLE DEFINING OCCURRENCES
In general, every entity has a single defining occurrence. In the
instances where multiple defining occurrences can occur, each defining
occurrence is represented by a DEFNAME node.
The entities which may have multiple defining occurrences are:
(a) deferred constants
(b) incomplete types
(c) non-generic (limited) private types
(d) discriminants of incomplete or (limited) private types
(e) non-generic formal parameters
(f) program units
With the exception of tasks and (limited) private types, the different
defining occurrences of one of these entities are represented by the same kind
of node. In addition, the different defining occurrences nave the same
attribute values (certain incomplete types and program units may have attributes
which cannot be set in the first defining occurrence due to interference b?
separate compilation: however, this is in exception rather than i rule). These
defining occurrences have an attribute, sm first, that refers to the node for
the first defining occurrence of the identifier, similar to the sm defn
attribute of used occurrences. The node that is the first defining occurrence
has an sm first attribute that references itself.
All used occurrences must reference the same defining occurrence, the one
that occurs first. Nevertheless, the attributes for all defining occurrences of
an entity must still be set with the appropriate values.
An entry declaration and its corresponding accept statement are not treated
as different definition points of the same entity. Thus the entry_id is the
unique defining occurrence; a used_name_id appears in an accept statement, the
sm defn attribute of which refers to the associated entry id. Hoc"', the
formal parts of the entry declaration and the accept statement- multiply define
the entry formal parameters.
I-.l lim.I i l lili H a l
RATIONALE
Any names appearing in a record representation clause or 4n enumeration
representation clause are considered used occurrences; this includes the names
of record types, record components, and enumeration literals.
4.3.2.1 MULTIPLE DEFINING OCCURRENCES OF TYPE NAMES
There are two forms of type declaration in which information about the type
is given at two different places: incomplete and private. In addition to the
multiple defining occurrences of the type names there are multiple defining
occurrences of the discriminant names if the types include discriminant parts.
The notion of an Incomplete type permits the definition of mutually
dependent types. Only the new name is introduced at the point of the incomplete
type declaration -- the structure of the type is given in a second type
declaration which generally must appear in the same declarative part. The
defining occurrences of both types are described by type Id nodes which have the
semantic attribute sm type spec. With one exception (which is discussed in the
following paragraph) the full type declaration must occur in the same
declarative part, hence the sm type spec attribute of both defining occurrences
can denote the full type specification.
A special case may be introduced when an incomplete type declaration occurs
within the private part of a package specification. The full type declaration
is not required to appear in the same declarative part; it may be given in the
declarative part of the package body, which is not necessarily in the same
compilation unit. Since forward references are not allowed in DIANA, if the
full type declaration is in a separate compilation unit then the sm type spec
attribute of the type id corresponding to the incomplete type declaration
denotes a special incomplete node (which is discussed in detail in the section
on types). The sm type spec attribute of the node for the full type declaration
references the full type specification.
Private types are used to hide information from the user of a package; a
private type declaration appears in the visible part of a package without any
structural information. The full declaration is given in the private part of
the package specification (this restriction ensures that there is no
interference from separate compilation). Unfortunately, the solution used for
incomplete types cannot be aoplied to private types -- if both der'n'o
occurrences had the same node type and attributes, it could not be determined
whether the type is a private one or not. This information is important when
the type is used outside of the package.
DIANA views the declarations as though they were declarations of different
entities -- one is a private type and the other a normal one. Both denote the
same type structure in their sm type spec attribute, however. The distinction
is achieved by introducing a new kind of a defining occurrence, namely the
private type Id. It has the attribute sm type spec which provides access to the
structural Tnformation given in the full type declaration. Limited private
types are treated in the same manner, except that their defining occurrence is a
1 private type id. In the case of (limited) private types the sm first
attribute- of the type id node refers to the private type id or
1_private type_id. The privatetypeid and 1_privatetypeId nodes do ;ot have
RATIONALE
an sm first attribute because they always represent the first defining
occurrence of the type name.
4.3.2.2 MULTIPLE DEFINING OCCURRENCES OF TASK NAMES
The only other entity to have its different defining occurrences
represented by different kinds of nodes is the task. Although a task is a
program unit, its defining occurrences cannot be treated like those of other
program units. The declaration of a task introduces either a task type or a
task object having an anonymous type, hence the first defining occurrence of a
task name is represented by a type_io or a variable Id. Any subsequent defining
occurrences of the task name must correspond to either a stub declaration or a
proper body declaration; these defining occurrences are represented by
taskbodyid nodes.
All of the information ceicerning the task is stored in the type
specification of the task. Even though used occurrences of the task name do not
reference the type specification (they denote the type_id or variable id), the
type specification may be reached from any of the defining occurrences.
4.3.3 USED OCCURRENCES
The nodes representing used occurrences are included in the class
representing expressions because certain names may appear in expressions.
Restrictions have been added to the semantic specification to differentiate the
used occurrences which can appear in expressions from those which cannot.
The nodes usedobject id and used char represent used occurrences of
entities having a value and a type; these nodes can appear in the context of an
expression. The former denotes objects and enumeration literals, the latter
denotes character literals (in this way identifiers consisting of a single
character are distinguished from character literals). To allow the nodes
representing expressions to be treated in a consistent manner, the attributes
sm value and sm exp type were added to the used objectid and used-char nodes.
The remainina kinds of used occurrences are represented by the used op and
used_name_id nodes. The occurrence of an operator is represented by a usedop,
and that of any other entity by a used_name_id.
The names of objects and literals may appear in contexts other than
expressions; in particular, in places where the Ada syntax requires a name.
Should those used occurrences be represented by usedobject id nodes or
used name Id nodes? In some instances it might be useful to have ready access
to more information than just the name (for example, the subtype of the objec:
denoted by the name might be helpful). Some names (such as the "object-name" in
a renaming declaration) must be evaluated just as an expression is evaluated.
On the other hand, a name appearing in the left-hand part of a named association
is not evaluated, and since the association is not designed for semantic
processing (a normalized list of expressions is created for that purpose), it
would be wasteful to record additional semantic attributes.
RATIONALE
It was decided that the name of an object or literal appearing in the I
left-hand part of a named association should be represented by a used name Id
because the attributes peculiar to a used object Id would not be needed Tor
semantic processing in that context. -Since the situation is not as clear in
other contexts, all other uses of the name of an object or literal are
represented by usedobject_id nodes.
I
I
I
I
I
I
1
RATIONALE
(b) anonymous base types created by constrained array definitions [ARM,
3.6]
(c) anonymous index subtypes created by constrained array definitions [ARM,
3.61
(d) anonymous task types introduced by task declarations creating single
task objects [ARM, 9.1]
The declarations of these anonymous types are not represented (to do so would
interfere with source reconstruction), hence their type specifications are
introduced by the appropriate semantic attributes. For instance, the node for
an anonymous task type is introduced by the sm obj type attribute of a
"variable id node. At some point it will be necessary to know that such types
are anonymous (i.e. that they have not yet been elaborated), consequently the
sm is anonymous attribute was added to all nodes except for those representing
universal types (which are always anonymous).
In order to maintain the consistency of this type representation scheme it
was necessary to include some anonymous types and subtypes which are not
discussed in the reference manual.
Type definitions containing subtype indications with explicit constraints
introduce anonymous subtypes. Hence the component subtype of an array or the
designated subtype of an access type may be anonymous. If the constraints on
the parent type and the parent subtype of a derived type are not the same then
the new base type is anonymous.
Every object and expression in DIANA has an attribute denoting its subtype
(sm obj type for objects and sm exp type for expressions). The subtype
specification contains the applicable constraint (necessary for operations such
as constraint checking) as well as a path to the base type (which is required
for processing such as type resolution). If a new constraint is imposed by an
object declaration or an expression then an anonymous type specification must be
created in order to record the new constraint. Object or component declarations
containing either a constrained array definition or a subtype indication with an
explicit constraint introduce anonymous subtypes for each entity in the
identifier list. Slices. aggregates, and string literals introduce anonymous
subtypes if it cannot by determined statically that the constraintý on the
expression and those on the array prefix or context subtype a'e the same.
Unlike class TYPE DEF, which is subdivided according to syntactic
similarities, class TYPE SPEC is decomposed into subclasses by the semantic
characteristics (i.e. attFibutes) various members have in common. When placing
the nodes in the hierarchy, certain compromises were made that cause a few nodes
to inherit an attribute that is not really needed. For instance, the
constrained array and constrained record nodes inherit the attribute sm derived,
even though they can never represent a derived type (they may, however,
represent a subtype of a derived type). It was deemed better to have an
occasional unneeded attribute than to cause confusion by defining common
attributes in several different places (i.e. moving the constrained array and
constrained record nodes outside of class DERIVABLE SPEC and duplicating the
attributes sm is anonymous, sm base type, and sm depends on dscrmt for them).
I-mm ••, • _m
RATIONALE
4.4.1 CONSTRAINED AND UNCONSTRAINED TYPES AND SUBTYPES
A TYPE SPEC node provides no indication as to whether the entity it
represents is a type or a subtype. In the Ada language, the name of a type
denotes not only the type, but the corresponding unconstrained subtype as well.
An attempt at differentiating types and subtypes would only cause confusion and
inconsistencies. A distinction is made, however, between base types and
subtypes of base types. The attribute sm base type denotes the base type, a
type specification where all representation and structural information can be
found. Obviously the sm base type attribute of a node corresponding to a base
type will contain a self-reference.
Certain nodes always represent base types; these are the task spec node,
and those in classes PRIVATE SPEC and UNCONSTRAINED COMPOSITE. The task spec
and PRIVATE SPEC nodes do not have an sm base type attribute at all. Ts a
result of their inclusion in class MON TASK the UNCONSTRAINED COMPOSITE nodes
have inherited this attribute; however, it always contains a selT-reference.
DIANA also distinguishes between constrained and unconstrained (sub)types
for the following classes of types: array, record, and access. The nodes in
class UNCONSTRAINED represent unconstrained types; those in class CONSTRAINED
represent constrained types.
This distinction proves to be very useful when performing certain semantic
checks involving array, record, or access types. For instance, the types in
these classes may have index or discriminant constraints imposed upon them;
however, an index or discriminant_constraint cannot be imposed on the type if it
is already constrained.
The fact that an object is of an unconstrained type rather than a
constrained type may also affect certain implementation decisions. For example,
in a complete assignment to a record object of an unconstrained type that has
default values for its discriminants, the constraints on the object may be
changed during execution. Hence an implementation may wish to handle objects of
an unconstrained record type in a manner that is different from the way in which
objects of a constrained type are treated.
All scalar types are constrained, and may be further constrained any numbe-
of times. Hence there is only one kind of node for each kind of scalar type,
and each SCALAR node has an sm range attribute which denotes the app icab÷
range constraint. The nodes for real types have an additional sm accuracy
attribute to record the value of the digits or delta expression. For some types
(such as the predefined types and enumeration types) the constraints are
implicit, therefore a range node which does not correspond to source code must
be created. The range node that i• constructed for an enumeration type will
denote the first enumeration literal as the lower bound and the last enumeration
literal as the upper bound. The range node for a predefined numeric type will
have for its bounds expressions (determined by the implementation) which do not
correspond to source code.
Constraints cannot be applied to a task type, therefore the question of
whether or not it is constrained is irrelevent.
Section 4.4
TYPES AND SUBTYPES
4.4 TYPES AND SUBTYPES
In the Ada language certain types and subtypes may be declared in more than
one way. For instance, the following sets of declarations produce equivalent
subtypes:
type CONSTRAINEDAR is array (INTEGER range 1 ............ 10) of BOOLEAN;
type INDEX is INTEGER range I ............ 10;
type UNCONSTRAINED AR is array (INDEX range <>) of BOOLEAN;
subtype CONSTRAINED_AR is UNCONSTRAINEDAR (INDEX);
the only difference being that the base type and index subtype corresponding to
the first declaration are anonymous. In order to reconstruct the source text it
is necessary that the syntax of the declarations be recorded; however, semantic
processing would be facilitated if the same representation were used for
CONSTRAINED AR regardless of which set of declarations produced it. In order to
satisfy both needs DIANA has two classes associated with types and subtypes.
The class TYPE DEF records syntax. The nodes belonging to this class are
not intended to be used for semantic processing, hence they have no semantic
attributes, and are never designated by any kind of attribute other tnan a
structural attribute. A TYPE DEF node may correspond to:
(a) a subtype indication
(b) the portion of a type declaration following the reserved word 'is'
(c) the type of an object as given in an object declaration
[ TYPE DEF issubdivided into classes according to syntactic similarities of
the source text which the nodes represent. The class structure of TYPE DEF has
no semantic meaning.
"TYPE DEF contains three nodes which are really intermediate nodes:
integer def, float def, and fixed def. The syntax of a numeric type definition
consists solely oT a range, fToating point, or fixed point constraint.
Unfortunately, the nodes representing these constraints are already members of
class CONSTRAINT -- to include them in TYPE DEF as well would have introduced
multiple class memberships. Instead, three new nodes were introduced into
I-
RATIONALE
TYPEDEF; each has a single structural attribute denoting the actual constraint.
Class TYPE SPEC is the complement of TYPE DEF; it represents the Ada
concept of types and subtypes. The nodes comprising TYPE SPEC are compact
representations of types and subtypes, suitable for semantic processing. It is
not necessary to traverse a lengthy chain of nodes in order to obtain all of the
pertinent information concerning the type/subtype, nor are special cases (i.e.
different structures) introduced by irrelevant syntactic differences. The nodes Icomprising class TYPE SPEC do not record source text; they contain semantic
attributes only, and are not accessible through structural attributes.
Because the TYPE SPEC nodes are not designed to record source code, but are
intended to represent the corncept of types and subtypes, there is not
necessarily a one-to-one correspondence between the types and subtypes declared
in the source and the TYPE SPEC nodes included in the DIANA tree. Ar
implementation must represent each of the universal types (which cannot be
explicitly declared in an Ada program), and additional nodes may be created to
represent various anonymous types (to be described later). Consequently, it is
not possibly to store the type specification information within the nodes
denoting defining occurrences of types and subtypes. Thus the type id and
subtype id nodes of class DEF NAME represent the NAMES of types and subtypes,
not the-types and subtypes themselves. Access to the corresponding type
specification is provided by means of the sm type spec attribute.
The construction of new TYPE SPEC nodes for a DIANA tree is governed by two
basic principles:
1. Each distinct type or subtype is represented by a distinct node from
class TYPE SPEC.
2. There are never two TYPE SPEC nodes for the same type or subtype
These principles ensure that the only action needed to determine whether or
not two entities have the same subtype or the same base type is the comparisor
of the associated TYPE SPEC nodes. If both denote the same node (not equivalent
nodes, but the SAME node) then the types are the same; if they reference
different nodes then the types are not the same.
Since a type definition always creates a new type, a new TYPESPEC IocC E.
created for every type definition. This is not necessarily true for subtype
declarations. If a subtype declaration does not include a constraint then it
does not introduce a new subtype (it in effect renames the subtype denoted by
the type mark), therefore a new TYPE SPEC node is not introduced. In this case
the sm type spec attribute of the subtype id denotes the TYPE SPEC node
associated with the type mark.
All anonymous types described in the Ada Reference Manual are represented
in DIANA; i.e.
(a) anonymous derived types created by numeric type definitions [ARM,
sections 3.5.4, 3.5.7, and 3.5.91
RATIONALE
The nodes representing constrained types have an additional attribute,
sm depends on dscrmt, which indicates whether or not the component subtype
depends on a discriminant. A subtype of a record component depends on a
discriminant if it has a constraint which contains a reference to a discriminant
of the enclosing record type. Within a record type definition, the only forms
of constraints which can contain a reference to a discriminant are index and
discriminant constraints. Since the only nodes for which this attribute could
ever be true are the constrained array, constrained record, and
constrained access nodes, it was not necessary to define an sm depends on dscrmt
attribute Tor any other TYPE SPEC nodes (although a component subtype may be a
private type with a discrimina_t constraint, such a subtype is represented by a
constrained record node rather than a PRIVATE SPEC node, as discussed in section
4.4.4).
rq
The sm depends on dscrmt attribute was defined because otherwise it would
not be easy to determine whether or not a component subtype depended on a
discriminant if the constraint were sufficiently complicated. This information
is essential because at certain times a component subtype that depends on a
discriminant is treated differently from one that does not. For instance, the
elaboration time of a component subtype is determined by whether or not it
depends on a discriminant. If the component subtype does not depend on a
discriminant then it is elaborated when the enclosing record type is elaborated:
"otherwise the component subtype is not fully elaborated until a discriminant
constraint is imposed on the enclosing record type (the expressions in the
component subtype indication which are not discriminants are evaluated during
the elaboration of the enclosing record type).
4.4.2 UNIVERSAL TYPES
The Ada programming language defines three universal types -- universal
integer, universal real, and universal fixed. The TYPE SPEC nodes for tne
universal types have no attributes of their own since their properties are fixed
-- they are not implementation dependent, nor can they be declared by a user.
For example, there is no need for the sm is anonymous attribute because
universal types are always anonymous. The universal types are used as the types
of named numbers and certain static expressions.
4.4.3 DERIVED TYPES
All types other than the universal types may be derived, although
restrictions may be placed on the location of certain kinds of derived type
definitions. For instance, a derived type definition involving a private type
is not allowed within the package specification declaring that private type
[ARM, 7.4.11; however, that private type may be derived outside of the packale
specification. Hence the attribute sm derived is defined for class
DERIVABLE SPEC. If a type is derived then sm derived references the TYPE SPEC
node of the parent type (not the parent subtype); otherwise the attribute is
void.
RATIONALE
A derived type definition creates a new base type whose properties are
derived from the parent type. In addition, it defines a subtype of the derived
type. A derived type definition in DIANA always results in the creation of a
new TYPE SPEC node for the new base type. Since its characteristics are derived
from the-parent type it will need the same kinds of attributes in order to Irepresent the appropriate values, thus the base type is represented by the same
kind of node as the parent type. IIf the parent type and the parent subtype of a derived type do not have the
same constraints, then a new TYPE SPEC node is created for the subtype of the
derived type. This node will record-the new constraint, and its base type will
be the newly created base type. The name of the derived type will denote this Isubtype, hence all references to the derived type will denote the type
specification of the subtype. As a result, the base type is anonymous.
If the base type is a record or enumeration type then a representation
clause may be given for the derived type if a representation clause was not
given for the parent type BEFORE the derived type definition. Hence it is
possible for the derived type to have a different representation from that of
the parent type. The information given in an enumeration representation clause
is recorded in the nodes for the literals of the enumeration type; the
information from the component clauses is encoded in the nodes for the
components (including discriminants) of the record type. Due to the possibility Iof different representations, it is not always feasible for the derived type to
share the enumeration literals or record components of the parent type.
DIANA requires that copies be made of the defining occurrences of the
enumeration literals, unless the parent type is a generic formal discrete type,
which does not have any literals. The new literals reference the derived type
as the type to which they belong, but have the same position number as the
corresponding original literals. If a representation clause is not given for
the derived type then the sm rep attribute will also have the same value. The
node for the derived type denotes these new defining occurrences as its Iliterals. Duplication has an additional advantage for enumeration liierals --
since the literal of the derived type overloads the corresponding literal of the
parent type, it is convenient to have two different defining occurrences when
processing used occurrences of the literals.
DIANA also requires the duolication of the discrimina-t oart and ""
comoonent list for a derived record type if a reoresentation clause is qiven ;or
that type. If an implementation determines that no Such clause is given, it can
choose whether to copy the defining occurrences or reference the structure of
the parent type. Because the defining occurrences do not reference the record
type to which they belong, no inconsistencies are introduced if the structure is
not copied when the representation does not change.
4.4.4 PRIVATE, LIMITED PRIVATE, AND LIMITED TYPES
A private type declaration separates the properties of the type that may be
used outside of the package from those which are hidden from the user. A
private type has two points of declaration -- the first declaration is the
private one, occurring in the visible part of the package specification; the
RATIONALE
second is a full type declaration that appears in the private part of the
package. Private and limited private types are represented by nodes from
PRIVATE SPEC, and complete type specifications by those belonging to
FULL TYPE SPEC.
A (limited) private declaration introduces a private or 1.private node, and
the subsequent full type declaration introduces the appropriate node from class
FULL TYPE SPEC. The (limited) private specification rather than the full type
specTficaTion is referenced as the type of an object, expression, etc. In
addition, all used occurrences of the type name will denote the typeid of the
private declaration. The PRIVATE SPEC nodes have an sm type spec attribute that
provides access to the full type ýpecification. In this way the distinction
between private and full types is preserved for the kinds of semantic processing
which require knowledge of whether or not a type is private, but the information
recorded in the full type specification is available for the processing which
needs it.
The specification of a (limited) private type may be viewed as being
distributed over two nodes, rather than being represented by two different
nodes. The full type specification can never be referenced as the type of an
object, expression, etc., hence the principle that there are never two TYPE SPEC
nodes for the same type or subtype is not violated by this representation of
(limited) private types.
An alternate solution was considered. It was proposed that all references
to the (limited) private type occurring either outside of the package or within
the visible part of the package denote the PRIVATE SPEC node, and those
references occurring within either the private parT of the package or the
package body denote the FULL TYPE SPEC node. Although there would be two
TYPE SPEC nodes for one type, within a given area (the two areas-being the one
in which the type structure is hidden and the one in which the type structure is
visible) it would appear as if there were only one node. With this approach the
uses of the type would reflect whether or not the structure of the type was
visible at that point in the source code. Unfortunately, upon closer
examination the previous assumptions proved to be untrue.
Consider the case of a subprogram declared in the visible part of a
package. Suppose the subprogram has a parameter of a private type that is
declared in the visible part of the same package. Although it is possible for
the parameter in the suborogram declaration to denote the private spe-ificati'r
as its type, and the parameter in the subprogram body declaration to denote the
full type specification as its type, ALL references to both the subprogram and
its parameters denote the first defining occurrences -- those in the package
specification, which reference the private specification. Suppose an object of
the private type were declared inside the subprogram body; it would refer to the
full type specification as its type. The subprogram body would then contain a
mixture of references to the private type -- some to the full type
specification, others to the private specification. It would no longer be
possible to simply compare TYPE SPEC nodes to determine it two entities have the
same type. As a consequence, this solution was rejected.
The private and 1 private nodes always represent base types. Although a
subtype of a (limited)-private type may be introduced, it will be represented by
a node from class FULLTYPESPEC rather than one from PRIVATE SPEC. Due to the
RATIONALE
restrictions placed on the creation of new TYPE SPEC nodes, a new node may be
created for such a subtype only-if a new constraint is imposed upon it (in other
words, the subtype is not a renaming of another type or subtype).
The kinds of constraints which may be imposed upon a (limited) private type
are restricted in those regions where the full structure of the type is hidden.
The structure (and therefore the class) of a private or limited private type
without discriminants is not visible outside of the package or in the visible
part of the package, therefore no new constraints may be imposed on such types
in these regions. If a private type has discriminants then its full type must
be a record type, and a discriminant_constraint is permitted even in the
locations where the structure of the rest of the record is unknown. That
subtype is represented by a constrained record node. If the declaration occurs
within the private part of the package or the package body then the structure of
the private type is visible, and the subtype is represented by the appropriate
node from class FULLTYPESPEC.
The I private node represents types which are limited private, not types
which are-limited. Types which are limited include task types, composite types
having a subcomponent which is limited, and types derived from a limited type.
Because these types are not explicitly declared to be limited, they are not
represented by a distinct node kind as the limited private types are (to do so
would require semantic analysis to determine when the distinct node kind was (appropriate). Instead, an additional attribute is defined where necessary.
Task types are always limited, hence there is no need to record that
information in the form of an additional attribute. This is not true for arrays
and records. Determining whether or not an array or record has any limited
subcomponents could be a very time-consuming process if the structure of the
composite type is very complicated. As a consequence, the sm is limited
attribute was defined for the class UNCONSTRAINED COMPOSITE. It has a boolean
value indicating whether or not the type is limited. Since derived tyoes are
represented by the same kind of nodes as their parent types, the fact that a
derived type is limited can be recorded in same way that it was recorded for the
parent type.
On the surface it may seem that a problem similar to that discussed for
composite types having limited components exists for composite types having
private components. A composite type that has private subcomoonents and 4s
declared outside of the package containing the private type definition his
certain restrictions placed on the operations allowed for the composite type.
The only operations permitted are those which are dependent on the
characteristics of the private declaration alone (see section 7.4.2 of the Ada
Reference Manual).
A closer examination reveals that at most it is necessary to check a
component type (as opposed to component types and subcomponent types) to
determine if an operation is legal or not. The operations allowed for types
which are composites of composites are also allowed for composite types with
private components (assignment, aggregates, catenation, etc.). Operations
involving the private component rather than the composite type as a whole may be
restricted; for instance, a selected component involving a component of the
private component is not allowed. Since the type of the private comn;ýnent is
determined during type resolution of the sub-expression, no lengthy searches are
mm I
"RATIONALE
required to determine that the component is private. Certain operations that
are e'lowed for arrays of non-composite objects, such as the relational
operators for arrays of scalar components and the logical operators for arrays
of boolean components, would not be allowed under the circumstances described
above because it would not be possible to determine if the component type were
indeed a scalar type or a boolean type. However, such a check involves only a
single component type.
A need could not be found for an attribute indicating that an array or
record has private subcomponents; hence none was defined.
4.4.5 INCOMPLETE TYPES
An incomplete type definition allows the definition of "mutually dependent
and recursive access types" (ARM, 3.8.1]. Like a private type, it has two
"points of definition: one for the incomplete type, and a second for the full
type specification.
Although the uses of the name of an incomplete type are restricted when
they occur before the end of the subsequent full type declaration (the name may
appear only in the subtype indication of an access type definition), the
incomplete type becomes an ordinary full type once its structure has been given.
Since there is no need to distinguish the incomplete type from a full type once
the structure of the full type is known, the solution adopted for private types
is not appropriate for incomplete types. In general, incomplete types are not
represented as such in DIANA; their full type specifications are represented by
nodes from class FULL TYPE SPEC, and attributes denoting the incomplete type
actually reference the fulT type specification.
Only one sort of incomplete type is represented by a distinct node in the
DIANA tree. Included in the class TYPE SPEC is the node incomplete. which was
introduced to handle an anomoly in the Ada programming language. The language
places the following restrictions on the placement of the full type declaration:
"If the incomplete type declaration occurs immediately within either a
declarative part or the visible part of a package specification, then the full
type declaration must occur later and immediately within the same declarative
part or visible part. If the incomolete tyoe declaraton occurs immediately
within the private part of a package, then the full type declaration must occur
later and immediately within either the private part itself, or the declarative
part of the corresponding package body." [ARM, 3.8.11
Because a package body may be in a separate compilation unit, it is
possible for the full type specification of an incomplete type declared in the
private part of a package to be in a separate compilation unit. In this case it
is not possible for refe'-ences to the incomplete type which occur in the packa-e
specification to denote the full type specification; DIANA forbids forward
references of that sort. It was necessary to define a node to reoresent the
incomplete type in this special case, hence the incomplete node was introduced.
It has a single attribute, sm discriminant s, to represent any discriminants
belonging to the incomplete type. If the full type specification is not in a
different compilation unit the incomplete node is not used to represent the
RATIONALE
incomplete type.
This problem does not arise for private types. The Ada language requires
that the full type specification of a private type be given in the private part
of the package specification, thus it may never occur in a separate compilation
unit.
DIAPA does not specify the manner in which the full type specification may
be accessed from the incomplete type specification in this special case -- to do
SO wOLld impose restrictions on an implementation. All references tu the
specification of this incomplete type will reference the incomplete node, even
those occurring after the full type declaration. Since an implementation must
provide some solution to the problem of reaching the full type specification for
references to the incomplete type occurring within the package specification,
there seemed to be no reason to deviate from the DIANA requirement that all
references to a part :ular entity denote the same node.
It should be noted that it is possible to reach the incomplete type
specification from the subsequent full type declaration. The sm first attribute
of the type id introduced by the full type declaration denotes the typeid of
the incompTete type declaration. Both type id nodes have an sm type spec
attribute denoting their respective type specifications.
4.4.6 GENERIC FORMAL TYPES
Although "a generic formal type denotes the subtype supplied as the
corresponding actual parameter in a generic instantiation" [ARM, 12.1.2], a
geeric formal type is viewed as being unique within the generic unit. Hence a
new TYPE SPEC node is introduced by each generic type declaration, and the
attributes of the node are set as if the generic type were a new type.
A generic formal type is represented by the DERIVABLE SPEC node that is
appropriate for its kind. The values of the attributes are set in a manner
which reflects the properties of the generic type within the generic unit. For
instance, sm base type contains a self-reference, sm derived is void, and
sm is anonymous is false, regardless of the whether or not any of the actua,
subtypes have the same attribute values. A representation specification cannot
be given for a generic formal type; this restriction is reflected in tne values
of all attributes which record representation information (sm size is void,
smis_packed is false, etc.).
Some of the attributes of a node representing a generic formal type may be
undefined because they require knowledge of the actual subtype. Since there may
be numerous instantiations it is not possible to set these attributes ir the
node representing the generic type. For example, a generic formal discrete type
is represented by an enumeration node; the attributes sm literal s and sT -r__e
are not defined because they depend on the actual subtype. The informat'on
recorded by such attributes is not necessary for the semantic processing of the
generic type within the generic unit.
RATIONALE
The TYPE SPEC nodes corresponding to generic formal types cotitain no
indication that they are indeed generic formal types. This information can be
deduced from the context of the declarations and recorded by an implementation
in whatever manner it finds to be muSt convenient.
4.4.7 REPRESENTATION INFORMATION
Representation specifications can be given for certain types and first
* named subtypes through pragmas and representation clauses. Although occurrences
of these pragmas and representation clauses remain in the DIANA tree to enable
the source to be reconstructed, they are additionally recorded with the
"TYPE_SPEC nodes corresponding to the type structures that they affect.
The occurrences of the language pragmas PACK and CONTROLLED are recorded
with the attributes sm is packed (for array and record types) and
sm is controlled (for access types).
A representation clause may be given for a record type, giving storage
information for the record itself and/or its components; a reference to this
specification is recorded in the semantic attributes of the TYPE SPEC node
representing the record type as well as the defining occurrences of the
discriminants and components. Similarly for enumeration types, information from
representation specifications for the enumeration literals is recorded with the
defining occurrences of the enumeration literals.
Length clauses may be applied to various types. The presence of a length
clause specifying the storage size for a task or access type is recorded with
the sm storage size attribute. A length clause may also be used to place a
limit on the number of bits allocated for objects of a particular type or first
named subtype. A size specification is indicated by one of two different
attributes, depending on the kind of type a particular node represents. The
TYPE SPEC nodes representing non-scalar types have an sm size attribute which is
of type EXP; it references the actual expression given in the length clause, and
is void if no length clause is given.
TYPE SPEC nodes for scalar types have a cd impl size attribute, which is of
the private type value. Unlike the attributes corresponding to other kinds oF
representation clauses, cd impl size does not necessarily contain the value
given in a length clause. It was introduced to facilitate the evaluation of
static expressions. DIANA always records the value of static expressions;
however, the static values of certain Ada attributes are implementation
dependent. Since these attributes are related to static types, it is convenient
to store this information with the node representing the type.
One such attribute is SIZE, which returns the actual number of bits
required to store any object of that type. The value of this attribute is
recorded with the cd impl size attribute, which has a value even if no length
clause is given for the type. A length clause merely specifies the upper bound
for the size of objects of that type, hence it is possible for the value of
"cd impl size Lo be smaller than that given in a length clause. Because the Ada
programming language restricts static types to the scalar types, this
implementation dependent attribute is not necessary for the nodes representing
RATIONALE
non-scalar types.
The other implementation dependent attribute defined in DIANA is the
cd impl small attribute for nodes representing fixed point types. It contains
the value to be returned for the Ada attribute SMALL. The user may specify in a
length clause a value for "small" (the smallest positive model number for the
type or first named subtype); this value is used in representing values of that
fixed point type, and may affect storage requirements for objects of that type.
If no length clause is given, then cd impl small will contain the value for
"small" selected by the implementation; in this case "small" for the base type
may differ from "small" for subtypes of that base type [ARM, 3.5.9]. 1
i
I
I
I
I
I
I
I
I
I
i
I
I
I
Section 4.5
CONSTRAINTS
4.5 CONSTRAINTSLj
The Ada programming language defines five kinds of constraints: range,
floating point, fixed point, index, and discriminant. Because constraints are
generally imposed on types or subtypes, DIANA handles constraints in a manner
that is similar to the way in which types and subtypes are treated.
There are both syntactic and semantic representations of certain
constraints in DIANA. However, the differences between the two are not as
rigidly observed for constraints as for types and subtypes. This is due to an
effort to reduce the number of nodes in the DIANA tree, and to the fact that in
many cases the syntactic representation of a constraint is also suitable for
semantic processing.
As a result, there are not. two distinct classes for representing
constraints. In general, the class CONSTRAINT represents the syntax of the
various constraints; there is no class defined to represent a semantic version
of a constraint. Although certain TYPE SPEC nodes (which represent the semantic
concept of.a type or subtype) define an attribute to denote a constraint, if a
node from class CONSTRAINT is- appropriate then it is referenced rather than
requiring a new "semantic" structure to be built.
To facilitate the process of constraint checking, an effort was made to
represent the constraints in DIANA in as consistent a manner as possible. The
CONSTRAINT node is not always suitable for the following kinds of constraints:
discriminant, floating point, fixed point, and index.
A discriminant_constraint is a series of discriminant associations. The
sequence of associations may contain a mixture of EXP and assoc nodes (i.e.
expressions and named associations); if named associations are used then the
associations do not even have to appear in the order in which the discriminants
are declared. Thus an additional sequence, designated by the
sm normalized dscrmt s attribute of the constrained record node, is created for
a discriminant_constraint. This sequence is a normalized version of the
syntactic sequence -- all named associations are replaced by the associated
expressions, in the order in which the corresponding discriminants are declared.
In the interest of economy, if the discriminant_constraint appears in the source
text in the normalized form, then the record subtype specification may reference
the same sequence of expressions that the discriminant_constraint denotes.
A different problem arises for fixed or floating point constraints in
TYPE SPEC nodes. A type specification in DIANA records the applicable
constraint. Because a fixed or floating point constraint contains twc parts,
RATIONALE
either of which is optional in a subtype declaration, it is possible for the
accuracy definition and the range constraint to be given in two different
constraints. Thus it is not sufficient for a REAL node to reference a
REAL CONSTRAINT node. Instead, the accuracy definition and the range constraint
are Fecorded by separate attributes (sm accuracy and smran e) in the REAL node.
Though the type specification does not reference a REAL COSTRAINT node, it may
possibly reference one or both of the constituents of a REALCONSTRAINT node.
The final kind of constraint to have an additional semantic representation
is the index_constraint. DIANA adheres to the semantics of the Ada language in
its representation of arrays created by constrained array definitions. An index
constraint for a constrained array node introduced by a constrained array
definition is a sequence of disErete type specifications; if an index subtype is
given by a type mark then the type specification corresponding to the type mark
appears at that index position. Otherwise, an anonymous subtype is created for
that particular index position. To allow array subtypes to be treated in a
uniform manner, the same approach is taken for the index constraints of all
constrained array subtypes -- those introduced by subtype declarations, slices,
aggregates, etc. (some of these may be anonymous). The new sequence is denoted
by the sm index subLype s attribute of the constrainedarray node.
It may be necessary to make copies of some constraints. Tne Ada
programming language allows multiple object declarations, which are equivalent
to a series of single object declarations. If the multiple object declaration
contains a constrained array definition then the type of each object is unique;
if the declaration contains a subtype indication with a constraint, then the
subtype of each object is unique. In either case, a new TYPE SPEC node is Icreated for each object in the identifier list. If the constraint is non-static
then each type specification has a unique constraint. Because the const- nt
designated by the as type def attribute of the object declaration is not
designed to be used for semantic processing, that constraint may be "shared"
with one of the TYPE_SPEC nodes.
Due to structural similarities, the class RANGE represents both an Ada
range and an Ada range constraint. The difference can always be determined from
the context. If the RANGE node is introduced by an as constraint attribute, as
in the case of a numeric type definition or a subtype indication, then it
represents a range constraint. Otherwise, it is a simple range (i.e. it is
introduced by a loop iteration scheme, a membership operato-, an ent-,
declaration, a choice, or a slice). A RANGE node appearina DIPECTLY in a
sequence of DISCRETE_RANGE nodes (corresponding to an index_constraint) is also
a simple range.
In order to avoid a multiple class membership for the class RANGE, which
when representing a range constraint should belong to class CONSTRAINT, and when
denoting a simple range should be a member of class DISCRETE RANGE, the classes
CONSTRAINT and DISCRETE RANGE were merged. Consequently, CONSTRAINT is a
combination of the Ada syntactic categories "constraint" and "discrete range".
By including DISCRETE RANGE in class CONSTRAINT, the discrete subtype node was
introduced into the class representing constraints. It was therefore necessa'j
to add a restriction in the semantic specification of DIANA prohibiting an
attribute having the type CONSTRAINT from referencing a discrete-subtype node.
RATIONALE
Discrete subtype indications are represented by the node discrete subtype.
Although discrete subtype indications are syntactically identical to any other
kind of subtype indication, the subtype Indication node could not be included in
class DISCRETE RANGE because it is already a member of class TYPE DEF; to do so
would have introduced multiple membership for node subtype indication. Hence
the discrete-subtype node was introduced. It has an as subtype indication
attribute which denotes the actual subtype indication, thus discretesubtype
serves as an intermediate node.
When a range constraint, a floating point constraint, or a fixed point
constraint is imposed on a type or subtype, it is necessary to perform
constraint checks to insure that the constraint is compatible with the subtype
given by the type mark. Unfortunately, the information required to do this is
not incorporated in the corresponding type specification. Although a SCALAR
node does have an sm base type attribute, it does not necessarily denote the
type specification corresponding to the type mark in the subtype indication (a
scalar subtype is constructed from the BASE TYPE of the type mark, not the type
mark itself).
To make the type specification corresponding to the type mark accessible
from the type specification of the new subtype, the sm type spec attribute was
defined for the classes RANGE and REAL CONSTRAINT. Although a range constraint
may be part of a floating point or fixed point constraint, it was not sufficient
to add sm type spec to the RANGE node alone; the accuracy definition must be
available as well. The definition of this attribute for both classes results in
redundancy for the range constraints which are part of fixed or floating point
constraints. The sm type spec attributes of the REAL CONSTRAINT node and the
corresponding RANGE node (if there is one) always -denote the same type
specification.
If the constraints are associated with a subtype indication then
sm type spec denotes the type specification of the type mark; however, RANGE and
REAL CONSTRAINT nodes can appear in other contexts. For instance, both may
appear in type definitions. The expressions for the bounds of a range
constraint associated with a type definition are not required to belong to the
same type, therefore it is not feasible for sm type spec to reference a
previously defined type. In this case sm type spec designates the type
specification of the new base type.
RANGE nodes rearesenting (discrete) ranges rather than range constraints
can appear as a part of slices, entry family declarations, loop iteration
schemes, membership operators, index constraints, and choices. In each of these
cases the bounds must be of the same type, hence sm type spec denotes the
appropriate base type, as specified in the Ada Reference Manual. For example,
the Ada Reference Manual states that "for a membership test with a range, the
simple expression and the bounds of the range must be of the same scalar type"
[ARM, 4.5.2]; therefore sm typespec for a RANGE node associated with a
membership operator denotes the type specification of that scalar type.
A RANGE attribute is represented by a different kind of node from the other
Ada attributes. Unlike the others (except for BASE, which is another special
case), the RANGE attribute does not return an expression; thus the attributes
sm value and sm exp type (defined for the other kinds of Ada attributes) do not
apply. In addition, the RANGE attribute does not appear in the same contexts as
RATIONALE
other Ada attributes. Consequently it is represented by a special
range attr bute node.
Section 4.6
EXPRESSIONS
"4.6 EXPRESSIONS
Expressions in a DIANA structure are represented by nodes from class EXP.
EXP also contains the class NAME because certain names can appear in
expressions.
The nodes representing expressions record both the syntax and the semantics
of expressions; therefore nodes in this class contain both structural and
semantic attributes, and may be denoted by both structural and semantic
attributes.
There are two kinds of expressions which have a syntactic component that
may vary: the membership operator may contain either a type mark or a range,
and the allocator may contain either a qualified expression or a subtype
indication. Unfortunately, in each case the variants do not belong to the same
DIANA class, therefore a single attribute could not be defined to represent the
syntactic component. The DIANA solution was to define two variants for each of
these expressions, each variant having the appropriate structural attribute to
record the syntax of that particular variant. The membership operator is
represented by the range membership and typemembership nodes, and the allocator
is represented'by the qualified allocator and subtypeallocator nodes.
All DIANA nodes representing expressions have an sm exp type attribute; it
denotes the subtype of the expression. The subtype is referenced rather than
the base type because the type specification of the subtype contains both the
applicable constraint AND a direct path to the specification of the base type.
In this way, all nodes representing expressions contain the information
necessary for semantic checking, constraint checking, etc. It should be noted
that this does not imply that sm exo type can never denote a base type -- in the
Ada programming language a type is not only a type, but an unconstrained subtype
as well.'a
Some expressions can have static values (see section 4.9 of the Ada
Reference Manual). The sm value attribute was defined for nodes which can
represent static expressions to permit the static value to be obtained without
traversing any corresponding subtrees. If the value of an expression
represented by a node having an sm value attribute is not static, then sm value
must have a distinguished value indicating that it is not evaluated.
Due to syntactic similarities, various nodes in class EXP can represent
entities other than expressions. The selected node not only represents selected
components of records, it represents expanded names as well. The indexed node
represents an indexed component; however, it may also denote a member of an
DIANA Reference Manual Draft Revision 4 Pane 4-36
RATIONALE
entry family. These nodes have the attributes sm value and sm exp type because
they can represent expressions; however, since these attributes are meaningless
for anything other than an expression, they are undefined if the node does not
represent an expression (an expanded name denoting an object or a literal is
considered an expression).
4.6.1 EXPRESSIONS WHICH INTRODUCE ANONYMOUS SUBTYPES
Certain expressions (such as slices) impose a new constraint on a type. To
enable the subtypes of expressions to be treated in a consistent manner, DIANA
requires that anonymous subtypes be created for these expressions. The
expressions which may introduce anonymous subtypes are slices, aggregates,
string literals, and allocators.
Although a constraint for one of these expressions may be explicit, it is
not necessarily different from an existing constraint. In the interest of
efficiency, DIANA allows an existing type specification to be referenced as the
subtype of the expression if can be STATICALLY determined that the constraints
are identical; however, an implementation is free to create an anonymous subtype
for each such expression if it finds that approach to be more convenient. For
example, if it can be determined statically that a slice has the same bounds as
the array prefix, then the slice node is allowed (but not required) to denote
the type specification of the array prefix as its subtype. If it can be
determined statically that an aggregate or string literal has the same
constraints as the context type (which is required to be determinable from the
context alone) then the type specification of the context type may be referenced
as the subtype of the expression.
The anonymous subtype is constructed from the appropriate base type and the
new constraint. The base type for a slice is obtained from the array prefix,
and the constraint is the discrete range. The base type for an aggregate or a
string literal is taken from the context; determining the constraint for these
expressions is more complicated (the constraint is not necessarily explicit).
Sections 4.2 and 4.3 of the Ada Reference Manual discuss this procedure in
detail.
An allocator containing a subtype indication with an explicit constraint
introduces an anonymous subtype. This subtype is not necessarily that of the
object created by the allocator (for further details, see section 4.6.5); Ihowever, a new type specification is created for it because constraint checks
must be performed to ensure that the constraint is compatible with the type
mark.
Unlike the other expressions which create anonymous subtypes, the allocator
does not introduce the anonymous subtype via the sm exp type attribute. Though
an allocator creates a new object, it RETURNS an access value. The anonymous
subtype is not the subtype of the access value returned by the allocator, hence
it cannot be denoted by sm exp type. The sm desig type attribute was defined
for allocators containing subtype indications; it denotes the type specification
corresponding to the subtype indication (if an explicit constraint is not given
then sm desig type references the type specification of the type mark).
RATIONALE
"4.6.2 FUNCTION CALLS AND OPERATORS
The Ada programming language allows operators (both predefined and
user-defined) to be given in either prefix or infix form. In addition, a
function can be renamed as an operator, and an operator renamed as a function.
Consequently, the form of use of a function or operator implies nothing about
whether the function or operator is predefined or user-defined. Since it serves
no semantic purpose to distinguish function calls from operators, all function
calls and operators are represented as function calls in a DIANA structure.
There are two exceptions to this method of representation: the
short-circuit operators and the membership operators. These operators cannot be
Tepresented as functions, therefore they cannot be overloaded. Unlike the
parameters of a function call, all of which are evaluated before the call takes
place, the evaluation of the second relation of a short-circuit operator is
dependent upon the result of the evaluation of the first relation. The second
relation is not evaluated when the first relation of an "and then" operator is
"true" or when the first relation of an "or else" operator is "false". A
membership operator requires either a type mark or a range, neither of which is
an expression, hence neither can be represented as a parameter. These operators
are represented by the short circuit and MEMBERSHIP nodes rather than
function call nodes.
The name of the function (a used occurrence) provides access to the
defining occurrence of the function or operator, making it possible to determine
the kind of function or operator represented by the function call. The
lx prefix attribute records whether the call is given in prefix or-infix form;
this information is required for subprogram specification conformance rules (the
default values of a parameter of mode "in" migtht be a function call or
operator).
The subtype of a function call is considered to be the return type. If the
function call is a predefined operator then the return type is the appropriate
base type, as specified in section 4.5 of the Ada Reference Manual. This means
that the subtypes of certain function calls may be unconstrained; for example,
the result of a catenation is always of an unconstrained array subtype. Since
it is not always possible to determine statically the constraints on a value
returned by a function call, it is not feasible to require an anonymous subtype
to be created for a call to a function with an unconstrained return type.
4.6.3 IMPLICIT CONVERSIONS
The Ada programming language defines various kinds of implicit type
conversions, some of which are recorded in a DIANA structure, while others are
not.
An implicit conversion of an operand of a universal type to a numeric type
may be required for an operand that is a numeric literal, a named number, or an
attribute. Although this implicit conversion is not recorded by the
introduction of a distinct node, it is in a sense recorded by the value of the
sm exp type attribute. If the context requires an implicit conversion of an
operand of a universal type, then the sm exp type attribute of the
RATIONALE
numeric literal, used object id, or attribute node denotes the target type
rather than the univeFsal type.
By allowing the smexptype attribute to reflect the result of the implicit
conversion, all of the information necessary to perform the conversion is
recorded in the node representing the literal, named number, or attribute; no
additional context information is required. In addition, the fact that an
expression is the operand of an implicit conversion can now be determined easily
by a DIANA user. For instance, a numeric literal is the operand of an implicit
conversion if sm exp type does not denote a universal type. If sm exp type did
not reflect e conversion, then in any context in which an operand of a
universal type would not be appropriate it would be necessary to check for the
existence of a convertible universal operand. Since scalar operands can appear
in numerous contexts that require non-universal types, a substantial amount of
checking would be involved. The DIANA approach localizes the checking for an
implicit conversion to the nodes which may represent convertible universal
operands.
The semantics of the Ada language force the determination of the existence
of an implicit type conversion during the semantic checking phase (an implicit Iconversion is applied only if the innermost complete context requires the
conversion for a legal interpretation); recording information that is already
available should not impose a hardship on an implementation. The
numeric literal, used object_id, and attribute node all represent used Ioccurrences; hence no conflicts should arise as a result of this representation
of implicit conversions (i.e. the sm obj type attribute of the number id still
denotes a universal type).
As a result of the DIANA representation of implicit conversions, the used
occurrences of a named number cannot always be represented by a single node,
since the sm..exp type attribute of the usedobject_id may reflect an implicit
conversion. However, a single used occurrence having a particular target type
may represent all used occurrences of that named number requiring that
particular type.
If the variable to the right of the assignment operator in an assiqnment
statement is an array, then the expression to the right of the assignment
operator is implicitly converted to the subtype of the array variable. This
implicit subtype conversion may also be oeeformed on the 'nitial valje ; --
array variable declaration. Many kinds of expressions produce anonymous array
subtypes, which have a DIANA representation. Since this representation is
introduced by the sm e:pity_ e attribute of the corresponding expression, tý'e
solution adopted for scalar operands is not suitable for arrays. Due to the
fact that the implicit subtype conversion can occur only in two well-defined
contexts, it was decided that it was not necessary to record the need for an
implicit conversion.
Certain type conversions take place during a call to a derived subprogram.
For formal parameters of the parent type the following conversions are
performed: the actual parameters c'responding to parameters of mode "in" and
"in out" are converted to the parent type before the call takes place;
parameters of mode "in out" and "out" are converted to the derived type after
the call takes place. If the result of a derived function is of the parent type
then the result is converted to the derived type.
RATIONALE
The conversion of parameters described above cannot be represented in the
sequence of actual parameter associations corresponding to the source code
without interfering with source reconstruction; however, these conversions could
be incorporated into the normalized actual parameter list. It was decided not
to record these conversions because the need for such a conversion is easily
detected by comparing the base types of the formal and actual parameters. Since
an implementation is already required to compare the (sub)types of formal and
actual parameters to determine which constraint checks are needed, checking for
the need for implicit conversions should impose no hardship. Requiring these
conversions to be represented would force calls to derived subprograms to be
treated as special cases when constructing a DIANA structure. The conversion of
a return value of the parent type is not represented for the same reasons.
4.6.4 PARENTHESIZED EXPRESSIONS
Under some circumstances parentheses have a semantic effect in the Ada
programming language. Consider the following procedure call:
P ( (A) );
The parentheses around the actual parameter "A" make it an expression rather
than a variable, hence the corresponding formal parameter must be of mode "in",
or the program containing this statement is in error. In addition, certain
parentheses (such as those contained in default expressions for formal
parameters of mode "in") must be preserved in order to perform conformance
checks. Hence DIANA defines a parenthesized node. Not only does it contain a
reference to the expression that it encloses, it records the value (if the value
is static) and the subtype of that expression.
Certain kinds of processing are not affected by the presence or absence of
parentheses. To allow the parenthesized node to be easily discarded as the
DIANA is read in, a restriction was added to the semantic specification of
DIANA: a semantic attribute which denotes an expression can never reference a
parenthesized node; it must designate the node representing the actual
expression instead. This principle also applies to sequences which are created
expressly for semantic attributes and may contain expressions, such as the
various normalized sequences. As a consequence of this restricticn, a
parenthesized node can be referenced by only one attribute -- a structural one.
Since many of the semantic attributes were introduced as "shortcuts", it would
be inappropriate for them to denote a parenthesized node anyway.
4.6.5 ALLOCATORS
The subtype of an object created by an allocator is determined in one of
two ways, depending on the class of the object. The subtype of an array or a
discriminated object is determined by the qualified expression, subtype
indication, or default discriminant values. The subtype of any other kind of
object is "the subtype defined by the subtype indication of the access type
definition" [ARM, 4.8]; i.e. it is the subtype determined by the context (the
Ada language requires this type to be determinable from the context alone).
RATIONALE
As a result of these requirements, the sm exp type attribute of an
allocator creating an object that is not an array or a discriminated object
denotes the type specification of the context subtype. Unfortunately, the value
of sm exp type is not as easily determined in the other case -- an appropriate
subtype is not always available for the smexp type attribute of an allocator
creating an array or a discriminated object. Since an allocator can create an
object with a unique constraint, a collection that is compatible with that
object may not exist. Consider the following declarations:
type AC is access STRING(1..10);
FIVE POSITIVE :- 5;
OBJ : AC := new STRING(1..FIVE);
Although the initialization of OBJ will result in a constraint error, the
declaration of OBJ is legal, and hence must be represented in the DIANA
structure.
It may seem that it would be simple to make an anonymous subtype for this
sort of allocator, just as anonymous subtypes are created for other kinds of
expressions. But due to the way in which access types are constrained, the
construction of an anonymous subtype cannot always be performed as it would be
for other classes of types.
The anonymous subtypes for other expressions are constructed from the base
type of the context type and the new constraint. The base type of an array or
record type cannot have a constraint already imposed upon it (constrained array
type definitions create anonymous unconstrained base types, and the syntax of a
record type definition does not allow a constraint); therefore the imposition of
a constraint on the base type does not cause an inconsistency.
The base type of an access type is not always unconstrained, nor does the
Ada language define an anonymous unconstrained base type for a constrained
access type. Associated with an access base type is a collection containing the
objects which are referenced by access values of that type. If that base type
is constrained (i.e. the designated subtype is constrained), then all of the
objects in its collection must have the same constraints. It would be
inappropriate to introduce an anonymous base type having an unccrstraineA
designated subtype.
Unfortunately, this means that there is no existing type that would be an
appropriate base for the anonymous subtype of the allocator in the previous
example. The objects which may be referenced by OBJ and the object created by
the evaluation of the allocator do not belong to the same collection, therefore
they should not have the same base type. One solution would be to create an
anonymous BASE type for the allocator; however, it cannot always be determined
statically whether or not the object created by an allocator belongs to the
collection of the context type. For instance, if the variable FIVE had the
value 10 rather than 5, then it would be inconsistent to construct an anonymous
base type for the allocator, since the object it creates belongs to the
collection associated with AR.
RATIONALE
It was decided that in the case of an allocator creating an array or a
discriminated object the sm exp type attribute would denote the context subtype,
just as it does for other kinds of allocators. Within the context of the
allocator it can easily be determined what constraint checks need to be
performed by comparing the subtype of the qualified expression or the subtype
introduced by the allocator with the designated subtype of the context type.
4.6.6 AGGREGATES AND STRING LITERALS
The Ada programming language allows the component associations of an
aggregate to be given in two forms: named and positional. If named
associations are used then the associations do not necessarily appear in the
same order as the associated components. To simplify subsequent processing of
the aggregate, the aggregate node contains a normalized list of component
associations.
Since records have a static number of components (the expression for a
discriminant governing a variant part must be static in an aggregate), it is
possible for the component associations to be replaced by a sequence of
expressions in the order of the components to which they correspond.
Unfortunately, the associations of array aggregates are not necessarily
static. In addition, it is not always desirable to replace a static range by
the corresponding number of component expressions, particularly if the range is
large. Hence the normalized list of component associations for an array
aggregate does not necessarily consist of expressions alone (obviously all
positional associations will remain as expressions in the normalized sequence).
A single component association may contain several choices. Since the
component associations in the normalized sequence must he in the proper order,
and since the original choices do not necessarily correspond to components which
are contiguous (much less in the proper order), each component association
containing more than one choice is decomposed into two or more associations.
The normalized sequence does not correspond to source code, hence the only
requirements imposed on the decomposition process are that the resulting
associations be semantically equivalent to the original ones, and that each
association be either the component expression itself or a named associat_o-
having a single choice.
An "others" choice does not necessarily denote consecutive components,
therefore it is treated as if it were an association with multiple choices.
Each component or range of components represented by the "others" choice is
represented by a component expression or a named association in the normalized
sequence.
If a choice in an array aggregate is given by a simple expression, and it
can be determined statically that the expression belongs to the corresponding
index subtype then that association may be replaced by the component expression.
A subaggregate is syntactically identical to an aggregate, therefore it is
represented in a DIANA structure by the same kind of node. The only problem
arising from this representation is caused by the sm exp type attribute. A
RATIONALE
subaggregate is an aggregate corresponding to a sub-dimension of a
multidimensional array aggregate. An aggregate corresponding to an array
component or a record component is NOT a subaggregate. Since a subaggregate
corresponds to a dimension rather than a component, it does not have a subtype.
A subaggregate does, however, have bounds (although the bounds may be implicit,
as specified in section 4.3.2 of the Ada Reference Manual). In order to
correctly represent the subaggregate, the sm discrete range attribute was
defined for the aggregate node; it denotes the bounds of the subaggregate, and
is void for an aggregate that is not a subaggregate. The sm exp type attributeof a subaggregate is void.
A string literal is not syntactically like an aggregate, therefore it is Irepresented by a string literal node. However, a string literal may be a
subaggregate if it occurs Win a multidimensional aggregate at the place of a
one-dimensional array of a character type" (ARM, 4.3.2]. To accomodate this Icase, the stringliteral and aggregate nodes were placed in the class AGG EXP,
and the sm discrete range attribute was defined for both nodes.
As previously stated, an aggregate may have an anonymous subtype. In most Icases the constraints for the subtype are obtained from the aggregate itself
with no conflict as to which constraints to use. However, in the case of an
aggregate which contains more than one subaggregate for a particular dimension.
the choice is not clear. To add to the confusion, the bounds of the Isubaggregates for a particular dimension are not necessarily the same. Though
the Ada language requires a check to be made that all of the (n-1)-dimensional
subaggregates of an n-dimensional multidimensional array aggregate '--ve the same Ibounds, a program containing a violation of this condition is not in error;
instead, a constraint error is raised when the aggregate is evaluated durinQ
execution. I
DIANA does not specify which subaggregate the constraint for a particular
dimension is taken from. If all of the subaggregates have the same bounds tnen
it does not matter which is chosen. If the bounds are not the same then it
still does not matter, since the constraint error will be detected regardless of Iwhich bounds are selected for the anonymous subtype.
!
I
I
I
I
I
I
I
Section 4.7
PROGRAM UNITS
4.7 PROGRAM UNITS
Numerous kinds of declarations exist for package and subprograms --
renaming declarations, generic instantiations, etc. The information peculiar to
each kind of declaration must be accessible from the defining occurrence of that
entity. Rather than have a different kind of defining occurrence with different
attributes for each kind of declaration, DIANA has only one for a package and
"one for each kind of subprogram. Each such defining occurrence has an
sm unit desc attribute which denotes a UNIT DESC node that not only indicates
the form of declaration, but records pertinent information related to the entity
*. as well. The UNIT DESC nodes for special kinds of package and subprogram
declarations are dTscussed in detail in the following sections.
The defining occurrence of a package or subprogram that is introduced by an
ordinary declaration does not denote a UNIT DESC node defined exclusively for a
particular kind of declaration. Instead, it-denotes the body of the subprogram
or package, if it is in the same compilation unit. Although this information is
not vital for a defining occurrence that does not correspond to a body
declaration, this "shortcut" may be used for optimization purposes.
4.7.1 RENAMED UNITS
The Ada programming language allows renaming declarations for packages,
subprograms, and entries. These declarations introduce new names for the
original entities. In a few special cases an entity may even be renamed as
another kind of entity. A package or subprogram renaming declaration has the
same DIANA structure as an ordinary package or subprogram declaration; the fact
that it is a renaming is indicated by the as unit kind attribute, which denotes
a renames unit node.
If the entity is being renamed as the same kind of entity (i.e. a package
is being renamed as a package, a procedure as a procedure, etc.) then uses of
the new name will have the same syntactic structure as uses of the old name, and
can appear in the same kinds of context. For instance, a used occurrence of the
name of a function which is renamed as a function will appear as a function call
within the context of an expression. The function call must be given in prefix
form, just as a function call containing the old name must. A function id can
represent the new name without conveying any incorrect semantic informatTon, and
used occurrences of this name can refer to the function id without introducing
any inconsistencies in the DIANA tree.
I
RATIONALE
In such cases the new name is represented by the same kind of DEF NAME node
as the original entity, the sm unit kind attribute of which denotes a
renames unit node. 8ecause the defining occurrence represents a new name rather
than a new entity, the remainder of the semantic attributes, except for sm spec
for a subprogram name, have the same values as those of the original entity.
Since a new formal part is given in the renaming of a subprogram, the sm spec
attribute must denote the formal part corresponding to the new name. Access to
the defining occurrence of the original unit is provided through the as_name
attribute of the renames unit node.
Entities which are renamed as other kinds of entities present special
cases. Consider a function renamed as an operator. Although a used occurrence
of the new name will still appear as a function call within the context of an
expression, a function call using the new name may be given in either infix or
prefix form. If a function id were used to represent the new name rather than
an operator id then the information conveyed by the type of the defining
occurrence node wotld not be correct. Though the entity is the same function,
its new name must be viewed as the name of an operator. The same is true for an
attribute renamed as a function -- though a used occurrence returns the value of
the attribute, it will look like a function call, not an attribute.
An entry renamed as a procedure presents a different problem. The syntax
for procedure calls and entry calls is identical; however, from a semantic
perspective, call statements using the new name are procedure calls, not entry
calls. A call statement containing the new name cannot be used for the entry
call statement in a conditional or timed entry call, nor can it be the prefix
for a COUNT attribute.
With the exception of an enumeration literal renamed as a function, all
entities which are renamed as other kinds of entities are represented by the
DEF NAME node which is appropriate for the new name. Applicable attributes in
the defining occurrence for the new name have the same values as the
corresponding attributes in the original entity. For instance, the operator id
and function id nodes have the same attributes, so that all semantic attributes
except for sm unit kind and sm spec may be copied. On the other hand, none of
the semantic attributes in a function id are applicable for an Ada attribute,
hence they should have the appropriate values; i.e. sm is inline is false,
smaddress is void, etc.
The only entity which can be renamed as another kind of entity without
changing either the syntactic or the semantic properties associated witn tne use
of the name is an enumeration literal that is renamed as a functicn. A!'
enumeration literal and a parameterless function call have the same appearance,
and there are no semantic restrictions placed on the use of the new name. The
new name can be represented by an enumeration id, and used occurrences can be
denoted by used object id nodes which reference That enumeration id (rather thar I
a function id)- as the defining occurrence. The values oT the semantic
attributes of the new enumeration id are copies of those of the origiral
enumeration-id.
RATIONALE
4.7.2 GENERIC INSTANTIATIONS
The Ada language defines a set of rules for an instantiation, specifying
which entity is denoted by each kind of generic formal parameter within the
generic unit. For example, the name of a generic formal parameter of mode "in
out" actually denotes the variable given as the corresponding generic actual
parameter. An obvious implementation of generic instantiations would copy the
generic unit and substitute the generic actual parameters for all uses of the
generic formal parameters in the body of the unit; however, this substitution
cannot be done if the body of the generic unit is compiled separately. In
addition, a more sophisticated implementation may try to optimize instantiations
by sharing code between several instantiations. Therefore the body of a generic
unit is not copied in DIANA in order to avoid constraining an implementation and
to avoid introducing an inconsistency in the event of a separately compiled
body.
Generic formal parameters may appear in the specification portion of the
generic unit; for instance, a formal parameter of a generic subprogram may be
declared to be of a generic formal type. The specification portion of the
instantiated unit will necessarily be involved in certain kinds of semantic
processing whenever the instantiated unit or a part of its specification is
referenced. For example, when that instantiated subprogram is called it is
necessary to know the types of its parameters. Semantic processing would be
facilitated if the entities given in the specification could be treated in a
"normal" fashion; i.e. it is desirable that the appropriate semantic
information be obtainable without a search for the generic actual parameter
every time semantic information is needed. Because there may be numerous
instantiations of a particular generic unit, it is not possible to simply add an
additional attribute to the defining occurrences of the generic formal
parameters in order to denote the corresponding actual parameters.
DIANA provides a solution in two steps, the first of which is the additior
of a normalized list of the generic parameters, including entries for all
default parameters. Within this sequence (sm decl s of the instantiation node)
each parameter entry is represented by a declarative node which does not
correspond to source code. Each declarative node introduces a new defining
occurrence node; the name (lx symrep) corresponds to the formal parameter,
however, the values of the semantic attributes are determined by the actual
parameter as well as the kind of declarative nooe introducing the defining
occurrence.
After the normalized declaration list has been created the specification
part of the generic unit is copied. Every reference to a generic formal
parameter in the original generic specification is changed to reference the
corresponding newly created defining occurrence. Since each DEF NAME node
contains the appropriate semantic information, specifications of instantiated
units do not have to be treated as special cases.
A DEF NAME node introduced by one of these special declarative nodes is not
consicered to be an additional defining occurrence of the generic formal
parameter; should a defining occurrence that is introduced by such a declarative
node have an sm first attribute, it will reference itself, not the node for the
formal parameter.
RATIONALE
Since this list of declarative nodes is a normalized list, all of the
object declarations which appear in it are SINGLE declarations, even though the
generic formal parameter may have been declared originally in a multiple object
declaration. The kind of declarative node created for a generic formal
parameter is determined by the kind of parameter as well as by the entity
denoted by the parameter.
The name of a formal object of mode "in" denotes "a constant whose value is
a copy of the value of the associated generic actual parameter" [ARM, 12.3].
Thus a formal object of mode "in" is represented by a constant declaration in
the normalized parameter list. The initial value is either either the actual
parameter or the default value, and the subtype of the constant is that of the
actual parameter.
The name of a formal object of mode "in out" denotes "the variable named by
the associated actual parameter" [ARM, 12.3]. Hence a formal object of mode "in
out" appears in the normalized parameter list as a renaming declaration in which
the renamed object is the actual parameter. The values of the attributes of the
new variableId are determined just as they would be for an ordinary renaming. I
The declarative nodes for both constant and variable declarations have an
attribute for the type of the object being declared. Unfortunately, as type def
is normally used to record syntax, but because the declarative node does not Icorrespond to source code, there is no syntax to record. A possible solution
would be for as type def to reference the TYPE DEF structure belonging to the
declaration of the actual parameter; however, this structure is not always
appropriate. If the context of the declaration of the actual parameter and that I
of the instantiation is not the same, then an expanded name rather than a simple
name might be required in the TYPE DEF structure for the special declarative
node. Rather than force an implementation to construct a new TYPE DEF structure Iin order to adhere to the Ada visibility rules, DIANA allows the value of
as type def in an OBJECT DECL node generated by an instantiation to be
undefined. 'Jince these decTarative nodes are introduced to facilitate semantic
processing, not to record syntax, this solution should not cause any problems.
Declarative nodes for objects in the copy of the specification are treated in
the same manner.
The name of a formal type denotes "the subtype named by the associated
generic actual parameter (the actual subtype)" [ARM, 12.31. A gene-ic forma-
type is represented in the normalized list by a subtype declaration. The name
in the subtype indication corresponds to the generic actual parameter, and the Isubtype indication does not have a constraint, hence the declaration effectively
renames the actual subtype as the formal type. The sm type spec attribute of
the subtypeId references the TYPE_SPEC node associated with the actual
parameter. I
The name of a formal subprogram denotes "the subprogram, enumeration
literal, or entry named by the associated generic actual parameter (the actual
subprogram)" [ARM, 12.3]. A generic formal subprogram appears in the normalized Ilist as a renaming declaration in which the newly created subprogram renames
either the subprogram given in the association list or that chosen by the
analysis as the default. The values of the attributes of the new DEF NAME node
are determined just as they would be for an ordinary renaming, with the
exception of the HEADER node, which is discussed in one of the subsequent
I
I
• I
RATIONALE
paragraphs.
References to generic formal parameters are not the only kind of references
that are replaced in the copy of the generic specification. Substitutions must
also be made for references to the discriminants of a generic formal private
type, and for references to the formal parameters of a generic formal
I.- subprogram.
The name of a discriminant of a generic formal type denotes "the
corresponding discriminant (there must be one) of the actual type associated
I '~ with the generic formal type" [ARM, 12.31. If a formal type has discriminants,
references to them are changed to designate the corresponding discriminants of
the base type of the newly created subtype (i.e. the base type of the actual
type). Since the new subtype id references the type specification of the actual
subtype, any direct manipulatTon of the subtypeid will automatically access the
correct discriminants.
The name of a formal parameter of a generic formal subprogram denotes "the
corresponding formal parameter of the actual subprogram associated with the
formal subprogram" [ARM, 12.3]. If a formal subprogram has a formal part, the
declarative node and defining occurrence node for the newly created subprogram
reference the HEADER node of the the actual subprogram. Any references to a
I, formal parameter are changed in the copy of the generic unit specification to
denote the corresponding formal parameter of the actual subprogram.
Consider the following example:
procedure EXAMPLE is
OBJECT : INTEGER := 10;
function FUNC ( DUMMY : INTEGER ) return BOOLEAN is
begin
return TRUE;
end FUNC;
generic
FORMAL 09J : INTEGEP:
with function FORMAL FUNC( X : INTEGER ) return BOOLEAN;
package GENERICPACK is
PACKOBJECT : BOOLEAN := FORMALFUNC ( X => FORMALOBJ );
end GENERICPACK;
package body GENERICPACK is separate;
package NEWPACK is new GENERIC_PACK ( FORMALOBJ => OBJECT,

FORMALFUNC => FUNC );

begin
null;
end EXAMPLE;
RATIONALE
If a DIANA structure were created for package EXAMPLE, then the normalized
parameter list for package NEW PACK would contain two declarative nodes. The
first would be a constant declaration for a new FORMALOBJ, which would be
Initialized with the INTEGER value 10. The second would be a renaming
declaration for a new FORMAL FUNC; the original entity would be FUNC, and the
header of the new FORMAL FUNC would actually be that of FUNC. The specification
for Pdckage NEW PACK wouTd be a copy of that of GENERIC_PACK; however, the
references to FORMAL FUNC and FORMAL OBJ would be changed to references to the
newly declared entities, and the reference to X would be changed to a reference
to DUMMY.
4.7.3 TASKS
The definition of the Ada programming language specifies that "each task
depends on at least one master" (ARM, 9.4]. Two kinds of direct dependence are
described in the following excerpt (section 9.4) from the Ada Reference Manual:
(a) The task designated by a task object that is the object, or a
subcomponent of the object created by the evaluation of an allocator
depends on the master that elaborates the corresponding access type
definition.
(b) The task designated by any other task object depends on the master
whose execution creates the task object.
Because of the dynamic nature of the second kind of dependency, DIANA does
not attempt to record any information about the masters of such task objects.
The first kind of dependency, however, requires some sort of information about
the static nesting level of the corresponding access type definition; hence the
sm master attribute was added to the type specification of access types. Its
value is defined only for those access types which have designated types that
are task types. This attribute provides access to the construct that would be
the master of a task created by the evaluation of an allocator returning a value
of that particular access type.
A master may be one of the following:
(a) a task
(b) a currently executing block statement
(c) a currently executing subprogram
(d) a library package
A problem arose over the type of sm master -- there is no one class in
DIANA that includes all of these constructs. The class ALL DECL contains
declarative nodes for tasks, subprograms, and packages; therefore it seemed
appropriate to add a "dummy" node representing a block statem-et to this class.
The block master node, which contains a reference to the actual block statement,
RATIONALE
was added to ALL DECL at the highest possible level, so that it would not be
possible to have black master nodes appearing in declarative parts, etc. Only
one attribute (as alT decl) other than sm master has the class ALL DECL as its
type; restrictions on the value of this attribute were added to t_e semantic
specification.
4.7.4 USER_DEFINED OPERATORS
The Ada programming language allows the user to overload certain operators
,* by declaring a function with an operator symbol as the designator. Because
these user-declared operators have user-declared bodies, etc., they are
represented by a different kind of node from the predefined operators. The
predefined operators are represented by bltn operator id nodes, which do not
have the facility to record all of the information needed for user-defined
operators. A user-defined operator is represented by an operator id node; it
has the same set of attributes as the function id.
A special case arises for the inequality operator. The user is not allowed
to explicitly overload the inequality operator; however, by overloading the
equality operator, the user IMPLICITLY overloads the corresponding inequality
operator. The result returned by the overloaded inequality operator is the
complement of that returned by the overloaded equality operator.
Since the declaration of the overloaded inequality operator is implicit,
the declaration is not represented in the DIANA tree (to do so would interfere
with source reconstruction). At first glance it may seem that a simple
implementation of the implicitly declared inequality operator would be to
replace all uses of the operator by a combinatioh of the "not" operator 3nd the
equality operator (i.e. "X /= Y" would be replaced by "not ( X = Y )" ). While
this approach may be feasible for occurrences of the inequality operator within
expressions, it will not work for occurrences in other contexts. For instance,
this representation would not b2 appropriate for a renaming of the implicitly
declared inequality operator, or for an implicitly declared operator that is
used as a generic actual parameter.
In order for used occurrences (used_name_id nodes) to have a defining
occurrence to reference, the implic 4 tly declared inequality operato_r is
represented by an operatorid. Unfortunately, this operator does not have a
header or a body to be referenced by the attributes of the operator id; some
indication that this operator is a special case is needed. Thus the
implicit noteq node was defined. Instead of referencing a body, the
as unit desc attribute of an operator id corresponding to an implicitly declared
inequality operator denotes an imp icitnoteq node, which provides access to
the body of the corresponding equality operator. The as header attribute of the
operator Id designates either the header of the corresponding equality operator,
or a copy of it.
RATIONALE
4.7.5 DERIVED SUBPROGRAMS
A derived type definition introduces a derived subprogram for each
subprogram that is an operation of the parent type (i.e. each subprogram having
either a parameter or a result of the parent type) and is derivable. A
subprogram that is an operation of a parent type is derivable if both the parent
type and the subprogram itself are declared immediately within the visible part
of the same package (the subprogram must be explicitly declared, and becomes
derivable at the end of the visible part). If the parent type is also a derived
type, and it has derived subprograms, then those derived subprograms are also
derivable.
The derived subprogram has the same designator as the corresponding
derivable subprogram; however, it does not have the same parameter and result
type profile. It should be noted that it would be possible to perform semantic
checking without an explicit representation of the derived subprogram. All used
occurrences of the designator could reference the defining occurrence of the
corresponding derivable subprogram. When processing a subprogram call with that
designator, the parameter and result type profile of the derivable subprogram
could be checked. If the profile of the derivable subprogram was not
appropriate, and a derived type was involved, then a check could be made to see
if the subprogram was derivable for that particular type (i.e. that a derived
subprogram does exist).
Unfortunately, the circumstances under which a derived subprogram is
created are complex; it would be very difficult and inefficient to repeatedly
calculate whether or not a derived subprogram existed. Hence derived
subprograms are explicitly represented in DIANA. The appropriate defining
occurrence node is created, and the sm unit desc attribute denotes a
derived subprog node, thereby distinguishing the derived subprogram from other
kinds oT subprograms. Once the new specification has been created, the derived
subprogram can be treated as any other subprogram is treated; it is no longer a
special case.
The specification of the derived subprogram is a copy of that of the
derivable subprogram, with substitutions made to compensate for the type
changes. As outlined in section 3.4 of the Ada Reference Manual, all references
to the parent type are changed to references to the derived type, and any
expression of the parent type becomes the operand of a type conversion that has
the derived type as the target type. The specification of the derived
subprogram deviates from the specification described in the Ada Reference Manuad
in one respect. The manual states that "any subtype of the parent type is
likewise replaced by a subtype of the derived type with a similar constraint"
(ARM, 3.41. If this suggestion were followed, both an anonymous subtype and a
new constraint would have to be created. Fortunately, both the requirements for
semantic checking and the semantics of calls to the derived subprogram allow a
representation which does not require the construction of new nodes (or
subtypes).
All references to subtypes of the parent type are changed to references to
the derived type in the specification of the derived subprogram. Because
semantic checking requires only the base type, this representation provides all
of the information needed to perform the checks. A call to a derived subprogram
is equivalent to a call to the corresponding derivable subprogram, with
RATIONALE
appropriate conversions to the parent type for actual parameters and return
values of the derived type. Though the derived subprogram has its own
specification, it does not have its own body, thus the the type conversions
described in section 3.4 of the Ada Reference Manual are necessary. In addition
to performing the required type conversions to the parent type, an
implementation could easily perform conversions to subtypes of the parent type
when appropriate, thereby eliminating the need to create an anonymous subtype of
the derived type. The derived subprog node provides access to the defining
occurrence of the corresponding derivable subprogram (and hence to the types and
subtypes of its formal parameters).
Although the defining occurrence of a derived subprogram is represented in
DIANA, its declaration is not, even though the Ada Reference Manual states that
the implicit declaration of the derived subprogram follows the declarations of
the operations of the derived type (which follow the derived type declaration
itself). Consequently the defining occurrence of a derived subprogram can be
referenced by semantic attributes alone.
Section 4.8
PRAGMAS
4.8 PRAGMAS
The Ada programming language allows pragmas to occur in numerous places,
most of which may be in sequences (sequences of statements, declarations,
variants, etc.). To take advantage of this fact, several DIANA classes have
been expanded to allow pragmas -- in particular, those classes which are used as
sequence element types and which denote syntactic constructs marking places at
which a pragma may appear. For instance, the class STM ELEM contains the node
stm-pragma and the class STM. All constructs which are defined as sequences of
statements in the Ada syntax are represented in DIANA by a sequence containing
nodes of type STMNELEM.
The approach taken for the representation of comments could have been
applied to pragmas; i.e. adding an attribute by which pragmas could be attached
to each node denoting a construct that could be adjacent to a pragma. This
approach has two disadvantages: there is a need to decide if a pragma should be
associated with the construct preceding it or the one following it; and the
attribute is "wasted" when a pragma is not adjacent to the node (which will be
the most common case). Since the set of classes needing expansion is a small
subset of the DIANA classes, it was decided to allow the nodes representing
pragmas to appear directly in the associated sequences, exactly as given in the
source.
The pragma node could not be added directly to each class needing it
without introducing multiple membership for the pragma node. Since the DIANA
classes are arranged in a hierarchy (if one excludes class the node void) such a
situation would be highly undesirable. Instead, the pragma node is included in
class USE PRAGMA, which is contained in class DECL, and an intermediate node is
included in the other classes. This intermediate node has an as pragma
attribute denoting the actual pragma node. The stm pragma node menticnec at tne
beginning of this section is an intermediate node.
Sequences of.the following constructs may contain pragmas:
(a) declarations (decl s and Item-s)
(b) statements (stm-s)
(c) variants (variants)
(d) select alternatives (testclauseelem s)
RATIONALE
(e) case statement alternatives (alternatives)
(f) component clauses (comuprep s)
(g) context clauses (contextelem-s)
(h) use clauses (usepragnas)
Unfortunately pragmas do not ALWAYS appear in sequences. In a few cases it
was necessary to add an as pragma s attribute to nodes representing portions of
source code which can contain pragmas. These cases are discussed in the
following paragraphs.
The comp_list node (which corresponds to a component list in a record type
definition) has an as pragma s attribute to represent the pragmas occurring
between the variant part and the end of the record type definition (i.e.
between the "end case" and the "end record").
The labeled node, which represents a labeled statement, has an as pragma s
attribute to denote the pragmas appearing between the label or label7sand the
statement itself.
Pragmas may occur before an alignment clause in a record representation
clause (i.e. between the "use record" and the "at mod"), hence the
alignment clause node also has an as pragma s attribute. If a record
representation clause does not have an alignment clause then a pragma occurring
after the reserved words "use record" is represented by an intermediate
coumprep pragma node in the comp rep_ sequence (in this case the comp_rep_s
sequence will have to be constructed whether any component clauses exist or
not).
Finally, the compilation-unit node defines an as pragma s attribute which
denotes a non-empty sequence in one of two cases. A compilation may consist of
pragmas alone, in which case the as pragma s denotes the pragmas given for the
compilation, and the other attributes are empty sequences or void.
If the compilation contains a compilation unit then as pragmas re resents
the pragmas which follow the compilation unit and are not associated with the
following compilation unit (if there is a compilation unit following it at aii).
INLINE and INTERFACE pragmas occurring between compilation units must be
associated with the preceding compilation unit according to the rules of the Ada
programming language. LIST and PAGE pragmas may be associated with either unit
unless they precede or follow a pragma which forces an association (i.e. a LIST
pragma preceding an INLINE pragma must be associated with the previous
compilation unit, since pragmas in DIANA must appear in the order given, and the
INLINE pragma belongs with the previous unit). These four pragmas are the only
ones which may follow a compilation unit.
Certain pragmas may be applied to specific entities. Although the presence
of these pragmas must be recorded as they occur in the source (to enable the
source to be constructed), it would be convenient if the information that they
conveyed were readily available during semantic processing of the associated
entity. Hence DI4NA defines additional attributes to record pertinent DragrrA
RATIONALE
information in the nodes representing defining occurrences of certain entities
to which pragmas may be applied. The following pragmas have corresponding
semantic attributes:
(a) CONTROLLED
sm is controlled in the access node
(b) INLINE
sm is inline in the generic_id and SUBPROGNAME nodes
(c) INTERFACE
sm interface in the SUBPROG NAME nodes
(d) PACK
sm is packed in the UNCONSTRAINED_COMPOSITE nodes
(e) SHARED
sm is shared in the variable id node
Although it may seem that the pragmas OPTIMIZE, PRIORITY, and SUPPRESS
should also have associated attributes, they do not. Each of these pragmas
applies to the enclosing block or unit. The information conveyed by the
OPTIMIZE and PRIORITY pragmas could easily be incorporated into the DIANA as it
is read in. The SUPPRESS pragma is more complicated -- not only is a particular
constraint check specified, but the name of a particular entity may be given as
well. SUPPRESS is too dependent upon the constraint checking mechanism of an
implementation to be completely specified by DIANA; in fact, the omission of the
constraint checks is optional.
## CHAPTER 5
EXAMPLES
EXAMPLES
This chapter consists of exampies of OIANA structures. Each example
contains a segment of Ada source code and an illustration of the resulting DIANA
structure. Each node is represented by a box, with its type appearing in the
upper left-hand corner. Structural attributes are represented as labeled arcs
which connect the nodes. All other Kinds of attributes appear inside the node
itself; code and semantic attributes are represented by a name and a value,
while lexical attributes representing names or numbers appear as strings (inside
of quotes). All sequences are depicted as having a header node, even if the
sequence is empty. If the copying of a node is optional, it is NOT copied in
these examples.
These illustrations DO NOT imply that all DIANA representations of these
particular Ada code segments must consist of the same combination of nodes and
arcs. For instance, an implementation is not required to have a heade node for
a sequence. The format for these examples was selected because it seemed to be
the most straightforward and easy to understand.
In certain instances an arc may poinc to a short text sequence describing
the node that is referenced rather than pointing to the node itself. This is
done for any of the Following reasons:
o the node is pictured in an exampic on another page
o the node is not oictured in any of the examples
o the node represents a predefined cntity which cannot be depicted
because it is imolementation-deoendent
o the node is on the same pace, but pointing to it would cause arcs to
cross and result in a picture that would be difficult to understand
LIST OF EXAMPLES
1 - Enumeration ',/pe Definition
2 - Ineger Type Definition
3a - Subtype Declaration
3b - Multiple Object Declaration
3c - Multiple Object Declaration with Anonymous Subtype
4a - Private Type Declaration
4b - Full Record Type Declaration
4c - Declarations of Subtype of Private Type
5a - Generic Procedure Declaration
5b - Generic Instantiation
6a - Array Type Definition
6b - Object Declaration with Anonymous Array Subtype
6c - Assignment of an Array Aggregate
7-------- 7------
V , CV
*IL,
* Z0
411 11
II,• i ,l I *i
-, - , .1 r I : I I I I
Ii - i ,. IU i I l ,i l ] 1 I CIli
WI - I Cl 00 " -- ' ' i'N"-
* 0 I I 13 I II), l , IS I I, t~.
i 01 -- ,SI C_I hi* A I C l • I CI• AI --*i
- - l l-~, , -i o,Ii oo, I>,-
,.1 •0. 1' U,0. €3 -- .*
.~ ~~~~~~ E• E":EEEE ''
- i I .i C S L .l CW - - -
I I
• I I-- >*.r
-- E EIE E -- i
I 41- -
41 r
.. :_E E'E'_
1 IZ• l • - I.-, - i I I = I , I -- ?
S.l I I I ) I i I I * ^ ' I n IU, I Ii1- - - - - - - -I IS I .4 C
"I I I
o . I A
-S ' - C
I -Z
II II I C I II
Iis
H1 L. I. I-- C
ila
* L I ' I -MI .Il
e-*W_I ' 41 -
I I IIC WI II
II 1U I N W
LA 4 feIIH
* I.-i4 il
cc z
I ~ ~~~ >' s 7ICI-W- S If C I ;O "
CIII- -> I - -- 6Y z--
Imi
S • I * GI l
I I I I I IZ I
I I I,I"
I I I ^ ^ S I
In
I I I 1
WI I at
0 I I I
"IA l1 14 1I., > ca
4 I"= WI I I 'I ,4
I I l I I -a CDo -
I I ! i I • I1 l I I " 5I1 I
I i.U W I I l I I" I-, I I - I I6 I:-
I W I iI I 1-1 4 I.6,r. c (A
• 4 - WIW 6 I1 1 , if
III ,iI l I •I Illl- L IW) '0• .-- , I
41 1 I I I I I II 1-.
- 01 4 III W I IP I I IWejl I: I
lI I 1 6
* WI--I I -- -I^
CL E,I CI t I' E E -.
*i a I I
CI II.-II
jIl
AI
Vr -- ---
I 0I I A1 0I
I~~ _141 I -Q
I InI 6"S
- '- ,I 41 I.. SI 0. I
. 1 I• l iJI i
U I II p. II s I I
I, I Si,1 I•I--
' II0 I I.IZ
I U I*I I •I.0 . I I I
*I I O. I0 I I 4,
-- I -- f I--il', I I -
I 1411 -- I "51-- -I - -I,
*I ,01 I••€ 141 41 21 I I 41
:SAM;II 4 1 -I0
I S
• I I I
iP-P I
PUo )•
ol!I I l
,COL,me ..0 ............ .0'00
-i. I.I 1I t
* I., III -l
P-it I El *i
ia z
* P0 ll I ,- •
'II
0 to
-, 6, I.• ,
. ............ ............ ............ ............ ............ ............ ............ ............ ............ 0 ) i
0 - C
tN AA)
| | _inl11 •
I III I i-- 4141NlI
-- is- ii IIiI UI -- SC-•-
I ,,' '0 0 --S*.ll ilia I i.' -
41 1, e , ii,,
I J. 4
cc: - :'Z ! -
Il "6. . -I CL 2 ,
(5• 1 - (a -- I 1 - 6
SI IM 6I
6 2- - - > -6E Z
III a • ............ .. III -
3 ..1 E E IS E 'Z
1.-i X E. d-CI', U-• rE ) , ,,, -
- - - - - - - - -jul SI
I,-I, ,--- --
u'U '71
41, ' . - - '--
01
SIl I~ I ) .) -ii t I• ) ,
"isA Q 4,
SI 2' V,.... o ,-_ t
- -C 0 , ll -o-
I-- I I 'II o - o - I a ,o-
iI II" Nt
* - -.. ............ .- -I-
Io Al A
- I I ' II A -
- - - - - - - - - - - - -
I . IJ 41 S,.1I A ) 0 N .
0. 'LII 10' I ' C )- 0.
0 C. I 4-- 41 414
- I I * I 441 01
I 'i ' I 1
- I U '
1
I.-:
I_I i:. I
I I MI Z
l .2I
SI MI I 0
I I - I
I IZ, > S
21
- I - I: I I
* IS. I, I 0 .
I I ' CS
I I V
* 11.1 S
'21
IjICI
I - -. IjI I
I CI SI A
* I I I
* I I II
I >I 5151
I dl dl
I I I I
I I I I
II. S IIV I
- 'U,'-.- I
In, @0
.1 - I)) I
* I 0' 'I I C
0
I
) AA I -
- - II I I
I I I L
I I I '-I 21 I S
I I U' I U1 ............ >O I C' 05 I -
A SI I 211 A ) 'S 21.S I U
I_I - . !I . !' Il C - 'I S
I ' AI
- 1
I 43 1' I 1
I UI V III Iii ,.- C0_I S
3 101 I I I I I II I , 0U I 0.
IJ I II_E->IIAI Al I I Al 'SISEEUI -
I WI L. , I l I 21 I II '*- AAAWI
- I I U 1 1 21' I 0
Z el c-. . I -U- - A I - 0.------I
I . I AA I )
* 21
0 I - 411 - ,. I.
U I II A I UI IA I I 0. -
.1
- I I 43 'A I I q
I '1 11 41
0.
21 I I_I N
I I aJ.
- I--- I I'AI I
21
21 - '
- -,'
21 -' ON
I I. 11 I I I AOl I 5
I, U' I II ECU' .C
0 10:0 I I
O 10.10.1 I iVCEO.'
I I I A. I I 50-A'
* 11 II I )C I'
S I IS I.SUSI
510. 21I TA0.
-. >I 1
141
1)1 II 1)1 .1.11
IEEE'
1. ' '
0.-
IA-
"" I
i • wD: Q- I : II
it , I UJI > 0 Z
0) 0>,
to 0r
.1~ -, >-u
- - 'D I, I t fI
* III II I
--r I 1O .-
, ,.1A H A• I ••I
31 A, ,I C
------ 0.r----- I
. I -- . I I,
NII - - I
I AI
I A 0
IE '
to &r J C9. -;C
In'--; VI .-
17 -- A
- - - -- - - - - - - - -
6 ' I I )I 1 1 W
o I d ............ - 1 1I II ,6 A 0 -
l1~~ _rl 4 I I I1CL - a.- ,6 .-
Is II, UL- 1 C I•I l Iq I -- I, I.I [i,
* IS II 1•1 * S I 00 4
I 1I 0 Ifl l. G.--1 I -= -
I I •~ . :1•
I I -- II 1 1 1 L I-t 0
I I IL 41) L
1 6- I I 3 3 3 L_I
•I Al I 411. . . .
Cl I I 41•I
>> 0 II II 0 -
II, r1 Iij.I I=
1 I) I --
0• •i i~ Ii l • = Ii I I I l
'-' 1 I
^>
a
. .C 1ll
It 10.
A ' ' I0c.I IC
I ~ ~ I.I
>.UI 41 41 U,0) 4"
CL101 I AlL'I10
AC.~_1 10 c'IIS
o.1 1C 6i CL 6 1,
'a CL 9 Q I% to
I_I 110 V
- ---- - - - - -
i A
- - - - - - - - - - - - - - - - - - - - - - - - - - - -
:-- 7 ----------------------- ,"I CL
I - ,: g l I. I.I
II- -1 -,.- - - -I. I >P '>!
I ; 1 I I I IJ I I 1 0
SI 14 Si I I 1Z' z_0
"-l- I " . . . . . I I I I I I I' >4
-- +II I I, 1 I . , '' 2
- 11 I .I I/I •ll I IIIA' ' ' i ............ 0. ............ a I A.•+III>1 :, '
S~ ~ ~ 7II ILc'II I I - 0
,,S,' 1 0.1 I I I V I. I
RI a uI a A II 0 I.
6. ;01! C-
I_I - ° 1 1 ' '': I IL'
41 I IHIS 11 I I 1I 0- il I 1 0
"I1"I I 'Al i i n A
,i 0. I 10 0 A'I A
I 0 ............ l)- - - I ............ ............ l I
601, toL I-
I- JIL I .lI lei I ' IIr -1
-,. '0 ' 1.-. * I- z .CL I
C04 Cl.0.0 A A A
01' t UJI. I 0 I 1 I I 'l . -g_l -[l
I llI I '• a- A > I .1 0 11(--' ,0! 1
I I, 0' 41 lII SI 0 .-- I ' U
- I'0"P .' 0A I II1) III V i.I I'
1' 1 6 ' . 1 I I - A0
I 1- 1I' ' .'0
I I CIEIE 1 1.. I II ' '-i I Ia I
1 1I I I A I 1>1 4); A 0
I0 ;U z I I '1'' II I '- -
I_C 'a IiA_I - IC
I' ' ''o
c c 0 la•
I I•. Iu w 0 3-- 1 EE
. '. Wi :' 4 EI AC14tr
0i CO 4 1 In I
6 1 01Co4 ; 0 1'- 1
Co '4 0-1 -- ' 01 ! C 77-
IloL34 9 0.A AI I i
-1 . '0.' 10 'C.S I I'
go ----- - - - -
a 0
al> C. ' ' 0'A C a 1 .- I
ei 0 -
Il IA 1 I.
1010 6.I I
IC 0 1 61 v2 I l4~
a CLI I- OY 000
oil 7I >1 aO'
a 0 7>I I 11 ) 6 I 3
SIIVf A, Jl '-I 0 A U
-~~~- I I i'. 11 10 ' I 0 4Z
0 101 Ic Ira 4Ill c 0 I .4 3' 63
0 ~ 0'01 IL , VlA
QAa Q'U' I al 'A A0.'
0 A. -77-C- - 7n
6.I 6.I I I
10 1
IIL t, V>
- 313l 1 I -- .
CL ' ZI I'I I'I cA
I 3 IUI0. 0 I --- 6 IA
I_I ~ ~~~~~~ II1.1 I 1 C 0
17 1 : I - Zf I i
IIA .'~~_A A 3 1 ~ - 3 ~ C
. '3 0 '0 c w ' 6CA -
a0 0 C A'o 0 I- )
0l 101 In- - 7 COL 4U0: -
0 cl go0- 3 -
z~ ~ : E' El El -'
A I2 I '30 I I ~ '
C- --'- - - - - - - - - - - ' - - -
I
• J = - --
1' ,.' • ' I -
L. I. €-,
A -I I
II I 0 ) I-
IC" I I I, -
Al f • ' 2-
--: ............ ..
I S
S'' , ' I,•,,,
--- ' •-1-----)0'
Ai
1I* v
I,:,- , I I C - I 5
1
aI
SII
C'~ ~~~ (D,6II II
* - I I I l
MO.-> I A
-1--- - -
6I, m13 V 4
* :I- u. dl *I I. G g' i,. A 6l
a l Q C a .012 3I 1
0 W I C 10 1 1 61 It I I L-,I L i
0l 4 T t ri C mT
a, - -S
to us 0 in 0 d I
•-- . [I 6' - 1 10QIt li I €
SIAl I i -: I 4-- II i,, I I C -
LIlI- 'I,. ' - 3. I I v
"1 JI,.0•1 I 1 li 5-
'Al
'-r-I CII
Si II i - -I- --i I
C- i I O r I_I I l l I I 13 I l IG
"CIl I 1 A IIIIIl' i I t I' -~
* I I I L ' I IlI --
141 'is i l I 1 lt i II AS il l I I C
I- I-) Ifl l'-C
A AA
I I
lOIll I - "
i . 1 1-0 -61
Is 4 -c -
6 CU
I ' I I •. 01A , al -
C_ ,L ............ .in'Aa s
7"" ° "s Css E
Al A0:: I A',, >•, >•, • EE - "
, ' , E E , I IE 6" --- -
I A, ' Il : I, I- o - , ÷ I I- o > -
0 -, , I rIU-" ' ............ A
'-' l iE,' ^~ IE 0 IU,--, -I j --
a2I IG 1; * II W A6 t W 61
"I 6' I l l 0 U I I - Go
* i .I ~
Al 141
I I.6.*
S IA7 ---- - - - - - - - - C c M
- - -I--- - -0 -
I toZ
o~~_C . 0 C
A A I A
.l)0. 61 In -
- - - - - - - - - - - - - - - - - - - - -
## CHAPTER 6
EXTERNAL REPRESENTATION OF DIANA
The contents of this chapter will be included at a later date.
## CHAPTER 7
THE DIANA PACKAGE IN ADA
The contents of this chapter will be included at a later date.
APPENDIX A
f4 DIANA CROSS_REFERENCE GUIDE
I.
i
I
I
I.
I.
I--
I - m n I
DIANA Reference Manual Draft Revision 4 Page A_2
DIANA CROSS_REFERENCE GUIDE
PARTITIONS 2
STRICT CLASSES 94
STRICT CLASSES NOT DEFINING ATTRIBUTES 35
STRICT CLASSES THAT DO NOT SERVE AS TYPES 55
STRICT CLASSES THAT DO NOT SERVE AS TYPES
AND DO NOT DEFINE ATTRIBUTES : 3
LEAF NODES : 207
LEAF NODES NOT DEFINING ATTRIBUTES 92
ATTRIBUTES 135
DIANA Reference Manual Draft Revision 4 Page A_3
DIANA CROSS_REFERENCE GUIDE
PARTITIONS (UNINCLUDED CLASSES)
ALL SOURCE
TYPE SPEC
STRICT CLASSES THAT DO NOT SERVE AS TYPES AND D0 NOT DEFINE ATTRIBUTES
FULL TYPE SPEC
GENERIC PARAM
SEQUENCES
DIANA Reference Manual Draft Revision 4 Page A_4
DIANA CROSS_REFERENCE GUIDE
STRICT CLASSES THAT DO NOT DEFINE ATTRIBUTES
ALIGNMENT CLAUSE
ALL DECL
ALTfRNATIVEELEM
BODY
CHOICE
COMP REP ELEM
CONSTRAIRT
CONTEXTELEM
DECL
DISCRETE_RANGE
EXP
FULL TYPE SPEC
GENERAL ASSOC
GENERIC PARAM
HEADER
ITEM
ITERATION
MEMBERSHIPOP
NAME
PARAM
PREDEF NAME
SEQUENCES
SHORT CIRCUIT OP
SOURCE NAME
STM
STM ELEM
TEST CLAUSE ELEM
TYPE DEF.
TYPE SPEC
UNIT DESC
UNIT KIND
USE PRAGMA
USED NAME
VARIANTELEM
VARIANTPART
DIANA Reference Manual Draft Revision 4 Page A_5
DIANA CROSS_REFERENCE GUIDE
STRICT CLASSES THAT DO NOT SERVE AS TYPES
AGGEXP UNCONSTRAINED
ALL -SOURCE UNCONSTRAINEDCOMPOSITE
ARR ACC DER DEF UNIT DECL
BLOCKLOOP UNIT_NAME
CALL STM USED_OBJECT
CLAUSES STM VC NAME
COMP_NAME
CONSTRAINED
CONSTRAINED DEF
DERIVABLE SPEC
OSCRMT PARAM DECL
ENTRY STM -
EXP DECL
EXP_EXP
EXP_VAL
EXP_VAL EXP
FOR_REV
FULL TYPE SPEC
GENERIC PARAM
ID DECL
ID S OECL
INITOBJECT NAME
LABEL NAME
MEMBERSHIP
NAMEEXP
NAME VAL
NAMED ASSOC
NAMED_REP
NONGENERICDECL
NONTASK
NON_TASK NAME
OBJECT DECL
OBJECT NAME
PARAM NAME
PRIVATE SPEC
QUALCONV
REAL
REAL CONSTRAINT
RENAME INSTANT
SEQUENCES
SIMPLE RENAME DECL
STMWITH EXP
STM_WITH-EXP NAME
STM_WITH-NAME
SUBP ENTRY HEADER
SUBPROG NAME
SUBPROG PACK NAME
TESTCLAUSE
TYPE_NAME
DIANA Reference Manual Draft Revision 4 Page A_6
DIANA CROSS_REFERENCE GUIDE
LEAF NODES (CLASSES WITHOUT MEMBERS)
abort decl s
accept deferredconstantdecl
access delay
access def deriveddef
address derived subprog
aggregate discrete range_s
alignment discrete subtype
all discriminant_id
alternative dscrmt_constraint
alternativepragma dscrmt decl
alternative s dscrmt decl s
and then entry
argument id entry_call
argument id s entryid
array enum literal_s
assign enumeration
assoc enumeration def
attribute enumeration-id
attribute id exception decl
block exceptionid
blockbody exit
block loop id exp s
block-master fixed
bltn operator_id fixed constraint
box default fixed-def
case float
character id float_constraint
choice exp float def
choice others for
choice range formal dscrt def
choice s formal-fixed-def
code formal float def
complist formal integerdef
comprep function call
como _reo oraqma function id
comp rep s functionspec
compilation general assoc s
compilation unit genericdecl
compltn unit_s genericid
component_id goto
cond clause if
cond entry implicitnoteq
constant decl in
constant id in id
constrainedaccess inmop
constrainedarray inout
constrained_arraydef in out id
constrained record incomplete
context elem s index
context pragma index_constraint
conversion index-s
DIANA Reference Manual Draft Revision 4 Page A_7
DIANA CROSS_REFERENCE GUIDE
indexed renames unit
instantiation return
integer reverse
integer def scalar_s
item s select-alt_pragma
iteration id select-alternative
]_private selected
lprivate def selective wait
lprivate-type-id short circuit
label id slice
labeled source name s
length enumrep stmpragma
loop stm s
namedefault strTngliteral
name s stub
named subprogentry_decl
no default subprogram body
not in subtype_allocator
nuIT access subtypedecl
nullcomp_decl subtype id
null _stm subtype_indication
number decl subunit
number id task body
numeric literal task_body_id
operator id task decl
or else task spec
out terminate
out id test clause elem s
packagebody timedentry-
packagedecl type-decl
packageid type id
packagespec type membershiD
params unconstrainedarraydef
parenthesized universal fixed
pragma universal integer
pragmaid universal-real
pragmas use
private usepragma_s
privatedef usedchar
private_zype_id usea name id
procedurecall used object id
procedure id used op
procedure_spec variable decl
qualified variable-id
qualified allocator variant
raise variantpart
range variant pragma
rangeattribute variant s
range membership void
record while
record def with
record-rep
renames exc decl
renamesobjdecl
DIANA Reference Manual Draft Revision 4 Page A_8
DIANA CROSS_REFERENCE GUIDE
LEAF NODES THAT DO NOT DEFINE ATTRIBUTES
access def null_comp_decl
address null stm
all number decl
and then number id
argumentjid operator id
assign or else
attribute id out
block loop_id out id
box default package_body
character id package_decl
choice others package_id
code parenthesized
component_id private
cond clause private def
cond-entry privatetype_id
constant decl procedure_call
conversion procedure id
delay procedure spec
derived def quaiifiea
dscrmt decl raise
entry_call renames exc decl
enumeration id renamesunit
exception decl return
fixed constraint reverse
fixed-def select-alternative
float selective wait
float constraint stub
float-def subprogentrydecl
for subtypeid
formal dscrt def taskbody
formal fixed def terminate
formal float def timed entry
formal integer def universal _fixed
function id universal integer
0oto universal real
if L,•ed char
in id used_name_id
inmop usea object id
in out usedop
in-out id variable decl
integer void
integer def
iteration id
1_private
lprivate def
1_privatetype_id
label id
lengthenum_rep
no default
not in
nuIT access
DIANA Reference 94nuaT Draft Revision 4 Page A_9
DIANA CROSS_REFERENCE GUIDE
PREDEFINED AND USER_DEFINED TYPES
sourceposition IS THE DECLARED TYPE OF:
ALLSOURCE.lx_srcpos
comments IS THE DECLARED TYPE OF:
ALL SOURCE.Ix comments
symbol_rep IS THE DECLARED TYPE OF:
DEFNAME.1x_symrep
DESIGNATOR.lx_symrep
stringliteral. lxsymrep
value IS THE DECLARED TYPE OF:
fixed.cd_impl_small
REAL.sm_accuracy
EXPVAL.smvalue
NAME VAL.sm value
USEDOBJECT.sm value
operator IS THE DECLARED TYPE OF:
Ditn ouerator ia.smroperator
number_rep IS THE DECLARED TYPE OF:
numeric literal.lx_numrep
I
DIANA Reference Manu al Draft Revision 4 Page A_1O
DIANA CROSS_REFERENCE GUIDE
Boolean IS THE DECLARED TYPE OF:
in.lx default
funcfiv,;i call. lx pref ix
C.,1iSTRAINED.sm_depends-on-dscrmt
DERIVABLE SPEC.smTIi s anonymous
access.sm is controlled
generic id.sm_ is_ inline
SUBPROG_NAME.sm is inline
UNCONSTRAINEDCOMPOSITE.smis-limited
UNCONSTRAINEDCOMPOSITE.Sm_ is_packed
variable_ id.sm is shared
VC NAME.sm rendrres _obj
Integer 15 THE DECLARED TYPE OF:
SCALAR.cd_inpl size
ENUM '.ITERAL.Srn-pos
.NUY LITERAL.srn rep
DIANA Reference Manual Draft Revision 4 Page A_11
DIANA CROSS_REFERENCE GUIDE
ATTRIBUTES
as_aligriment-clause :ALIGNMENTCLAUSE
-record rep
as_all _deci ALL DECL
<= compilation-unit
as_alternative s :alternative s
-block-body
<= case
as_block _body :block-body
<= block
as_body :BODY
<= SUBUNITBOCC
as choice s :choice s
-alternative
-named
<=variant
as_comp_ list con ls
<= record-def
<= variant
as_:omp_ '-rp -S -omo 'ep-s
<= record rep
as _c_mcitr -unit s ::mzltn unit_s
<= compilation
as_constraint :CONSTRAINT
<= constrained array_def
<= CONSTRAINEDDEF
as context elem-s :context elem s
<=ompiiation-unit
as_decl _s :decl _s
<= comp_list
<= task-decl
as_decl _si deci-s
-package spec
as_decl _s2 :decl-s
<= package spec
as designator :DESIGNATOR
<= selected
DMANA Reference Manual Draft Revision 4 Page A_12
DIANA CROSS_REFERENCE GUIDE
as discreterange DISCRETE_RANGE
<= choice range
"<= entry
<= FOR REV
"4= slice
asddiscreteranges discreterange_s
<= index_constraint
as dscrmt -declis : dscrmt decl s
<= typedecl
as enum_literal_s : enum-literal s
<=-enumeration def
as_exp : EXP
<= alignment
<= attribute
<= choice exp
<= comp rep
<= DSCRMT PARAM DECL
<= EXP DECL
<= EXP VAL EXP
<= NAMED ASSOC
<= NAMED_REP
<= range attribute
<= REAL CONSTRAINT
4= STM WITH EXP
<= TEST ,AUSE
<= while
asexpi EXP
4= range
4= short-circuit
asexp2 EXP
<= range
<= short circuit
asexps :exp-s
4= indexed
asgeneralassoc s general assoc s
4= aggregate
<= CALL STM
<= dscrmt_constraint
<= function call
4= instantiation
<= pragma
as header HEADER
<= subprogrambody
<= UNITDECL
DIANA Reference Manual Draft Revision 4 Page A_13
DIANA CROSS_REFERENCE GUIDE
as index s .index s
<= unconstrainedarraydef
as item s : item s
<= block-body
<= genericdecl
as iteration : ITERATION
<= loop
as list : Seq Of GENERAL_ASSOC
<= generalassoc-s
as list : Seq Of SOURCENAME
<= sourcename s
as list : Seq Of ENUM_LITERAL
<= enumliterals
as_list : Seq Of DISCRETE_RANGE
- discrete range_s
as list : Seq Of SCALAR
<= scalar_s
as_list : Seq Of index
<= index s
as_list : Seq Of dscrmtdecl
- dscrmt decl s
as_list : Seq Of VARIANTELEM
<= variant s
as_list : Seq Of CHOICE
<= choice s
as list : Seq Of ITEM
<= item s
as list : Seq Of EXP
<= exp-S
as list : Seq Of STM_ELEM
<= stmrs
as_list : Seq Of ALTERNATIVE ELEM
<= alternative s
as_list : Seq Of PARAM
<= param_s
as list : Seq Of DECL
<= decl s
DIANA Reference Manual Draft Revision 4 Page A_14
DIANA CROSS_REFERENCE GUIDE
as_list Seq Of TESTCLAUSEELEM
<= test clauseelem_s
as list : Seq Of NAME
<= name-s
as list : Seq Of compilation-unit
<= compltnunit-s
as list : Seq Of pragma
<= pragma_s
as_list : Seq Of CONTEXTELEM
<= context elem s
as_list : Seq Of USEPRAGMA
<= usepragmas
as list : Seq Of COMPREPELEM
<= como reD s
as list : Seq Of argumentid
<= argument id s
as membership op : MEMBERSHIPOP
<= MEMBERSHIP
as_name : NAME
<= accept
<= comp_rep
<= deferred constant decl
<= DSCRMT PARAM DECL
<= functionspec
<= index
<= name-default
<= NAME EXP
- QUAL CON'
<= range-attribute
<= RENAME INSTANF
<= REP
<= SIMPLE RENAME DECL
<= STM WITH EXP NAME
<= STM_WITH-NAME
<= subtypeindication
<= subunit
<= type membership
<= variant part
as_name s name s
<- abort
<= use
<= with
DIANA Reference Manual Draft Revision 4 Page A_15
DIANA CROSS_REFERENCE GUIDE
as_param 5 param-s
<=accept
<=SUBP ENTRY HEADER
as_pragnia :pragmd
<= al ternat ive_pragma
<=comp rep pragma
<=context pragma
se 1elect al tpragma
-stm-pragma
<-variant pragma
as_pragma s :pragna_s
<=lignment
<= coup-list
<=. compilation-unit
<= labeled
as qualified qualified
<=qualif ied allocator
as_range RANGE
<=comp_rep
<=range membership
<=REAL CONSTRAINT
as_short -circuit -op SHORTCIRCUITOP
<=short-circuit
as source name SOURCENAME.
<= BLOCK LOOP
<=FOR REV
<-ID DECL
<= SUBUNITBODY
as_source name s source name s
<=DSCRMT PARAMDEOL
-1TOSDECL
<=labeled
as_stm STM
<= labeled
as stm s stm-s
-accept
<= alternative
< = block-body
<= CLAUSES STM
<= loop
< = TEST_CLAUSE
as_stm-si stm s
<= ENTRY STM
DIANA Reference Manual Draft Revision 4 Page A_16
DIANA CROSS_REFERENCE GUIDE
as stm s2 : stms
<= ENTRYSTM
assubtype indication : subtype_indication
<= ARR ACC DERDEF
<= discrete subtype
<= subtype allocator
<= subtype decl
as subunitbody : SUBUNIT BODY
<= subunit
as test clause elem s : test clauseelem_s
<=-CLAUSESSTM-
as_type_def : TYPEDEF
<= OBJECT DECL
<= type_decl
as_type_mark name : NAME
<= renames-obj_decl
as unit kind : UNITKIND
- <= NON GENERICDECL
as_use_pragmas : usepragma_s
<= with
as_used name : USEDNAME
<=-assoc
as_used name id : used_name_id
<=-attrTbute
<= pragma
<= range-attribute
asvariant_part : VARIANT PART
<= Como list
as_ variant s : variant s
<= variant-part
cdimpl size : Integer
<= SCALAR
cdimpl small : value
<= fixed
Ix comments : comments
<= ALLSOURCE
lx default : Boolean
<= in
DIANA Reference Manual Draft Revision 4 Page A_17
DIANA CROSS_REFERENCE GUIDE
Ix-numrep :number rep
<- numeric-literal
lx prefix :Boolean
<= function-call
lx_srcpos :source position
<= ALL_SOURCE
lx_syinrep :symbol_rep
<=DEF NAME
<= DESIGNATOR
<= string-literal
sin accuracy :value
<= REAL
sin address :EXP
<= entry-id
<= SUBPROG -PACKNAME
<= task _spec
<= VC NAME
sin argument_id -s :argument id s
<=pragma-id
sin-base_type :TYPESPEC
<= NONTASK
sm_body :BODY
<= generic-id
<= task _body -id
-= task_ spec
sin-comp_list :comp_ list
<= record
smnrcomo-reQ COMP _REP ELEM
<= COMPNAME
sin_comp_t,ýP,ý TYPESPEC
<= array
sin decl -s :deci _s
<= instantiation
-~ task-spec
sm_defn :DEF NAMIE
<= DESIGNATOR
sm_depends on dscrint :Boolean
<=CORSTýAINEO
sin derivable :SOURCENAME
OIANUA Reference Manual Draft Revision 4 Page A_18
DIANA CROSS_REFERENCE GUIDE
<= derivedsubprog
sm.derived : TYPE_SPEC
<= DERIVABLE SPEC
smdesigtype : TYPE_SPEC
<= access
<= constrained access
<= subtypeallocator
sm discrete range : DISCRETERANGE
<= AGG EXP
sm_discriminant s : dscrmt decls
<= incomplete
<= PRIVATESPEC
<= record
sm_equal : SOURCE_NAME
<= implicit_not eq
sm_exp_type : TYPESPEC
<= EXP EXP
<= NAME EXP
<= USED_OBJECT
sm first : DEF_NAME
<= constant id
<= discriminant_id
<= PARAM NAME
<= typeid
<= UNITNAME
sm_generic_param_s : item_s
<= genericid
sm index s : index s
<= array
sm_indexsubtypes : scalar_s
<= constrainedarray
sm_init exp : EXP
<=-INIT OBJECTkNAME
smninterface : PREDEFNAME
<= SUBPROGNAME
smnisanonymous : Boolean
<: DERIVABLESPEC
sm is controlled : Boolean
<= access
DIANA Reference Manual Draft Revision 4 Page A_19
DIANA CROSS_REFERENCE GUIDE
sm is inline : Boolean
<= generic id
<= SUBPROG_NAME
sm is limited : Boolean
<= UNCONSTRAINED COMPOSITE
smis_packed : Boolean
<- UNCONSTRAINED COMPOSITE
sm is shared : Boolean
<= variable id
sm literal s : enum-literal s
<- enumeration
sm master : ALL DECL
<= access
sm_normalized comp_s : general assoc s
- aggregate
sm normalized dscrmt s : exp s",
<= constrained_record -
smnormalizedparamns : exps
<= CALL STM
<= function call
sm_objtype : TYPESPEC
<= OBJECTNAME
sm_operator : operator
<= bltnoperatorid
smpos : Integer
<= ENUM_LITERAL
sm_range : RANGE
<= SCALAR
sm renames exc : NAME
<= exceptionid
smrenames obj : Boolean
<= VC NAME
sm_rep : Integer
<= ENUMLITERAL
sm_representation : REP
<= record
smisize : EXP
DIANA Reference Manual Draft Revision 4 Page A_20
DIANA CROSS_REFERENCE GUIDE
<= task spec
<. UNCONSTRAINED
sm_spec : HEADER
<= entry id
<= NONTASKNAME
sm stm : STM
<= block master
<= exit
<= LABEL NAME
sm_storage -size : EXP
"<=access
<= taskspec
sm_typespec : TYPE_SPEC
<= index
<= PRIVATESPEC
<= RANGE
<= REAL CONSTRAINT
<= task-bocy_id
<= TYPE_NAME
sm unit desc UNITDESC
- <=-SUBPROGPACKNAME
sm value" value
<= EXP VAL
<= NAME VAL
<= USED_OBJECT
DIANA Reference Manual Draft Revision 4 Page A_21
DIANA CROSS_REFERENCE GUIDE
NODES AND CLASSES
** abort
IS INCLUDED IN:
STM
STM ELEM
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
as_name s : name s
(INHERITED_FROM-ALLSOURCE):
lxsrcpos : source position
Ix comments : comments
** accept
IS INCLUDED IN:
STM
STM ELEM
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
as_name : NAME
as_stms : stm s
as param s : param s
(INHERITED_FROM ALL SOURCE):
lx srcpos : sourceposition
Ix comments : comments
** access
IS INCLUDED IN:
UNCONSTRAINED
NON TASK
FULL TYPE SPEC
DERIVABLESPEC
TYPE SPEC
NODE ATTRIBUTES:
(NODE SPECIFIC):
smstorage_size : EXP
sm master : ALL DECL
sm desigtype : TYPE SPEC
sm is controlled : Boolean
(INHERITED_FROM UNCONSTRAINED):
sm size : EXP
(INHERITED_FROM NONTASK):
sm base type : TYPE SPEC
(INHERITED_FROM DERIVABLE_SPEC):
sm derived : TYPE SPEC
sm_is_anonymous : Boolean
** access def
S.... m mmmnmllm llll ll l I
DIANA Reference Manual Draft Revision 4 Page A_22
DIANA CROSS_REFERENCE GUIDE
IS INCLUDED IN:
ARR ACC DER DEF
TYP[_DEF
ALL SOURCE
NODE ATTRIBUTES:
(INHERITED FROM ARRACCDERDEF):
as subtype indication : subtypeindication
(INHERITED FROM ALL SOURCE):
lx srcpos : source_position
lx comments : comments
** address
IS INCLUDED IN:
NAMEDREP
REP
DECL
ITEM
ALL DECL
ALL_SOURCE
NODE ATTRIBUTES:
(INHERITED FROM NAMEDREP):
as_exp : EXP
(INHERITED FROM REP):
as_name : NAME
(INHERITED_FROM ALLSOURCE):
lx_srcpos : source_position
Ix comments : comments
** AGG EXP
CLASS MEMBEPS:
aggregate
string_literal
IS INCLUDED IN:
EXP EXP
EXP
GENEPAL ASSOC
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
sm discrete range : DISCRETERANGE
(INHERITED FROM EXPEXP):
sm exp type : TYPE_SPEC
(INHERITED FROM ALL SOURCE):
lx_srcpos : sourceposition
lx comments : comments
** aggregate
IS INCLUDED IN:
AGG EXP
EXP_EXP
EXP
DIANA Reference Manual Draft Revision 4 Page A_23
DIANA CROSS_REFERENCE GUIDE
GENERAL ASSOC
ALL SOU4CE
NODE ATTRIBUTES:
(NODE SPECIFIC):
as general assocs : generalassoc_s
sm normalized_comp-s : generalassoc s
(INHERITED FROM AGG EXP):
sm discrete range : DISCRETERANGE
(INHERITED FROM EXPEXP):
sm exp_type : TYPE SPEC
(INHERITED FROM ALL SOURCE):
Ix srcpos : source position
Ixcomments : comments
** alignment
IS INCLUDED IN:
ALIGNMENTCLAUSE
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
as pragmajs :ragrra_s
as exp EXP
(INHERITED FROM ALLSOURCE):
lx_srcpos : source-position
Ix comments : comments
** ALIGNMENT CLAUSE
CLASS MEMBERS:
alignment
void
IS INCLUDED IN:
ALL SOURCE
NODE ATTRIBUTES:
(INHERITED FROM ALL SOURCE):
lx_srcpos : source position
lx comments : comments
iS THE DECLARED TYPE OF:
recora_rep.as_alignment :!ause
** all
IS INCLUDED IN:
NAME EXP
NAME
EXP
GENERAL ASSOC
ALL SOURCE
NODE ATTRIBUTES:
(INHERITED FROM NAME EXP):
as_name : NAME
sm_exptype TYPE_SPEC
(INHERITED_FROM ALLSOURCE):
S. . . . , , m iI l l I l lI
DIANA Reference Manual Draft Revision 4 Page A_24
DIANA CROSS_REFERENCE GUIDE
lx_srcpos: source position
Ix-conunents :commIen_ts
**ALL DECL
CLASS MEMBERS:
block master
void
ITEM
subunit
DSCRMT__PARANDECL
DECL
SUBUNITTBODY
dscrmtd5ec 1
PARAM
IDSOECL
ID DECL
nu_ll_comp_decl
REP
USE PRAGMA
subprogram body
task DOGy
pack age body
in
in out
Out
EXPDECL
deferred constant-deci
exception decl
type deci
UNIT DECL
task _deci
subtype decl
SIMPLE RENAME DEOL
NAMEDREP -
record rep
use
'OBJECTDECL
numoer aeci
gener_c deci
NON GENERIC DECL
renamies-objdec 1
renames exc-dec]
length enum-rep
address
constant dec1
variable deci
subprog_ent ry dec 1
package-dec 1
IS INCLUDED IN:
ALLSOURCE
NODE ATTRIBUTES:
(INHERITED FROM ALL_SOURCE):
DIANA Reference Manual Draft Revision 4 Page A_25
DIANA CROSS_REFERENCE GUIDE
lx srcpos : source position
Ix comments : comments
IS THE DECLARED TYPE OF:
compilation unit.as all decl
acceis.sm master
** ALLSOURCE
CLASS MEMBERS:
DEF NAME
index
compilationunit
compilation
comp_list
VARIANT PART
ALIGNMENT CLAUSE
VARIANT EZEM
CONTEXT ELEM
COMP REP ELEM
ALTERNATTVE ELEM
ITERATION
SHORT CIRCUIT OP
MEMBERSHIP OP
TEST CLAUSE ELEM
UNIT OESC
HEADER
CHOICE
CONSTRAINT
GENERAL ASSOC
STM ELEM
SEQUENCES
TYPE CEF
ALL DECL
SOURCE NAME
PREDEF_NAME
variant part
void
variant
vdr dnt pragma
context oragma
with
comp_rep
compreppragma
alternative
alternative pragma
FOR REV
white
and then
or else
inmop
not in
TEST CLAUSE
select alt_pragma
DIANA Reference ganual Draft Revision 4 Page A_26
DIANA CROSS_REFERENCE GUIDE
UNIT KIND
derived subprog
implicit noteq
BODY
SUBP ENTRY HEADER
packagespec
choice exp
choice others
choice range
DISCRETE RANGE
dscrmt_constraint
index_constraint
REAL CONSTRAINT
"NAMED ASSOC
EXP
STM
stm_pragma
alternative s
variant s
usepragmfla_s
test clause elem s
stm s
source name s
scalar_s
pragmas
param s
name s
index s
item s
exps
enum_literal_s
discrete_range_s
general assoc s
dscrmt dec _s
decl s
context elem s
compltnunit s
comc reDs
choice s
argument id s
enumeration def
record def
ARR ACC DER DEF
CONSTRATNED DEF
privatedef
1_private def
formal dscrt def
formal-float-def
formal fixed-def
formal-integer def
block master
ITEM
subunit
OBJECTNAME
DIANA Reference Manual Draft Revision 4 Page A_27
DIANA CROSS_REFERENCE GUIDE
LABEL NAME
UNIT NAME
TYPE NAME
entry_id
exception id
attribute-id
bltn operator_id
argument id
pragma id
for
reverse
cond clause
select alternative
RENAME INSTANT
GENERIC PARAM
blockbody
stub
procedure spec
functionspec
entry
RANGE
discrete subtype
float constraint
fixed constraint
named
assoc
NAME
EXP EXP
labeled
null stm
abort
STM WITH EXP
STM_WITH-NAME
accept
ENTRY STM
BLOCKLOOP
CLAUSES STM
terminate
constrained arraydef
aerived def
access def
unconstrained array def
subtype indication
integerdef
fixed def
float def
DSCRMT PARAMDECL
DECL
SUBUNIT BODY
INIT OBJECT NAME
ENUMLITERAL
iteration id
label id
block loopid
DIANA Reference Manual Draft Revision 4 Page A_28
DIANA CROSS_REFERENCE GUIDE
NON TASK NAME
task body_id
type id
subtype_id
privatetypeid
1_private_type_id
renames unit
instantiation
name default
no default
box-_default
range
range attribute
DESIGNATOR
NAME EXP
EXP VAL
subtypeallocator
qualified allocator
AGG EXP
return
delay
STM WITHEXPNAME
case
goto
raise
CALL STM
cond entry
timed_entry
block
if
selective wait
dscrmt decl
PARAM
ID S DECL
ID DECL
nuTl comp decl
PEP
USE PRAGMA
subprogrambody
task body
packagebody
VC NAME
number id
COMP_NAME
PARAR NAME
enumeration id
character id
SUBPROG PACK NAME
generic-id
USED OBJECT
USED NAME
NAMEVAL
all
DIANA Reference Manual Draft Revision 4 Page A_29
DIANA CROSS_REFERENCE GUIDE
slice
indexed
short circuit
numeric literal
EXP VAL_EXP
nulT access
aggregate
string-literal
assign
code
exit
entry_call
procedure-call
in
in out
out
EXP DECL
deferred constant decl
exception•decl
typedecl
UNIT DECL
task decl
subtype decl
SIMPLE RENAME DECL
NAMED REP -
record rep
use
pragma
variable id
constant id
componentid
discriminant_id
in id
out id
in out id
SUBPROG NAME
package id
used char
used objectid
usedop
used_name_id
attribute
selected
function call
MEMBERSHTP
QUALCONV
parenthesized
OBJECT DECL
number decl
generic decl
NON GENERIC DECL
renames_objdecl
renames exc decl
length_enum rep
DIANA Reference Manual Oraft Revision 4 Page A_30
DIANA CROSS_REFERENCE GUIDE
address
procedure id
operatorid
function id
range-membership
typemembership
conversion
qualified
constant decl
variable decl
subprogentrydecl
package decl
NODE ATTRIBUTES:
(NODE SPECIFIC):
lx srcpos : sourceposition
lx comments : comments
** alternative
IS INCLUDED IN:
ALTERNATIVE ELEM
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
as choices : choice s
as stms : stm s
(INHERITED FROM ALL SOURCE):
lx_srcpos : source position
lx comments : comments
** ALTERNATIVEELEM
CLASS MEMBERS:
alternative
alternative_pragma
IS INCLUDED IN:
ALL SOURCE
NODE ATTRIBUTES:
(LNHERITED FROM ALLSOURCE):
Ix srcpos : source position
Ix comments : comments
IS THE DECLARED TYPE OF:
alternative s.aslist [Seq Of]
** alternatibpragma
IS INCLUDED IN:
ALTERNATIVE ELEM
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
aspragma : pragma
(INHERITED FROM ALLSOURCE):
lx_srcpos : sourceposition
DIANA Reference Manual Draft Revision 4 Page A_31
DIANA CROSS_REFERENCE GUIDE
lxcomments comm_fents
**alternative s
IS INCLUDED IN:
SEQUENCES
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
as list Seq Of ALTERNAT1VE_ELEM
(INHERITED FROM ALL_SOURCE):
Ix srcpos isource_position
IXconmients commnents
IS THE DECLARED TYPE OF:
block body. as alternative s
case.as alternative s
**and-then
IS INCLUDED IN:
SHORT CIRCUIT OP
ALL SOURCE-
NODE ATTRIBUTES:
(INHERITED FROM ALLSOURCE):
lx_srcpos :source-position
lx comments :comments
**argument-id
IS INCLUDEC IN:
PREDEFNAME
DEF NAPE
ALL SOURCE
NODE ATTRIBUTES:
(INHERITED FROM DEFNAME):
lx symrep : symbol rep
(INHERITED FROM ALL SOURCE):
lx _srcp_s : source _position
Ix comments :c'omments
IS THE DECLARED TYPE OF:
argument-id-s. asjist (Seq Of]
**argument id s
IS INCLUDED IN:
SEQUENCES
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
as_list : Seq Of argument id
(INHERITED FROM ALL__SOURCE):
lx_srcpos : source position
lx comments : comments
IS THE DECLARED TYPE OF:
DIANA Reference Manual Draft Revision 4 Page A_32
DIANA CROSS_REFERENCE GUIDE
pragma id.sm_argument id s
* ARRACCDERDEF
CLASS MEMBERS:
constrained_array_def
derived def
access def
unconstrained_array-def
IS INCLUDED IN:
TYPE DEF
ALL 3OURCE
NODE ATTRIBUTES:
"(NODE SPECIFIC):
as subtype indication : subtypeindication
(INHERITED FROM ALLSOURCE):
Ix srcpos : source_position
Ix comments : comments
** array
IS INCLUDED IN:
UNCONSTRAINED COMPOSITE
UNCONSTRAINED
NON TASK
FULL TYPE SPEC
DERIVABLE_SPEC
TYPE SPEC
NODE ATTRIBUTES:
(NODE SPECIFIC):
sm indexs : index s
sm comotype : TYPE SPEC
(INHERITED FROM UNCONSTRAINED COMPOSITE):
sm is limited : Boolean
sm is packed : Boolean
(INHERITED FROM UNCONSTRAINED):
sm size : EXP
(INHERITED FROM NON TASK):
sm base type : TYPESPEC
(INHERITED FROM DERIVABLESPEC):
sm derived : TYPE SPEC
sm isanonymous : Boolean
** assign
IS INCLUDED IN:
STM WITH EXP NAME
STM WITH EXP
STM
STM ELEM
ALL SOURCE
NODE ATTRIBUTES:
(INHERITED FROM STM WITH EXPNAME):
as_name : NAME
DIANA Reference Manual Draft Revision 4 Page A_33
DIANA CROSS_REFERENCE GUIDE
(INHERITED FROM STM WITH EXP):
as_exp : EXP
(INHERITED FROM ALL SOURCE):
lx_srcpos : sourceposition
Ix commeents : comiments
** assOc
IS INCLUDED IN:
NAMED ASSOC
GENERAL ASSOC
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
as used name : USED NAME
(INHERITED_FROM-NAMEDASSOC):
as exp : EXP
(INHERITED FROM ALLSOURCE):
Ix srcpos : sourceposition
Ix comments : comments
** attribute
IS INCLUDED IN:
NAME VAL
NAME EXP
NAME
EXP
GENERAL ASSOC
ALL SOURCE
NODE ATTRIBUTES:
(NOCE SPECIFIC):
as_used_name_id : used_name_id
as_exp : EXP
(INHERITED FROM NAMEVAL):
sm value : value
(INHERITED FROM NAMEEXP):
as_name NAME
sm exo type : TYPE .EC
(INHERITED FROM ALLSOURCE):
ix srcpos source Dositior
lx comments : comments
** attribute id
IS INCLUDED IN:
PPEDEF NAME
DEF NAME
ALL SOURCE
NODE ATTRIBUTES:
(INHERITED FROM DEF NAME):
lx symrep : symbol_rep
(INHERITED FROM ALL SOURCE):
lx_srcpos : sourceposition
DIANA Reference Manual Draft Revision 4 Page A_34
DIANA CROSS_REFERENCE GUIDE
Ix comments comments
** block
IS INCLUDED IN:
BLOCK LOOP
STM
STM ELEM
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
*as block body : block body
(INHERITED FROM BLOCK LOOP):
as source name : SOURCENAME
(INHERITED FROM A:LLSOURCE):
Ix_srcpos : source position
block-body Ix comments : comments
IS INCLUDED IN:
BODY
UNIT OESC
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
as item s : item s
as alternative s : alternative s
Sas stm s :stm_s
(INHERITED -FROM ALLSOURCE):
lxsrcpos : sourceposition
Ix comments : comments
IS THE DECLARED TYPE OF:
block.as block body
** BLOCKLOOP
CLASS MEMBERS:
loop
block
IS INCLUDED IN:
STM
STM ELEM
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
as source name : SOURCENAME
(INHERITED FROM ALL SOURCE):
lx srcpos : sourceposition
Ix comments : comments
** block_loop_id
IS INCLUDED IN:
DIANA Reference Manual Draft Revision 4 Page A_35
DIANA CROSS_REFERENCE GUIDE
LABEL NAME
SOURCE NAME
DEF NAME
ALL SOURCE
NODE ATTRIBUTES:
(INHERITED FROM LABELNAME):
sm stm : STM
(INHERITED_FROM DEF NAME):
lx symrep : symbolrep
(INHERITED FROM ALL SOURCE):
lx srcpos : source position
Ix comments : comments
** block master
IS INCLUDED IN:
ALL DECL
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
sm stm : STM
(INHERITED FROM ALL SOURCE):
Ix srcpos : sourceposition
lx comments : comments
** bltnoperator_id
IS INCLUDED IN:
PREDEF NAME
DEF NAME
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
sm operator : operator
(INHERITED FROM DEF NAME):
lx_symrep : symbol_rep
(INHERITED FROM ALLSOURCE):
ly srcoos : source Dosition
Ix-comments : comments
** BODY
CLASS MEMBERS:
block body
void
stub
IS INCLUDED IN:
UNIT DESC
ALL 3OURCE
NODE ATTRIBUTES:
(INHERITED FROM ALL SOURCE):
lxsrcpos : sourceposition
Ix comments : comments
IS THE DECLARED TYPE OF:
DIANA Reference Manual Draft Revision 4 Page A_36
OIANA CROSS_REFERENCE GUIDE
generic_id.smbody
SUBUNITBODY.ds_body
task_body_id.sm body
taskspec.smbody
* box-default
IS INCLUDED IN:
GENERIC PARAM
UNIT KIND
UNIT DESC
ALL SOURCE
NODE ATTRIBUTES:
(INHERITED FROM ALLSOURCE):
lx_srcpos : sourceposition
Ix comments : comments
** CALL STM
CLASS MEMBERS:
entry call
procedure_call
IS INCLUDED IN:
STM WITH_NAME
STM
STM ELEM
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
asgeneral _assoc s : generaldssoc_S
sm normalizedparams : exp s
(INHERITED FROM STMWITHNAME):
as_name : NAME
(INHERITED FROM ALLSOURCE):
lxsrcpos : source position
lx comments : comments
** case
IS INCLUDED IN:
STM WITHEXP
STM
STM ELEM
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
as alternative s : alternative s
(INHERITED FROM STMWITHEXP):
as_exp : EXP
(INHERITED FROM ALL SOURCE):
lx_srcpos : source position
Ix comments : comments
* character id
DIANA Reference Manual Draft Revision 4 Page A_37
DIANA CROSS_REFERENCE GUIDE
IS INCLUDED IN:
ENUM_LITERAL
OBJECT NAME
SOURCE NAME
DEF NAME
ALL SOURCE
NODE ATTRIBUTES:
(INHERITED FROM ENUMLITERAL):
smPos Integer
smrep : Integer
(INHERITED FROM OBJECTNAME):
sm objtype TYPE_SPEC
(INHERITED FROM DEF NAME):
1x symrep : symbol_rep
(INHERITED FROM ALL SOURCE):
1x srcpos : sourceposition
lx_comments comments
** CHOICE
CLASS MEMBERS:
choice exp
choice others
choice range
IS INCLUDED IN:
ALL SOURCE
NODE ATTRIBUTES:
(INHERITED FROM ALLSOURCE):
Ixsrcpos : source-position
lx comments : comments
IS THE DECLARED TYPE OF:
choices.as list CSeq Of]
** choice exp
IS INCLUDED IN:
CHOICE
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
as exp : EXP
(INHERITED FROM ALLSOURCE):
1x srcpos : source_position
Ix comments : comments
** choice-others
IS INCLUDED IN:
CHOICE
ALL SOURCE
NODE ATTRIBUTES:
(INHERITED FROM ALLSOURCE):
lxsrcpos : source_position
DIANA Reference Manual Draft Revision 4 Page A_38
DIANA CROSS_REFERENCE GUIDE
Ix comments connents
** choice range
IS INCLUDED IN:
CHOICE
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
as discreterange : DISCRETERANGE
(INHERITED FROM ALLSOURCE):
lx srcpos : source_position
lx comments : comments
** choice s
IS INCLUDED IN:
SEQUENCES
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
as list : Seq Jf CHOICE
(INHERITED FROM ALL SOURCE):
Ix srcpos : sourceposition
lx comments : comments
IS THE DECLARED TYPE OF:
alternative.as choice s
named.as choice s
variant.aschoices
** CLAUSESSTM
CLASS MEMBERS:
if
selective'wait
IS INCLUDED IN:
STM
STM ELEM
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
as test clauseelems ; testclause_elem s
as_stm s : stm s
(INHERITED FROM ALL SOURCE):
1x_srcpos : sourceposition
Ix comments : comments
** code
IS INCLUDED IN:
STM WITH EXP NAME
STM- WITH_EXP
STM
STMELEM
DIANA Reference Manual Draft Revision 4 Page A_39
DIANA CROSS_REFERENCE GUIDE
ALL SOURCE
NODE ATTRIBUTES:
(INHERITED FROM STMWITHEXPNAME):
as_name NAME
(INHERITED FROM STMWITHEXP):
as_exp EXP
(INHERITED FROM ALL SOURCE):
Ix srcpos : source_position
lx comments : comments
** comp_list
IS INCLUDED IN:
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
as decl s : decl s
as pragma s : pragma s
as variant part : VARIANTPART
(INHERITED FROM ALL SOURCE):
lx srcpos : source positicn
Ix comments : comments
IS THE DECLARED TYPE OF:
variant.as_comp_list
record.sm comp_list
record def.as_complist
** COMP_NAME
CLASS MEMBERS:
component_id
discriminant_id
IS INCLUDED IN:
INIT OBJECT NAME
OBJECT NAME
SOURCE NAME
DEF NAME
ALL SOURCE
NODE ATTRIBUTES:
(NOCE SPEC:FIC):
smcomp rep : COMPREPELEM
(INHERITED FROM INIT OBJECT_NAME):
sm init exp : EXP
(INHERITED FROM OBJECTNAME):
sm _objtype : TYPE_SPEC
(INHERITED FROM DEFNAME):
lx_symrep : symbolrep
(INHERITED FROM ALLSOURCE):
lx_srcpos : source position
Ix comments : comments
** comp_rep
IS INCLUDED IN:
DIANA Reference Manual Draft Revision 4 Page A_40
DIANA CROSS_REFERENCE GUIDE
COMP REP ELEM
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
as_name : NAME
as_range : RANGE
"as exp : EXP
(INHERITED FROM ALL_SOURCE):
Ix srcpos : sourceposition
Ix comments : comments
•* COMPREPELEM
CLASS MEMBERS:
comp_rep
void
comp reppragma
IS INCLUDED IN:
ALL SOURCE
NODE ATTRIBUTES:
(INHERITED FROM ALLSOURCE):
1x _srcpos : sourceposition
Ix comments : comments
IS THE DECLARED TYPE OF:
comp rep_s.aslist (Seq Of]
COMP_NAME.smcomprep
** comp_rep_pragma
IS INCLUDED IN:
COMP REP ELEM
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
as pragma : pragma
(INHERITED FROM ALLSOURCE):
1xsrcpos : source position
lx comments : comments
• * comp_rep_s
IS INCLUDED IN:
SEQUENCES
ALL_SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
as list : Seq Of COMP_REP_ELEM
(INHERITED FROM ALLSOURCE):
Ix_srcpos : source position
Ix comments : comments
IS THE DECLARED TYPE OF:
recordrep.ascomp_rep_s
•* compilation
DIANA Reference Manual Draft Revision 4 Page A_41
DIANA CROSS_REFERENCE GUIDE
IS INCLUDED IN:
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
as compltn unit s : compltnunits
(INHERITED FROM ALL SOURCE):
Ix_srcpos : sourceposition
Ix comments : comments
** compilation-unit
IS INCLUDED IN:
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
ascontextelems : context elem-s
as_pragma s : pragma_s
as all decl : ALL DECL
(INHERITED FROM ALLSOURCE):
lx_srcpos : sourceposition
Ix comments : comments
TS THE DECLARED TYPE OF:
compltn unit s.as list [Seq Of]
** compltn-unit s
IS INCLUDED IN:
SEQUENCES
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
as list : Seq Of compilationunit
(INHERITED FROM ALLSOURCE):
Ix_srcpos : source_position
Ix comments : comments
IS THE DECLARED TYPE OF:
compilation.as_compltn_unit_s
** component_id
IS INCLUDED IN:
COMP_NAME
INIT OBJECT NAME
OBJECT NAME
SOURCE_NAME
DEF NAME
ALL SOURCE
NODE ATTRIBUTES:
(INHERITED FROM COMP_NAME):
smcomprep : COMPREPELEM
(INHERITED FROM INIT OBJECTNAME):
sm init exp : EXP
(INHERITED FROM OBJECTNAME):
DIANA Reference Manual Draft Revision 4 Page A_42
DIANA CROSS_REFERENCE GUIDE
"sm obj type TYPE SPEC
(INHERITED FROM DEF_NAME):
Slx_symrep : symbolrep
(INHERITED FROM ALL SOURCE):
1x srcpos : sourceposition
Ix-comments : comments
** cond clause
IS INCLUDED IN:
TEaT CLAUSE
TEST_CLAUSE ELEM
ALL SOURCE
NODE ATTRIBUTES:
(INHERITED FROM TESTCLAUSE):
as exp : EXP
as stm s : stm s
(INHERITED_FROM ALL SOURCE):
1x srcpos : sourceposition
lx comments : comments
** condentry
IS INCLUDED IN:
ENTRY STM
STM
STM ELEM
ALL SOURCE
NODE ATTRIBUTES:
(INHERITED FROM ENTRYSTM):
as stm sl : stm s
asastm s2 : stms
(INHERITED FROM ALLSOURCE):
Ix srcpos : source position
Ix comments : comments
** constant decl
IS INCLUDED IN:
OBJECT OECL
EXP OECL
ID S DECL
DEZL
ITEM
ALL DECL
ALL SOURCE
NODE ATTRIBUTES:
(INHERITED FROM OBJECTDECL):
astype def : TYPE DEF
(INHERITED FROM EXPDECL):
as_exp : EXP
(INHERITED FROM IDSDECL):
as source name s : source name s
(INHERITED_FROM AEL_SOURCE):
DIANA Reference Manual Draft Revision 4 Page A_43
DIANA CROSS_REFERENCE GUIDE
Ix_srcpos : sourceposition
ix comments : comnments
** constant Id
IS INCLUDED IN:
VC NAME
INTT OBJECT NAME
OBJECT NAME-
SOURCE_NAME
QEF NAME
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
sm first : DEFNAME
(INHERITED FROM VCNAME):
sm renames-obj : Boolean
sm address : EXP
(INHERITED FROM INITOBJECTNAME):
sm_initexp : EXP
(INHERITED FROM OBJECTNAME):
smobjtype : TYPESPEC
(INHERITED FROM DEFNAME):
lx_symrep : symbolrep
(INHERITED FROM ALLSOURCE):
lxsrcpos : source_position
lx comments : comments
** CONSTRAINED
CLASS MEMBERS:
constrainedarray
constrainedaccess
constrainedrecord
IS INCLUDED IN:
NON TASK
FULL TYPE SPEC
DERIVABLE_SPEC
TYPE SPEC
NODE ATTRIBUTES:
(NODE SPECIFIC):
sm depends on dscrmt : Boolean
(INHERITED FROM NON_TASK):
sm_basetype : TYPE SPEC
(INHERITED FROM DERIVABLESPEC):
sm derived : TYPE SPEC
smis_anonymous : Boolean
** constrained access
IS INCLUDED IN:
CONSTRAINED
NON TASK
FULLTYPESPEC
DIANA Reference Manual Draft Revision 4 Page A_44
DIANA CROSS_REFERENCE GUIDE
DERIVABLE SPEC
TYPE SPEC
NODE ATTRIBUTES:
(NODE SPECIFIC):
sm desig type : TYPE_SPEC
(INHERITED FROM CONSTRAINED):
sm depends on dscrmt : Boolean
,-. (INHERITED FROM NONTASK):
sm base type : TYPE_SPEC
(INHERITED FROM DERIVABLESPEC):
sm derived : TYPE SPEC
sm1is_anonymous : Boolean
** constrainedarray
IS INCLUDED IN:
CONSTRAINED
NON TASK
FULL TYPE SPEC
DERIVABLE_SPEC
TYPE SPEC
NODE ATTRIBUTES:
(NODE SPECIFIC):
sm indexsubtype s : scalar_s
(INHERITED FROM CONSTRAINED.):
sm depends on dscrmt : Boolean
(INHERITED FROM NON TASK):
sm base type : TYPESPEC
(INHERITED_FROM DERIVABLESPEC):
sm derived : TYPE SPEC
sm_isanonymous : Boolean
** constrained_arraydef
IS INCLUDED IN:
ARR ACC DER DEF
TYPE DETF
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
as constraint : CONSTRAINT
(INHERITED FROM ARRACCOEROEF):
as subtype_indication : subtypeindication
(INHERITED FROM ALLSOURCE):
lx srcpos : sourceposition
Ix comments : comments
** CONSTRAINEDDEF
CLASS MEMBERS:
subtypeindication
integer_def
fixed def
float def
DIANA Reference Manual Draft Revision 4 Page A_45
DIANA CROSS_REFERENCE GUIDE
IS INCLUDED IN:
TYPE DEF
ALL 3OURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
as constraint : CONSTRAINT
(INHERITED_FROM ALLSOURCE):
Ix srcpos : source position
Ix comments : comments
** constrained record
IS INCLUDED IN:
CONSTRAINED
NON TASK
FULL TYPE SPEC
DERIVABLE_SPEC
TYPE SPEC
NODE ATTRIBUTES:
(NODE SPECIFIC):
sm normalized dscrmt s : exp_s
(INHERITED FROM CONSTRAINED):
smdepends_ondscrmt : Boolean
(INHERITED FROM NON TASK):
smbase type : TYPE_SPEC
(INHERITED FROM DERIVABLE_SPEC):
sm derived : TYPE SPEC
sm_is anonymous : Boolean
** CONSTRAINT
CLASS MEMBERS:
void
DISCRETE RANGE
dscrmt_constraint
index_constraint
REAL CONSTRAINT
PANGE
discrete subtype
float constraint
fixed constraint
range
range attribute
IS INCLUDED IN:
ALL SOURCE
NODE ATTRIBUTES:
(INHERITED FROM ALLSOURCE):
lx srcpos : sourceposition
Ix comments : comments
IS THE DECLARED TYPE OF:
constrainedarraydef.as constraint
CONSTRAINEDDEF.as constraint
** CONTEXTELEM
DIANA Reference Manual Draft Revision 4 Page A_46
DIANA CROSS_REFERENCE GUIDE
CLASS MEMBERS:
contextpragma
with
IS INCLUDED IN:
ALL SOURCE
NODE ATTRIBUTES:
(INHERITED FROM ALLSOURCE):
lIx_srcpos : sourceposition
lx comments : comments
IS THE DECLARED TYPE OF:
context elem s.as list [Seq Of]
** context elem s
IS INCLUDED IN:
SEQUENCES
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
as list : Seq Of CONTEXTELEM
(INHERITED FROM ALLSOURCE):
Ix_srcpos : sourceposition
lx comments : comments
IS THE DECLARED TYPE OF:
compilationunit.ascontextelem_s
** contextpragrm
IS INCLUDED IN:
CONTEXT ELEM
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
aspragma : pragma
(INHERITED FROM ALLSOURCE):
lxsrcpos : sourceposition
Ix comments : comments
** conversion
IS INCLUDED IN:
QUAL CONV
EXP VAL EXP
EXP_VAL
EXP_EXP
EXP
GENERAL ASSOC
ALL SOURCE
NODE ATTRIBUTES:
(INHERITED FROM QUALCONV):
as_name : NAME
(INHERITED_FROM EXPVALEXP):
as_exp : EXP
DIANA Reference Manual Draft Revision 4 Page A_47
DIANA CROSS_REFERENCE GUIDE
(INHERITED FROM EXPVAL):
sm value : value
(INHERITED FROM EXPEXP):
smexp_type : TYPE SPEC
(INHERITED FROM ALL_SOURCE):
lx_srcpos : source_position
Ix_comments : comments
** DECL
CLASS MEMBERS:
10 S DECL
ID DECL
nuTl compdecl
REP
USE PRAGMA
EXP_OECL
deferred constant decl
exceptiondecl
type decl
UNIT DECL
task decl -
subtype_deci
SIMPLERENAME DECL
void
NAMED REP
record_rep
use
pragma
OBJECTDECL
number decl
generic decl
NON GENERICDECL
renamesobj decl
renames exc decl
length_enum-rep
address
constant decl
variable decl
suoprog entryaecl
package decl
IS INCLUDED IN:
ITEM
ALL DECL
ALL_SOURCE
NODE ATTRIBUTES:
(INHERITED FROM ALL SOURCE):
lx srcpos : sourceposition
Ix comments : comments
IS THE DECLARED TYPE OF:
decls.as list [Seq Of)
** declis
I
DIANA Reference Manual Draft Revision 4 Page A_48
DIANA CROSS_REFERENCE GUIDE
IS INCLUDED IN:
SEQUENCES
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
as list : Seq Of DECL
(INHERITED_FROM ALLSOURCE):
Ix srcpos : sourceposition
lx comments : comments
IS THE DECLARED TYPE OF:
instartiation.sm dec1 s
task spec.sm decT s
task decl.as decl-s
"packagespec.as decl sl
,as_decls2
comp_list.as decl-s
** DEF NAME
CLASS MEMBERS:
SOURCE NAME
PREDEF NAME
OBJECT_NAME
LABELNAME
UNIT NAME
TYPE NAME
void
entryid
exception id
attribute id
bltn_operator_ id
"argument_id
pragmdaid
INIT OBJECT NAME
ENUM_LITERAE
iteration id
label id
block loon id
NON TASK NAME
taskbody_id
type id
subtype_id
private type id
1_private_type_id
VC NAME
number id
COMP_NAME
PARAM NAME
enumeration id
character id
SUBPROGPACK NAME
generic_id
variable id
constant id
DIANA Reference Manual Draft Revision 4 Page A_49
DIANA CROSS_REFERENCE GUIDE
component_id
discriminant_id
in id
out id
in out id
SURPROG NAME
packageiid
procedure id
operatorTid
function id
IS INCLUDED IN:
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
lx symrep : symbol_rep
(INHERITED FROM ALLSOURCE):
Ix srcpos : sourceposition
Ix comments : comments
IS THE DECLARED TYPE OF:
PARAM NAME.sm first
DESIGNATOR.sm_defn
discriminant_id.sm first
typeid.sm first
constant id.sm first
UNIT NAME.sm first
** deferred constant decl
IS INCLUDED IN:
ID S DECL
DECL
ITEM
ALL DECL
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
as_name : NAME
(INHERITED FROM IDS DECL):
as source name s : source name s
(INHERITED FROM ALL SOURCE):
1x_srcpos : sourceposition
Ix comments : comments
** delay
IS INCLUDED IN:
STM WITH EXP
STM
STM ELEM
ALL SOURCE
NODE ATTRIBUTES:
(INHERITED FROM STMWITH_EXP):
as_exp : EXP
(INHERITED FROM ALLSOURCE):
DIANA Reference Manual Draft Revision 4 Page A_50
DIANA CROSS_REFERENCE GUIDE
lxsrcpos : source position
lx comments : comments
** DERIVABLE SPEC
CLASS MEMBERS:
FULL TYPE SPEC
PRIVATE SPEC
taskspec
NON TASK
private
! p:-ivate
SCALAR
CONSTRAINED
UNC3NSTRAINED
enumeration
REAL
integer
constrainedarray
constrained access
"constrained record
UNCONSTRAINED COMPOSITE
access
float
fixed
array
record
IS INCLUDED IN:
TYPE SPEC
NODE ATTRIBUTES:
(NODE SPECIFIC):
sm "arived : TYPE SPEC
sm_is anonymous : Boolean
** derived def
IS INCLUDED IN:
APP ACC DER DEF
TYPE DEF -
ALL SOURCE
"NODE ATTRIBUTES:
(INHERITED FROM ARR ACC DER DEF):
as_subtypeTndication : subtypeindication
(INHERITED FROM ALLSOURCE):
lx_srcpos : source-position
Ix comments : comments
** derived subprog
IS INCLUDED IN:
UNIT DESC
ALI SOURCE
NODE ATTR 'JTES:
(NODE SPECIFIC):
DIANA Reference Manual Draft Revision 4 Page A_51
DIANA CROSS_REFERENCE GUIDE
sm derivable : SOURCE NAME
(INHERITED FROM ALL SOURCE):
Ix srcpos : source position
lx comments : comments
** DESIGNATOR
CLASS MEMBERS:
USED OBJECT
USED NAME
used char
used-object_id
usedop
used_name_id
IS INCLUDED TN: -
NAME
EXP
GENERAL ASSOC
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
sm defn : DEF NAME
lx symrep : symbolrep
(INHERITED FROM ALLSOURCE):
1xsrcpos : sourceposition
Ix comments : comments
IS THE DECLARED TYPE OF:
selected.asdesignator
"* DISCRETE RANGE
CLASS MEMBEPS:
RANGE
discretesubtype
range
void
rangeattribute
rS rNCL!DEO E'1:
CONSTRAINT
ALL SOURCE
NODE ATTRIBUTES:
(INHERITED FROM ALL SOURCE):
lx_srcpos : source_position
lx comments : comments
IS THE DECLARED TYPE OF:
entry.as_discrete_range
FOR REV.as discreterange
AGG EXP.sm discreterange
slile.as dTscrete range
choice range.asdiscrete range
discreteranges.aslist [Seq Of]
** discrete_range_s
DIANA Reference Manual Draft Revision 4 Page A_52
DIANA CROSS_REFERENCE GUIDE
IS INCLUDED IN:
SEQUENCES
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
as list : Seq Of DISCRETERANGE
(INHERITED FROM ALLSOURCE):
Ix srcpos : sourceposition
Ix comments : comments
IS THE DECLARED TYPE OF:
indexconstraint.as discreterange_s
** discretesubtype
IS INCLUDED IN:
DISCRETE RANGE
CONSTRAINT
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
as subtypeindication : subtype indication
(INHERITEO FROM ALL SOURCE):
1x_srcpos : source_position
lx comments : comments
** discriminant_id
IS INCLUDED IN:
COMP_NAME
TNIT OBJECT NAME
OBJECT NAME
SOUPCE NAME
DEF NAME
ALL_SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
sm first : DEFNAME
(INHERITED FROM COMP_NAME):
sm :omprep : COMPREPELEM
(iNHEP17ED PROM INI T OBJECTNAME):
sm init exp : EXP
(INHERITED FROM OBJECTNAME):
sm_obj type : TYPESPEC
(INHERITED FROM DEFNAME):
lx_symrep : symbol-rep
(INHERITED FROM ALL SOURCE):
1x_srcpos : sourceposition
Ix comments : comments
** dscrmt_constraint
IS INCLUDED IN:
CONSTRAINT
ALL SOURCE
DIANA Reference Manual Draft Revision 4 Page A_53
DIANA CROSS_REFERENCE GUIUE
NODE ATTRIBUTES:
(NODE SPECIFIC):
as generalassocs : general_assocs
(INHERITED FROM ALLSOURCE):
Ix srcpos : source_position
Ix comments : comments
** dscrmt decl
IS INCLUDED IN:
DSCRMT PARAMDECL
ITEM
ALL DECL
ALL SOURCE
NODE ATTRIBUTES:
(INHERITED FROM OSCRMTPARAMDECL):
as source name s source name s
as exp : EXP
as_name : NAME
(INHERITED FROM ALL SOURCE):
Ix_srcoos : source position
lx comments : comments
IS THE DECLARED TYPE OF:
dscrmt decl s.as list (Seq Of]
** dscrmt aec. s
IS INCLUDED. IN:
SEQUENCES
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
as list : Seq Of dscrmt decl
(INHERITED FROM ALLSOURCE):
Ix_srcpos : source position
Ix comments : comments
IS THE DECLARED TYPE OF:
PRIVATE SPEC.sm discriminant s
incomplete.sm QTscriminant s-
record.sm _iscriminant_ s
type_decl.asdscrmtdecl s
** OSCRMTPARAM DECL
CLASS MEMBERS:
dscrmt decl
PARAM
in
in out
ouJ
IS INCLUDED IN:
ITEM
ALL DECL
ALL_SOURCE
DIANA Reference Manual Draft Revision 4 Page A_54
DIANA CROSS_REFERENCE GUIDE
NODE ATTRIBUTES:
(NODE SPECIFIC):
- as sourcename s source name s
as exp EXP
as_name NAME
(INHERITED FROM ALLSOURCE):
Ix srcpos : sourceposition
Ix-comments : comments
** entry
IS INCLUDED IN:
SUBP ENTRY HEADER
HEADER
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
as discrete range : DISCRETE_RANGE
(INHERITED FROM SUBP ENTRYHEADER):
asparam s : paramns
(INHERITED FROM ALL SOURCE):
Ix srcpos : sourceposition
Ix comments : comments
** entry_call
IS INCLUDED IN:
CALL STM
STM WITH_'IAME
STM -
STM ELEM
ALL SOURCE
NODE ATTRIBUTES:
(INHERITED FROM CALLSTM):
asgeneral assoc s : generalassoc s
sm normalizedparam s : exp_s
(INHERITED FROM STMWITHNAME):
as_name : NAME
(INHERITED FROM ALLSOURCE):
1x_srcpos : sourceposition
Ix comments : comments
** entry_id
IS INCLUDED IN:
SOURCE NAME
DEF NAME
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
sm_spec : HEADER
sm address : EXP
(INHERITED FROM DEFNAME):
lx_symrep : symbol_rep
DIANA Reference Manual Draft Revision 4 Page A_55
DIANA CROSS_REFERENCE GUIDE
(INHERITED FROM ALLSOURCE):
1x srcpos : source_position
1x comments : comments
** ENTRY STM
CLASS MEMBERS:
cond entry
timed entry
IS INCLUDED IN:
STM
STM ELEM
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
as_stm-sl : stm-s
as stm s2 : stms
(INHERITED FROM ALL SOURCE):
1x_srcpos : sourceposition
lx comments : comments
** ENUMLITERAL
CLASS MEMBERS:
enumeration id
character id
IS INCLUDED IN:
OBJECT NAME
SOURCE_NAME
DEF NAME
ALL_SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
smpos : Integer
sm rep : Integer
(INHERITED FROM OBJECT_NAME):
smobj_type : TYPE_SPEC
(INHERITED FROM DEFNAME):
lxsymrep : symbol_rep
(INHERITED FROM ALLSOURCE):
Ixsrcpos : source-position
Ix comments : comments
IS THE DECLARED TYPE OF:
enum_literal_s.as list [Seq Of]
** enum literal_s
IS INCLUDED IN:
SEQUENCES
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
as list : Seq Of ENUMLITERAL
(INHERITED FROM ALLSOURCE):
S... " •' II III II
DIANA Reference Manual Draft Revision 4 Page A_56
DIANA CROSS_REFERENCE GUIDE
lx srcpos : sourceposition
1x comments : comments
IS THE DECLARED TYPE OF:
enumeration.sm literal s
enumeration def.as enum_literal_s
** enumeration
IS INCLUDED IN:
SCALAR
NON TASK
FULL TYPE SPEC
DERIVABLE_SPEC
TYPE SPEC
NODE ATTRIBUTES:
(NODE SPECIFIC):
sm literal s : enum_literals
(INHERITED_FROM SCALAR):
sm range : RANGE
cdiimpl size : Integer
(INHERITED FROM NONTASK):
sm base type : TYPE_SPEC
(INHERITED FROM DERIVABLESPEC):
sm derived : TYPE SPEC
smiisanonymous Boolean
** enumeration def
IS INCLUDED IN:
TYPE DEF
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
as enum_literal_s : enum-literal s
(INHERITED FROM ALL SOURCE):
lx srcpos : sourceposition
.lx_comments : comments
** enumeration id
IS INCLUDED IN:
ENUM_LITERAL
OBJECT NAME
SOURCE_NAME
DEF NAME
ALL_SOURCE
NODE ATTRIBUTES:
(INHERITED FROM ENUMLITERAL):
smpos : Integer
sm rep : Integer
(INHERITED_FROM OBJECT NAME):
sm obj_type : TYPE_SPEC
(INHERITED_FROM DEF NAME):
lx_symrep : symbolrep
DIANA Reference Manual Draft Revision 4 Page A_57
DIANA CROSS_REFERENCE GUIDE
(INHERITED FROH ALLSOURCE):
lx srcpos : sourceposition
Ix comments : comments
** exception decl
IS INCLUDED IN:
ID S DECL
DEL-
ITEM
ALL DECL
ALL SOURCE
NODE ATTRIBUTES:
(INHERITED FROM I1_SDECL):
as source name s : source name s
(INHERITED_FROM A:LSOURCE):
Ix srcpos : sourcejposition
lx comments : comments
** exception id
IS INCLUDED IN:
SOURCE NAME
DEF NAME
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
sm renames exc : VAME
(INHERITED FROM DEFNAME):
lx_symrep . : symbol rep
(INHERITED FROM ALLSOURCE):
1xsrcpos : source position
Ix comments : comments
** exit
.IS INCLUDED IN:
STM WITH EXP NAME
STM_WITH-EXP
STM
STM ELEM
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
sm stm : STM
(INHERITED_FROM STMWITHEXPNAME):
as_name : NAME
(INHERITED_FROM STMWITHEXP):
as_exp : EXP
(INHERITED FROM ALLSOURCE):
lx srcpos : sourceposition
Ix comments : comments
** EXP
DIANA Reference Manual Draft Revision 4 Page A_58
DIANA CROSS_REFERENCE GUIDE
CLASS MEMBERS:
void
NAME
EXP EXP
DESTGNATOR
NAME EXP
EXP VAL
subtype-allocator
-. qualified allocator
AGG EXP
USED OBJECT
USED_NAME
NAME VAL
all
slice
indexed
short circuit
numerTc literal
EXP VAL EXP
nulT access
aggregate
string_literal
used char
used_object-id
usedop
used_name_id
attribute-
selected
function call
MEMBERSHTP
QUALCONV
parenthesized
rangemembership
typemembership
conversion
qualified
IS INCLUDEO IN:
GENERAL ASSOC
ALL SOURCE
"* NODE ATTRIBUTES:
(INHERITED FROM ALL SOURCE):
Ix_srcpos : sourceposition
lx comments : comments
IS THE DECLARED TYPE OF:
comp_rep.as_exp
alignment.as exp
NAMED REP.as exp
entryid.smaddress
taskspec.smstorage size
.sm size
.sm_address
SUBPROG PACK NAME.sm address
while.as_exp
DIANA Reference Manual Draft Revision 4 Page A_59
DIANA CROSS_REFERENCE GUIDE
TEST CLAUSE.as_exp
ST?4 WITH EXP.as exp
EXPVAL T XP. aseixp
short circuit. as_exp1.
.as_exp2
NAMED ASSOC. as_exp
attribute as exp
exp s.as list (Seq Of)
access. im storage size
choice exp.as exp
DSCRMT PARAM DECL .as_exp
REAL -CUNSTRATNT as_exp
range -attribut e.as_exp
range as expi.
.as_exp2
UNCONSTRAINED. sm size
VC NAME. sm addre-ss
INIT OBJECTNAME.sm mint_exp
EXPDUECL.as_exp
**EXP_DECL
CLASS MEMBERS:
OBJECTDECL
number-deci
constant-deci
variable decl
IS INCLUDED IN:
ID S OECL
DEffL_
ITEM
ALL DECL
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
as exp : EXP
(INHERITED FROM IDSDECL):
as souece -na-me s : source name-s
(INHERITED FROM ALLSOURCE):
hx_srcpos : source_position
lx comments : comments
**EXP_EXP
CLASS MEMBERS:
EXPVAL
subtype allocator
qualified allocator
AGGEXP
short circuit
numeric literal
EXP VALEXP
null ac-cess
aggregate
DIANA Reference Manual Draft Revision 4 Page A_60
DIANA CROSS_REFERENCE GUIDE
string literal
MEMBERSHIP
QUALCONV
parenthesized
range-membership
type-membership
conversion
"qualified
IS INCLUDED IN:
EXP
GENERAL ASSOC
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
smexp type : TYPESPEC
(INHERITED FROM ALL SOURCE):
lx srcpos : source position
lx comments : comments
•* exp_s
IS INCLUDED IN:
SEQUENCES
ALLSOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
as list : Seq Of EXP
(INHERITED FROM ALLSOURCE):
Ix srcpos : sourceposition
1x comr-ents : comments
IS THE DECLARE5 TYPE OF:
function call.sm normalizedparam_s
CALLSTM.sm normalizedparam_s
indexed.as_exp_s
constrained record.sm normalized dscrmt s
•* EXP VAL
CLASS MEMBERS:
snort circuit
numeTic literal
EXP VAL EXP
nulT access
MEMBERSHIP
QUALCONV
parenthesized
rangemembership
type-membership
conversion
qualified
IS INCLUDED IN:
EXPEXP
EXP
GENERALASSOC
DIANA Reference Manual Draft Revision 4 Page A_61
DIANA CROSS_REFERENCE GUIDE
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
sm value : value
(INHERITED FROM EXPEXP):
smexptype : TYPE_SPEC
(INHERITED FROM ALLSOURCE):
lx srcpos : source position
1x comments : comments
** EXP VALEXP
CLASS MEMBERS:
MEMBERSHIP
QUAL CONV
parenthesized
range membership
type membership
conversion
qualified
IS INCLUDED IN:
EXP VAL
EXP_EXP
EXP
GENERAL ASSOC
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
as_exp : EXP
(INHERITED FROM EXPVAL):
sm value :.value
(INHERITED FROM EXP EXP):
smnexp type : TYPE_SPEC
(INHERITED FROM ALLSOURCE):
lxsrcpos : source_position
lx comments : comments
** fixed
IS !NCLUDED IN:
REAL
SCALAR
NON TASK
FULL TYPE SPEC
DERIVABLE_SPEC
TYPE SPEC
NODE ATTRIBUTES:
(NODE SPECIFIC):
cd impl_small : value
(INHERITED FROM REAL):
sm accuracy : value
(INHERITED FROM SCALAR):
sm range : RANGE
cd implsize : Integer
DIANA Reference Manual Draft Revision 4 Page A_62
DIANA CROSS_REFERENCE GUIDE
(INHERITED FROM NON_TASK):
sm base type : TYPE SPEC
(INHERITED_FROM-DERIVABLESPEC):
sm derived : TYPE SPEC
sm_is anonymous : Boolean
** fixed_constraint
IS INCLUDED IN:
"REAL CONSTRAINT
CONSTRAINT
ALL SOURCE
NODE ATTRIBUTES:
(INHERITED FROM REAL CONSTRAINT):
smtype spec : TYPE_SPEC
as_exp : EXP
as range : RANGE
(INHERITED FROM ALL SOURCE):
Ix_srcpos : sourceposition
lx comments : comments
** fixed aef
IS INCLUDED IN:
CONSTRAINED DEF
TYPE DEF
ALL SOURCE
NODE ATTRIBUTES:
(INHERITED FROM CONSTRAINEDDEF):
as constraint : CONSTRAINT
(INHERITED FROM ALL SOURCE):
lx _srcpos : source-position
Ix comments : comments
** float
IS INCLUDED IN:
PEAL
SCALAR
NON TASK
FULL TYPE SPEC
DERIVABLE_SPEC
"TYPE SPEC
NODE ATTRIBUTES:
(INHERITED FROM REAL):
sm accuracy : value
(INHERITED FROM SCALAR):
sm range : RANGE
cdimpl size : Integer
(INHERITED FROM NON TASK):
sm basetype : TYPE_SPEC
(INHERITED FROM DERIVABLESPEC):
sm derived : TYPE SPEC
sm_isanonymous : Boolean
DIANA Reference Manual Draft Revision 4 Page A_63
DIANA CROSS_REFERENCE GUIDE
** float constraint
IS INCLUDED IN:
REAL CONSTRAINT
CONSTRAINT
ALL SOURCE
NODE ATTRIBUTES:
(INHERITED FROM REALCONSTRAINT):
sm typespec : TYPE SPEC
as exp : EXP
as range : RANGE
(INHERITED_FROM ALLSOURCE):
lx srcpos : source position
Ix comments : comments
** float def
IS INCLUDED IN:
CONSTRAINED DEF
TYPE DEF
ALL SOURCE
NODE ATTRIBUTES:
(INHERITED FROM CONSTRAINEDDEF):
as constraint : CONSTRAINT
(INHERITED FROM ALL SOURCE):
lx_srcpos : source position
lx comments : comments
** for
IS INCLUDED IN:
FOR REV
ITERATION
ALL SOURCE
NODE ATTRIBUTES:
(INHERITED FROM FORREV):
as source name : SOURCE NAME
as discreTe_range : DISCRETERANGE
(INHERITED FROM ALLSOURCE):
,x_srcpos : source position
Ix comments : comments
**- FOR REV
CLASS MEMBERS:
for
reverse
IS INCLUDED IN:
ITERATION
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
as source name : SOURCENAME
DIANA Reference Manual Draft Revision 4 Page A_64
DIANA CROSS_REFERENCE GUIDE
as discreterange : DISCRETE_RANGE
(INHERITED FROM ALLSOURCE):
1x srcpos : source position
Ix comments : comments
** formal dscrt def
"IS INCLUDED IN:
TYPE DEF
ALL SOURCE
NODE ATTRIBUTES:
* '(INHERITED FROM ALLSOURCE):
, -Ix srcpos : source position
Ix comments : comments
** formal fixed def
IS INCLUDED IN:
TYPE DEF
ALL SOURCE
"NODE ATTRIBUTES:
(INHERITED FROM ALLSOURCE):
Ix_srcpos : source position
Ix comments : comments
** formal float def
IS INCLUDED IN:
TYPE DEF
ALL SOURCE
NODE ATTRIBUTES:
(INHERITED FROM ALLSOURCE):
Ix_srcpos : source position
Ix comments : comments
** formal integerjdef
IS INCLUDED IN:
TYPE DEF
ALL SOURCE
NODE ATTRIBUTES:
(INHERITED FROM ALLSOURCE):
1x srcpos : sourceposition
lx comments : comments
** FULL TYPE SPEC
CLASS MEMBERS:
task spec
NON TASK
SCATAR
CONSTRAINED
UNCONSTRAINED
enumeration
DIANA Reference Manual Draft Revision 4 Page A_65
DIANA CROSS_REFERENCE GUIDE
REAL
integer
constrained_array
constrained-access
constrained record
UNCONSTRAINED COMPOSITE
access
float
fixed
array
record
IS INCLUDED IN:
DERIVABLE SPEC
TYPE SPEC
NODE ATTRIBUTES:
(INHERITED FROM DERIVABLESPEC):
sm derived : TYPE SPEC
smnisanonymous : Boolean
** function call
IS INCLUDED IN:
NAME VAL
NAME EXP
NAME
EXP
GENERAL ASSOC
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
as general_assoc s : general_assoc_s
sm normalized param_s : exps
lx prefix : Boolean
(INHERITED_FROM NAMEVAL):
sm value : value
(INHERITED_FROM NAMEEXP):
as_name : NAME
sm exp type TYPE_SPEC
(INHERITED FROM ALLSOURCE):
Ix_srcpos : source_pcsition
lx comments : comments
** function id
IS INCLUDED IN:
SUBPROG NAME
SUBPROG PACK NAME
NON TASK NAME
UNIT NAME
SOURCE NAME
DEF NAME
ALL SOURCE
NODE ATTRIBUTES:
(INHERITED FROM SUBPROGNAME):
i
DIANA Reference Manual Draft Revision 4 Page A_66
* DIANA CROSS_REFERENCE GUIDE
sm is inline : Boolean
sm interface : PREDEF NAME
(INHERITED FROM SUBPROG PACKNAME):
sm unit desc : UNIT DESC
sm address : EXP
(INHERITED FROM NONTASKNAME):
sm spec : HEADER
(INHERITED FROM UNITNAME):
sm first : DEF NAME
(INHERITED FROM DEF_NAME):
lx symrep : symbol rep
(INHERITED FROM ALLSOURCE):
Ix srcpos : sourceposition
,xfunction spec xcomments : comments
IS INCLUDED IN:
SUBP ENTRYHEADER
HEADER
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
as_name : NAME
(TNHERITED FROM SURPENTRY HEADER):
asparam s : param-s
(INHERITED FROM ALL_SOURCE):
lx_srcpos :.source position
lx comments : comments
** GENERALASSOC
CLASS MFMBERS:
NAMED ASSOC
EXP
named
assoc
void
NAME
EXP EXP
DESIGNATOR
NAME EXP
EXP VAL
subtype allocator
qualified allocator
AGG EXP
USED OBJECT
USED NAME
"•AME VAL
all
slice
indexed
short circuit
numeric-literal
I-..
DIANA Reference Manual Draft Revision 4 Page A_67
DIANA CROSS_REFERENCE GUIDE
EXP VAL EXP
nulT access
aggregate
stringliteral
used char
used objectid
used op
used_name_id
attribute
selected
function call
MEMBERSHTP
QUALCONV
parenthesized
rangemembership
typemembership
conversion
qualified
IS INCLUDED IN:
ALL SOURCE
NODE ATTRIBUTES:
(INHERITED FROM ALLSOURCE):
lx srcpos : source_position
lx comments : comments
IS THE DECLARED TYPE OF:
general assoc s.aslist [Seq Of)
** general assoc_s
IS INCLUDED IN:
SEQUENCES
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
as list : Seq Of GENERAL ASSOC
(INHERITED FROM ALLSOURCE):
Ix srcpos : source position
Ix comments : comments
IS THE DECLARED TYPE OF:
instantiation.as general_assoc_s
function call.as general_assoc_s
CALLSTM.asgeneral-assocs
aggregate.asgeneral-assoc-s
.sm normalized_comp_s
dscrmt_constraint.asgeneralassoc s
pragma.asgeneralassoc-s
** generic deci
IS INCLUDED IN:
UNIT DECL
ID DECL
DECL
ITEM
DIANA Reference Manual Draft Revision 4 Page A_68
DIANA CROSS_REFERENCE GUIDE
ALL DECL
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
as item s : item s
(INHERITED -FROM -UNITDECL):
as header : HEADER
(INHEkITED FROM ID_DECL):
as source name : SOURCE NAME
(INHERITED FROM ALL SOURCE):
lx_srcpos : sourceposition
lx comments : comments
•* genericid
IS INCLUDED IN:
NON TASK NAME
UNIT NAME
SOURCE NAME
DEF NARE
ALL_SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
smgeneric_param_s : item s
sm is inline : Boolean
sm body : BODY
(INHERITED FROM NONTASKNAME):
sm_spec : HEADER
(INHERITED FROM UNITNAME):
sm first : DEF _NAME
(INHERITED FROM DEFNAME):
lx_symrep : symbolrep
(INHERITED FROM ALLSOURCE):
lx srcpos : sourceposition
lx comments : comments
•* GENERICPARAM
CLASS MEMBERS:
name default
no default
box default
IS INCLUDED IN:
UNIT KIND
UNIT DESC
ALL SOURCE
NODE ATTRIBUTES:
(INHERITED FROM ALLSOURCE):
lx srcpos : source position
Ix comments : comments
•* goto
IS INCLUDED IN:
DIANA Reference Manual Draft Revision 4 Page A_69
DIANA CROSS_REFERENCE GUIDE
STM WITH NAME
STM
STM ELEM
ALL SOURCE
NODE ATTRIBUTES:
(INHERITED FROM STMWITHNAME):
as_name : NAME
(INHERITED_FROM ALLSOURCE):
Ix srcpos : sourcejposition
lx comments : comments
** HEADER
CLASS MEMBERS:
SUBP ENTRY HEADER
package_spec
procedurespec
functionspec
entry
IS INCLUDED IN:
ALL SOURCE
NODE ATTRIBUTES:
(INHERITED FROM ALLSOURCE):
1x srcpos : sourceposition
1x comments : comments
IS THE DECLARED TYPE OF:
entryid.sm_spec
subprogrambody.asheader
NON TASK NAME.smspec
UNITOECL.as header
** IDDECL
CLASS MEMBERS:
type_decl
UNIT DECL
task decl
subtype decl
SIMPLERENAMEDECL
generi. decl
NON GENERIC DECL
renames_objdecl
renames exc decl
subprog entry dec1
packagedecl
IS INCLUDED IN:
DECL
ITEM
ALL DECL
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
as source name : SOURCE NAME
(INHERITED_FROM ATLSOURCE):
DIANA Reference Manual Draft Revisio. 4 Page A_70DIANA CROSS_REFERENCE GUIDE
1xsrcpos : sourceposition
Ix comments : comments
** IDSDECL
CLASS MEMBERS:
EXP DECL
deferred constant decl
exception decl
OBJECTDEOL
* number-decl
constant decl
variable-decl
IS INCLUDED IN:
DECL
ITEM
ALL DECL
ALL SOURCE
NODE ATTRIBUTES:
"(NODE SPECIFIC):
as source name s : source name s
(INHERITED FROM ALL_SOURCE):
1x srcpos : sourceposition
Ix comments : comments
** if
IS INCLUDED IN:
CLAUSES STM
STM
STM ELEM
ALLSOURCE
NODE ATTRIBUTES:
(INHERITED FROM CLAUSES STM):
as test clause elem-s : test clause elem s
as stm s : stms
(INHERITED_FROM ALLSOURCE):
lx_srcpos : source position
lx comments : comments
** implicit noteq
., IS INCLUDED IN:
UNIT DESC
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
sm_equal : SOURCENAME
(INHERITED FROM ALL SOURCE):
Ix_srcpos : sourceposition
lx comments : comments
** in
DIANA Reference Manual Draft Revision 4 Page A_71
DIANA CROSS_REFERENCE GUIDE
IS INCLUDED IN:
PARAM
DSCRMTPARAMDECL
ITEM
ALL DECL
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
lx default : Boolean
(INHERITED FROM DSCRMTPARAMDECL):
assourcename s : source name s
as_exp : EXP
as_name : NAME
(INHERITED FROM ALL SOURCE):
Ix_srcpos : source position
1x comments : comments
** in id
IS INCLUDED IN:
PARAM NAME
INIT OBJECT NAME
OBJECT NAME-
SOURCE_NAME
DEF NAME
ALL SOURCE
NODE ATTRIBUTES:
(INHERITED FROM PARAM_NAME):
sm first : DEF NAME
(INHERITED FROM [NIT_OBJECTNAME):
sm initexp : EXP.
(INHERITED FROM OBJECT_NAME):
sm obj type : TYPESPEC
(INHERITED FROM DEF NAME):
lx_symrep : symbolrep
(INHERITED FROM ALLSOURCE):
lx_srcpos : source_position
lx comments : comments
** in_op
IS INCLUDED IN:
MEMBERSHIP OP
ALL SOURCE
NODE ATTRIBUTES:
(INHERITED FROM ALLSOURCE):
lx_srcpos : sourceposition
Ix comments : comments
** in out
IS INCLUDED IN:
PARAM
DSCRMTPARAMDECL
DIANA Reference Manual Draft Revision 4 Page A_72
DIANA CROSS_REFERENCE GUIDE
ITEM
ALL DECL
ALL SOURCE
NODE ATTRIBUTES:
(INHERITED FROM DSCRMTPARAMDECL):
as source name s : source name s
as exp : EXP
as_name : NAME
(INHERITED_FROM ALL SOURCE):
Ix_srcpos : sourceposition
1x comments : comments
•* in out id
IS INCLUDED IN:
PARAM NAME
INIT OBJECT NAME
OBJECT NAME-
SOURCE_NAME
DEF NAME
ALL SOURCE
NODE ATTRIBUTES:
(INHERITED FROM PARAMNAME):
sm first : DEF NAME
(INHERITED FROM INITOBJECTNAME):
sm initexp : EXP
(INHERITED FROM OBJECTNAME):
smobj type : TYPE_SPEC
(INHERITED FROM DEFNAME):
lx symrep : symbolrep
(INHERITED FROM ALLSOURCE):
lx_srcpos : source position
lx comments : comments
•* incomplete
IS INCLUDED IN:
TYPE SPEC
NODE ATTRIBUTES:
(NODE SPECIFIC):
sm discriminant s : dscrmt decl s
•* index
IS INCLUDED IN:
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
as_name : NAME
sm type spec TYPE_SPEC
(INHERITED FROM ALL SOURCE):
-xx_srcpos : sourceposition
Ix comments : comments
IS THE DECLARED TYPE OF:
DIANA Reference Manual Draft Revision 4 Page A_73
DIANA CROSS_REFERENCE GUIDE
index s.as list [Seq Of]
** index_constraint
IS INCLUDED IN:
CONSTRAINT
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
as discrete_range_s : discrete range_s
(INHERITED_FROM ALL_SOURCE):
lx srcpos : sourceposition
lx comments : comments
** index s
IS INCLUDED IN:
SEQUENCES
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
as list : Seq Of index
(INHERITED FROM ALLSOURCE):
lx_srcpos : source_position
1x comments : comments
IS THE DECLARED TYPE OF:
array.sm index s
unconstrained array def.as_index_s
** indexed
IS INCLUDED IN:
NAME EXP
NAME
EXP
GENERAL ASSOC
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
asexps : exp_s
(INHERITED FROM NAMEEXP):
as_name : NAME
smexptype : TYPESPEC
(INHERITED FROM ALLSOURCE):
lx_srcpos : source position
Ix comments : comments
** INITOBJECTNAME
CLASS MEMBERS:
VC NAME
number id
COMP_NAME
PARAM NAME
DIANA Reference Manual Draft Revision 4 Page A_74
DIANA CROSS_REFERENCE GUIDE
variable id
constant-id
component_id
discriminant_id
in id• O_t id
out id
in out id
IS INCLUDED INT
OBJECT NAME
SOURCE NAME
DEF NAME
ALL SOURCE
NODE ATTRIBUTES:
"* (NODE SPECIFIC):
smninit exp : EXP
(INHERITED FROM OBJECTNAME):
sm objtype : TYPE_SPEC
(INHERITED FROM DEF NAME):
lx_symrep : symbol rep
(INHERITED FROM ALLSOURCE):
lxsrcpos : source position
Ix comments : comments
•* instantiation
IS INCLUDED IN:
RENAME INSTANT
UNIT KTND
UNIT_DESC
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
asgeneralassoc s : generalassoc s
sm decl s : decl s
(INHERITED FROM RENAMEINSTANT):
as_name : NAME
(INHERITED FROM ALLSOURCE):
Ix_srcpos : source oosition
Ix comments : comments
•* integer
IS INCLUDED IN:
SCALAR
NON TASK
FULL TYPE SPEC
DERIVABLE_SPEC
TYPE SPEC
NODE ATTRIBUTES:
(INHERITED FROM SCALAR):
sm range : RANGE
"cd implsize : Integer
(INHERITED FROM NON TASK):
smbase_type : TYPESPEC
DIANA Reference Manual Draft Revision 4 Page A_75
DIANA CROSS_REFERENCE GUIDE
(INHERITED FROM DERIVABLE_SPEC):
sm derived : TYPE_SPEC
sm is anonymous : Boolean
Sinteger def
IS INCLUDED IN:
CONSTRAINEDDEF
TYPE DEF
ALL SOURCE
NODE ATTRIBUTES:
(INHERITED FROM4 CONSTRAINED DEF):
as constraint : CONSTRAINT
(INHERITED FROM ALL -SOURCE):
lx_srcpos : source position
lx conunents : comments
**ITEM
CLASS MEMBERS:
DSCRMTPARAMDEOL
DECL
SUBUN IT_BODY
dscrmt-dec 1
PARAM
1D0SDECL
IDDDECL
null _conp_deci
REP
USE PRAtMA
subprogram body
task -body
package body
in
in out
out
EXPOECL
deferred -cons tant-decl
exception_deci
type_dec!
UNITOECL
task decl
subtype decl
S IMPLE -RENAME..9ECL
void
NAMED REP
record -rep
use
pragnia
OBJECT OECL
number dec
generic deci
NONGENERICDECL
renames_obj-deci
DIANA Reference 1Manual Draft Revision 4 Page A_76
DIANA CROSS_REFERENCE GUIDE
renames exc decl
length enumjrep
address
constant decl
variable-decl
subprog entry_decl
package decl
IS INCLUDED IN:
ALL DECL
ALL SOURCE
NODE ATTRIBUTES:
(INHERITED FROM ALLSOURCE):
lx srcpos : sourceposition
Ix comments : comments
IS THE DECLARED TYPE OF:
item s.as list (Seq Of]
* item s
IS INCLUDED IN:
SEQUENCES
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
as list : Seq Of ITEM
(INHERITED FROM ALL SOURCE):
1x_srcpos : sourceposition
Ix comments : comments
IS THE DECLARED TYPE OF:
genericjid.sm_genericparam_s
generic decl.as item s
blockbody.as_item_s-
** ITERATION
CLASS MEMBERS:
void
FOP REV
white
for
reverse
IS INCLUDED IN:
ALL SOURCE
NODE ATTRIBUTES:
(INHERITED FROM ALLSOURCE):
lx srcpos : source position
Ix comments : comments
IS THE DECLARED TYPE OF:
loop.asiteration
** iteration id
IS INCLUDED IN:
OBJECTNAME
DIANA Reference Manual Draft. Revision 4 Page A_77
DIANA CROSS_REFERENCE GUIDE
SOURCE NAME
DEF NAME
ALL SOURCE
NODE ATTRIBUTES:
(INHERITED FROM OBJECT_NAME):
sm obj type : TYPE SPEC
(INHERITED FROM DEF NAME):
1x symrep : symbolrep
(INHERITED_FROM ALLSOURCE):
lx_srcpos : sourceposition
Ix comments : comments
1_private
IS INCLUDED IN:
PRIVATE SPEC
DERIVABLE SPEC
TYPE SPEC
NODE ATTRIBUTES:
(INHERITED FROM PRIVATE_SPEC):
sm discriminant s : dscrmt decl s
smitypespec : TYPE_SPEC
(INHERITED FROM DERIVABLESPEC):
sm derived : TYPE SPEC
sm_isanonymous : Boclean
** 1_privatedef
IS INCLUDED IN:
TYPE DEF
ALL SOURCE
NODE ATTRIBUTES:
(INHERITED FROM ALL SOURCE):
Ix_srcpos : source_position
Ix comments : comments
** l_privatetypeid
IS INCLUDED IN:
TYPE NAME
SOURCE NAME
DEF NAME
ALL SOURCE
NODE ATTRIBUTES:
(INHERITED FROM TYPENAME):
sm_type_spec : TYPE_SPEC
(INHERITED FROM DEFNAME):
lxsymrep : symbolrep
(INHERITED FROM ALL SOURCE):
lx_srcpos : source position
Ix comments : comments
* label id
DIANA Reference Manual Draft Revision 4 Page A_78
DIANA CROSS_REFERENCE GUIDE
IS INCLUDED IN:
LABEL NAME
SOURCr NAME
DEF NAME
ALL SOURCE
NODE ATTRIBUTES:
(INHERITED FROM LABELNAME):
sm stm : STM
' (INHERITED FROM DEFNAME):
lx symrep : symbolrep
(INHERITED FROM ALL SOURCE):
Ix srcpos : sourceposition
Ix comments : comments
** LABEL NAME
CLASS MEMBERS:
"label id
block loop id
"*'IS INCLUDED IN:
SOURCE NAME
DEF NAME
ALLSOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
sm stm : STM
(INHERITED FROM DEFNAME):
lx symrep : symbol_rep
(INHERITED FROM ALL SOURCE):
7x -srcpos : sourceposition
Ix comments : comments
"* labeled
IS INCLUDED IN:
STM
STM ELEM
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
as source name s : source name s
as_stm : STM
as pragma s : pragma_s
(INHERITED FROM ALL_SOURCE):
Ix srcpos : sourceposition
Ix comments : comments
** lengthenumrep
IS INCLUDED IN:
NAMED REP
REP
DECL
ITEM
DIANA Reference Manual Draft Revision 4 Page A_79
DIANA CROSS_REFERENCE GUIDE
ALL DECL
ALL SOURCE
NODE ATTRIBUTES:
(INHERITED FROM NAMED REP):
as_exp : EXP
(INHERITED FROM REP):
as_name : NAME
(INHERITED_FROM ALL SOURCE):
lx_srcpos : sourceposition
Ix comments : comments
** loop
IS INCLUDED IN:
BLOCK LOOP
STM
STM ELEM
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
as iteration : ITERATION
as stm s : stm s
(INHERITED_FROM BLOCK LOOP):
as source name : SOURCENAME
(INHERITED_FROM ALL SOURCE):
lx_srcpos : sourceposition
Ix comments : comments
** MEMBERSHIP
CLASS MEMBERS:
rangemembership
typemembership
IS :NCLUDEDO IN:
EXP VAL EXP
EXP_VAL
EXP EXP
EXP
GENERAL ASSOC
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
as membership op : MEMBERSHIPOP
(INHERITED_FROM EXPVAL_EXP):
as_exp : EXP
(INHERITED FROM EXPVAL):
sm value : value
(INHERITED_FROM EXPEXP):
smexptype : TYPE_SPEC
(INHERITED FROM ALLSOURCE):
lx_srcpos : source_position
lx comments : comments
** MEMBERSHIPOP
DIANA Reference Manual Draft Revision 4 Page A_80
DIANA CROSS_REFERENCE GUIDE
CLASS MEMBERS:
In_op
not in
IS INCLUDED IN:
ALL SOURCE
NODE ATTRIBUTES:
(INHERITED FROM ALLSOURCE):
lx srcpos : sourcejposition
Ix comments : comments
IS THE DECLARED TYPE OF:
MEMBERSHIP.as membership_op
** NAME
CLASS MEMBERS:
DESIGNATOR
NAME EXP
void
USED OBJECT
USED NAME
NAME VAL
all
slice
indexed
used-char
used object id
usedop
used_name_id
attribute
selected
function call
IS INCLUDED IN:
EXP
GENERAL ASSOC
ALL SOURCE
NODE ATTRIBUTES:
(INHERITED FROM ALLSOURCE):
lx srcpos : source-position
lx7comments : comments
"* -IS THE DECLARED TYPE OF:
comprep.asname
REP.as_name
name default.asname
exception id.smrenames exc
subunit.as_name
name s.as Tist [Seq Of]
accept.as_name
RENAME INSTANT.as_name
renames objdec .astype mark name
SIMPLE RENAME DECL.as_name
deferredconstantdecl.as_name
function spec.asname
STM WITH NAME.as_name
DIANA Reference Manual Draft Revision 4 Page A_81
DIANA CROSS_REFERENCE GUIDE
STM WITH EXP NAME.as_name
QUAL CONV.as_name -
type -membership.as_name
NAME EXP.as_name
variant part.as_name
DSCRMT PARAM DECL.as_name
index.as_name
range attribute.asname
subtype-indication.as_name
** name-default
IS INCLUDED IN:
GENERIC PARAM
UNIT KIND
UNIT DESC
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
as_name : NAME
(INHERITED FROM ALLSOURCE):
lx_srcpos : source_position
Ix comments : comments
** NAME EXP
CLASS MEMBERS:
NAME VAL
all
slice
indexed
dttribute
sc 1
"• ted
function call
IS INCLUDED IN:
NAME
EXP
GENERAL ASSOC
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
as_name : NAME
smexptype : TYPE_SPEC
(INHERITED FROM ALL SOURCE):
lx srcpos : source_position
Ix comments : comments
** name s
IS INCLUDED IN:
SEQUENCES
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
DIANA Reference Manual Draft Revision 4 Page A_82
DIANA CROSS_REFERENCE GUIDE
as list : Seq Of NAME
(INHERITED FROM ALLSOURCE):
lx_srcpos : sourceposition
Ix comments : cumments
"IS THE DECLARED TYPE OF:
with.as_name s
abort.as_name s
use.as_name s
** NAMEVAL
CLASS MEMBERS:
attribute
"selected
function call
IS INCLUDED IN:
NAMEEXP
NAME
EXP
GENERAL ASSOC
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
sm value : value
(INHERITED FROM NAMEEXP):
as_name : NAME
sm_exp type : TYPESPEC
(INHERITED FROM ALLSOURCE):
Ixsrcpos : source position
Ix comments .: comments
** named
IS INCLUDED IN:
NAMED ASSOC
GENERAL ASSOC
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
as choice s : choice s
(INHERITED FROM NAMEDASSOC):
as_exp : EXP
(INHERITED FROM ALLSOURCE):
lx_srcpos : sourceposition
lx comments : comments
** NAMEDASSOC
CLASS MEMBERS:
named
assoc
IS INCLUDED IN:
GENERAL ASSOC
ALLSOURCE
DIANA Reference Manual Draft Revision 4 Page A_83
DIANA CROSS_REFERENCE GUIDE
NODE ATTRIBUTES:
(NODE SPECIFIC):
as_exp : EXP
(INHERITED FROM ALLSOURCE):
1x_srcpos : source_position
Ix comments : comments
** NAMED REP
CLASS MEMBERS:
length_enum_rep
address
IS INCLUDED IN:
REP
DECL
ITEM
ALL DECL
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
as exp : EXP
(INHERITED FROM REP):
as_name : NAME
(INHERITED_FROM ALL SOURCE):
lx_srcpos : sourceposition
lx comments : comments
** no-default
IS INCLUDED IN:
GENERIC PARAM
UNIT KIND
UNIT DESC
ALL SOURCE
NODE ATTRIBUiLS:
(INHERITED FROM ALL SOURCE):
Ixsrcpos : source_position
Ix comments : comments
** NON GENERICOECL
CLASS MEMBERS:
subprogentrydecl
packagedecl
IS INCLUDED IN:
UNIT DECL
ID DECL
DECL
ITEM
ALL DECL
ALL SOURCE
NOCE ATTRIBUTES:
(NODE SPECIFIC):
as unit kind : UNIT KIND
DIANA Reference Manual Draft Revision 4 Page A_84
DIANA CROSS_REFERENCE GUIDE
(INHERITED FROM UNITDECL):
as header : HEADER
(INHERITED FROM IDDECL):
as source name : SOURCE_NAME
(INHERITED_FROM ALL SOURCE):
Ix srcpos : sourcepositior,
lx comments : comments
,- ** NON TASK
CLASS MEMBERS:
SCALAR
"CONSTRAINED
UNCONSTRAINED
enumeration
REAL
integer
constrained_array
constrainedaccess
constrained record
UNCONSTRAINEDCOMPOSITE
access
float
fixed
array
record
IS INCLUDED IN:
FULL TYPE SPEC
DERIVABLE_SPEC
TYPE SPEC
NODE ATTRIBUTES:
(NODE SPECIFIC):
sm basetype : TYPESPEC
. -(INHERITED FROM DERIVABLE _SPEC):
sm derived : TYPE SPEC
sm isanonymous : Boolean
• ** NONTASKNAME
CLASS MEMBERS:
SUBPROG PACK NAME
generic_id
SUBPROG NAME
package id
procedure id
operator id
function id
IS INCLUDED IN:
UNIT NAME
SOURCE NAME
DEF NAME
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
DIANA Reference Manual Draft Revision 4 Page A_85
DIANA CROSS_REFERENCE GUIDE
smspec : HEADER
(INHERITED FROM UNITNAME):
sm first : DEF NAME
(INHERITED FROM DEF NAME):
Ixsymrep : symbol rep
(INHERITED FROM ALLSOURCE):
lx_srcpos : sourcejposition
Ix comments : comments
** not-in
IS INCLUDED IN:
MEMBERSHIPOP
ALL SOURCE
NODE ATTRIBUTES:
(INHERITED FROM ALLSOURCE):
lx_srcpos : sourcejposition
Ix comments : comments
** null access
IS INCLUDED IN:
EXP VAL
EXP EXP
EXP
GENERAL ASSOC
ALL SOURCE
NODE ATTRIBUTES:
(INHERITED FROM EXPVAL):
sm value : value
(INHERITED FROM EXPEXP):
sm_exp_type : TYPE_SPEC
(INHERITED FROM ALLSOURCE):
lx_srcpos : source position
lx comments : comments
** nullcomp_decl
IS INCLUDED IN:
DECL
ITEM
ALL DECL
ALL SOURCE
NODE ATTRIBUTES:
(INHERITED FROM ALLSOURCE):
lx_srcpos : source position
lx_comments : comments
**null _stn
IS INCLUDED IN:
STM
STMELEM
ALLSOURCE
I
I. DIANA Reference Manual Draft Revision 4 Page A_86
DIANA CROSS_REFERENCE GUIDE
NODE ATTRIBUTES:
(INHERITED FROM ALLSOURCE):
Ix srcpos : sourceposition
lx comments : comments
•* number decl
IS INCLUDED IN:
EXP DECL
0IS DECL
DEýL-
ITEM
ALL DECL
"ALL SOURCE
NODE ATTRIBUTES:
(INHERITED FROM EXPDECL):
as_exp : EXP
(INHERITED FROM ID_SDECL):
as source name s : sourcename_s
(INHERITED FROM AEL_SOURCE):
lx_srcpos : source-position
Ix comments : comments
•* number id
IS INCLUDED IN:
INIT OBJECT NAME
OBJECT NAME-
SOURCE NAME
DEF NAME
ALL SOURCE
NODE ATTRIBUTES:
(INhERITED FROM INIT OBJECTNAME):
sm init exp : EXP
(INHERITED FROM OBJECTNAME):
sm obj type : TYPESPEC
(INHERITED FROM DEF NAME):
Ix symrep : symbol_rep
(INHERITED FROM ALL SOURCE):
1x srcpos : source position
Ix comments : comments
** numeric literal
IS INCLUDED IN:
EXP VAL
EXP EXP
-' EXP
GENERAL ASSOC
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
lx numrep : numberrep
(INHERITED FROM EXP_VAL):
DIANA Reference Manual Draft Revision 4 Page A_87
DIANA CROSS_REFERENCE GUIDE
sm value : value
(INHERITED_FROM EXPEXP):
sm exp type : TYPE_SPEC
(INHERITED FROM ALL SOURCE):
lx srcpos : sourceposition
lx comments : comments
* OBJECTDECL
CLASS MEMBERS:
constant decl
variable decl
IS INCLUDED IN:
EXP DECL
ID ý DECL
DECL
ITEM
ALL DECL
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
as typedef : TYPEDEF
(INHERITED FROM EXPDECL):
as_exp : EXP
(INHERITED FROM IDSDECL):
as source name s : sourcename s
(INHERITED FROM ALL SOURCE):
lx_srcpos : sourceposition
lx comments : comments
** OBJECTNAME
CLASS MEMBERS:
INIT OBJECT NAME
ENUMLITERAL
iteration id
VC NAME
number id
COMP_NAME
PARAM NAME
enumeration id
character id
variable id
constant id
component_id
discriminant_id
in id
out id
in out id
IS INCLUDED IN:
SOURCE NAME
DEF NAME
ALL SOURCE
NODE ATTRIBUTES:
DIANA Reference Manual Draft Revision 4 Page A_88
DIANA CROSS_REFERENCE GUIDE
(NODE SPECIFIC):
sm obj type : TYPE SPEC
(INHERITED FROM DEFNAME):
lx symrep : symbolrep
(INHERITED FROM ALLSOURCE):
Ix srcpos : sourceposition
Ixxcomments : comments
** operator_id
"IS INCLUDED IN:
SUBPROG NAME
"SUBPROG_PACK NAME
NON TASK NAME
UNIT NAME
SOURCE NAME
DEF NAME
ALL SOURCE
NODE ATTRIBUTES:
* '(INHERITED FROM SUBPROGNAME):
sm is inline : Boolean
"sm interface : PREDEF NAME
., (INHERITED FROM SUBPROGPACK_NAME):
sm unit desc : UNIT DESC
sm address : EXP
(INHERITED_FROM NONTASKNAME):
smspec : HEADER
(INHERITED FROM UNITNAME):
sm first : DEF _NAME
(INHERITED FROM OEF__NAME):
lx symrep : symbol_rep
(INHERITED FROM ALLSOURCE):
lxsrcpos : sourceposition
Ix comments : comments
** or-else
IS INCLUDED IN:
SHORT CIPCUIT OP
ALL SOURCE
NODE ATTRIBUTES:
(INHERITED FROM ALLSOURCE):
lx srcpos : source_position
Ix comments : comments
** out
IS INCLUDED IN:
"PARAM
DSCRMT PARAMDECL
ITEM
ALL DECL
ALL_SOURCE
NODE ATTRIBUTES:
DIANA Reference Manual Draft Revision 4 Page A_89
DIANA CROSS_REFERENCE GUIDE
(INHERITED FROM DSCRMTPARAMDECL):
assource_names sourcenames
as exp EXP
as_name NAME
(INHERITED FROM ALL SOURCE):
lx_srcpos : source position
Ix comments : comments
** outId
IS INCLUDED IN:
PARAM NAME
INIT 6BJECT NAME
OBJECT NAME-
SOURCENAME
DEF NAME
ALL SOURCE
NODE ATTRIBUTES:
(INHERITED FROM PARAM NAME):
sm first : DEF NAME
(INHERITED FROM INITOBJECT NAME):
sm init exp : EXP
(INHERITED FROM OBJECT_NAME):
sm obj type : TYPESPEC
(INHERITED FROM DEF NAME):
Ix symrep : symbol_rep
(INHERITED FROM ALLSOURCE):
Ix_srcpos : sourceposition
Ix_comments : comments
** package body
IS INCLUDED IN:
SUBUNIT BODY
ITEM
ALL DECL
ALL SOURCE
NODE ATTRIBUTES:
(INHERITED FROM SUBUNIT_BODY):
as source name : SOURCE NAME
as body - : BODY
(INHERITED FROM ALLSOURCE):
lx srcpos : source_position
lx comments : comments
** package decl
IS INCLUDED IN:
NON GENERIC DECL
UNIT DECL
ID DECL
DECL
ITEM
ALLDECL
DIANA Reference Manual Draft Revision 4 Page A_90
DIANA CROSS_REFERENCE GUIDE
ALL SOURCE
NODE ATTRIBUTES:
(INHERITED FROM NON GENERICDECL):
as unit kind : UNIT_KIND
(INHERITED FROM UNITDECL):
as header : HEADER
(INHERITED_FROM ID_DECL):
as source name : SOURCE NAME
(INHERITED_FROM ALL SOURCE):
lx srcpos : sourceposition
Ix comments : comments
** packageid
IS INCLUDED IN:
SUBPROG PACK NAME
NON TASK NAME
"UNIT NAME
SOURCE NAME
DEF NAME
ALL SOURCE
NODE ATTRIBUTES:
(INHERITED FROM SUBPROGPACKNAME):
sm unit desc : UNIT DESC
sm address : EXP
(INHERITED FROM NONTASKNAME):
sm_spec : HEADER
(INHERITED FROM UNITNAME):
sm first : DEF NAME
(INHERITED FROM DEF_NAME):
lx_symrep : symbol_rep
(INHERITED FROM ALL SOURCE):
Ix srcpos : sourceposition
lx comments : comments
** packagespec
IS INCLUDED IN:
HEADER
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
as decl sl : decl s
as decl s2 : decl-s
(INHERITED_FROM-ALLSOURCE):
1x srcpos : sourceposition
Ix comments : comments
** PARAM
CLASS MEMBERS:
in
in out
out
DIANA Reference Manual Draft Revision 4 Page A_91
DIANA CROSS_REFERENCE GUIDE
IS INCLUDED IN:
DSCRMT PARAMDECL
ITE4
ALL DECL
ALL SOURCE
NODE ATTRIBUTES:
(INHERITED FROM DSCRMTPARAMDECL):
as source name s source name s
as exp EXP
as_name : NAME
(INHERITED_FROM ALL SOURCE):
lx_srcpos : sourceposition
Ix comments : comments
IS THE DECLARED TYPE OF:
param s.as_list [Seq Of]
** PARAM NAME
CLASS MEMBERS:
in id
out id
in out id
IS INCLUDED IN:
INIT OBJECT NAME
OBJECT NAME-
SOURCE_NAME
DEF NAME
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
sm first DEF NAME
(INHERITED FROM INITOBJECTNAME):
sm init exp : EXP
(INHERITED FROM OBJECT_1N.,-r):
smiobj type : TYPESPEC
(INHERITED FROM DEFNAME):
1x_symrep : symbolrep
(INHERITED FROM ALLSOURCE):
lx_srcpos : source _position
Ix comments : comments
Sparam-s
IS INCLUDED IN:
SEQUENCES
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
as list : Seq Of PARAM
(INHERITED FROM ALLSOURCE):
lx_srcpos : source_position
lx comments : comments
IS THE DECLARED TYPE OF:
accept.asparam_s
DIANA Reference Manual Draft Revision 4 Page A_92
DIANA CROSS_REFERENCE GUIDE
SUBPENTRYHEADER.as_param_s
** parenthesized
IS INCLUDED IN:
EXP VAL EXP
EXP_VAL
EXP_EXP
EXP
"GENERAL ASSOC
ALL SOURCE
NODE ATTRIBUTES:
(INHERITED FROM EXPVALEXP):
as_exp : EXP
(INHERITED FROM EXPVAL):
sm value : value
(INHERITED FROM EXPEXP):
smexp type : TYPE SPEC
(INHERITED FROM ALLSOURCE):
lx srcpos : sourceposition
"pram Ix comments : comments
**pragma
IS INCLUDED IN:
USE PRAGMA
DECL
ITEM
ALL DECL
ALL_SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
as_used_name_id : used_name_id
as general assoc s : generalassocS
(INHERITED FROM ALL_SOURCE):
lx_srcpos : sourceposition
lx comments : comments
IS THE DECLARED TYPE OF:
como _rep pragma.aspragma
context-pragma.as pragma
pragmas.as_list rSeq Of]
select altpragma.as_pragma
alternative pragma.as_pragma
stm_pragma.as_pragma
variantpragma.aspragma
** pragma id
IS INCLUDED IN:
PREDEF NAME
DEF NAME
ALL_SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
DIANA Reference Manual Draft Revision 4 Page A_93
DIANA CROSS_REFERENCE GUIDE
sm_argument-ids :argument-id S
(INHERITED FROM DEF_NARE):
lx symrep :symbol rep
(INHERITED FROM ALL_SOURCE):
lx_srcpos :source_position
lx comments :comments
**pragia-s
IS INCLUDED IN:
SEQUENCES
ALL_SOURCE
M_ODE ATTRIBUTES:
(NODE SPECIFIC):
as list : Seq Of pragma
(INHERITED FROM ALLSOURCE):
lx_srcpos : source position
Ix comments : comments
IS THE DECLARED TYPE OF:
al ignment.as_pragna_S
compilation_unit.as_pragmaS
labeied.as_pragma-s
compli1St. aspragna_s
**PREDEF NAME
CLASS MEMBERS:
attribute-id
void
bi tn -operator_id
argument id
pragmd id
IS INCLUDED IN:
DEF NAME
ALL SOURCE
NODE ATTRIBUTES:
(INHERITED FROM DEF NAME):
lx symrep :symbol ret3
(INHERITEO FROM ALLSOURCE):
ix_srcpos : sourCe_posit_nn
Ix comments : comments
IS THE DECLARED TYPE OF:
SUBPROG NAME. sm_interf ace
**private
IS INCLUDED IN:
PRIVATE SPEC
DERIVABLE SPEC
TYPESPEC
NODE ATTRIBUTES:
(INHERITED FROM PRIVATE_SPEC):
sm_discriminants : dscrmt deci s
SM-type_spec : TYPE SPEC
DIANA Reference Manual Draft Revision 4 Page A_94
DIANA CROSS_REFERENCE GUIDE
(INHERITED FROM DERIVABLESPEC):
sm derived : TYPE SPEC
sm_is anonymous : Boolean
** private_def
IS INCLUDED IN:
TYPE DEF
ALL SOURCE
. NODE ATTRIBUTES:
(INHERITED FROM ALL SOURCE):
1x_srcpos : source_psition
Ix comments : comments
** PRIVATE SPEC
CLASS MEMBERS:
private
i private
IS INCLUDED IN:
DERIVABLE SPEC
TYPE SPEC
NODE ATTRIBUTES:
(NODE SPECIFIC):
sm discriminants : dscrmt decl s
smntype spec : TYPE_SPEC -
(INHERITED FROM DERIVABLESPEC):
smnderived : TYPE SPEC
sm isanonymous : Boolean
** private typeid
IS INCLUDED IN:
TYPE NAME
SOURCE NAME
DEF NAME
ALLSOURCE
NODE ATTRIBUTES:
(INHERITED FROM TYPENAME):
sm typespec : TYPESPEC
(INHERITED FROM DEF NAME):
lx symrep : symbol_rep
(INHERITED FROM ALL SOURCE):
Ix srcpos : sourcejposition
lx comments : comments
** procedurecall
IS INCLUDED IN:
CALL STM
STMIITHNAME
STM
STM ELEM
ALL_SOURCE
DIANA Reference Manual Draft Revision 4 Page A_95
DIANA CROSS_REFERENCE GUIDE
NODE ATTRIBUTES:
(INHERITED FROM CALLSTM):
as_general _assocs : generalassoc s
sm normalized param s : exp_s
(INHERITED_FROM STM_WITH_NAME):
as_name : NAME
(INHERITED FROM ALL SOURCE):
1x_srcpos : sourceposition
Ix comments : comments
** procedure id
IS INCLUDED IN:
SUBPROG NAME
SUBPROG_PACKNAME
NON TASK NAME
UNIT NAME
SOURCE NAME
DEF NAME
ALL SOURCE
NODE ATTRIBUTES:
(INHERITED FROM SUBPROGNAME):
sm is inline : Boolean
sm_interface : PREDEFNAME
(INHERITED FROM SUBPROG PACK NAME):
sm unit desc : UNIT DESC
sm address : EXP
(INHERITED FROM NONTASK_NAME):
sm spec : HEADER
(INHERITED FROM UNITNAME):
sm first : DEF NAME
(INHERITED FROM DEFNAME):
lx symrep : symbol _rep
(INHERITED FROM ALLSOURCE):
lx_srcpos : source position
Ix comments : comments
** procedure_soec
IS INCLUDED IN:
SUBP ENTRYHEADER
HEADER
ALL SOURCE
NODE ATTRIBUTES:
(INHERITED FROM SUBPENTRY HEADER):
as_param_s : param_s
(INHERITED FROM ALLSOURCE):
lx_srcpos : sourceposition
lx comments : comments
** QUAL_CONV
CLASS MEMBERS:
conversion
DIANA Reference Marual Draft Revision 4 Page A_96
DIANA CROSS_REFERENCE GUIDE
qualified
IS INCLUDED IN:
EXP VAL EXP
EXP_VAL
EXP EXP
EXP
GENERAL ASSOC
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
as_name : NAME
(INHERITED FROM EXPVALEXP):
as_exp : EXP
(INHERITED FROM EXPVAL):
sm value : value
(INHERITED FROM EXP_EXP):
smexp_type : TYPE_SPEC
(INHERITED FROM ALLSOURCE):
Ixsrcpos : sourceposition
Ix comments : comments
** qualified
IS INCLUDED IN:
QUAL CONV
EXP VAL EXP
EXP_VAL
EXP EXP
EXP
GENERAL ASSOC
ALL SOURCE
NODE ATTRIBUTES:
(INHERITED FROM QUALCONV):
as_name : NAME
(INHERITED FROM EXPVALEXP):
as_exp : EXP
(INHERITED FROM EXP_VAL):
sm value : value
(INHERITED FROM EXPEXP):
sm exptype : TYPESPEC
(INHERITED FROM ALL_SOURCE):
lx srcpos : sourceposition
lx comments : comments
IS THE DECLARED TYPE OF:
qualifiedallocator.as_qualified
** qualified_allocator
IS INCLUDED IN:
EXP EXP
EXP
GENERAL ASSOC
ALL SOURCE
NODE ATTRIBUTES:
DIANA Reference Manual Draft Revision 4 Page A_97
DIANA CROSS_REFERENCE GUIDE
(NODE SPECIFIC):
asqualified : qualified
(INHERITED FROM EXPEXP):
sm exp type : TYPE SPEC
(INHERITED_FROM ALLSOURCE): I
lx_srcpos : source_position
Ix comments : comments
** raise
IS INCLUDED IN:
STM WITH NAME
STM
STM ELEM
ALL SOURCE
NODE ATTRIBUTES:
(INHERITED FROM STMWITH_NAME):
as_name : NAME
(INHERITED FROM ALLSOURCE):
lx_srcpos : sourceposition
Ix comments comments
** RANGE
CLASS MEMBERS:
range
void
rangeattribute
IS INCLUDED IN:
DISCRETE RANGE
CONSTRAINT
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
sm_type_spec : TYPESPEC
(INHERITED FROM ALLSOURCE):
lx_srcpos source_position
lx comments . comments
IS THE DECLARED TYPE OF:
camp rep.as_range
range membership.as_range
REAL CONSTRAINT.asrange
SCALAR.smrange
** range
IS INCLUDED IN:
RANGE
DISCRETE RANGE
CONSTRAINT
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
as_exp1 . EXP
DIANA Reference Manual Draft Revision 4 Page A_98
DIANA CROSS_REFERENCE GUIDE
asexp2 : EXP
(INHERITED FROM RANGE):
"sm type spec : TYPESPEC
(INHERITED FROM_ALLSOURCE):
lx srcpos : sourceposition
Ix comments : comments
** range-attribute
IS INCLUDED IN:
RANGE
DISCRETE RANGE
CONSTRAINT
"ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
as_name : NAME
as_exp : EXP
as_used name id : used_name_id
(INHERITED FROM RANGE):
sm type spec : TYPESPEC
(INHERITED FROM ALLSOURCE):
lx_srcpos : sourceposition
Ix comments : comments
** range-membership
IS INCLUDED IN:
MEMBERSHIP
EXP VAL EXP
EXP_VAL
EXP EXP
EXP
GENERAL ASSOC
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
as range : RANGE
(INHERITED FROM MEMBERSHIP):
as membership_op : MEMBERSHIPOP
(INHERITED_FROM EXPVALEXP):
as exp : EXP
(INHERITED FROM EXPVAL):
sm value : value
(INHERITED FROM EXPEXP):
smexptype : TYPE_SPEC
(INHERITED FROM ALLSOURCE):
Ix_srcpos : source position
lx comments : comments
** REAL
CLASS MEMBERS:
float
__ __ _
DIANA Reference Manual Draft Revision 4 Page A_99
DIANA CROSS_REFERENCE GUIDE
fixed
IS INCLUDED IN:
SCALAR
NON TASK
FULL TYPE SPEC
DERIVABLE_SPEC
TYPE SPEC
NODE ATTRIBUTES:
(NODE SPECIFIC):
sm accuracy : value
(INHERITED_FROM SCALAR):
sm range : RANGE
cd impl size : Integer
(INHERITED_FROM-NONTASK):
smbase_type : TYPE_SPEC
(INHERITED FROM DERIVABLESPEC):
sm derived : TYPE SPEC
smnis anonymous : Boolean
** REALCONSTRAINT
CLASS MEMBERS:
float constraint
fixed constraint
IS INCLUDED IN:
CONSTRAINT
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
smtype_spec : TYPESPEC
as_exp : EXP
as range : RANGE
(INHERITED FROM ALLSOURCE):
Ix_srcpos : source_position
lx comments : comments
** record
IS INCLUDED IN:
UNCONSTRAINEDCOMPOSITE
UNCONSTRAINED
NON TASK
FULT TYPE SPEC
DERIVABLE_SPEC
TYPE SPEC
NODE ATTRIBUTES:
(NODE SPECIFIC):
sm discriminant s : dscrmt declis
sm representation : REP
sm comp_list : comp_list
(INHERITED_FROM UNCONSTRAINEDCOMPOSITE):
sm is limited : Boolean
sm is packed : Boolean
(INHERITED_FROM UNCONSTRAINED):
DIANA Reference Manual Draft Revision 4 Page A_100
DIANA CROSS_REFERENCE GUIDE
sm size : EXP
(INHERITED FROM NON_TASK):
sm basetype : TYPE_SPEC
(INHERITED FROM DERIVABLE SPEC):
sm derived : TYPE SPEC
sm isanonymous : Boolean
** record def
IS INCLUDED IN:
TYPE DEF
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
as complist : complist
(INHERITED FROM ALL SOURCE):
lx_srcpos : sourceposition
Ix comments : comments
** recordrep
IS INCLUDED IN:
REP
DECL
ITEM
ALL DECL
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
asalignmentclause : ALIGNMENTCLAUSE
ascomp_rep_s : comp_rep_s
(INHERITED FROM REP):
as_name : NAME
(INHERITED FROM ALLSOURCE):
lx_srcpos : source position
lx comments : comments
** RENAME INSTANT
CLASS MEMBERS:
renames-unit
instantiation
IS INCLUDED IN:
UNITKIND
UNIT DESC
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
as_name : NAME
(INHERITED FROM ALLSOURCE):
Ixsrcpos : sourceposition
lx comments : comments
** renames exc decl
DIANA Reference Nanual Draft Revision 4 Page A_1O1
DIANA CROSS_REFERENCE GUIDE
IS INCLUOED IN:
SIMPLE RENAMEDECL
ID DECE
DECL
ITEM
ALL DECL
ALL SOURCE
NODE ATTRIBUTES:
(INHERITED FROM SIMPLERENAMEDECL):
as_name : NAME
(INHERITED FROM ID DECL):
as source name : SOURCE NAME
(INHERITED_FROM AELLSOURCE):
1x_srcpos : sourceposition
lx comments : comments
** renamesobj_decl
IS INCLUDED IN:
SIMPLE RENAMEDECL
ID DECE
DECL
ITEM
ALL DECL
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
astype mark name : NAME
(INHERITED FROM SIMPLE_RENAMEDECL):
as_name : NAME
(INHERITED FROM ID DECL):
as source name : SOURCE NAME
(INHERITED FROM AEL_SOURCE):
lx_srcpos : source_position
Ix_comments : comments
I
** renames-unit
IS INCLUDED IN:
RENAME INSTANTUNIT KIND
UNIT DESC
ALL SOURCE
NODE ATTRIBUTES:
(INHERITED FROM RENAMEINSTANT):
as_name : NAME
(INHERITED FROM ALL SOURCE):
lx_srcpos : source_positionlx comments : comments
** REP
CLASS MEMBERS:
I
!
DIANA Reference Manual Draft Revision 4 Page A_I02
DIANA CROSS_REFERENCE GUIDE
void
NAMED REP
record rep
lengthenum_rep
address
IS INCLUDED IN:
"DECL
ITEM
ALL DECL
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
as_name : NAME
(INHERITED_FROM ALLSOURCE):
lx srcpos : sourceposition
lx comments : comments
IS THE DECLARED TYPE OF:
record.smrepresentation
•* return
IS INCLUDED IN:
STM WITHEXP
STM
STM ELEM
ALL SOURCE
NODE ATTRIBUTES:
(INHERITED FROM STMWITHEXP):
as exp : EXP
(INHERITED FROM ALLSOURCE):
1x_srcpos : sourceposition
lx comments : comments
•* reverse
IS INCLUDED IN:
FORREV
ITERATION
ALL SOURCE
NODE ATTRIBUTES:
(INHERITED FROM FOR_REV):
as source name : SOURCE NAME
as discrete_range : DISCRETE_RANGE
(INHERITED FROM ALLSOURCE):
lx srcpos : source position
Ix comments : comments
-* ** SCALAR
CLASS MEMBERS:
enumeration
REAL
integer
float
DIANA Reference Ianual Draft Revision 4 Page A_103
DIANA CROSS_REFERENCE GUIDE
fixed
IS INCLUDED IN:
NON TASK
FULT TYPE SPEC
DERIVABLE_SPEC
TYPE SPEC
NODE ATTRIBUTES:
(NODE SPECIFIC):
sm_range : RANGE
cd impl_size : Integer
(INHERITED_FROM NONTASK):
smbasetype : TYPE SPEC
(INHERITED FROM DERIVABLE_SPEC):
sm derived : TYPE SPEC
sm_is anonymous : Boolean
IS THE DECLARED TYPE OF:scalar_s.as list [Seq Of)
* scalar_s
IS INCLUDED IN:
SEQUENCES
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
as list : Seq Of SCALAR |
(INHERITED_FROM ALLSOURCE):
lx_srcpos : source position
lx comments : comments
IS THE DECLARED TYPE OF: _
constrained array.sm_index_subtype_s
** selectaltpragma
IS INCLUDED IN:
TEST CLAUSE ELEM
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
aspragma : pragma
(INHERITED FROM ALLSOURCE):
lx_srcpos : source_position
lx comments : comments
** select alternative
IS INCLUDED IN:
TEST CLAUSE
TEST CLAUSE ELEM
ALL SOURCE
NODE ATTRIBUTES:
(INHERITED FROM TEST CLAUSE):
as_exp : EXP
as stms : stms
I
DIANA Reference Manual Draft Revision 4 Page A_104
DIANA CROSS_REFERENCE GUIDE
(INHERITED FROM ALLSOURCE):
lx srcpos sourcejposition
1x comments : comments
** selected
IS INCLUDED IN:
NAME VAL
NAME_EXP
NAME
EXP
GENERAL ASSOC
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
as designator : DESIGNATOR
(INHERITED FROM NAMEVAL):
sm value : value
(INHERITED FROM NAMEEXP):
as_name : NAME
sm exp-type : TYPESPEC
(INHERITED FROM ALLSOURCE):
Ix_srcpos : sourceposition
lx comments : comments
** selective-wait
IS INCLUDED IN:
CLAUSESSTM
STM
STM ELEM
ALLSOURCE
NODE ATTRIBUTES:
(INHERITED FROM CLAUSESSTM):
as test clause elem s : test clause elems
as stm s : stm s
(INHERITED FROM ALLSOURCE):
Ix _srcpos : source position
Sx _comments : comments
"- ** SEQUENCES
CLASS MEMBERS:
alternative_s
variant s
usepragma_s
testclauseelem_s
stm_s
source name s
scalars
pragma s
param_s
name s
index s
I.-
DIANA Reference Manual Draft Revision 4 Page A_105
DIANA CROSS_REFERENCE GUIDE
item s
exp sI
enum_literal_s
discre:e_range_s
general _assocs
dscrmt -decl s
decl s
context elem s
compltn unit s
comp_rep_s
choice s
argument id s
IS INCLUDED IN:
ALL SOURCE
NODE ATTRIBUTES:
(INHERITED FROM ALLSOURCE):
lx srcpos : source_position
lx comments : comments
** short-circuit
IS INCLUDED IN:
EXP VAL
EXP_EXP
EXP
GENERAL ASSOC
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
asexpl : EXP
as exp2 : EXP
as short circuit op : SHORTCIRCUITOP
(INHERITED_FROM EXPVAL)-:
sm value : value
(INHERITED_FROM EXPEXP):
sm exp type : TYPESPEC
(INHERITED_FROM ALL SOURCE):
lxsrcpos : sourceposition
ix comments : comments
** SHORTCIRCUIT OP
CLASS MEMBERS:
and then
or else
IS INCLUDED IN:
ALL SOURCE
NODE ATTRIBUTES:
(INHERITED FROM ALLSOURCE):
lxsrcpos : sourceposition
Ix comments : comments
IS THE DECLARED TYPE OF:
short circuit.asshortcircuitop
DIANA Reference Manual Draft Revision 4 Page A_106
DIANA CROSS_REFERENCE GUIDE
** SIMPLERENAME DECL
CLASS MEMBERS:
renamesobjdecl
renames exc decl
IS INCLUDED IN:
ID DECL
DECL
ITEM
ALL DECL
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
as_name : NAME
(INHERITED FROM ID_DECL):
as source name : SOURCENAME
(INHERITED FROM ALL SOURCE):
1x_srcpos : sourceposition
Ix comments : comments
** slice
IS INCLUDED IN:
NAME EXP
NAME
EXP
GENERAL ASSOC
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
as discreterange : DISCRETERANGE
(INHERITED FROM NAMEEXP):
as_name : NAME
sm exptype : TYPESPEC
(INHERITED FROM A!-LLSCURCE):
lxsrcpos : source position
lx comments : comments
** SOURCENAME
CLASS MEMBERS:
OBJECT NAME
LABEL NAME
UNIT NAME
TYPE NAME
void
entry_id
exception id
INIT OBJECT NAME
ENUM_LTERAL
iteration id
label id
block loopid
NONTASKNAME
DIANA Reference Manual Draft Revision 4 Page A_1O1
DIANA CiOSS-REFERENCE GUIDE
task body_id
typeid
subtype id
private-type_id
1_private_type_id
VC NAME
number id
COMP_NAME
PARAR NAME
enumeration id
character id
SUBPROG PACK NAME
generic-id -
variable id
constant id
component_id
discriminant_id
in id
out id
in out id
SUBPROG NAME
packaqe id
procecure id
operator id
function id
IS INCLUDED IN:
DEF NAME
ALL SOURCE
NODE ATTRIBUTES:
(INHERITED FROM DEFNAME):
Ix_symrep : symbol_rep
(INHERITED FROM ALLSOURCE):
Ix_srcpos : sourceposition
Ix comments : comments
IS THE DECLARED TYPE OF:
SUBUNIT BODY.as source-name
implicit noteq-.sm_equal
derived-subprog.smderivable
FOR REV.as source name
BLOCKLOOP.as source name
source name s.as list [Seq Of]
IDDECL.as source name
** source name s
IS INCLUDED IN:
SEQUENCES
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
as list : Seq Of SOURCENAME
(INHERITED FROM ALLSOURCE):
Ix_srcpos : sourceposition
lx comments : comments
DIANA Reference Manual Draft Revision 4 Page A_108
DIANA CROSS_REFERENCE GUIDE
IS THE DECLARED TYPE OF:
labeled.as source name s
DSCRMT PARAMDECL.as source name s
ID_S_DECL.as source name s
** STM
CLASS MEMBERS:
labeled
null stm
abort
STM WITH EXP
STM- WITH_NAME
accept
ENTRY STM
BLOCK LOOP
CLAUSES STM
terminate
return
delay
STM WITHEXPNAME
case
goto
raise
CALL STM
cond entry
timed entry
loop
block
if
selective wait
assign
code
exit
entrycall
procedurecall
IS INCLUDED IN:
STM ELEM
ALL SOURCE
NODE ATTRIBUTES:
(INHERITED FROM ALL SOURCE):
Ix_srcpos : sourceposition
lx comments : comments
IS THE DECLARED TYPE OF:
block master.sm stm
exit.Sm stm
LABEL NAME.sm stm
labeled.as stm
** STM ELEM
CLASS MEMBERS:
STM
stmpragma
DIANA Reference Manual Draft Revision 4 Page A_109
DIANA CROSS_REFERENCE GUIDE
labeled
null stm.
abort
STM WITH EXP
STMWITHNAME
accept
ENTRY STM
BLOCK LOOP
CLAUSES STM
terminate
return
delay
STM WITHEXPNAME
case
goto
raise
CALL STM
cond-entry
timed entry
loop
block
if
selective wait
assign
code
exit
entry call
procedure call
IS INCLUDED IN:
ALL SOURCE
NODE ATTRIBUTES:
(INHERITED FROM ALLSOURCE):
lx srcoos : sourceposition
Ix comments : comments
IS_THE DECLARED TYPE OF:
stm s.as list [Seq Of]
** stm-pragma
IS INCLUDED IN:
STM ELEM
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
aspragma : pragma
(INHERITED FROM ALLSOURCE):
lx_srcpos : source_position
lx comments : comments
** stms
IS INCLUDED IN:
SEQUENCES
ALLSOURCE
DIANA Reference Manual Draft Revision 4 Page A_110
DIANA CROSS_REFERENCE GUIDE
NODE ATTRIBUTES:
(NODE SPECIFIC):
as list : Seq Of STM_ELEM
(INHERITED FROM ALLSOURCE):
Ix_srcpos : source position
lx comments : comments
IS THE DECLARED TYPE OF:
ENTRY STM.as stm sl
*.as_stm-s2
accept.as stm s
blockbody.as_stm-s
loop.as stm s
alternative.as stms
TEST CLAUSE.as_stm s
CLAUSESSTM.as_stm-s
** STM WITHEXP
CLASS MEMBERS:
return
delay
STM WITHEXP NAME
case
assign
code
exit
IS INCLUDED IN:
STM
STM ELEM
ALLSOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
as exp : EXP
(INHERITED FROM ALLSOURCE):
Ixsrcpos : sourceposition
lx_comments : comments
SSTMWITHEXPNAME
CLASS MEMBERS:
assign
code
exit
IS INCLUDED IN:
STM WITHEXP
STM
STM ELEM
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
as_name : NAME
(INHERITED FROM STMWITHEXP):
as_exp : EXP
(INHERITED FROM ALLSOURCE):
DIANA Reference Manual Draft Revision 4 Page A_111
DIANA CROSS_REFERENCE GUIDE
lxsrcpos : sourceposition
lx comments : comments
** STM WITH NAME
CLASS MEMBERS:
goto
raise
CALL STM
entry call
procedure call
IS INCLUDED IN:
STM
STM ELEM
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
as_name : NAME
(INHERITED_FROM ALLSOURCE):
lx_srcpos : source_position
]x comments : comments
** string-literal
IS INCLUDED IN:
AGG EXP
EXP_EXP
EXP
GENERAL ASSOC
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
lx_symrep : symbolrep
(INHERITED FROM AGG EXP):
sm discrete range : DISCRETERANGE
(INHERITED_FROM EXPEXP):
smexptype : TYPESPEC
(INHERITED FROM ALLSOURCE):
lx_srcpos : sourceposition
Ix comments : comments
** stub
IS INCLUDED IN:
BODY
UNIT DESC
ALL SOURCE
NODE ATTRIBUTES:
(INHERITED FROM ALLSOURCE):
lx_srcpos : sourceposition
Ix comments : comments
** SUBPENTRYHEADER
DIANA Reference Manual Draft Revision 4 Page A_112
DIANA CROSS_REFERENCE GUIDE
CLASS MEMBERS:
procedurespec
functior spec
entry
IS INCLUDED IN:
HEADER
ALL SOUkCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
asparam s : param-s
(INHERITED FROM ALLSOURCE):
Ix srcpos : sourceposition
Ixcomments : comments
subprog entrydecl
IS INCLUDED IN:
NON GENERIC DECL
UNIT DECL
ID DECL
DECL
ITEM
ALL DECL
ALL SOURCE
NODE ATTRIBUTES:
(INHERITED FROM NON GENERICDECL):
as unit kind : UNIT KIND
(INHERITED FROM UNIT DECL):
as header : HEADER
(INHERITED FROM IDOECL):
as source name : SOURCENAME
(INHERITED FROM ALL_SOURCE):
lx_srcpos : sourceposition
lx comments : comments
** SUBPROG NAME
CLASS MEMBERS:
procedure id
operator_id
"function id
IS INCLUDED IN:
SUBPROG PACK NAME
NON TASK NAME
"UNIT NAME
SOURCE NAME
DEF NAME
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
sm is inline : Boolean
sm interface : PREDEF NAME
(INHERITED FROM SUBPROGPACK_NAME):
sm unit desc : UNITDESC
DIANA Reference Manual Draft Revision 4 Page A_113
DIANA CROSS_REFERENCE GUIDE
sm address : EXP
(INHERITED FROM NONTASKNAME):
sm spec : HEADER
(INHERITED_FROM UNITNAME):
sm first : DEF NAME
(INHERITED_FROM DEFNAME):
Ix symrep : symbolrep
(INHERITED FROM ALLSOURCE):
lx srcpos : source position
Ix comments : comments
** SUBPROG PACK NAME
CLASS MEMBERS:
SUBPROG NAME
packageiid
procedureid
operator id
function id
IS INCLUDED IN:
NON TASK NAME
UNIT NAME
SOURCE NAME
DEF NAME
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
sm unit desc : UNIT DESC
sm address : EXP
(INHERITED FROM NONTASK_NAME):
sm_spec : HEADER
(INHERITED FROM UNITNAME):
sm first : DEF NAME
(INHERITED FROM DEFNAME):
lx symrep : symbolrep
(INHERITED FROM ALL SOURCE):
lx_srcpos : source position
Ix comments : comments
** subprogram_body
IS INCLUDED IN:
SUBUNIT 900Y
ITEM
ALL DECL
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
as header : HEADER
(INHERITED FROM SUBUNITBODY):
as source name : SOURCE NAME
as body : BODY
(INHERITED FROM ALLSOURCE):
lx_srcpos : sourcejosition
DIANA Reference Manual Draft Revision 4 Page A_114
DIANA CROSS_REFEPENCE GUIDE
Ix comments comments
** subtypeallocator
IS INCLUDED IN:
EXP3EXP
EXP
GENERAL ASSOC
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
as subtype indication : subtype indication
smndesig type : TYPE_SPEC
(INHERITED FROM EXPEXP):
sm exp_type : TYPE_SPEC
(INHERITED FROM ALL SOURCE):
Ix srcpos : source position
"lx comments : comments
** subtype_decl
IS INCLUDED IN:
ID DECL
DECL
ITEM
ALL DECL
ALLSOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
as subtypeindication : subtypeindication
(INHERITED FROM IDDECL):
as source name : SOURCE NAME
(INHERITED FROM ALL SOURCE):
lx_srcpos : source position
lx comments : comments
** subtypeid
IS INCLUDED IN:
TYPE NAME
SOURCE NAME
DEF NAME
ALL SOURCE
NODE ATTRIBUTES:
(INHERITED FROM TYPENAME):
sm_type_spec : TYPE_SPEC
(INHERITED FROM DEFNAME):
lx_symrep : symbolrep
(INHERITED FROM ALLSOURCE):
Ix_srcpos : sourceposition
lx comments : comments
** subtypeindication
DIANA Reference Manual Draft Revision 4 Page A_115
DIANA CROSS_REFERENCE GUIDE
IS INCLUDED IN:
CONSTRAINEDDEF
TYPE DEF
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
as_name : NAME
(INHERITED FROM CONSTRAINEDDEF):
as constraint : CONSTRAINT
(INHERITED FROM ALL SOURCE):
lx_srcpos : sourceposition
lx comments : comments
IS THE DECLARED TYPE OF:
subtype allocator.as subtype indication
discrete subtype.as_subtypeT indication
subtypedecl.as subtypeindication
ARRACCDERDEF.as_subtypeindication
** subunit
IS INCLUDED IN:
ALL DECL
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
as_name : NAME
as_subunit body : SUBUNIT_BODY
(INHERITED FROM ALL_SOURCE).:
Ix_srcpos : source position
Ix_comments : comments
** SUBUNIT BODY
CLASS MEMBERS:
subprogrambody
task body
packagebody
IS INCLUDED IN:
ITEM
ALL DECL
ALL_SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
as sourcename : SOURCE NAMEas_body : BODY-
(INHERITED FROM ALLSOURCE):
lx_srcpos : sourceposition
1x comments : comments
IS THE DECLARED TYPE OF:
subunit.as subunit body
** task-body
IS INCLUDED IN:
I
I
DIANA Reference Manual Draft Revision 4 Page A_116
DIANA CROSS_REFERENCE GUIDE
SUBUNIT BODY
ITEM
ALL DECL
ALL_SOURCE
NODE ATTRIBUTES:
(INHERITED FROM SUBUNITBODY):
as source name : SOURCE NAME
as body : BODY
(INHERITED_FROM ALL SOURCE):
lx srcpos : source position
Ix comments : comments
** task-body-id
IS INCLUDED IN:
UNIT NAME
SOURCE NAME
DEF NAME
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
sm typespec TYPE SPEC
sm body : BODY
(INHERITED FROM UNIT_NAME):
sm first : DEF NAME
(INHERITED FROM DEFNAME):
lx_symrep : symbolrep
(INHERITED FROM ALL SOURCE):
Ix_srcpos : source position
lx Comments : comments
** task decl
IS INCLUDED IN:
ID DECL
DECL
ITEM
ALL DECL
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
as decl s : decl s
(INHERITED FROM IDDECL):
as source name : SOURCE NAME
(INHERITED FROM ATLLSOURCE):
lx srcpos : sourceposition
Ix comments : comments
** taskspec
IS INCLUDED IN:
FULL TYPE SPEC
DERIVABLE_SPEC
TYPESPEC
DIANA Reference Manual Draft Revision 4 Page A_117
DIANA CROSS_REFERENCE GUIDE
NODE ATTRIBUTES:
(NODE SPECIFIC):
sm decl s : decl s
sm storagesize : EXP
sm siie : EXP
sm address : EXP
sm_body : BODY
(INHERITED_FROM DERIVABLE_SPEC):
sm derived : TYPE SPEC
sm_isanonymous : Boolean
** terminate
IS INCLUDED IN:
STM
STM ELEM
ALL SOURCE
NODE ATTRIBUTES:
(INHERITED FROM ALL SOURCE):
lx_srcpos : sourcepositionIx comments : comments
** TEST_CLAUSE
CLASS MEMBERS:
cond clause
select alternative
IS INCLUDED IN:
TEST CLAUSE ELEM
- ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
as_exp : EXP
as stms : stms
(INHERITED_FROR ALLSOURCE):
Ix_srcpos : source_position
lx comments : comments
** TESTCLAUSEELEM
CLASS MEMBERS:
TEST CLAUSE
select alt_pragmd
cond clause
select alternative
IS INCLUDED INT
ALL SOURCE
NODE ATTRIBUTES:
(INHERITED FROM ALLSOURCE):
lx srcpos : sourceposition
lx comments : comments
IS THE DECLARED TYPE OF:
testclauseelems.aslist [Seq Of)
DIANA Reference Manual Draft Revision 4 Page A_118
DIANA CROSS_REFERENCE GUIDE
** test clause elem s
"IS INCLUDED IN:
SEQUENCES
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
as list : Seq Of TESTCLAUSEELEM
(INHERITED FROM ALL SOURCE):
Ix_srcpos : sourceposition
lx comments : comments
IS THE DECLARED TYPE OF:
CLAUSES STM.as test clause elem s
** timed-entry
IS INCLUDED IN:
ENTRY STM
STM
STM ELEM
ALL SOURCE
NODE ATTRIBUTES:
(INHERITED FROM ENTRYSTM):
as stm s1 : stm s
as_stm-s2 : stm-s
(INHERITED_FROM ALLSOURCE):
Ix_srcpos : sourceposition
lx comments : comments
** type_deci
IS INCLUDED IN:
ID0DECL
DECL
ITEM
ALL DECL
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
as dscrmt decl s : dscrmt decl s
as typedef - : TYPE DEF -
(INHERITED FROM ID_OECL):
as source name : SOURCE_NAME
(INHERITED FROM ALL SOURCE):
lx_srcpos : sourceposition
lx comments : comments
** TYPE DEF
CLASS MEMBERS:
enumerationdef
record def
ARR ACC DER DEF
CONSTRATNED_DEF
DIANA Reference Manual Draft Revision 4 Page A_119
DIANA CROSS_REFERENCE GUIDE
void
private def
lIprivate def
formal dscrt def
formal float def
formal fixed def
formal integerdef
constrained_arraydef
derived def
access def
unconstrained arraydef
subtypeindication
integer-def
fixed def
float def
IS INCLUDED IN:
ALL SOURCE
NODE ATTRIBUTES:
(INHERITED FROM ALLSOURCE):
lx srcpos : source_position
lx comments : comments
IS THE DECLARE. TYPE OF:
typedecl.as_type def
OBJECTDECL.as_typedef
** type_id
IS INCLUDED IN:
TYPE NAME
SOURCE NAME
DEF NAME
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
sm first : DEFNAME
(INHERITED FROM TYPENAME):
sm type spec : TYPESPEC
(INHERITED FROM DEFNAME):
lx symrep : symbol_rep
(INHERITED FROM ALLSOURCE):
lx_srcpos : sourceposition
Ix comments : comments
** type-membership
IS INCLUDED IN:
MEMBERSHIP
EXP VAL EXP
EXP_VAL
EXP_EXP
EXP
GENERAL ASSOC
ALL SOURCE
NODE ATTRIBUTES:
DIANA Reference Manual Draft Revision 4 Page A_120
DIANA CROSS_REFERENCE GUIDE
(NODE SPECIFIC):
as_name NAME
(INHERITED FROM MEMBERSHIP):
as membershipop : MEMBERSHIPOP
(INHERITED FROM EXPVALEXP):
as exp : EXP
(INHERITED FROM EXPVAL):
sm value : value
-' (INHERITED FROM EXPEXP):
sm exp type : TYPE_SPEC
(INHERITED FROM ALLSOURCE):
lx srcpos : sourceposition
Ix comments : comments
* TYPENAME
CLASS MEMBERS:
type_id
subtype_id
privatetype id
1 private type id
IS INCLUDED IN: -
SOURCE NAME
DEF NAME
ALL SOURCE
.NODE ATTRIBUTES:
(NODE SPECIFIC):
sm_type spec : TYPESPEC
(INHERITED FROM DEF_NAME):
lx_symrep : symbol_rep
(INHERITED FROM ALLSOURCE):
lx_srcpos : source-position
Ix comments : comments
* TYPESPEC
CLASS MEMBERS:
DERIVABLE SPEC
incompiete
void
universal-integer
universal real
universal fixed
FULL TYPE SPEC
PRIVATE SPEC
taskspec
NON TASK
private
1 private
SCALAR
CONSTRAINED
UNCONSTRAINED
enumeration
REAL
DIANA Reference Manual Draft Revision 4 Page A_121
DIANA CROSS_REFERENCE GUIDE
integer
constrained_array
constrained access
constrained record
UNCONSTRAINEDCOMPOSITE
access
float
fixed
array
record
IS THE DECLARED TYPE OF;
task body_id.sm_typespec
PRIVATE SPEC.sm_type_spec
subtype allocator.sm_desigtype
EXP EXP.smexptype
USEDOBJECT.sm_exp_type
NAME EXP.sm_exp_type
constrained access.smdesigtype
access.sm desigtype
index.sm_type_spec
array.smcomptype
REAL CONSTRAINT.sm_type_spec
RANGE.sm_type_spec
NON TASK.smbasetype
DERTVABLE SPEC.smderived
TYPE NAME.sm_type spec
OBJECTNAME.sm_obj_type
- UNCONSTRAINED
CLASS MEMBERS:
UNCONSTRAINEDCOMPOSITE
access
array
record
IS INCLUDED IN:
NON TASK
FULL TYPE SPEC
DERIVABLE_SPEC
TYPE SPEC
NODE ATTRIBUTES:
(NODE SPECIFIC):
sm size : EXP
(INHERITED FROM NON TASK):
smbasetype : TYPESPEC
(INHERITED FROM DERIVABLE_SPEC):
sm derived : TYPESPEC
smiisanonymous : Boolean
** unconstrainedarray def
IS INCLUDED IN:
ARR ACC DER DEF
TYPEDEF
DIANA Reference Manual Draft Revision 4 Page A_122
DIANA CROSS_REFERENCE GUIDE
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
as index s index s
(INHERITED FROM ARR ACCDERDEF):
assubtype indication : subtypeindication
(INHERITED FROM ALLSOURCE):
Ix_srcpos : sourceposition
Ix comments : comments
** UNCONSTRAINED_COMPOSITE
CLASS MEMBERS:
array
record
IS INCLUDED IN:
UNCONSTRAINED
NON TASK
FULL TYPE SPEC
DERIVABLE_SPEC
TYPE SPEC
NODE ATTRIBUTES:
(NODE SPECIFIC):
sm is limited : Boolean
sm is packed : Boolean
(INHERITED FROM UNCONSTRAINED):
sm size : EXP
(INHERITED FROM NON TASK):
sm basetype : TYPESPEC
(INHERITEO FROM DERIVABLE SPEC):
sm derived : TYPE SPEC
sm_is anonymous : Boolean
** UNITDECL
CLASS MEMBERS:
generic decl
NON GENERIC DECL
subprogentrydecl
packagedecl
IS INCLUDED IN:
ID DECL
DECL
ITEM
ALL DECL
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
as header : HEADER
(INHERITED FROM IDDECL):
as source name : SOURCE_NAME
(INHERITED FROM ALL SOURCE):
lxsrcpos : source position
lx comments : comments
DIANA Reference Manual Draft Revision 4 Page A_123
DIANA CROSS_REFERENCE GUIDE
* UNIT DESC
CLASS MEMBERS:
UNIT KIND
derivedsubprog
implicit_noteq
BODY
void
RENAME INSTANT
GENERIZPARAM
block body
stub
renames unit
instantTation
name default
no default
box default
IS INCLUDED IN:
ALL SOURCE
NODE ATTRIBUTES:
(INHERITED FROM ALLSOURCE):
lx_srcpos : sourceposition
lx comments : comments
IS THE DECLARED TYPE OF:
SUBPROGPACKNAME.sm unit desc
** UNITKIND
CLASS MEMBERS:
void
RENAME INSTANT
GENERIC PARAM
renames-unit
instantiation
name default
no default
box default
IS INCLUDED IN:
UNIT DESC
ALL SOURCE
NODE ATTRIBUTES:
(INHERITED FROM ALLSOURCE):
lx_srcpos : sourceposition
lx comments : comments
IS THE DECLARED TYPE OF:
NONGENERICDECL.as unit kind
** UNIT NAME
CLASS MEMBERS:
NON TASK NAME
tasW bodyid
SUBPROGPACKNAME
DIANA Reference Manual Draft Revision 4 Page A_124
DIANA CROSS_REFERENCE GUIDE
genericId
SUBPROG NAME
package id
procedure id
operatorId
function id
IS INCLUDED IN:
SOURCE NAME
DEF NAME
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
sm first : DEF NAME
(INHERITED FROM DEF_NAME):
lx_symrep : symbolrep
(INHERITED FROM ALLSOURCE):
lx srcpos : sourceposition
lx comments : comments
** universal fixed
IS INCLUDED IN:
TYPE SPEC
NODE ATTRIBUTES:
** universal-integer
IS INCLUDED IN:
TYPESPEC
NODE ATTRIBUTES:
** universal real
IS INCLUDED IN:
TYPE SPEC
NODE ATTRIBUTES:
** use
IS INCLUDED IN:
USE PRAGMA
DECL
ITEM
ALL DECL
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
as_name s : name s
(INHERITED FROM ALLSOURCE):
lx srcpos : sourceposition
1x comments : comments
** USE PRAGMA
DIANA Reference Manual Draft Revision 4 Page A_125
DIANA CROSS_REFERENCE GUIDE
CLASS MEMBERS:
use
pragma
IS INCLUDED IN:
DECL
ITEM
ALL DECL
ALL SOURCE
NODE ATTRIBUTES:
(INHERITED FROM ALL SOURCE):
1x_srcpos : sourceposition
Ix comments : comments
IS THE DECLARED TYPE OF:
usepragmas.aslist (Seq Of]
** usepragma_s
IS INCLUDED IN:
SEQUENCES
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
as list : Seq Of USEPRAGMA
(INHERITED FROM ALLSOURCE):
1x_srcpos : source_position
lx comment; : comments
IS THE DECLARED TYPE OF:
with.asuse_pragma_s
** used-char
IS INCLUDED IN:
USED OBJECT
DESIGNATOR
NAME
EXP
GENERAL ASSOC
ALL SOURCE
NODE ATTRIBUTES:
(INHERITED FROM USEDOBJECT):
smexptype : TYPE SPEC
sm value : value
(INHERITED FROM DESIGNATOR):
sm defn : DEF NAME
Ix symrep : symbol-rep
(INHERITED FROM ALLSOURCE):
lx_srcpos : sourceposition
Ix comments : comments
** USED NAME
CLASS MEMBERS:
ised op
used_name_id
DIANA Reference Manual Draft Revision 4 Page A_126
DIANA CROSS_REFERENCE GUIDE
IS INCLUDED IN:
DESIGNATOR
NAME
EXP
GENERAL ASSOC
ALL SOURCE
NODE ATTRIBUTES:
(INHERITED FROM DESIGNATOR):
sm defn : DEF NAME
lx symrep : symbolrep
(INHERITED FROM ALLSOURCE): -
lx srcpos : sourceposition
lx comments : comments
IS THE DECLARED TYPE OF:
assoc.as used name
** used_name_id
IS INCLUDED IN:
USED NAME
DESIGNATOR
NAME
EXP
GENERAL ASSOC
ALL SOURCE
NODE ATTRIBUTES:
(INHERITED FROM DESIGNATOR):
sm defn : DEF NAME
Ix symrep : symbol-rep
(INHERITED FROM ALLSOURCE):
1x_srcpos : sourceposition
1x comments : comments
IS THE DECLARED TYPE OF:
attribute.as_used_name_id
rangeattribute.asusedname id
E O pragma.asusedname id
** USEDOBJECT
CLASS MEMBERS:
used char
used object id
IS INCLUDED IN:
DESIGNATOR
NAME
EXP
GENERAL ASSOC
ALL SOURCF
NODE ATTRIBUTES:
(NODE SPECIFIC):
smexp_type : TYPE SPEC
sm value : value
(INHERITED FROM DESIGNATOR):
sm defn : DEFNAME
DIANA Reference Manual Draft Revision 4 Page A_127
DIANA CROSS_REFERENCE GUIDE
lx_symrep : synbol_rep
(INHERITED FROM ALLSOURCE):
lx_srcpos : source position
1xcomments : comments
** used object-id
IS INCLUDED IN:
USED OBJECT
DESIGNATOR
NAME
EXP
GENERAL ASSOC
ALL SOURCE
NODE ATTRIBUTES:
(INHERITED FROM USEDOBJECT):
smexptype : TVPE SPEC
sm value : value
(INHERITED FROM DESIGNATOR):
sm defn : DEF NAME
lx symrep : symbol_rep
(INHERITED FROM ALLSOURCE):
lx_srcpos : source_position
lx comments : comments
** used op
IS INCLUDED IN:
USED NAME
DESIGNATOR
NAME
EXP
GENERAL ASSOC
ALL SOURCE
NODE ATTRIBUTES:
(INHERITED FROM DESIGNATOR):
sm defn : DEF NAME
Ix symreo : symbol-rep
(INHERITED FROM ALLSOURCE):
1x_srcpos : sourceposition
Ix comments : comments
** variable deci
IS INCLUDED IN:
OBJECT DECL
EXP DECL
ID S DECL
DECL
ITEM
ALL DECL
ALL SOURCE
NODE ATTRIBUTES:
(INHERITED FROM OBJECTDECL):
DIANA Reference Manual Draft Revision 4 Page A_128
DIANA CROSS_REFERENCE GUIDE
astypedef : TYPEDEF
(INHERITED FROM EXPDECL):
as_exp : EXP
(INHERITED FROM 10DSDECL):
as source name s : sourcename_s
(INHERITED FROM ALLSOURCE):
lxsrcpos : sourceposition
Ix comments : comments
** variable id
IS INCLUDED IN:
VC NAME
INTT OBJECT NAME
OBJECT NAME-
SOURCE NAME
DEF NAME
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
sm is shared : Boolean
(INHERITED FROM VCNAME):
sm renamesobj : Boolean
sm address : EXP
(INHERITED FROM INIT OBJECTNAME):
sm initexp : EXP
(INHERITED FROM OBJECTNAME):
sm_obj type : TYPESPEC
(INHERITED FROM DEFNAME):
lx_symrep : symbol_rep-
(INHERITED FROM ALLSOURCE):
lx srcpos : sourceposition
lx comments : comments
** variant
IS INCLUDED IN:
VARIANT ELEM
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
as choice s : choice s
as comp_list : comp_list
(INHERITED FROM ALL SOURCE):
lx srcpos : sourceposition
Ix comments : comments
** VARIANT ELEM
CLASS MEMBERS:
variant
variantpragma
IS INCLUDED IN:
ALL SOURCE
DIANA Reference Nanual Craft Revision 4 Page A_129
DIANA CROSS_REFERENCE GUIDE
NODE ATTRIBUTES:
(INHERITED FROM ALL_SOURCE):
lx srcpos : sourceposition
lx comments : comments
IS THE DECLARED TYPE OF:
variants.aslist [Seq Of]
** VARIANT PART
CLASS MEMBERS:
variantpart
void
IS INCLUDED IN:
ALL SOURCE
NODE ATTRIBUTES:
(INHERITED FROM ALLSOURCE):
lx_srcpos : sourceposition
lx comments : comments
IS THE DECLARED TYPE OF:
comp_list.as variantpart
** variant_part
IS INCLUDED IN:
VARIANT PART
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
as_name : NAME
as_variant s : variant s
(INHERITED FROM ALLSOURCE):
lx_srcpos : source_position
lx comments : comments
** variant_pragma -
IS INCLUDED IN:
VARIANT ELEM
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
aspragma : pragma
(INHERITED FROM ALLSOURCE):
lx_srcpos : sourceposition
lx comments : comments
** variant s
IS INCLUDED IN:
SEQUENCES
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
as list : Seq Of VARIANT ELEM
DIANA Reference Manual Draft Revision 4 Page A_130
DIANA CROSS_REFERENCE GUIDE
(INHERITED FROM ALLSOURCE):
lx_srcpos : sourceposition
lx comments : comments
IS THE DECLARED TYPE OF:
variantpart.asvariant s
"**VC NAME
CLASS MEMBERS:
variable id
constant id
IS INCLUDED IN:
INIT OBJECT NAME
OBJECT NAME
SOURCE_NAME
DEF NAME
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
sm renamesobj : Boolean
sm address EXP
(INHERITED FROM TNITOBJECTNAME):
sm initexp : EXP
(INHERITED_FROM OBJECT NAME):
smobj_type : TYPESPEC
(INHERITED FROM DEFNAME):
lx_symrep : symbolrep
(INHERITED FROM ALLSOURCE):
lx srcpos : source position
lx comments : comments
** void
IS INCLUDED IN:
PREDEF NAME
COMP REP ELEM
ALIGNMENT CLAUSE
ALL DECL
BODY
UNIT KIND
NAME
ITERATION
SOURCE NAME
"TYPE SPEC
"TYPE DEF
VARIANT PART
REP
RANGE
CONSTRAINT
EXP
DEF NAME
ALLSOURCE
UNIT DESC
DECL
DIANA Reference Manual Draft Revision 4 Page A_131
DIANA CROSS_REFERENCE GUIDE
DISCRETE RANGE
GENERAL_ASSOC
ITEM
NODE ATTRIBUTES:
(INHERITED FROM REP):
as_name : NAME
(INHERITED FROM RANGE):
sm type spec : TYPE SPEC
(INHERITED_FROM-DEFNAME):
lx symrep : symbolrep
(INHERITED FROM ALL SOURCE):
Ix_srcpos : source_position
Ix comments : comments
** while
IS INCLUDED IN:
ITERATION
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
as exp : EXP
(INHERITED FROM ALL SOURCE):
lx_srcpos : source_position
lx comments : comments
** with
IS INCLUDED IN:
CONTEXT ELEM.
ALL SOURCE
NODE ATTRIBUTES:
(NODE SPECIFIC):
as_name s : name s
asuse_pragma s : use_pragmas
(INHERITED FROM ALL_SOURCE):
lxsrcpos : source_position
Ix comments : comments
APPENDIX B
REFERENCES
DIANA Reference Manual Draft Revision 4 Page B_2
REFERENCES
(1) P.F. Albrecht, P.E. Garrison, S.L. Graham, R.H. Hyerle, P. Ip, and
B. Krieg-Bruckner. "Source-to-Source Translation: Ada to Pascal and
Pascal to Ada." In Symposium on the Ada Programming Language, pages
183-193. ACM- SIGP N Ton-, ee-embe-r, 1980.
[2] B. M. Brosgol, J.M. Newcomer, D.A. Lamb, D. Levine, M. S. Van
Deusen, and W.A. Wulf. TCOLada : Revised Report on An Intermediate
Representation for the Premi'minary- AT Language. -Te-nical Report
CMU_CS-80-105, Ca-rniege-Mellon University, Computer Science Department,
February, 1980.
(3] J.N. Buxton. Stoneman: Requirements for Ada Programming Support
Environments. Technical Report, DARPA, Februar'y 1980.
[4] M. Dausmann, S. Drossopoulou, G. Goos, G. Persch, G. Winterstein.
AIDA Introduction and User Manual. Technical Report Nr. 38/80,
Tn-stitut fuer Informi-Tik TT'-UnTversitaet Karlsruhe, 1980.
(5] M. Dausmann, S. Drossopoulou, G. Persch, G. Winterstein. On
Reusinq Units of Other Program Libraries. Technical Report Nr. 31/8ff,
Institut fu-er Informatik II, Universitaet Karlsruhe, 1980.
[6] Formal Definition of the Ada Pro rammin3 Language November 1980
e-dition, Honeywell, T-nc.,7-ii Honeywell Bull, INWRA,1980.
[7] J.D. Ichbiah, B. Krieg-Brueckner, B.A. Wichmann, H.F. Ledgard, J.C.
Heliard, J.R. Abrial, J.G.P. Barnes, M.. Woodger, 0. Roubine, P.N.
HiIfinger, R. Firth. Reference Manual for the Ada Programming
Language The revised reference manua-lT-July-T98U edition, Honeywell,
Inc., and Cii-Honeywell Bull, 1980.
[8] J.D. Ichbiah, B. Krieg-Brueckner, B.A. Wichmann, H.F. Ledgard, J.C.
Heliard, J.R. Abrial, J.G.P. Barnes, M. Woodger, 0. Roubine, P.N.
Hilfinger, R. Firth. Reference Manual for the Ada Programming
Language Draft revised MIL_STD 181"S-,•uly--T987-edit--on, Honeywell,
Inc., and Cii-Honeywell Bull, 1982.
[9] J.R. Nestor, W.A. Wulf, D.A. Lamb. IDL - Interface Description
Language: Formal Description. TecFniTcaT Report CMU_CS-8-3TT9,
Carnegie-Mellon University, Computer Science Department, August, 1981.
[10] G. Persch, G. Winterstein, M. Dausmann, S. Drossopoulou, G. Goos.
AIDA Reference Manual. Technical Report Nr. 39/80, Institut fuer
Tn7formatikITI,7UnTvv-eFs-taet Karlsruhe, November, 1980.
(11] Author unknown. Found on a blackboard at Eglin Air Force Base.
