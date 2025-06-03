package org.basaltmc.registry

import org.basaltmc.common.Identifier

/**
 * One rule of an item's `minecraft:tool` component: a block tag, whether the tool counts as the
 * correct one for that tag's drops, and how fast it mines it (`null` when the rule only excludes).
 */
public data class ToolRule(
    public val blockTag: String,
    public val correctForDrops: Boolean,
    public val speed: Float?,
)

/**
 * Which blocks each tool mines properly, and how fast.
 *
 * parity: `ToolComponent`. Both answers come from the **first** rule whose tag contains the block,
 * which is what makes a diamond pickaxe correct for stone but not for the handful of blocks its
 * `incorrect_for_diamond_tool` tag excludes: that exclusion is listed first and therefore wins.
 *
 * Tag membership is not known here (`basalt-registry` holds no datapack content), so it is asked
 * for: the server resolves the real block tags out of the operator's own jar and passes a lookup in.
 */
public object ItemTools {
    private val rulesByItem: Map<Identifier, List<ToolRule>> =
        ALL_ITEM_TOOLS.associate { (path, rules) -> Identifier.vanilla(path) to rules }

    /** This item's mining rules, or empty if it is not a tool at all. */
    public fun rulesFor(item: Identifier): List<ToolRule> = rulesByItem[item].orEmpty()

    /**
     * Whether holding [item] means [block] drops what it should.
     *
     * A block that does not require a tool at all drops regardless, so callers check that first;
     * this answers only the tool half. An item with no rules (a bare hand, a stick, a diamond) is
     * never correct, which is exactly why stone gives nothing when punched.
     */
    public fun isCorrectForDrops(
        item: Identifier?,
        block: Identifier,
        tagContains: (tag: String, block: Identifier) -> Boolean,
    ): Boolean {
        if (item == null) return false
        val rule = rulesFor(item).firstOrNull { tagContains(it.blockTag, block) } ?: return false
        return rule.correctForDrops
    }

    /**
     * How fast [item] mines [block], as a multiplier on the bare-hand speed of 1. Only reported for
     * completeness: the dig itself is timed by the client, which knows this from its own copy of
     * the same data.
     */
    public fun miningSpeed(
        item: Identifier?,
        block: Identifier,
        tagContains: (tag: String, block: Identifier) -> Boolean,
    ): Float {
        if (item == null) return 1f
        val rule = rulesFor(item).firstOrNull { tagContains(it.blockTag, block) } ?: return 1f
        return rule.speed ?: 1f
    }
}
