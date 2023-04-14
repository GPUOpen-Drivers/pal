<!-- Copyright (c) 2022 Advanced Micro Devices, Inc. All Rights Reserved -->

# DevDriver Coding Standards

## 1. Introduction

This document establishes the coding standard for the DevDriver Team and is mainly concerned with C++. For Rust the
rules suggested by Cargo fmt are to be followed.

This Coding Standard is meant to be iterated on as required, make sure that this file is kept largely up to
date with the current team convention/consensus. Some rules can be enforced through the .clang-format, (**CL**)
shall be mentioned where this is applicable.

***

## 2. General

These are general formatting rules that are largely enforced by the .clang-format

* Lines **should** not exceed the 120 columns width limit (**CL**). This may be broken in cases where code looks
  cleaner going slightly over the limit. The penalty system in the format file can be tweaked to further loosen this.

* Spaces **should** be used instead of tabs (**CL**).

* Indents should be 4 spaces wide (**CL**).

* **Do not** add additional whitespace unless for alignment reasons. Formatting can add additional whitespace for
  alignment.

* An additional empty line can be added between lines for readability (**CL**). Formatting will clean up additional
  lines

## 3. General Language Restrictions

* Core DevDriver code ***must not*** throw any exceptions. Tool- & client-side code can.

* ***C++14*** is considered the minimum and all compilers must support the standard. Newer standards should not be used unless the PAL team updates their standard as this could lead to compatibility issues.

* `nullptr` ***should be used*** instead of `0` or `NULL` when assigning/comparing null pointer values.

* Try to *use common-convention & well-defined* C++ features where possible instead of creating multiple aliases for the
  same funcitonality.

* `goto` statement ***must not** be used

* *Avoid* keeping commented out lines of code where possible if these need to stay provide reasoning.

* Functions ***must have*** a single exit-point. Multiple `returns` within a function is not allowed.

***

## 4. Naming Conventions

Do not use single character prefixes that are not stated in this section.

* All pointer variables ***must*** be prefixed with a 'p'. Example: `SomeObject* pObject`.
  The pointer alignment ***should*** be as stated above (**CL**).

* All handle variables ***must*** be prefixed with a 'h'.

* All global variables ***must*** be prefixed with a "g_".

* Thread Local variables ***must*** be prefixed with "tls_".

* Static non-const member variables ***must*** be prefixed with "s_".

* Static const member variables ***must*** not be prefixed and ***must*** be declared in UpperCamelCase.

* Non Static member variables ***must*** be prefixed with "m_".

* Interface class names should be prefixed with an "I".

* All namespace, class, structure, typedef, enumeration, enumeration value, constexpr variable, static constant variable, static
constant member variable (class constant), function, and method names ***must*** use UpperCamelCase capitalization.

* All non-constexpr/non-static variable, member variable, and function parameter names ***must*** use lowerCamelCase capitalization.

* Any abbreviations used ***must*** follow the correct style as above and must be industry standard abbreviations, if unsure, dont abbreviate.

* Preprocessor Macros and Defines should be all UPPERCASE and use _ to seperate words, Example `#define SOME_MACRO_HERE`.

***

## 5. Documentation and Copyright

### 5.1 Copyright

All source and include files ***must contain*** the AMD copyright notice.

`Copyright (c) [StartYear]-[EndYear] Advanced Micro Devices, Inc. All Rights Reserved`

### 5.2 Documentation

* Comments ***should*** use `///`, `///<` should only be used for inline comments.

* Developers should update documentatian related to the changes they made when necessary.

* Classes ***should*** have a comment describing the purpose and the use of the class

* Functions ***should*** have a comment immediately preceding them that describes what it does and how to use it.
    * Inputs and outputs *should* be described.
    * Lifetime of references and pointers for the object should be described in relation to the function call.
    * If the function allocates memory that should be freed by the caller.
    * Whether an argument can be a null pointer.
    * Performance implications on usage of the function.
    * An assumptions made regarding synchronization.
    * Anything tricky about the actual implementation of the function.

* Comments ***should not*** be extremely verbose. Comments should be avoided for trivial self-explanatory code.

* Class data members that serve special purposes should have a comment describing them.
  If certain values have special significance that ***must*** be documented as well.

* Global variables should have a comment describing the use and the justification for it being global.

***

## 6. Namespaces

* The `using NameSpace` keywords ***should not*** be in a header file.

* The `using NameSpace` keywords ***should not*** precede an include directive.

* The contents of the namespace ***must not*** be indented.

* The closing bracket of the namespace should be followed by a comment identifying the namespace. (**CL**).
  ```cxx
  namespace NameHere
  {
    /* ... */
  } // namespace NameHere
***

## 7. Types and Declarations

* All definitions of must use the intrinsic type for the following types: char, bool, float, double and void.

* Typedefs for width specified integer types are defined in ddcdefs.h

* Global variables are *strongly discouraged*.

* `constexpr` ***should*** be used instead of macro definitions unless necessary.

* Pointer and References ***must*** be left-aligned ie:- `Object* pObject`. (**CL**)

* Each Definition *should* be declared on a seperate line

* Each statement *should* assign a value to no more than one variable. ie:- No a=b=c

* Local variables ***should*** be declared at first point of use.

* Local variables ***should*** be initialized with declaration where possible.

* Aggregate types (`struct`, `union`, etc.) ***must*** be initialized prior to use, whenever possible. Prefer empty initialization (`T foo = { }`) over just a declaration when an appropriate constructor is not available.

***

## 8. Functions

### 8.1 Static Functions

* Functions with internal linkage ***must*** be specified with static keyword.

* All static functions must be prototyped.

### 8.2 Formatting

* Function definitions ***must*** display each parameter on a separate line with indentation.

* In function definition, the return type and function name ***should*** reside on a single source line.

* Input parameters ***should*** be before the output parameters. Unless it is necessary to maintain a certain function signature.

* Declaration, definition, parameters *should* all be aligned properly for readability.

* Function definition ***should*** be documented by commenting.

* Functions should be broken down into smaller functions for readability, functions ***should not*** get excessively long or indented.
***

## 9. Classes

### 9.1 General

* Constructors and Destructors ***should not*** call virtual methods. It can result in undefined behavior.

* Access Modifiers ***should not*** be indented (**CL**).

### 9.2 Inheritance

* Multiple Inheritance ***should not*** be used.

* Overriden virtual functions ***should*** be declared with the `override` keyword.

* Any functions that don't need to be further overriden ***must*** be declared `final`.

* All destructors throughout the inheritance hierarchy ***must*** be declared virtual.

* Derived classes with no child class ***must*** be declared final.

### 9.3 Constructors and Destructors

* Constructors *must* only be used initializing the variables. For further complex actions use an init method instead.
  As a failure in the contructor will not call the Destructor.

* Initializer list *should* be declared in the same order as variables. The initialization takes place in that order
  regardless of the order of the list.

* Base class destructors ***should*** be public or protected, if protected declare a Destroy() function for object destruction.

### 9.4 Methods

* Definition of short methods that are never likely to change *can* reside in an include file. Such methods should not be
  more than 3-4 lines

* Inherited, non-virtual functions *should* not be redefined.

* Methods *should* not be overloaded unless for trivial functions where the alternative is uglier or error prone.

* Default arguments ***should not*** be used in vitual methods.

* Default arguments for non-virtual functions are *discouraged*.

* Class name, return type and method name must be on the same line for method definitions.

* Method definitions should display each parameter on separate lines with indentation.

* Calls ***must be*** formatted so there is no space between the name and opening parenthesis. (**CL**)

* There ***should not*** be any implicit conversions implemented with operator methods.

***

## 10. Const Usage

* Use `const` where possible.

* Floating point constants ***should*** be suffixed with "f" or "d" to indicate a float or double to prevent implicit
  conversion. Specifiers like "ul" "ull" should be used to prevent other implicit conversions.

* Variables with an invariant values *should* be declared with const where possible.

* Pointers to invariant data ***should*** declare the data invariant ex: "`const SomeObject* pObject`".

* Pointers with invariant address ***should*** be declared as const pointers ex: "`SomeObject* const pObject`".

* Methods that don't modify the logical object *should* be declared as const.

***

## 11. Casting

* C++ style casting operators ***must*** be used instead of C-style casts.

* *Minimize Casting*

* `dynamic_cast<T>` ***must not*** be used as RTTI should be disabled.

* `const_cast<T>` ***must*** not be used to eliminate constness of a variable. Where it is unavoidable, a justification **must** be provided as a comment.

***

## 12. Conditionals and Loops

* Space ***must*** be left after the conditional and before the opening bracket. However, no space should be left after the
  opening bracket. i.e.:- `if (a == b)`

* A for loop ***should not*** contain more than one initialization.

* Initialization of loop variables *should* be kept close to the loop.

* Braces must bracket the body of all conditionals/loops. Including single line blocks
  ```cxx
  if(..)
  {
  //loop body
  }
* Conditionals with sub expressions should be split across lines. i.e.:- use of `&&` / `||`

***

## 13. Concurrency and Thread Safety

* Code with thread safety requirements ***should*** be clearly documented as such.

* *Avoid* writing lock-free code.

* Code enclosed in critical sections *should* be kept to a minimum.

* *Avoid* calls to remote components inside critical sections.

* If a critical section is held during a call, it ***should*** be documented typically with an assert.

***

## 14. Misc

* *Prefer* compile-time asserts over runtime checks.

* Pointers *should* be set to `nullptr` after de-allocation.

* Template meta programming constructs ***must not*** be used.

* Templates ***should*** not be used where there is a risk of code size explosion.

* Forward Slashes (/) ***should*** be used for file paths.

* Files ***must*** be named in lowercase as this prevents issues with case-sensitivity.

***

## 15. Autoformatting

Documents can be formatted through your editor making use of the defined clang-format file. You may need to
install extensions to get this to work correctly.

* On *Visual Studio* the document can be formatted by clicking anywhere on the file and then
*Edit->Advanced->Format Document*.

* For Visual Studio Code you can right click anywhere on the document and a format document option will show up.

Feel free to add any editor specific tips to this section.

