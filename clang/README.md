# C language Family Front-end

Welcome to Clang. 

This is a compiler front-end for the C family of languages (C, C++ and Objective-C) which is built as part of the LLVM compiler infrastructure project.

Unlike many other compiler frontends, Clang is useful for a number of things beyond just compiling code: we intend for Clang to be host to a number of different           source-level tools. One example of this is the Clang Static Analyzer.

If you're interested in more (including how to build Clang) it is best to read the relevant websites. Here are some pointers:

* Information on Clang:      http://clang.llvm.org/

* Building and using Clang:    http://clang.llvm.org/get_started.html

* Clang Static Analyzer:    http://clang-analyzer.llvm.org/

* Information on the LLVM project:    http://llvm.org/

* If you have questions or comments about Clang, a great place to discuss them is on the Clang forums:    
  
  [Clang Frontend - LLVM Discussion Forums](https://discourse.llvm.org/c/clang/)

* If you find a bug in Clang, please file it in the LLVM bug tracker:
  
    https://github.com/llvm/llvm-project/issues

## Cilk support

This version of Clang supports the `cilk_spawn`, `cilk_sync`, `cilk_scope`, and `cilk_for` keywords from Cilk.  In particular, this version of Clang supports the use of `cilk_spawn` before a function call in a statement, an assignment, or a declaration, as in the following examples:

```
cilk_spawn foo(n);
```

```
x = cilk_spawn foo(n);
```

```
int x = cilk_spawn foo(n);
```

When spawning a function call, the call arguments and function arguments are evaluated before the spawn occurs.  When spawning an assignment or declaration, the LHS is also evaluated before the spawn occurs.

For convenience, this version of Clang allows `cilk_spawn` to spawn an arbitrary statement, as follows:

```
cilk_spawn { x = foo(n); }
```

Please use this syntax with caution!  When spawning an arbitrary statement, the spawn occurs before the evaluation of any part of the spawned statement.  Furthermore, some statements, such as `goto`, are not legal to spawn.
