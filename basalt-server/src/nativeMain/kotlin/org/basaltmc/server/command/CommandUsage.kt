package org.basaltmc.server.command

/**
 * The one-line syntax summary `/help` prints, e.g. `gamemode <gamemode> [<target>]`.
 *
 * parity: brigadier's `CommandDispatcher.getSmartUsage`, including its bracket rules: a branch
 * whose parent is already executable is optional and gets `[...]`, one that is not gets `(...)`,
 * and several alternatives are joined with `|`. Vanilla's `/help` output is exactly these strings
 * with a slash in front.
 */
internal fun smartUsage(
    node: CommandNode,
    source: CommandSource,
    optional: Boolean = false,
    deep: Boolean = false,
): String {
    val self = if (optional) "[${usageText(node)}]" else usageText(node)
    if (deep) return self

    val childOptional = node.executor != null
    val open = if (childOptional) "[" else "("
    val close = if (childOptional) "]" else ")"
    val children = (node.redirect ?: node).sortedChildren.filter { source.hasPermission(it.requiredLevel) }

    when {
        children.size == 1 -> return "$self ${smartUsage(children[0], source, childOptional, childOptional)}"
        children.size > 1 -> {
            val childUsages = children.map { smartUsage(it, source, childOptional, true) }.distinct()
            if (childUsages.size == 1) {
                val usage = childUsages.single()
                return "$self " + if (childOptional) "[$usage]" else usage
            }
            if (childUsages.size > 1) return "$self " + childUsages.joinToString("|", prefix = open, postfix = close)
        }
    }
    return self
}

private fun usageText(node: CommandNode): String = if (node is ArgumentNode) "<${node.name}>" else node.name
