package org.basaltmc.server.runtime.world

import org.basaltmc.server.registry.BlockStateDefinitions
import org.basaltmc.wire.common.Identifier
import kotlin.math.abs
import kotlin.math.sqrt

/**
 * What a sculk sensor hears.
 *
 * parity: `VibrationSystem` over `SculkSensorBlock`. Every game event carries a frequency, and that
 * frequency is the signal the sensor gives off. It stays awake thirty ticks and takes ten to
 * settle, which is what stops one alarm covering a whole corridor.
 */
public object Vibrations {
    /** parity: `SculkSensorBlock.ACTIVE_TICKS` and `COOLDOWN_TICKS`. */
    public const val ACTIVE_TICKS: Int = 30
    public const val COOLDOWN_TICKS: Int = 10

    /** parity: `SculkSensorBlockEntity`'s own listener range. */
    public const val RANGE: Int = 8

    private val SENSOR = Identifier.vanilla("sculk_sensor")
    private val CALIBRATED = Identifier.vanilla("calibrated_sculk_sensor")

    /**
     * parity: `VibrationSystem.VIBRATION_FREQUENCY_FOR_EVENT`, the whole table. What is missing from
     * it is the events this build does not raise yet, not a different set of numbers.
     */
    private val FREQUENCIES: Map<String, Int> =
        mapOf(
            "step" to 1,
            "swim" to 1,
            "flap" to 1,
            "projectile_land" to 2,
            "hit_ground" to 2,
            "splash" to 2,
            "bounce" to 2,
            "item_interact_finish" to 3,
            "projectile_shoot" to 3,
            "instrument_play" to 3,
            "entity_action" to 4,
            "elytra_glide" to 4,
            "unequip" to 4,
            "entity_dismount" to 5,
            "equip" to 5,
            "entity_interact" to 6,
            "shear" to 6,
            "entity_mount" to 6,
            "entity_damage" to 7,
            "drink" to 8,
            "eat" to 8,
            "container_close" to 9,
            "block_close" to 9,
            "block_deactivate" to 9,
            "block_detach" to 9,
            "container_open" to 10,
            "block_open" to 10,
            "block_activate" to 10,
            "block_attach" to 10,
            "prime_fuse" to 10,
            "note_block_play" to 10,
            "block_change" to 11,
            "block_destroy" to 12,
            "fluid_pickup" to 12,
            "block_place" to 13,
            "fluid_place" to 13,
            "entity_place" to 14,
            "lightning_strike" to 14,
            "teleport" to 14,
            "entity_die" to 15,
            "explode" to 15,
        )

    /** The signal this event is worth, or zero for one nothing listens to. */
    public fun frequencyOf(event: String): Int = FREQUENCIES[event] ?: 0

    /**
     * Tells every sensor in range that something happened. Answers what changed, which the caller
     * sends out and settles the redstone around.
     *
     * parity: `VibrationSystem.Listener.handleGameEvent`: the sensor takes the vibration only while
     * it is inactive, and the signal it gives is the event's own frequency.
     */
    public fun emit(
        world: LoadedWorld,
        event: String,
        x: Double,
        y: Double,
        z: Double,
        changes: MutableList<BlockChange>,
    ) {
        val frequency = frequencyOf(event)
        if (frequency <= 0) return
        val blockX = x.toInt()
        val blockY = y.toInt()
        val blockZ = z.toInt()
        for (offsetY in -RANGE..RANGE) {
            for (offsetZ in -RANGE..RANGE) {
                for (offsetX in -RANGE..RANGE) {
                    val sensorX = blockX + offsetX
                    val sensorY = blockY + offsetY
                    val sensorZ = blockZ + offsetZ
                    val stateId = world.state.getBlockState(sensorX, sensorY, sensorZ) ?: continue
                    val block = BlockStateDefinitions.blockOf(stateId) ?: continue
                    if (block != SENSOR && block != CALIBRATED) continue
                    // parity: the listener's own radius is a sphere, not the cube walked here.
                    val distance =
                        sqrt(
                            (offsetX * offsetX + offsetY * offsetY + offsetZ * offsetZ).toDouble(),
                        )
                    if (distance > RANGE) continue
                    activate(world, sensorX, sensorY, sensorZ, stateId, block, frequency, changes)
                }
            }
        }
    }

    /** parity: `SculkSensorBlock.activate`, which lights it and books the thirty ticks it stays on. */
    private fun activate(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        stateId: Int,
        block: Identifier,
        frequency: Int,
        changes: MutableList<BlockChange>,
    ) {
        val properties = BlockStateDefinitions.propertiesOf(stateId)
        if (properties["sculk_sensor_phase"] != "inactive") return
        val turned =
            BlockStateDefinitions.stateIdOf(
                block,
                mapOf("sculk_sensor_phase" to "active", "power" to frequency.toString()),
                stateId,
            ) ?: return
        if (!world.state.setBlockState(x, y, z, turned)) return
        changes += BlockChange(x, y, z, turned)
        world.state.scheduleBlockTick(x, y, z, block, ACTIVE_TICKS)
    }

    /**
     * The scheduled half: an active sensor goes to cooldown and gives up its signal, and one already
     * cooling down goes quiet.
     *
     * parity: `SculkSensorBlock.tick` over `deactivate`.
     */
    public fun tick(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
    ): BlockChange? {
        val stateId = world.state.getBlockState(x, y, z) ?: return null
        val block = BlockStateDefinitions.blockOf(stateId) ?: return null
        if (block != SENSOR && block != CALIBRATED) return null
        val phase = BlockStateDefinitions.propertiesOf(stateId)["sculk_sensor_phase"] ?: return null
        val wanted =
            when (phase) {
                "active" -> mapOf("sculk_sensor_phase" to "cooldown", "power" to "0")
                "cooldown" -> mapOf("sculk_sensor_phase" to "inactive", "power" to "0")
                else -> return null
            }
        val turned = BlockStateDefinitions.stateIdOf(block, wanted, stateId) ?: return null
        if (!world.state.setBlockState(x, y, z, turned)) return null
        if (phase == "active") world.state.scheduleBlockTick(x, y, z, block, COOLDOWN_TICKS)
        return BlockChange(x, y, z, turned)
    }

    /** How far a vibration reaches, for the callers that filter before they emit. */
    public fun isInRange(
        sensorX: Int,
        sensorY: Int,
        sensorZ: Int,
        x: Double,
        y: Double,
        z: Double,
    ): Boolean = abs(sensorX - x) <= RANGE && abs(sensorY - y) <= RANGE && abs(sensorZ - z) <= RANGE
}
