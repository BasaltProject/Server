package org.basaltmc.server.registry

import org.basaltmc.wire.common.Identifier

/**
 * Every block's properties and how its state ids run through them.
 *
 * The ids are the nested loops of the properties, outermost first, so a state id and a set of
 * values convert into each other with arithmetic instead of a table per family.
 *
 * parity: `StateDefinition`, which builds the same product in the same order.
 */
public object BlockStateDefinitions {
    /** One block: where its states start, and each property with the values it cycles through. */
    public class Definition internal constructor(
        public val block: Identifier,
        public val firstStateId: Int,
        internal val names: List<String>,
        internal val values: List<List<String>>,
    ) {
        internal val strides: IntArray =
            IntArray(names.size).also { strides ->
                var stride = 1
                for (index in names.indices.reversed()) {
                    strides[index] = stride
                    stride *= values[index].size
                }
            }

        public val stateCount: Int get() = if (names.isEmpty()) 1 else strides[0] * values[0].size

        public val propertyNames: List<String> get() = names

        public fun valuesOf(property: String): List<String> {
            val index = names.indexOf(property)
            return if (index < 0) emptyList() else values[index]
        }

        internal fun propertiesOf(stateId: Int): Map<String, String> {
            val offset = stateId - firstStateId
            val properties = LinkedHashMap<String, String>(names.size)
            for (index in names.indices) {
                properties[names[index]] = values[index][(offset / strides[index]) % values[index].size]
            }
            return properties
        }
    }

    private val byBlock: Map<Identifier, Definition>

    /** Every block's first state id, sorted, so a state id finds its owner by binary search. */
    private val starts: IntArray
    private val owners: Array<Definition?>
    private val ownerBlocks: Array<Identifier>

    init {
        val definitions = LinkedHashMap<Identifier, Definition>(ALL_BLOCK_STATE_DEFINITIONS.size)
        for (encoded in ALL_BLOCK_STATE_DEFINITIONS) {
            val fields = encoded.split('|', limit = 3)
            val block = Identifier.vanilla(fields[1])
            val names = mutableListOf<String>()
            val values = mutableListOf<List<String>>()
            for (property in fields[2].split(';')) {
                if (property.isEmpty()) continue
                val separator = property.indexOf('=')
                names += property.substring(0, separator)
                values += property.substring(separator + 1).split(',')
            }
            definitions[block] = Definition(block, fields[0].toInt(), names, values)
        }
        byBlock = definitions

        // A block with a single state is not in the table above, so its own id starts and ends it.
        val firsts = mutableListOf<Triple<Int, Identifier, Definition?>>()
        for ((stateId, path) in ALL_DEFAULT_BLOCK_STATES) {
            val block = Identifier.vanilla(path)
            val definition = definitions[block]
            firsts += Triple(definition?.firstStateId ?: stateId, block, definition)
        }
        firsts.sortBy { it.first }
        starts = IntArray(firsts.size) { firsts[it].first }
        ownerBlocks = Array(firsts.size) { firsts[it].second }
        owners = Array(firsts.size) { firsts[it].third }
    }

    public fun of(block: Identifier): Definition? = byBlock[block]

    /** Which block owns [stateId], properties or not. */
    public fun blockOf(stateId: Int): Identifier? = index(stateId)?.let { ownerBlocks[it] }

    /** The property values of [stateId], empty for a block that has none. */
    public fun propertiesOf(stateId: Int): Map<String, String> {
        val index = index(stateId) ?: return emptyMap()
        return owners[index]?.propertiesOf(stateId) ?: emptyMap()
    }

    /**
     * The state id of [block] with [properties] applied, the rest kept from [fromStateId] when it
     * belongs to the same block and from the block's first state otherwise.
     *
     * An unknown property or value is refused rather than ignored: silently placing a block facing
     * the wrong way is worse than not placing it.
     */
    public fun stateIdOf(
        block: Identifier,
        properties: Map<String, String>,
        fromStateId: Int? = null,
    ): Int? {
        val definition = byBlock[block] ?: return if (properties.isEmpty()) defaultOf(block) else null
        val current =
            if (fromStateId != null && blockOf(fromStateId) == block) {
                definition.propertiesOf(fromStateId)
            } else {
                definition.propertiesOf(definition.firstStateId)
            }
        var stateId = definition.firstStateId
        for (index in definition.names.indices) {
            val name = definition.names[index]
            val value = properties[name] ?: current.getValue(name)
            val position = definition.values[index].indexOf(value)
            if (position < 0) return null
            stateId += position * definition.strides[index]
        }
        if (properties.keys.any { it !in definition.names }) return null
        return stateId
    }

    private fun defaultOf(block: Identifier): Int? = Blocks.REGISTRY.idOf(block)

    private fun index(stateId: Int): Int? {
        var low = 0
        var high = starts.size - 1
        var found = -1
        while (low <= high) {
            val middle = (low + high) / 2
            if (starts[middle] <= stateId) {
                found = middle
                low = middle + 1
            } else {
                high = middle - 1
            }
        }
        if (found < 0) return null
        val definition = owners[found]
        val count = definition?.stateCount ?: 1
        return if (stateId < starts[found] + count) found else null
    }
}
