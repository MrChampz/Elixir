# Instructions for AI agents

## C++ naming

- Never use `b` as a prefix or suffix to denote a `bool`. Prefer concise names that are clear from their context, such as `Cancelled` or `Completed`; use predicates such as `IsVisible`, `HasFocus`, `CanRender`, or `ShouldUpdate` when needed for clarity.

## C++ formatting

- Keep all function parameters or call arguments on the same line when they fit within the 95-character line limit. Otherwise, place each parameter or argument on its own line, with the opening parenthesis after the function name and the closing parenthesis on a separate line. Do not mix multiple parameters or arguments on one line with others on separate lines. Apply this rule to declarations, definitions, constructors, and calls.
- Do not reformat existing code without the user's confirmation.
- Never place a lambda inside an `if` condition or a `return` expression, including lambdas passed to algorithms such as `std::ranges::all_of`. Evaluate the expression first and store its result in a clearly named variable, then use that variable in the condition or return statement.

```cpp
void FuncX(
    int x,
    int y,
    int z
)
{
}
```

## Engine error handling

- Do not add `static_assert` to Engine code. Keep compile-time validation in tests.
- Never use exceptions in Engine code. Do not introduce `throw`, `try`, or `catch`.
- Use `EE_CORE_ERROR` for recoverable errors and `EE_CORE_ASSERT` for violated internal invariants. Error paths must stop safely without relying on assertions being enabled.

## C++ documentation

- Write Doxygen documentation in English for public types, public members, and public functions.
- Follow ISO 24495-1 plain-language principles: state the purpose first, use short direct sentences, and use familiar terms.
- Document every parameter of every function and method with `@param`, including constructors and private methods.
- Use `@return`, `@tparam`, and `@pre` when they add information needed to use the API correctly.
- Use `@brief` for functions and methods. Do not use `@brief` for struct data members; write their descriptions directly.
- Keep documentation within the 95-character line limit configured in `.clang-format`. If a single-line or trailing documentation comment would exceed that limit, use a multiline documentation block and wrap its text.
- When there is no description beyond `@brief`, place `@param` and other tags on consecutive lines immediately after `@brief`, without a blank line.
- When there is a description beyond `@brief`, leave one blank documentation line between `@brief` and that description, and another between the description and the parameter tags.
- Document public data members when their purpose, unit, ownership, valid range, or relationship to another member is not immediately clear.
- Keep private method summaries to a short single sentence and document all their parameters. Document private data only when it is necessary to explain an invariant or a non-obvious relationship.

Without an additional description:

```cpp
/**
 * @brief Advance the animation playback.
 * @param deltaSeconds Elapsed time in seconds since the previous update.
 */
void Update(float deltaSeconds);
```

With an additional description:

```cpp
/**
 * @brief Advance the animation playback.
 *
 * Sample the current sequence and rebuild the model pose.
 *
 * @param deltaSeconds Elapsed time in seconds since the previous update.
 */
void Update(float deltaSeconds);
```

For a struct data member:

```cpp
/** Maximum number of bone matrices uploaded in one frame. */
uint32_t BoneCapacity = 65'536;
```

## Validation

- This project targets macOS and Windows. Validate changes for Windows compilation and linking implications even when only macOS is available locally; the CI runs tests on Windows.
- When a public API crosses a shared-library boundary, verify that its out-of-line symbols use `ELIXIR_API` where Windows DLL export and import require it.
