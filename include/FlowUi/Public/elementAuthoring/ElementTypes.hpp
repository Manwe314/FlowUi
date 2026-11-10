#pragma once

namespace FlowUi {

class App;

/** Empty builder parameter object used when an element omits Parameters. */
struct NoElementParameters {};

template <typename Element>
struct ElementBuildContext;

template <typename Element>
struct ElementInteractionContext;

} // namespace FlowUi
