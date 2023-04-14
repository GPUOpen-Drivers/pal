```
Copyright (c) 2019-2023 Advanced Micro Devices, Inc. All rights reserved.
```

<!--
-->

Introduction
============

This document establishes coding standards for the AMD Platform
Abstraction Library (PAL).

These standards consist of both required standards and recommended
guidelines. The language used in each item, especially any bold
language, establishes whether the item is required or recommended. Words
such as ***must*** and ***required*** establish a required standard.
Words such as ***should***, ***can***, ***avoid***, ***encouraged***,
***discouraged*** and ***recommended*** establish optional but
recommended guidelines.

The required coding standards must be strictly followed for all code
development in PAL. Both the requirements and recommendations will be
considered during code review.

Fundamental Underlying Principle:
---------------------------------

The Pal code will be read, debugged and changed much more than it is written.

Write code with this in mind, and invest more time at the
creation stage in order to reduce the effort required by everyone.

Clear and simple code is usually better than code that is 'more
efficient' but clever or difficult.

Why Coding Standards
--------------------

It is both useful and important to understand why these standards exist. While
each developer may have their own preferences on how to code, having a standard
set of guidelines streamlines for collaboration.

The first goal of this standard is to create a canonical format for PAL code,
in terms of indentations, newlines, naming convention, etc. Code written to a
consistent standard is far faster to read and easier to reason about as
developers become familiar with the standard. PAL in particular emphasizes
readability; when there is ambiguity in the standard's formatting, the most
readable solution will usually be the accepted one.

It is also well known in the industry that C++ is a large language with a lot
of potential for misuse. This standard guides developers away from problematic
or incorrect practices to support code that is high quality.

Any active developer can request a change to the standard where they find it
deficient to help provide a better standard that includes contributions from
the users.

Coding Standards and Guidelines
===============================

General
-------

-   Source lines ***must*** be no more than 120 characters long.

    -   On VS Code, you can set a vertical ruler to display the 120 character
        limit by setting "editor.rulers" to 120 in `Settings.json` (press
        CTRL + P and search for Settings.json)

        - The color of the vertical ruler can also be customized with the following
          json object:
          `"workbench.colorCustomizations": {"editorRuler.foreground": "#colorcode" }}`

    -   On Visual Studio, you can use the Editor Guidelines extension (found on the Visual
        Studio marketplace)

>  This is a point of frustration for some developers in an age when
>  everyone is using 24"+ widescreen monitors. However, we find that the 120 column
>  limit keeps side-by-side diffs easily readable and enables a better
>  experience for those that use portrait monitors or prefer editing
>  code in a side-by-side split screen. Hitting the 120 column limit is
>  a strong clue that the code is too deeply nested or that a
>  particular statement should be broken down into sub-expressions that
>  would be easier to read anyway.

-   Spaces ***must*** be used for spacing and indentation. Configure
    your editor to insert spaces in place of TABs.

-   Four characters ***must*** be used for indentation.

-   Each source line ***must*** not contain more than one statement.

-   There ***should*** not be trailing whitespace. Consider enabling
    visible whitespace in your editor to avoid this.

General Language Restrictions
-----------------------------

-   The Standard Template Library (STL) ***must not*** be used. The PAL
    utility collection (namespace Util) should be used instead and
    expanded as necessary.

-   When necessary for implementing a templated function or class, STL
    helpers from the `type_traits` header and `std::numeric_limits`
    **may** be used.

-   The std `atomic` header ***should*** be used to implement atomic
    values and thread barriers.

-   Standard I/O stream functionality (i.e., `cin`, `cout`, `cerr`, `clog`)
    ***must not*** be used.

-   goto statements ***must not*** be used.

-   Operator overloading is ***strongly discouraged***; however, it may
    be used at the project architects' discretion in cases where the
    meaning of the operator is preserved (e.g., iterators).

-   Code ***must not*** throw exceptions.

-   Non-const references ***must not*** be used except where needed in
    required constructors, etc.

-   `const` references ***may*** be used as an alternative to `const`
    pointers in cases where a function argument would/should be passed
    by value if it weren't a large structure.

-   `nullptr` ***must*** be used instead of `0` or `NULL` when assigning or
    comparing null pointer values.

-   ***Avoid*** magic numbers; replace with predefined constants with
    meaningful names.

-   Assertions (and any other code wrapped in `#if DEBUG`) ***must not***
    change the state of the program, only verify the current state.

-   Assertions ***should*** be aggressively used to verify invariants
    and document the expected state of the program.

-   Assertions ***must not*** be used to catch *unlikely* events like OS
    or memory allocation calls failing. The `PAL_ALERT` macro ***should
    be*** used for reporting this sort of failure, when deemed useful.

-   All target compilers **must** fully support C++17. The following C++
    constructs are explicitly allowed:

    -   Storage class ***must*** be specified for all enums to allow
        forward declaration on all platforms. See "Enumerations".

    -   Strong enums (`enum class`) ***may*** be used as applicable.

    -   `nullptr` ***must*** be used, as described above.

    -   `static_assert` ***must*** be used for compile-time assertions.

    -   `constexpr` ***should*** be used to identify compile-time constant
        values and functions.

    -   Use of `auto` is ***discouraged*** except:

        -   In cases where specifying the type is either impossible or
            ugly enough to reduce readability. E.g., declaring
            iterators, pointers/references to anonymous structures.

        -   In cases where the type is specified in the right-hand
            initialization expression.

-   **Avoid** overloading function names for class hierarchies. This can
    lead to tedious casting of the class that defines the function.

Preprocessor
------------

-   Preprocessor `#if` constructs ***must*** be used instead of `#ifdef`
    and `#ifndef` except in these cases:

    - It's determining a default for a macro.
    ```cpp
    #ifndef SOME_MACRO
        #define SOME_MACRO
    #endif
    ```

    -   It's the recommended way of doing things for a particular platform,
        e.g. `#if defined(__unix__)`

-   Preprocessor macros are ***highly discouraged***. Instead, methods
    or functions ***should*** be implemented and used.

-   ***Avoid*** the use of blocks of code that are commented out or
    compiled out with an `#if 0` / `#endif` wrapper. If there is a
    compelling reason to keep blocks of code like this, the block of
    code must be well commented with this reason.

-   Constants or enumerations ***should*** be used instead of `#defines`.

-   Compiler-specific preprocessor `#pragma` directives ***must
    not*** be used to disable compiler warnings. That is, the
    directive "`#pragma warning`" ***must not*** be used.

Naming Conventions
----------------------
> More than anything else, strict naming conventions may appear to
> be nothing more than arbitrary control-freakery. They are not: each
> of these does improve the readability and maintainability of the
> code, and also eliminate the most likely cause of format wars.

-   Single character variable name prefixes ***must not*** be used
    unless otherwise identified in this section. Hungarian notation
    ***must not*** be used.

-   All pointer variables ***must*** be prefixed with a "p" character.
    For example:
```cpp
SomeObject* pObject;
```

-   All handle variables ***must*** be prefixed with an "h".

-   Global variables ***must*** be prefixed with "g\_". See example
    below:
```cpp
extern uint32 g_someGlobalVar;
```
-   Thread-local variable ***must*** be prefixed with "tls\_".

-   Static, non-constant member variables (class variables) ***must***
    be prefixed with "s\_". See example below:
```cpp
static uint32* s_pSingleton;
```
-   Static constant member variables (class constants) ***must not*** be
    prefixed and ***must*** use UpperCamelCase capitalization.

-   Non-static member variables ***must*** be prefixed with "m\_".
    See example below and throughout:

```cpp
uint32 m_flags;
uint32* m_pValue;
```

-   Interface class names ***must*** be prefixed with "I". An interface
    class has no declared constructor, a virtual destructor, and a set
    of pure virtual functions that have no implementation. For
    example:

```cpp
class IDevice { ... };
```
-   All namespace, class, structure, typedef, enumeration, enumeration
    value, constexpr variable, static constant variable, static
    constant member variable (class constant), function, and method
    names ***must*** use UpperCamelCase capitalization. That is, the
    first letter of every word of the name must be upper case and all
    other letters lower case. See example below.

-   All non-constexpr/non-static variable, member variable, and function
    parameter names ***must*** use lowerCamelCase capitalization. That
    is, the first letter of the name must be lower case, the first
    letter of all subsequent words in the name must be upper case, and
    all other letters must be lower case. See the
    example below.

-   Abbreviations / acronyms in names ***must not*** be capitalized, but
    follow the correct style for the variable type. For example:
    `m_lodLevel`, `ConvertPsIl` instead of `m_LODLevel`, `ConvertPSIL`.

-   Abbreviations ***should*** either be industry standard abbreviations
    (e.g., PS = pixel shader) or widely used hardware abbreviations
    (e.g., SRD = shader resource descriptor). When in doubt, err on
    the side of less abbreviated names (e.g., ConstBuf over CB).

-   See "Structures and Typedefs" for additional guidelines. If
    preprocessor macros and defines are needed, the macro and define
    names must be UPPERCASE and use underscores to separate words.
    For example:

```cpp
#define DEPRECATED_FEATURE
#define PAL_DISALLOW_COPY_AND_ASSIGN(typeName) \
    typeName(const typeName&);                 \
    void operator=(const typeName&);
```
-   Examples:
```cpp
// Function name is UpperCamelCase.
// Parameter names are lowerCamelCase.
void ComputeSomething(
    uint32 usageIndex,
    float factor)
{
    // constexpr values are upper camel case.
    constexpr uint32 SquareSides = 4;
    // Non-constexpr, non-static constants are lower camel case.
    const float factorSqrt = Math::Sqrt(factor);
    ...
}

// Class name is UpperCamelCase.
class VertexShader
{
public:
    // Method name is UpperCamelCase.
    void ProcessFlowCntl();

private:
    // Member variable has "m_" prefix and is lowerCamelCase.
    uint32 m_flowCntlCnt;
}

// Structure names are UpperCamelCase.
struct SomeType
{
    uint32 elemType;
    uint32 reg;
};
```

Namespaces
----------

-   Namespaces ***may*** be defined to restrict scope.

-   The using namespace keyword ***must not*** reside in a header file.

-   The using namespace keyword ***must not*** precede a `#include`
    directive.

-   Contents of a namespace ***must not*** be indented.

-   The closing bracket of a namespace ***must*** be followed by a
    comment identifying the namespace that was closed. Example:

```cpp
namespace Util
{
// Contents of namespace are not indented.
uint32 SomeFunction();
...
} // Util
```

-   Qualified namespaces are encouraged:
```cpp
namespace Pal::Formats
{
...
} // Pal::Formats
```

Files
-----

-   Source files ***must*** have an extension of ".cpp".

-   Include files ***must*** have an extension of ".h".

-   The include path for the PAL library ***must*** only include the
    root pal directory and the public interface directories. All
    `#include` directives for internal headers ***must*** specify the
    full path relative to the root of the pal source.

-   In general, each class implementation ***should*** consist of at
    least one unique source file and at least one unique include file.
    More source and or include files ***may*** be used if the class
    implementation consists of a large number of source lines.
    Multiple classes ***may*** be contained in a single source and
    include file if these classes are small and the grouping is
    intuitive.

-   The body of include files ***must*** begin with a `#pragma once`
    declaration to prevent multiple inclusions. This pragma is
    portable across all target compilers, less error-prone than
    `#ifdef` include guards, and may improve compile speed on some
    implementations.

-   All headers ***must*** forward declare or include headers with
    declarations on which they depend. That is, every header
    ***must*** be able to be compiled when it is the only header
    included by a source file.

-   Linkage ***should*** be kept to a minimum; use forward declarations
    liberally and don't include a header in another header unless it's
    a requirement (i.e. a has-a or is-a relationship). Put
    implementation in the source file and rely on link-time code
    generation to handle inlining rather than being aggressive about
    it in the header file.

Documentation and Comments
--------------------------

### Copyright

All source and include files developed ***must*** contain an AMD copyright
notice:

```cpp
Copyright (C) [year] Advanced Micro Devices, Inc.  All rights reserved.
```

<!--
-->

### Public Interface

The public PAL interface (files in inc/) documentation can be generated
from source using the open source software Doxygen, and therefore the comment
requirements for files in inc/ are directed at producing high-quality
documentation:

-   Developers changing these files should ensure that the interface
    documentation is error and warning free. Documentation can be built
    by running make in doc/interface/. This requires:

    -   All files, classes, enums, structures, functions, typedefs,
        macros and variables ***must*** have doxgyen comments.

    -   All parameters and return values of functions ***must*** have
        doxygen comments.

    -   Parameters ***should*** be marked as \[in\], \[out\], or
        \[in,out\] as appropriate. Non-doxygen constructs like
        \[retained\] or \[destroyed\] ***must not*** be used.

-   Developers ***should*** examine the generated documentation for
    areas they changed to ensure that it is formatted as expected.
    Developers should familiarize themselves with the syntax of doxygen
    in order to maintain high quality documentation.

-   Specific usage of @brief, etc. is not mandated. The developer
    ***should*** use the capabilities of doxygen as appropriate with a
    focus on generating good-looking, useful documentation.

Use multi-line style Doxygen comments for file headers and class declarations.
```cpp
/**
 ***********************************************************************************************************************
 * @brief Summary of what the following body of code does.
 *
 * A more detailed description goes here.
 ***********************************************************************************************************************
 */
```

Use triple-slash style Doxygen comments for functions, methods, enums, structs, and unions.
```cpp
/// @brief Summary of what the following body of code does.
///
/// @param [in]  parameterName0  Description of this input parameter.
/// @param [out] parameterName1  Description of this output parameter.
///
/// @returns Omit if this isn't a function/method, or it's a void one.
```

### Internal code

Internal PAL code (anything not in inc/, and private members of classes
in inc/) ***should not*** use doxygen-style documentation. Instead,
the following standards apply:

>  Some doxygen-style comments may linger in PAL, but those should be removed
>  over time, opportunistically.

-   All function and class header comments ***must*** start with a line
    consisting of "// " followed by 117 "=" in order to fill the
    entirety of the allowed 120 columns. This serves as a visual cue
    when scrolling through files and is a helpful reminder on the column
    limit. The actual header comments ***must not*** repeat the name of
    the entity being documented. The "// ====" line ***must*** be
    included before every function even if there is no comment text
    (e.g., for a trivial constructor or destructor).

-   Every class definition ***must*** have an accompanying comment
    describing what it is for and how it should be used. For example:

```cpp
// =======================================================================
// Singleton class that handles global functionality for a particular PAL
// instantiation.
//
// Responsibilities include tracking all supported physical GPUs (a.k.a.
// adapters) that are available in the system, abstracting any interaction
// with the OS and kernel mode driver, and constructing OS-specific
// objects for other PAL components.
class Platform
{
   ...
}
```

-   Every function definition ***should*** have a comment immediately
    preceding it that describes what the function does and how to use
    it. Types of things to mention in this comment:

    -   What the inputs and outputs are.

    -   For class methods, whether the object remembers
        pointer/reference arguments beyond the duration of the method
        call, and whether it will free them or not.

    -   If the function allocates memory that the caller must free.

    -   Whether any of the arguments can be a null pointer.

    -   If there are any performance implications of how a function is
        used.

    -   If the function is re-entrant. What are its synchronization
        assumptions?

    -   Any tricky bits about the implementation of the actual function.

-   However, these comments ***must not*** be overly verbose or state
    the completely obvious. It is not necessary to provide comments for
    trivial constructors/destructors, trivial getters/setters, etc.

-   Class data members ***should*** have a comment describing what it is
    used for. If the variable can take sentinel values with special
    meanings, such as a null pointer or -1, document this. Comments are
    not required for completely obvious or ubiquitous uses (for example,
    `m_pDevice`, which is in almost every class of the PAL core, doesn't
    need a comment saying that it is a "pointer to the device associated
    with this object").

-   Global variables ***must*** have a comment describing what they are
    used for and a justification for being global.

-   Throughout, special care ***should*** be taken to document any
    synchronization assumptions made by a class or function (i.e., this
    function should *never* be called from multiple threads, etc.).

#### Motivation
The PAL team decided on this unique commenting style universally. We used to work on drivers that required full Doxygen comments in every file. It was a poor experience because:

- If we're going to use Doxygen, we want to do it right. That means exhaustively commenting every function parameter and every member variable. This is a huge waste of time when almost all member variables and function parameters are trivially understandable based on their name alone. Otherwise, a simple informal block comment above the variables or above the function gets the point across.
- Doxygen is meant to generate docs, and such the formatting is rather tedious and complex if you're never going to generate those docs. The only thing worth formally documenting in PAL is the interface, everything else is just "read the code" documented. Thus we're much better served with a very plain style that just gets the point across internally.
- The only good thing about Doxygen is the generated docs. We assume client driver engineers would generate Doxygen docs to help them read up on our interface headers. This is the only reason we still Doxygen for the public interface.

<!--
-->
Types and Declarations
----------------------

This section covers general requirements for all variables. There is a
separate section covering specific requirements for member variables.

-   All definitions (variable, function, etc.) ***must*** use intrinsic
    types for the following variable types: char, bool, float, double,
    and void.

-   A set of typedefs are available in palUtil.h for width-specified
    integer formats (int8, uint32, etc.) which ***must*** be used
    in lieu of the int and unsigned int intrinsic types.

> These typedefs break the UpperCamelCase convention for typedefs in
> order to be consistent with the built-in types.

-   Operating system type abstractions (e.g., DWORD, BOOL) ***may
    only*** be used in OS-specific code that directly interacts with
    the operating system.

-   Global variables are ***strongly*** ***discouraged***.

-   Static variables local to a particular function ***must not*** be
    used. constexpr ***should*** be used instead.

-   Static variables in header files ***must not*** be used. `constexpr`
    ***should*** be used instead. If the variable cannot be `constexpr`
    (e.g., the initialization expression is not constexpr), it
    ***should*** be declared `inline`.

> Including for fixed constants. If the compiler fails to identify a
> constant accurately, then it generates initialisation code assuming
> it is a static. The alternative is therefore less expensive and
> error-prone.

-   All pointer variable definitions ***must*** be specified with the
    asterisk immediately following the type. For example:

```cpp
Device* pDevice;
```
-   All references ***must*** be specified with the ampersand
    immediately following the type. For example:

```cpp
MyFunc(const MyInputStruct& inputInfo);
```

-   Each variable definition ***must*** be on a separate line with a
    semicolon at the end of the declaration. Multiple variable
    definitions ***must not*** reside on the same line. For example:

```cpp
int32 numIterations;
float scale;
uint32* pFlags;
```

-   Each source statement ***must*** assign a value to no more than one
    variable. Multiple variable assignments, such as "a = b = c;"
    ***must not*** be used.

-   Local variables ***should*** be declared at point of first use.

-   Local variables ***should*** be initialized at their declaration
    where possible.

-   Structures ***should*** be initialized completely. The preferred
    method is to use "= { };" which initializes the entire structure
    to 0 (the C / C++ specification states that members not explicitly
    initialized are set to 0) in a concise way which is also highly
    visible to the optimizer.

```cpp
SomeStruct structData = { };
```

-   Some structures may not allow the "= { }" form (e.g. if an enum is
    present) and these ***should*** use memset instead.

-   If a structure is not completely initialized (e.g. in extremely
    performance-critical code) a comment ***must*** be added to
    document the reason for not doing so.

-   const ***should*** be used wherever possible; see "Const Usage".

-   Local variables ***must*** be initialized via assignment
    (`uint32 foo = 2;`) rather than construction (`uint32 foo(2);`).

General Functions
-----------------

### Calling Conventions

-   A calling convention ***must not*** be specified unless a specific
    convention is required by the ABI.

-   If a function's calling convention differs from the default, both
    the function prototype and definition ***must*** explicitly
    specify the calling convention. The calling convention specified
    ***must*** be one of the following abstracted calling conventions:
    `PAL_STDCALL` (`__stdcall`), or `PAL_CDECL` (`__cdecl`).

### Inline Functions

-   Short functions (3-4 statements) may be defined in a header file to
    be inlined.

-   The `inline` specifier is ***required*** for functions defined in header
    files that are not implicitly inline to avoid One-Definition Rule (ODR)
    violations. Implicitly inline functions that ***should not*** be defined
    with the `inline` specifier are
        - Function templates
        - Constexpr functions
        - Functions defined within a class/struct/union definition
        - Deleted functions
        - Member functions of template classes that are not full
          specializations

-   The `PAL_FORCE_INLINE` compiler directive is ***strongly
    discouraged*** and ***should not*** be used except in cases where
    it provably provides a significant benefit. In general, rely on
    link-time code generation and the compiler to do inlining, but this
    hint ***may*** be used when it provides a benefit.

> The engineer ***must*** be able to provide performance results, or
> at least a demonstration that the compiler-generated object code is
> clearly superior.

### Static Functions

-   Functions with local scope (scope restricted to a source file)
    ***must*** be specified with the static keyword. These functions
    are referred to as static functions.

-   All static functions ***must*** provide a prototype.

-   The prototypes (declarations) of all static functions ***must*** be
    grouped together and ***must*** occur early in the source file.

-   Functions declared `static` in a header are ***discouraged*** unless
    there is a specific, documented technical reason. To avoid multiple
    symbol defininition errors prefer the `inline` specifier.

### Formatting and Commenting

-   Function definitions (but not prototypes) ***must*** display each
    parameter on a separate line with an initial indentation of four
    spaces. See the examples below.

-   In the definition of a function, the return type and function name
    ***must*** reside on a single source line. See the below examples.

-   Parameters ***should*** be ordered such that input parameters come
    before output parameters. The exception is functions that wrap
    stdlib functions with well-known signatures (i.e., strcpy), where
    the stdlib signature should be replicated.

-   Declarations, definitions, parameters, initializers and expressions
    ***should*** be aligned appropriately to improve readability.

-   Developers ***must*** maintain the surrounding style when modifying
    existing code.

-   Function definitions (the actual implementation) ***should*** be
    preceded by a function comment header, and ***must*** be preceded
    by the "`// ====...`" comment delimiter. See "Documentation and
    Comments" for details.

-   Examples:

```cpp
// Function prototype example
HwlFactory* SomeHwlInit(const Adapter* pAdapter, bool fastInit);

// ==========================================================================
// Function definition example. Comment header with details on the usage of
// the function.
HwlFactory* SomeHwlInit(
    const PhysicalGpu* pPhysicalGpu,
    bool fastInit)
{
    ...
}
```

### Function Contents

-   ***Prefer*** short and easily understood functions. As a rough
    guideline, consider splitting functions over 100 lines into more
    manageable subunits.

-   ***Prefer*** breaking complicated expressions into smaller
    expressions, using clearly named constant variables or
    factored-out functions to document intermediate values. Such
    decomposition is invariably superior to describing calculations in
    comments.

> Such named constant variables are better than comments; they are
> more likely to be read, more likely to be understood, their
> intermediate values are visible in the debugger, and they are more
> likely than comments to be updated and correct after change.

-   ***Avoid*** excessive indentation. Difficulty fitting a function
    within the line width limits due to indentation is an indication
    that a less indented solution (factoring out parts of the
    function, making better use of else if, etc.) would be wise.

> A common place this is encountered is code which performs several
> error checks. Usually, this can be refactored to a flatter
> architecture by performing code akin to
> `if (no error) { error = operation1; } if (no error) { error = operation2; }` etc.

-   Each function ***should*** have only one return point. Early return
    statements are ***discouraged*** and should only be used in cases
    where:
       1. The function's clean-up at the return points is trivial (such
        as just having local variables which require no clean-up, or RAII
        objects).
       1. The function is short enough for readers to see the multiple
        returns and understand the flow of the function without having to
        scroll up and down repeatedly, ***or*** the additional return points
        are early-outs which make the rest of the function body easier to
        understand.

Classes
-------

### General

-   Virtual methods ***must not*** be called from constructors or
    destructors. They can be undefined behavior or extremely error-prone
    depending on the situation due to initialization and destruction order.

-   The assignment operator must be disabled using the
    `PAL_DISALLOW_COPY_AND_ASSIGN` macro in the private section of
    the class. See the example below.

```cpp
class HwlFactory
{
public:
    // Public members belong here.
protected:
    // Protected members belong here.
private:
    PAL_DISALLOW_COPY_AND_ASSIGN(HwlFactory);
};
```

-   A friend declaration ***should not*** be used. Its use is ***highly
    discouraged***. It may be allowed at the architects' discretion in
    certain idiomatic situations (e.g., iterators).

-   Class declaration access specifiers ***must*** occur in the
    following order: public, protected, private.

-   Every class declaration ***must*** provide no more than one
    occurrence of each access specifier.

-   Class declaration access specifiers ***must*** be left justified
    (i.e. without indentation).

-   Classes ***should not*** be used only for collections of static
    functions; namespaces ***should*** be used for that purpose.

-   In declarations, variables ***must*** follow functions; the two
    ***must not*** be interleaved except by access specifier.

### Inheritance

-   Multiple inheritance ***must not*** be used.

-   All virtual functions declared in the base class ***must*** be
    declared `virtual` throughout the inheritance hierarchy.

-   Overridden base class `virtual` functions in derived classes ***must***
    be declared with the `override` keyword.

-   If a function overrides a base class virtual function and does not need to
    be further overriden down in the hierarchy tree, then it ***can*** be declared final.
    Thus, the `final` keyword ***should not*** be used unless there is an `override`
    keyword.

-   All destructors throughout an inheritance hierarchy ***must*** be
    declared virtual.

-   Derived classes which have no child classes ***must*** be
    declared using the `final` keyword.

### Constructors

-   Constructors ***must not*** perform risky operations that might fail
    execution. If an exception is thrown in a constructor, the
    destructor is not called to clean up. ***Prefer*** empty
    constructors using only initializer lists. If any non-trivial work
    must be done to initialize an object, perform it in an Init()
    method.

-   Copy constructors must be disabled using the
    `PAL_DISALLOW_COPY_AND_ASSIGN` macro in the private section of
    the class. See the related example in section 3.10.1.

-   Conversion constructors ***must*** be declared explicit. Conversion
    constructors are single argument constructors that construct objects
    from other data types. This is necessary to avoid unintended constructor
    conversion. An exception to this is where the implicit conversion can be
    considered a "decay", that is, possibly losing information and not actually
    needing to execute any real code.

```cpp
class Token
{
public:
    explicit Token(uint32 tokenVal);

protected:
    uint32 m_tokenVal;
};
```

-   ***Prefer*** member brace initialization to zero initialize members instead of
    `memset`. E.g.,

```cpp
class Device : public IDevice
{
public:.
    Device();

private:
    DeviceFinalizeInfo m_finalizeInfo;
};

// preferred initialization
Device::Device()
    :
    m_finalizeInfo{}
{
    ...
}

// avoid if possible
Device::Device()
{
    ...
    memset(&m_finalizeInfo, 0, sizeof(m_finalizeInfo));
    ...
}
```

-   ***Prefer*** initialization to assignment in constructors. In the example
    below, `m_tokenVal` and `m_count` are initialized instead of assigned in
    the constructor.

```cpp
class Token
{
public:
    explicit Token(uint32 tokenVal);

protected:
    uint32 m_tokenVal;
    uint32 m_count;

};

// Member initialization example.
Token::Token(
    uint32 tokenVal)
    :
    m_tokenVal(tokenVal),
    m_count(0)
{
    ...
}
```

-   Items in the initialization list ***must*** appear in the order in
    which they are specified in the class declaration (as that is the
    order in which the initializations occur regardless of their order
    in the initialization list).

-   If the order of initialization is significant, comment it.

-   `PAL_NEW` does not initialize the allocated memory to 0s. Initializer
    lists ***must*** initialize all member values, even if they are
    simply set to 0.

### Destructors

-   The destructor of a class that will be subclassed ***must*** be
    declared virtual. Such base class destructors ***must*** be either
    public or protected. If the base class destructor is protected, a
    public `Destroy()` method must be provided for object destruction
    instead.

```cpp
class HwlFactory
{
public:
    virtual ~HwlFactory();
    ...
};
```

-   The destructor of a derived class ***must*** be declared virtual.
    That is, the destructor of a class that inherits from a base class
    ***must*** be declared virtual.

### Methods

-   The compiler default calling convention for class methods ***must***
    be used.

-   The implementation of short methods that are never likely to change
    ***can*** reside in the class declaration in an include file. This
    method is implicitly inline. Implicitly inline methods ***must
    not*** consist of more than three to four statements.

-   The explicit `inline` specifier ***should not*** be used unless it is
    used to resolve One-Definition Rule (ODR) violations.

-   The `PAL_FORCE_INLINE` compiler directive is ***strongly
    discouraged*** and ***should not*** be used.

-   Inherited, non-virtual method ***must not*** be redefined.

-   Method names ***should not*** be overloaded. Exceptions are allowed
    in cases where the code encapsulated is trivial and the
    alternative is both uglier and more prone to error. For example,
    an overload may be used on a simple, single-line pointer
    conversion member function to allow for one case giving a const
    result from a const pointer and one case giving a non-const result
    from a non-const pointer, or on a simple utility function that
    requires both 32-bit and 64-bit implementations.

-   Default arguments ***must not*** be used with virtual methods.
    It introduces ambiguity if the default is different across classes.

-   Default arguments in non-virtual methods are ***discouraged***.

-   In the definition of a class method, the return type, class name,
    and method name ***must*** all reside on a single source line. For
    example:

```cpp
uint32 SomeClass::Optimize(
    uint32 offset,
    uint32 count)
{
    ...
}
```

-   Method definitions (but not prototypes) ***must*** display each
    parameter on a separate line with initial indentation of four
    spaces. If the method has parameters and is const, the closing
    bracket and const ***must*** appear on a new line, also with an
    indentation of four spaces, after the final parameter. If the
    method has no parameters and is const, const must appear on the
    same line as the function name. Examples:

```cpp
uint32 SomeClass::ConstFunc() const
{
    ...
}

uint32 SomeClass::AnotherConstFunc(
    uint32 param
    ) const
{
    ...
}
```

-   Method, function, and macro invocations ***must be*** formatted so
    that there is no space between the function name and the opening
    parenthesis. There ***must be*** no space between the opening
    parenthesis and the parameter list. That is, the formatting
    ***must*** be `doSomething(arg1, arg2)`, not
    `doSomething (arg1, arg2)` or `doSomething( arg1, arg2 )`.

-   There ***must not*** be any implicit conversion to another type implemented
    with an `operator` method. An exception to this is where the implicit
    conversion can be considered a "decay", that is, possibly losing information
    and not actually needing to execute any real code.

### Member Variables

-   See naming conventions in addition to this section.

-   Member variables ***must not*** be declared public.

-   Member variables ***should*** be declared private unless the design
    of the class hierarchy requires revealing such member variables to
    derived classes.

-   A member variable ***must not*** be declared mutable unless it
    required by the class design. The use of mutable member variables
    is ***discouraged***.

Const Usage
-----------

-   Use the const specifier ***whenever possible***.

-   Floating point constants ***must*** be suffixed with an "f" to
    prevent an implicit conversion from double to float. Constants of
    other types ***should*** limit conversions using the "ul", "ull",
    etc. suffixes.

-   Variables whose value is invariant ***should*** be declared with the
    const specifier. For example:

```cpp
const float pi = 3.141592f;
```

-   Temporary 'variables' in functions ***should*** be declared const
    where possible.

> It is entirely right that constant temporaries should greatly
> outnumber genuine variable temporaries. Constant temporaries are
> effectively comments, but being actually part of the code they are
> both in more convenient positions than comments and likely to be
> better maintained. It also encourages the breaking down of
> expressions into smaller and more easily managed subunits, in the
> safe knowledge the compiler can optimize the variable away.

-   A class member variable whose value is invariant ***should*** be
    declared with the const specifier. The compiler will enforce that
    these members can only be set during the initialization list of
    the class constructor. It is acceptable to use `const_cast` in a
    constructor to enable a member variable to be const that cannot be
    initialized in the initializer list.

-   Pointer variables that point to invariant data ***should*** declare
    the data as const. This is a pointer to a const. For example:

```cpp
const SomeStaticTable* pTable; // pointer can change but contents cannot
```

-   Pointer variables with an invariant address but point to data that
    can change ***should*** specify that the pointer is const. This is
    a const pointer. For example:

```cpp
float* const pRunningSum; // pointer cannot change but contents can
```

-   Pointer variables with an invariant address value that points to
    invariant data ***should*** specify that both the address value
    and the contents pointed to by the address are const. This is a
    const pointer to a const. For example:

```cpp
const char* const pStr = "Hello"; // neither pointer nor content change
```

-   An object method that doesn't modify the logical object ***should***
    be specified as a const method. A logical object is the set of
    member variables and data referenced by member variable pointers
    that represent the observable state of the object.

Casting
-------

-   C++-style casting operators ***must*** be used instead of C-style
    casting. C-style casting ***must not*** be used.

-   ***Minimize*** casting.

-   The `static_cast` operator ***must not*** be used to downcast a base
    class pointer to a derived class pointer except for shared objects
    across a defined interface (such as a hardware layer interface, or
    OS-abstraction interface, where the HWL or OS-specific code knows
    the derived type). It is recommended that these casts be performed
    by small functions to clearly document their usage.

-   When using a `static_cast` operator to fix a truncation warning
    (typically, 64-bit to 32-bit conversion) an assert ***must*** be
    added that the source was small enough to be legally cast.

-   Functional cast notation ***may*** be used in place of `static_cast` if
    the type is an enum or a fundamental type. E.g.,
```cpp
// static_cast for defining enum values
enum HardwareStageFlagBits : uint32
{
    HwShaderLs = (1 << static_cast<uint32>(HardwareStage::Ls)),
    HwShaderHs = (1 << static_cast<uint32>(HardwareStage::Hs)),
    HwShaderEs = (1 << static_cast<uint32>(HardwareStage::Es)),
    HwShaderGs = (1 << static_cast<uint32>(HardwareStage::Gs)),
    HwShaderVs = (1 << static_cast<uint32>(HardwareStage::Vs)),
    HwShaderPs = (1 << static_cast<uint32>(HardwareStage::Ps)),
    HwShaderCs = (1 << static_cast<uint32>(HardwareStage::Cs)),
};

// functional cast version, equivalent to the one above
enum HardwareStageFlagBits : uint32
{
    HwShaderLs = (1 << uint32(HardwareStage::Ls)),
    HwShaderHs = (1 << uint32(HardwareStage::Hs)),
    HwShaderEs = (1 << uint32(HardwareStage::Es)),
    HwShaderGs = (1 << uint32(HardwareStage::Gs)),
    HwShaderVs = (1 << uint32(HardwareStage::Vs)),
    HwShaderPs = (1 << uint32(HardwareStage::Ps)),
    HwShaderCs = (1 << uint32(HardwareStage::Cs)),
};
```

-   The `dynamic_cast` operator ***must not*** be used. The `dynamic_cast`
    operator requires runtime type informatation (RTTI). RTTI adds
    runtime overhead and is therefore disabled in PAL.

-   The `reinterpret_cast` operator ***must not*** be used except in
    three scenarios:

    -   An object pointer needs to be reinterpreted across an interface
        between PAL and external components (e.g., runtime, shader
        compiler). Consider encapsulating these casts in a small
        function to clearly document such use.

    -   The contents of an intrinsic type needs to be reinterpreted as
        another intrinsic type:

```cpp
float guardbandScale = 1.5f;
uint32 regGuardbandScale;

// Assign the bits of guardbandScale to regGuardbandScale
// without performing a type cast

regGuardbandScale = *reinterpret_cast<uint32*>(&guardbandScale);
```

-   Certain memory-allocation like functions may not return `void*` and
    may need a `reinterpret_cast`. Use of `reinterpret_cast` ***should***
    be accompanied by a clear comment explaining why it is required.

### `const_cast`

The `const_cast` operator ***must not*** be used to eliminate the
constness or volatility of an object. Exceptions are
permitted only when the following conditions hold:

-   The function or data item in question has important thread safety
    requirements.

-   Marking the function or data item in question const significantly
    increases confidence in thread safety by allowing the compiler to
    detect thread safety violations.

-   Alternatives to using `const_cast` would be unusually onerous or
    themselves generate further thread safety issues.

-   The code using the `const_cast` is involved in a one-time
    initialization before other threads could reference the object.
    Any operation involving a `const_cast` ***should*** be a one-way
    transition and not later reversible.

-   The use of the cast is clearly documented in code comments,
    particularly as to why this is preferable to any alternatives.

Structures and Typedefs
-----------------------

-   Structures ***must*** be plain-old data (POD) types.

-   Typedefed structures and typedefed classes ***should not*** be used.

-   Structure declarations ***must not*** provide access specifiers
    (public, protected, private).

Enumerations
------------

-   The size of the integer-representation of all enumeration types
    ***must*** be fully specified. The syntax for this in accordance
    with the C++ 0x standard is 'enum MyEnum : int { ... }; '.

-   This integer-representation ***must*** be fully specified in both
    the actual definition of the enumeration type, and in all forward
    declarations referencing that type. Furthermore, the integer
    representations specified in all forward declarations and in the
    actual type definitions ***must always match***. Failure to meet
    these requirements will result in compilation errors on C++ 0x
    standard-compliant compilers, such as GCC (version 4.8 or newer).

Ifs, Loops, and Switch Statements
---------------------------------

-   Variables ***must not*** be assigned within a conditional. The
    project build scripts are set up to enforce this restriction.

-   A space ***must*** be left between the conditional keyword and the
    opening bracket. There ***must not*** be a space left between the
    opening bracket and the conditional statement. For example, a
    statement ***must*** be formatted `if (a == 0)`, not `if(a == 0)`,
    `if ( a == 0 )`, or `if( a == 0 )`.

-   A for loop statement ***must*** contain no more than one
    initialization. That is, a for loop statement ***must not***
    contain multiple initializations. For example, in this loop
    statement `for (a = 0, b = 1; a < maxA; a++)`, the "`b = 1`" is a
    second initialization and does not adhere to the standards.

-   A for loop ***may*** contain multiple iteration statements but this
    is ***discouraged***. For example, in the loop statement
    `for (a = 0; a < maxA; a++, pElem++)`, "`pElem++`" is a second iteration
    statement. Its use is allowed but discouraged.

-   The initialization of loop variables, including any that are
    involved in the iteration limit condition ***should*** be kept
    close to the loop, or commenting should be used to explain where
    the limit comes from.

-   Ranged-based for-loops are ***allowed***. The rules for `auto` apply here
    as well. E.g.,
```cpp
Util::Vector<int32, 16, Allocator> data = ...;
for (int32 i : data)
{
    f(i);
}
```

-   Braces (curly brackets) ***must*** bracket the body of all if/else
    statements, including single statement bodies.

```cpp
if (usage > maxUsage)
{
    // Do something.
}
else
{
    // Do something else.
}
```

-   Opening and closing braces ***must*** reside in the same column.

-   Braces must bracket the body of all for, do/while, and while
    statements including single statement bodies.

```cpp
for (i = 0; i < maxIter; i++)
{
    // Do something.
}

while (done == FALSE)
{
    // Do something.
}
```

-   Switch/case statements ***should not*** include a default case
    unless specifically necessary, particularly when switching on an
    enum type where the compiler will issue a warning for a missed
    enum only if the default case is missing. Switch/case statements
    that use a default case should comment why it is necessary.

-   All switch/case statements ***must*** be of the following form:

```cpp
switch (blah)
{
case Blah1:
    // Handle Blah1 case.
    break;
case Blah2:
    // Handle Blah2 case.
    break;
}
```

-   Any time a case statement is used without a corresponding break by
    design, a comment ***must*** record why the break is not needed.
    The exception to this case is when a group of cases all execute
    the same code. The example below shows where a comment is and is
    not required.

```cpp
switch (operation)
{
case OpGo:
    // Handle Go case.
    break;
case OpStop:
    // Handle Stop case.
    break;
case OpPause:
case OpPauseSignaled:
    // Handle identical pause cases.
    break;
case OpYield:
    RecordYields();
    // FALLTHROUGH: After the record, the Yield operation then needs
    // to carry out the default operation.
default:
    // Handle the default, unspecified cases. The default case is necessary
    // because...
    break;
}
```

-   When testing booleans, `if (x == true)` should be used instead of `if (x)`.
    It is permissible to omit the `== true` if the expression forms a clearly
    readable statement without it e.g. `if (image.HasDsMetadata())` or
    `if (hasDsMetadata)`.

-   When testing non-booleans, `if (x != 0)` or `if (x != nullptr)`
    ***must*** be used instead of `if (x)`.

-   `if (x == false)`, `if (x == 0)` or `if (x == NULL)`
     ***must*** be used instead of `if (!x)`.

-   Subexpressions in if statements that use comparison operators
    ***must*** have explicit bracketing.

```cpp
if ((pResource == nullptr) &&
    (flags.abort == true) &&
    (id >= MinIdToCheck))
{
    ...
}
```

-   Yoda conditions such as `if (nullptr != x)` or `if (0 == x)` **should not** be used;
    the constant should be on the right of the expression for readability and consistency.
    `if (x != nullptr)` or `if (x == 0)` should be used instead.

-   Prefer to break complex tests across multiple lines and / or
    introduce intermediate const booleans that are easier to
    understand.

Error Checking
--------------

-   Prefer to use Util::Result error codes to bool success / fail
    markers or custom error codes.

-   Memory allocation errors ***should*** be consistently reported with
    Util::OutOfMemory.

-   Programming errors ***should*** be checked for with asserts.
    ***Avoid*** runtime checks.

> A runtime check is as likely to mask a bug as reveal it.

-   No parameter validation should be performed in performance critical
    functions, such as command buffer building routines. It is up to
    the client to ensure correct parameters are passed in.

-   Expected failures (e.g. memory allocation, errors calling operating
    system functions) ***must*** always be checked with a runtime test
    and a clean fallback implemented. An assert ***must not*** be
    added for success of these operations, `PAL_ALERT` can be used for
    that purpose.

-   ***Prefer*** compile-time asserts over run-time asserts where
    possible.

Memory Allocation
-----------------------------

-   ***Avoid*** explicit memory management where possible. In
    particular, ***avoid*** memory management except where there is a
    clear path to return an error to the PAL client.

-   All system memory allocation and deallocation operations ***must***
    use the PAL macros instead of the standard language allocation and
    deallocation routines. The PAL macros are:
    -   `PAL_NEW`, `PAL_DELETE`, `PAL_DELETE_THIS`, and `PAL_SAFE_DELETE`
         in place of `new` and `delete`.

    -   `PAL_NEW_ARRAY`, `PAL_DELETE_ARRAY` and `PAL_SAFE_DELETE_ARRAY`
         in place of `new[]` and `delete[]`.

    -   `PAL_MALLOC`, `PAL_CALLOC`, `PAL_FREE` and `PAL_SAFE_FREE` in
         place of `malloc`, `calloc` and `free`.

    -   `PAL_PLACEMENT_NEW` in place of placement new. It ***should*** be
        used for objects that are created in already allocated memory
        (e.g., objects that  implement interface objects, have the same
        lifetime of an interface object, etc.).

-   Return values from memory allocations ***must*** be checked for
    failure. The cleanest possible fallback path ***should*** be
    implemented.

-   Pointers should be set to `nullptr` after deallocation. The
    `PAL_SAFE_DELETE`, `PAL_SAFE_DELETE_ARRAY` and `PAL_SAFE_FREE`
    macros handle this automatically.

Construction and Destruction
-----------------------------

### Simple Construction and Initialization

If initialization of an object `Object` has no observable failures, i.e.,
either the operations in the constructor cannot fail or all failures can be
handled in the constructor without additional handling code at the caller,
then only the definition of constructors is ***required*** for `Object`. Asserts
are ***allowed*** in constructors, as they guarantee pre- and post-conditions
and if they are triggered, they halt execution.

Typically, objects that have simple construction and initialization do not
require a complex destruction mechanism. If a destructor is not performing
any operations it can be omitted or if required, e.g., for base classes, it
can be empty.

E.g.,

```cpp
class Object
{
public:
    Object(int32 i)
        :
        m_integer(i),
        m_double(1.0)
    { }

private:
    int32  m_integer;
    double m_double;
};

class BaseObject
{
public:
    BaseObject(int32 i)
        :
        m_integer(i),
        m_double(1.0)
    { }

    virtual BaseObject()
    { }

private:
    PAL_DISALLOW_COPY_AND_ASSIGN(BaseObject);

    int32  m_integer;
    double m_double;
};
```

### Generic Construction and Initialization

#### Construction

If an instance of a class can potentially fail during construction, then
construction ***must*** be split into two functions. Apart from the
constructor(s), `Init()` function(s) ***must*** be provided that return
`Pal::Result` with the result of the initialization.

The constructor(s) ***must*** perform all initialization that has no
observable failures. The `Init()` function(s) ***must*** perform all
initialization that may fail.

Instances of a class that has `Init()` function(s) are only fully
constructed if both the constructor and an `Init()` function are called.
If an `Init()` function is not called, the object is in an unspecified state
and calling any other function than `Init()` or the destructor is undefined
behavior.

-   If the `Init()` function returns `Pal::Result::Success` then the object is
    fully constructed and in a well-defined, valid state.

-   If the `Init()` function returns an error code, the object is in a invalid
    state. The only functions that can be called are `Destroy()` or the destructor.

#### Destruction

If a class has an `Init()` function, it will probably need a non-trivial
destructor. In that case, a destructor ***must*** be provided that clean ups
everything that was setup either in the constructor(s) and/or an `Init()`
function.

If an instance of a class is expected to be constructed on preallocated memory
(i.e., after `PAL_PLACEMENT_NEW`), a `Destroy()` function ***must*** also be
defined that calls the destructor.

E.g.,

```cpp
class Object
{
public:
    Object()
    {
        // initialization that requires no error reporting
    }

    /// Initializes this instance of Object and returns the result of
    /// the operation.
    Pal::Result Init(...)
    {
        Pal::Result result = Pal::Result::Success;

        // initialization that requires error reporting in case of failure
        result = ...

        return result;
    }

    /// Destroys this instance of Object.
    void Destroy()
    {
        this->~Object();
    }

private:
    ~Object()
    {
        // all cleanup related operations
        // clean-up typically does not fail
    }
};

/// creation of object
void* pPlacementAddr = ...;
auto* const pObject = PAL_PLACEMENT_NEW(pPlacementAddr) Object;
Pal::Result result = pObject->Init(...);
if (result != Pal::Result::Success)
{
    pObject->Destroy();
}
```

#### Destruction with Implicit Deallocation

In general, deallocating memory as part of the object's destruction process
(i.e., the function that cleans-up the object needs to also deallocate the
memory it occupies) is ***discouraged***, as memory allocation for a created
object is responsibility of the caller. However, if that need arises, a
`DestroyInternal()` function ***must*** be provided.

E.g.,
```cpp
class MyAllocator;

class Object
{
public:
    Object(MyAllocator* pAllocator)
        :
        m_pAllocator(pAllocator)
    {
        // initialization that requires no error reporting
    }

    /// Initializes this instance of Object and returns the result of the
    /// operation.
    Pal::Result Init(...)
    {
        Pal::Result result = Pal::Result::Success;

        // initialization that requires error reporting in case of failure
        result = ...

        return result;
   }

    /// Destroys this instance of Object.
    void Destroy()
    {
        this->~Object();
    }

    /// Destroys this instance and deallocates.
    void DestroyInternal()
    {
        // save allocator, as ~Object() may set m_pAllocator to nullptr
        // (e.g., in debug)
        MyAllocator* pAllocator = m_pAllocator;
        Destroy();
        PAL_FREE(this, pAllocator);
    }

private:
    ~Object()
    {
        // all cleanup related operations
    }

    MyAllocator* m_pAllocator;
};

MyAllocator allocator;

/// creation of object with explicit deallocation after destruction
{
    void* pPlacementAddr = PAL_MALLOC(sizeof(Object), &allocator, Pal::SystemAllocType::AllocInternal);
    if (pPlacementAddr != nullptr)
    {
        auto* const pObject = PAL_PLACEMENT_NEW(pPlacementAddr) Object(&allocator);
        Pal::Result result = pObject->Init(...);
        if (result != Pal::Result::Success)
        {
            pObject->Destroy();
            PAL_FREE(pPlacementAddr, &allocator);
        }
    }
}

/// creation of object with implicit deallocation during destruction
{
    void* pPlacementAddr = PAL_MALLOC(sizeof(Object), &allocator, Pal::SystemAllocType::AllocInternal);
    if (pPlacementAddr != nullptr)
    {
        auto* const pObject = PAL_PLACEMENT_NEW(pPlacementAddr) Object(&allocator);
        Pal::Result result = pObject->Init(...);
        if (result != Pal::Result::Success)
        {
            pObject->DestroyInternal(); // no PAL_FREE required
        }
    }
}
```

### Error Handling During Construction

#### Member Variables

If an object is a member variable and fails to be constructed during its `Init()`, the
destructor of the outer object is responsible for that object clean-up.

E.g.,

```cpp
class MyAllocator;

class OuterObject
{
public:
    OuterObject()
    { ... }

    ~OuterObject()
    {
        PAL_DELETE(m_pObject, m_pAllocator);
    }

    Pal::Result Init()
    {
        Pal::Result result = Pal::Result::Success;

        m_pObject = PAL_NEW(InnerObject, m_pAllocator, allocType) InnerObject;
        if (m_pObject == nullptr)
        {
            result = Pal::Result::ErrorOutOfMemory;
        }
        else
        {
            result = m_pObject->Init();
        }

        // if result != Result::Success, not necessary to clean-up m_pObject
        // ~OuterObject() is responsible for m_pObject cleanup
        return result;
    }

private:
   MyAllocator* m_pAllocator;
   InnerObject* m_pObject;
};
```

#### Factories

If a factory returns an object via an out parameter, then the returned object needs to be
properly destroyed if its construction fails inside the factory.

E.g.,

```cpp
class ObjectCreator
{
public:
    Pal::Result CreateObject(
        const ObjectCreateInfo& createInfo,
        otherArgs...,
        void* pPlacementAdr,
        Object** ppObject)
    {
        // construct pObject
        auto* const pObject = PAL_PLACEMENT_NEW(pPlacementAddr) Object(otherArgs...);

        // initialize pObject
        Result result = pObject->Init();

        // do more initialization on the object
        if (result == Result::Success)
        {
            result = pObject->MoreInitOperations();
        }

        if (result == Result::Success)
        {
            // notify caller of successful initialization
            (*ppObject) = pObject;
        }
        else
        {
            // initialization failed, destroy object
            pObject->Destroy();
        }

        return result;
    }
};
```

Concurrency and Thread Safety
-----------------------------

-   Code with thread-safety requirements ***must*** be clearly
    documented as such.

-   ***Avoid*** writing lock-free code without a very clear need;
    critical sections suffice for most cases.

> Lock-free code is a maintenance burden: an order of magnitude
> harder to make correct and modification tends to require reworking
> the algorithm from scratch.

-   Code enclosed in critical sections ***should*** be kept to a
    minimum.

-   ***Avoid*** function calls to remote components inside critical
    sections. These increase the risk of deadlock (if another critical
    section is taken) and the CS being held for an extended period.
    Consider that future maintainers of the called code may not be
    aware that it is called from within a CS.

-   If a critical section is held during a remote function call, it
    ***should*** be documented as such in that function, ideally with
    an assert that the expected critical section is held.

-   const functions are more thread-safe (they are much less likely to
    modify program state, although they must take care that the data
    on which they rely cannot be modified elsewhere) so ***should***
    be used where possible.

Templates
---------

### Use of Templates

Templates ***should*** only be used in the following restricted
situations:

-   Type-safe containers.

-   Type-safe memory allocation functions.

-   Where a template function is the only alternative to a preprocessor
    macro.

-   Extreme performance situations where an inline template function is
    the only route to high performance.

Non-trivial use of templates ***must*** be approved before a substantial
implementation effort is expended.

Templates ***must*** be kept simple and obvious and be used where their
ability to parameterize a type or value at compile-time offers specific
advantages. "Clever" use of templates is not desirable.

### General Template Guidelines

-   Template meta-programming constructs ***must not*** be used.

> TMP requires substantially different skills to 'conventional' C++
>  programming.

-   ***Avoid*** compile-time (static) polymorphism (implicit interfaces).
    Templates therefore ***should not*** depend on the
    properties of any types supplied as template parameters.
    Exceptions to this rule are likely to be small functions which
    only accept value types.

> The interfaces of runtime polymorphism are explicit and validated
> by the compiler using the class definition; the interfaces of
> compile-time polymorphism are implicit and based only on valid
> expressions, and are therefore provide unintentional points of
> customization and are harder to make correct.

-   Templates ***should not*** be used where there is a risk of object
    code size explosion.

-   ***Avoid*** using CV-qualified parameters with templates.

### Class Template Guidelines

-   ***Avoid*** non-type parameters on class templates as likely to
    promote code size explosion; should they be required, consider
    mitigation strategies.

-   ***Avoid*** over-specified template classes, implement only
    functions that are required now. Do not over-invest time and
    effort covering all possible operations for completeness; if
    someone needs a function in the future, they can add it. Untested
    code is not working code.

### Function Template Guidelines

-   Function templates ***should*** generally be implicitly inline
    (defined in a header file), given the restrictions in "Use of
    Templates".

### Container Guidelines

-   Type-safe containers which are expected to contain pointer types
    ***should*** have void\* partial specializations.

-   To repeat: ***avoid*** over-specification. If a function is not
    needed, don't implement it.

-   Containers should be relatively easily interchanged. The interface
    of a new container ***must*** follow the patterns of the
    interfaces to existing containers. (That is not to say there
    should be an interface class for them).
