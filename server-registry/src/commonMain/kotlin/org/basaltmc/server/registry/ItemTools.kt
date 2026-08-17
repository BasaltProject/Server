package org.basaltmc.server.registry

import org.basaltmc.wire.common.Identifier

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
 * Tag membership is not known here (`server-registry` holds no datapack content), so it is asked
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
     * How fast [item] mines [block], as a multiplier on the bare-hand speed of 1.
     *
     * The dig itself is timed by the client, which knows this from its own copy of the same data.
     * The server needs it anyway to decide whether a block comes apart in one go: that is the one
     * moment vanilla asks the question server-side too.
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

    /**
     * How much of [block] one tick of mining takes off, 1 or more meaning it breaks at once.
     *
     * parity: `BlockState.getDestroyProgress` over `Player.getDestroySpeed`. Effects, attributes,
     * being underwater and being off the ground are not modelled, so this is the dry-land,
     * unaffected case: tool speed, the mining efficiency the enchantment adds, and the 30 or 100
     * divisor that makes the wrong tool slow rather than useless.
     */
    public fun destroyProgressPerTick(
        item: Identifier?,
        block: Identifier,
        hardness: Float,
        requiresCorrectTool: Boolean,
        efficiencyLevel: Int,
        /** parity: `MobEffectUtil.getDigSpeedAmplification`, or -1 for nobody hasted. */
        hasteAmplifier: Int = -1,
        /** parity: `MobEffects.MINING_FATIGUE`'s amplifier, or -1 for nobody slowed. */
        miningFatigueAmplifier: Int = -1,
        underwater: Boolean = false,
        inAir: Boolean = false,
        tagContains: (tag: String, block: Identifier) -> Boolean,
    ): Float {
        if (hardness < 0f) return 0f
        if (hardness == 0f) return Float.MAX_VALUE
        var speed = miningSpeed(item, block, tagContains)
        // parity: the enchantment only helps a tool that is already faster than a bare hand, and
        // its `minecraft:mining_efficiency` amount is `levels_squared` with one added.
        if (speed > 1f && efficiencyLevel > 0) speed += efficiencyLevel * efficiencyLevel + 1f
        if (hasteAmplifier >= 0) speed *= 1f + (hasteAmplifier + 1) * HASTE_PER_LEVEL
        if (miningFatigueAmplifier >= 0) speed *= fatigueScale(miningFatigueAmplifier)
        // parity: `Player.getDestroySpeed`: under water without the aqua affinity attribute it is a
        // fifth, and off the ground it is a fifth again.
        if (underwater) speed *= SUBMERGED_MINING_SPEED
        if (inAir) speed /= IN_AIR_DIVISOR
        val correct = !requiresCorrectTool || isCorrectForDrops(item, block, tagContains)
        return speed / hardness / if (correct) CORRECT_TOOL_DIVISOR else WRONG_TOOL_DIVISOR
    }

    /** parity: the four steps `MobEffects.MINING_FATIGUE` multiplies by. */
    private fun fatigueScale(amplifier: Int): Float =
        when (amplifier) {
            0 -> 0.3f
            1 -> 0.09f
            2 -> 0.0027f
            else -> 0.00081f
        }

    private const val HASTE_PER_LEVEL = 0.2f
    private const val SUBMERGED_MINING_SPEED = 0.2f
    private const val IN_AIR_DIVISOR = 5f
    private const val CORRECT_TOOL_DIVISOR = 30f
    private const val WRONG_TOOL_DIVISOR = 100f
}
