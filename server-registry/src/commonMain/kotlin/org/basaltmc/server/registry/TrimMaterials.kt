package org.basaltmc.server.registry

import org.basaltmc.wire.common.Identifier

/**
 * Which trim material each item provides.
 *
 * parity: `DataComponents.PROVIDES_TRIM_MATERIAL`, a default component on the ingot or gem rather
 * than a list anywhere. An item absent from here cannot trim armour, which is the whole rule a
 * smithing trim recipe checks.
 */
public object TrimMaterials {
    private val byItem: Map<Identifier, Identifier> =
        ALL_TRIM_MATERIALS.associate { (item, material) ->
            Identifier.vanilla(item) to Identifier.vanilla(material)
        }

    public fun of(item: Identifier): Identifier? = byItem[item]
}
