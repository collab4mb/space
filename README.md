
# Bikeshedding
- `snake_case_functions_and_variables`,
- `TypedefedCamelCaseStructsAndEnums`,
- two space tabs, no actual tab characters
- prefer [unity builds](https://en.wikipedia.org/wiki/Unity_build) where possible
- don't use `{}` for single statement blocks following `if`/`while` / `for` etc.
- `sokol` for cross-platform system abstractions (SDL2 is a pain to compile ... writing our own might make sense since collectively we have so much hardware to test on ... but I'd rather make a game, not an ~~engine~~ crossplatform abstraction layer)
- collaboration is done via github, all of us get merge permissions, but get major PRs reviewed before merging
