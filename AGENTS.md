# Instructions for AI agents

## C++ naming

- Never use `b` as a prefix or suffix to denote a `bool`. Prefer concise names that are clear from their context, such as `Cancelled` or `Completed`; use predicates such as `IsVisible`, `HasFocus`, `CanRender`, or `ShouldUpdate` when needed for clarity.

## C++ documentation

- Write Doxygen documentation in English for public types, public members, and public functions.
- Follow ISO 24495-1 plain-language principles: state the purpose first, use short direct sentences, and use familiar terms.
- Use `@param`, `@return`, `@tparam`, and `@pre` only when they add information needed to use the API correctly.
- Document public data members when their purpose, unit, ownership, valid range, or relationship to another member is not immediately clear.
- Use a short single-sentence comment for private methods. Document private data only when it is necessary to explain an invariant or a non-obvious relationship.
