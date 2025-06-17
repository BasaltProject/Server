package org.basaltmc.server.registry

/**
 * Whether a block state can stay where it is, from the real block classes' own `canSurvive`.
 *
 * The generated table holds one rule per distinct answer, and a rule says, per direction, what the
 * neighbour there has to be: any block with a sturdy face, one that supports a centre or a rigid
 * block, or a member of a listed set. A state survives when any one of those is satisfied, which is
 * what a probe of one neighbour at a time measures.
 *
 * parity: `BlockBehaviour.canSurvive`, minus the handful of blocks in [isModelled].
 */
public object BlockSupport {
    private const val NONE = 0
    private const val STURDY = 1
    private const val CENTRE = 2
    private const val RIGID = 3
    private const val FIRST_SET = 4

    /** 0 means it stands anywhere, 1 means a rule this build does not model. */
    private const val ALWAYS = 0
    private const val UNKNOWN = 1
    private const val FIRST_RULE = 2

    private const val DIRECTIONS = 6
    private const val INDEX_ALPHABET = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"
    private const val BASE64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"

    /** Twelve entries per rule: the six directions dry, then the same six in water. */
    private val rules: Array<IntArray>
    private val sets: Array<ByteArray>
    private val byState: IntArray
    private val randomTicking: BooleanArray

    /** Which block owns each state, filled on first use: only set-shaped rules need it. */
    private var blockOfState: IntArray? = null
    private var waterlike: BooleanArray? = null

    init {
        rules =
            Array(ALL_SUPPORT_RULES.size) { rule ->
                val fields = ALL_SUPPORT_RULES[rule].split('|')
                IntArray(DIRECTIONS * 2) { field ->
                    when (val token = fields.getOrElse(field) { "" }) {
                        "" -> NONE
                        "*" -> STURDY
                        "c" -> CENTRE
                        "r" -> RIGID
                        else -> FIRST_SET + token.removePrefix("#").toInt()
                    }
                }
            }
        sets = Array(ALL_SUPPORT_SETS.size) { decodeBase64(ALL_SUPPORT_SETS[it]) }
        byState = IntArray(BLOCK_SUPPORT_INDEX.length / 2)
        for (state in byState.indices) {
            val high = INDEX_ALPHABET.indexOf(BLOCK_SUPPORT_INDEX[state * 2])
            val low = INDEX_ALPHABET.indexOf(BLOCK_SUPPORT_INDEX[state * 2 + 1])
            byState[state] = high * INDEX_ALPHABET.length + low
        }
        randomTicking = BooleanArray((RANDOM_TICKING_STATES.maxOrNull() ?: 0) + 1)
        for (state in RANDOM_TICKING_STATES) randomTicking[state] = true
    }

    /** Whether anything has to hold this state up at all. */
    public fun needsSupport(stateId: Int): Boolean = ruleOf(stateId) != ALWAYS

    /**
     * Whether this build knows the state's rule.
     *
     * False for the few whose condition one neighbour at a time cannot see: the multi-face blocks
     * (glow lichen, sculk vein, resin clump), a piston head, a big dripleaf stem, and the two that
     * ask what must *not* be beside them (cactus, chorus flower). They are never broken.
     */
    public fun isModelled(stateId: Int): Boolean = ruleOf(stateId) != UNKNOWN

    /** Whether the state is water or holds it, which is what "there is water about" is asked of. */
    public fun holdsWater(stateId: Int): Boolean {
        val table = waterlike ?: buildWaterlike().also { waterlike = it }
        return stateId in table.indices && table[stateId]
    }

    /** parity: `BlockState.isRandomlyTicking`, which is what makes crops grow and ice melt. */
    public fun ticksRandomly(stateId: Int): Boolean = stateId in randomTicking.indices && randomTicking[stateId]

    /**
     * Whether [stateId] survives with those six neighbours around it.
     *
     * [wet] is for the blocks that live in or beside water (sugar cane, seagrass, a small
     * dripleaf): they answer against a second set of directions, and a state that only survives
     * there is kept rather than broken when the caller cannot tell.
     */
    public fun canSurvive(
        stateId: Int,
        down: Int,
        up: Int,
        north: Int,
        south: Int,
        west: Int,
        east: Int,
        wet: Boolean = false,
    ): Boolean {
        val rule = ruleOf(stateId)
        if (rule == ALWAYS || rule == UNKNOWN) return true
        val fields = rules[rule - FIRST_RULE]
        val slots = if (wet) DIRECTIONS * 2 else DIRECTIONS
        for (slot in 0 until slots) {
            val requirement = fields[slot]
            if (requirement == NONE) continue
            val face = BlockFace.entries[slot % DIRECTIONS]
            val neighbour =
                when (face) {
                    BlockFace.DOWN -> down
                    BlockFace.UP -> up
                    BlockFace.NORTH -> north
                    BlockFace.SOUTH -> south
                    BlockFace.WEST -> west
                    BlockFace.EAST -> east
                }
            if (satisfies(requirement, neighbour, face.opposite)) return true
        }
        return false
    }

    private fun satisfies(
        requirement: Int,
        neighbour: Int,
        towards: BlockFace,
    ): Boolean =
        when (requirement) {
            STURDY -> BlockShapes.isFaceSturdy(neighbour, towards)
            CENTRE -> BlockShapes.supportsCentre(neighbour, towards)
            RIGID -> BlockShapes.supportsRigid(neighbour, towards)
            else -> contains(sets[requirement - FIRST_SET], blockIdOf(neighbour))
        }

    private fun contains(
        set: ByteArray,
        blockId: Int,
    ): Boolean {
        val index = blockId / 8
        return index in set.indices && (set[index].toInt() shr (blockId % 8)) and 1 != 0
    }

    private fun ruleOf(stateId: Int): Int = if (stateId in byState.indices) byState[stateId] else ALWAYS

    /** A state's block registry id, which is what the listed sets are written in terms of. */
    private fun blockIdOf(stateId: Int): Int {
        val table = blockOfState ?: buildBlockOfState().also { blockOfState = it }
        return if (stateId in table.indices) table[stateId] else -1
    }

    private fun buildBlockOfState(): IntArray {
        val ids = HashMap<String, Int>(ALL_BLOCK_IDS.size)
        for ((id, path) in ALL_BLOCK_IDS) ids[path] = id
        val table = IntArray(byState.size) { -1 }
        for (state in table.indices) {
            val block = BlockStateDefinitions.blockOf(state) ?: continue
            table[state] = ids[block.path] ?: -1
        }
        return table
    }

    /** Water itself, plus every state carrying `waterlogged=true`. */
    private fun buildWaterlike(): BooleanArray {
        val table = BooleanArray(byState.size)
        for (state in table.indices) {
            val block = BlockStateDefinitions.blockOf(state) ?: continue
            // Every level of it, not just the source block, which is one state of sixteen.
            if (block.path == "water") {
                table[state] = true
                continue
            }
            val definition = BlockStateDefinitions.of(block) ?: continue
            if ("waterlogged" !in definition.propertyNames) continue
            table[state] = BlockStateDefinitions.propertiesOf(state)["waterlogged"] == "true"
        }
        return table
    }

    private fun decodeBase64(text: String): ByteArray {
        val trimmed = text.trimEnd('=')
        val bytes = ByteArray(trimmed.length * 6 / 8)
        var buffer = 0
        var bits = 0
        var written = 0
        for (character in trimmed) {
            buffer = (buffer shl 6) or BASE64.indexOf(character)
            bits += 6
            if (bits >= 8) {
                bits -= 8
                bytes[written++] = ((buffer shr bits) and 0xFF).toByte()
            }
        }
        return bytes
    }
}
