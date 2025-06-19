package org.basaltmc.server.registry

import org.basaltmc.wire.common.Identifier

/**
 * What burns in a furnace and for how long.
 *
 * parity: `FuelValues.vanillaBurnTimes`. Vanilla builds this table in code and not in data, so the
 * generated list is that call's own answer, tags already flattened and the non-flammable woods
 * already taken back out.
 */
public object Fuels {
    private val ticksByItem: Map<String, Int> = ALL_FUEL_VALUES.toMap()

    /** How long [item] keeps a furnace lit, or 0 when it is not fuel. */
    public fun burnTicks(item: Identifier): Int = ticksByItem[item.path] ?: 0

    public fun isFuel(item: Identifier): Boolean = burnTicks(item) > 0

    /** Every fuel there is, for `/give`-style listing and for tests that want the whole set. */
    public val all: Set<String> get() = ticksByItem.keys
}
