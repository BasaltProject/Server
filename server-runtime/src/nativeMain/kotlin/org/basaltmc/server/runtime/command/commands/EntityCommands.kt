package org.basaltmc.server.runtime.command.commands

import kotlinx.serialization.json.Json
import org.basaltmc.server.assets.ServerJarAssets
import org.basaltmc.server.registry.EntityTypes
import org.basaltmc.server.runtime.command.Command
import org.basaltmc.server.runtime.command.CommandContext
import org.basaltmc.server.runtime.command.CommandNodeBuilder
import org.basaltmc.server.runtime.command.CommandSyntaxException
import org.basaltmc.server.runtime.command.CommandTarget
import org.basaltmc.server.runtime.command.Coordinates
import org.basaltmc.server.runtime.command.DoubleArgument
import org.basaltmc.server.runtime.command.EntitiesArgument
import org.basaltmc.server.runtime.command.EntityAnchor
import org.basaltmc.server.runtime.command.EntityAnchorArgument
import org.basaltmc.server.runtime.command.FloatArgument
import org.basaltmc.server.runtime.command.NbtCompoundArgument
import org.basaltmc.server.runtime.command.PermissionLevel
import org.basaltmc.server.runtime.command.RegistryArgument
import org.basaltmc.server.runtime.command.ResourceLocationArgument
import org.basaltmc.server.runtime.command.RotationArgument
import org.basaltmc.server.runtime.command.SuggestionContext
import org.basaltmc.server.runtime.command.SuggestionProviders
import org.basaltmc.server.runtime.command.Vec3Argument
import org.basaltmc.server.runtime.command.world
import org.basaltmc.server.runtime.entity.Attribute
import org.basaltmc.server.runtime.entity.AttributeModifier
import org.basaltmc.server.runtime.entity.AttributeOperation
import org.basaltmc.server.runtime.entity.ServerEntity
import org.basaltmc.server.runtime.player.PlayerRegistry
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.nbt.NbtFloat
import org.basaltmc.wire.protocol.play.AnimatePacket
import org.basaltmc.wire.protocol.play.SetEntityLinkPacket
import org.basaltmc.wire.text.Component
import org.basaltmc.wire.text.ComponentJson
import org.basaltmc.wire.text.PlainTextSerializer
import org.basaltmc.wire.text.TextColor
import kotlin.math.atan2
import kotlin.math.sqrt
import kotlin.uuid.ExperimentalUuidApi
import kotlin.uuid.Uuid

/**
 * parity: `SummonCommand`.
 *
 * What appears is a real entity of the right type at the right place, which is what the client
 * draws and what the other entity commands then act on. It has no AI and does not move on its own:
 * behaviour is the entity work, not the command's, and pretending otherwise here would be worse
 * than the gap.
 */
@OptIn(ExperimentalUuidApi::class)
public class SummonCommand(
    private val players: PlayerRegistry,
) : Command(name = "summon", permissionLevel = PermissionLevel.GAMEMASTERS) {
    override fun build(root: CommandNodeBuilder) {
        root.argument("entity", RegistryArgument.resource("minecraft:entity_type")) {
            suggestsFromClient(SuggestionProviders.SUMMONABLE_ENTITIES)
            executes { context -> summon(context, null) }
            argument("pos", Vec3Argument) {
                executes { context -> summon(context, context.get("pos")) }
                argument("nbt", NbtCompoundArgument) {
                    executes { context -> summon(context, context.get("pos")) }
                }
            }
        }
    }

    /**
     * The part of the NBT tail this build models. Everything else is reported rather than dropped
     * quietly: an entity that came out different from what was asked for is worth one line.
     */
    private fun applyNbt(
        context: CommandContext,
        entity: ServerEntity,
        nbt: NbtCompound,
    ) {
        val ignored = mutableListOf<String>()
        for (key in nbt.keys) {
            when (key) {
                "CustomName" -> entity.customName = plainName(nbt.getString(key))
                "Health" -> entity.health = (nbt[key] as? NbtFloat)?.value?.toDouble() ?: entity.health
                else -> ignored += key
            }
        }
        if (ignored.isNotEmpty()) {
            context.source.sendSystemMessage(
                Component.text("  Ignored, not modelled yet: ${ignored.joinToString(", ")}", TextColor.GRAY),
            )
        }
    }

    /** `CustomName` is a text component; only its plain text survives into an entity name here. */
    private fun plainName(value: String): String =
        runCatching { PlainTextSerializer.serialize(ComponentJson.deserialize(Json.parseToJsonElement(value))) }
            .getOrElse { value }

    private fun summon(
        context: CommandContext,
        coordinates: Coordinates?,
    ): Int {
        val id = context.get<Identifier>("entity")
        if (id == EntityTypes.PLAYER) {
            throw CommandSyntaxException(
                Component.translatable("commands.summon.failed.uuid"),
            )
        }
        val type =
            EntityTypes[id]
                ?: throw CommandSyntaxException(
                    Component.translatable("commands.summon.failed"),
                )
        val world = context.world()
        val (x, y, z) = coordinates?.resolve(context.source.position) ?: context.source.position
        val entity =
            ServerEntity(
                entityId = players.allocateEntityId(),
                uuid = Uuid.random(),
                type = id,
                protocolType = type.protocolId,
                x = x,
                y = y,
                z = z,
            )
        if (context.has("nbt")) applyNbt(context, entity, context.get("nbt"))
        world.state.spawnEntity(entity)
        context.success(
            Component.translatable("commands.summon.success", Component.text(id.toString())),
            broadcastToAdmins = true,
        )
        return 1
    }
}

/** parity: `DamageCommand`, minus the damage *type*, which needs the damage type registry to matter. */
public class DamageCommand(
    private val serverJar: ServerJarAssets? = null,
) : Command(name = "damage", permissionLevel = PermissionLevel.GAMEMASTERS) {
    override fun build(root: CommandNodeBuilder) {
        root.argument("target", EntitiesArgument(single = true)) {
            argument("amount", FloatArgument(min = 0f)) {
                executes { context -> damage(context) }
                argument("damageType", RegistryArgument.resource("minecraft:damage_type")) {
                    suggestsFromServer { context ->
                        context.matchingIds(serverJar?.dataIds("damage_type", ".json").orEmpty())
                    }
                    executes { context -> damage(context) }
                    // Where it came from and who dealt it. Neither changes the damage in this
                    // build, which has no knockback or attribution: they are accepted, not acted on.
                    literal("at") {
                        argument("location", Vec3Argument) { executes { context -> damage(context) } }
                    }
                    literal("by") {
                        argument("entity", EntitiesArgument(single = true)) {
                            executes { context -> damage(context) }
                            literal("from") {
                                argument("cause", EntitiesArgument(single = true)) {
                                    executes { context -> damage(context) }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    private fun damage(context: CommandContext): Int {
        val target = context.get<List<CommandTarget>>("target").single()
        val amount = context.get<Float>("amount").toDouble()
        when (target) {
            is CommandTarget.Player ->
                target.session.connection.hurt(amount.toFloat(), "generic", context.get<Identifier>("damageType"))
            // parity: an item entity takes damage from fire and explosions, neither of which this
            // reaches: `/damage` on one does nothing rather than pretending.
            is CommandTarget.Item -> Unit
            is CommandTarget.Entity -> {
                val entity = target.entity
                entity.health -= amount
                if (entity.health <= 0.0) {
                    entity.killed = true
                    entity.removed = true
                }
            }
        }
        context.success(
            Component.translatable(
                "commands.damage.success",
                Component.text(amount.toString()),
                Component.text(target.name),
            ),
            broadcastToAdmins = true,
        )
        return 1
    }
}

/** parity: `RideCommand`, whose two forms are mounting and dismounting. */
public class RideCommand : Command(name = "ride", permissionLevel = PermissionLevel.GAMEMASTERS) {
    override fun build(root: CommandNodeBuilder) {
        root.argument("target", EntitiesArgument(single = true)) {
            literal("mount") {
                argument("vehicle", EntitiesArgument(single = true)) { executes { context -> mount(context) } }
            }
            literal("dismount") { executes { context -> dismount(context) } }
        }
    }

    private fun mount(context: CommandContext): Int {
        val rider = context.get<List<CommandTarget>>("target").single()
        val vehicle = context.get<List<CommandTarget>>("vehicle").single()
        if (rider.entityId == vehicle.entityId) {
            throw CommandSyntaxException(Component.translatable("commands.ride.mount.failure.loop"))
        }
        val world = context.world()
        (vehicle as? CommandTarget.Entity)?.entity?.passengerIds?.add(rider.entityId)
        (rider as? CommandTarget.Entity)?.entity?.vehicleId = vehicle.entityId
        val packet = SetEntityLinkPacket(rider.entityId, vehicle.entityId)
        for (player in context.environment.playersIn(world)) {
            player.connection.sendPlayPacket(SetEntityLinkPacket.ID) { packet.write(it) }
        }
        context.success(
            Component.translatable(
                "commands.ride.mount.success",
                Component.text(rider.name),
                Component.text(vehicle.name),
            ),
            broadcastToAdmins = true,
        )
        return 1
    }

    private fun dismount(context: CommandContext): Int {
        val rider = context.get<List<CommandTarget>>("target").single()
        val entity = (rider as? CommandTarget.Entity)?.entity
        val vehicleId =
            entity?.vehicleId
                ?: throw CommandSyntaxException(
                    Component.translatable(
                        "commands.ride.not_riding",
                        Component.text(rider.name),
                    ),
                )
        entity.vehicleId = null
        context
            .world()
            .state
            .entity(vehicleId)
            ?.passengerIds
            ?.remove(rider.entityId)
        val vehicleName =
            context
                .world()
                .state
                .entity(vehicleId)
                ?.let { it.customName ?: it.type.path } ?: "?"
        val packet = SetEntityLinkPacket(rider.entityId, SetEntityLinkPacket.NO_VEHICLE)
        for (player in context.environment.playersIn(context.world())) {
            player.connection.sendPlayPacket(SetEntityLinkPacket.ID) { packet.write(it) }
        }
        context.success(
            Component.translatable(
                "commands.ride.dismount.success",
                Component.text(rider.name),
                Component.text(vehicleName),
            ),
            broadcastToAdmins = true,
        )
        return 1
    }
}

/** parity: `RotateCommand`, all three forms: an absolute rotation, facing a point, facing an entity. */
public class RotateCommand : Command(name = "rotate", permissionLevel = PermissionLevel.GAMEMASTERS) {
    override fun build(root: CommandNodeBuilder) {
        root.argument("target", EntitiesArgument(single = true)) {
            argument("rotation", RotationArgument) {
                executes { context ->
                    val (yaw, pitch) = context.get<Pair<Float, Float>>("rotation")
                    rotate(context, yaw, pitch)
                }
            }
            literal("facing") {
                argument("facingLocation", Vec3Argument) { executes { context -> facePosition(context) } }
                literal("entity") {
                    argument("facingEntity", EntitiesArgument(single = true)) {
                        executes { context -> faceEntity(context, eyes = false) }
                        argument("facingAnchor", EntityAnchorArgument) {
                            executes { context ->
                                faceEntity(context, context.get<EntityAnchor>("facingAnchor") == EntityAnchor.EYES)
                            }
                        }
                    }
                }
            }
        }
    }

    private fun facePosition(context: CommandContext): Int {
        val target = context.get<List<CommandTarget>>("target").single()
        val (x, y, z) = context.get<Coordinates>("facingLocation").resolve(context.source.position)
        val (yaw, pitch) = anglesTowards(target, x, y, z)
        return rotate(context, yaw, pitch)
    }

    private fun faceEntity(
        context: CommandContext,
        eyes: Boolean,
    ): Int {
        val target = context.get<List<CommandTarget>>("target").single()
        val other = context.get<List<CommandTarget>>("facingEntity").single()
        val y = other.y + if (eyes) EYE_HEIGHT else 0.0
        val (yaw, pitch) = anglesTowards(target, other.x, y, other.z)
        return rotate(context, yaw, pitch)
    }

    /** parity: `Entity.lookAt`, which measures from the feet unless an anchor says otherwise. */
    private fun anglesTowards(
        target: CommandTarget,
        x: Double,
        y: Double,
        z: Double,
    ): Pair<Float, Float> {
        val dx = x - target.x
        val dy = y - target.y
        val dz = z - target.z
        val horizontal = sqrt(dx * dx + dz * dz)
        // parity: `Entity.lookAt`, whose yaw is measured from +Z and grows clockwise.
        val yaw = (-atan2(dx, dz) * DEGREES_PER_RADIAN).toFloat()
        val pitch = (-(atan2(dy, horizontal) * DEGREES_PER_RADIAN)).toFloat()
        return yaw to pitch
    }

    private fun rotate(
        context: CommandContext,
        yaw: Float,
        pitch: Float,
    ): Int {
        val target = context.get<List<CommandTarget>>("target").single()
        when (target) {
            is CommandTarget.Player -> target.session.connection.teleportTo(target.x, target.y, target.z, yaw, pitch)
            is CommandTarget.Entity -> {
                target.entity.yaw = yaw
                target.entity.pitch = pitch
            }
            // A dropped item is drawn spinning on its own clock, so it has no rotation to set.
            is CommandTarget.Item -> Unit
        }
        context.success(
            Component.translatable("commands.rotate.success", Component.text(target.name)),
            broadcastToAdmins = true,
        )
        return 1
    }

    private companion object {
        const val DEGREES_PER_RADIAN = 57.2957795130823
        const val EYE_HEIGHT = 1.62
    }
}

/** parity: `SwingCommand`, which is the arm swing everyone else sees. */
public class SwingCommand : Command(name = "swing", permissionLevel = PermissionLevel.GAMEMASTERS) {
    override fun build(root: CommandNodeBuilder) {
        root.argument("targets", EntitiesArgument(single = false)) {
            executes { context -> swing(context, offhand = false) }
            literal("mainhand") { executes { context -> swing(context, offhand = false) } }
            literal("offhand") { executes { context -> swing(context, offhand = true) } }
        }
    }

    private fun swing(
        context: CommandContext,
        offhand: Boolean,
    ): Int {
        val targets = context.get<List<CommandTarget>>("targets")
        val action = if (offhand) AnimatePacket.SWING_OFFHAND else AnimatePacket.SWING_MAIN_HAND
        for (target in targets) {
            val packet = AnimatePacket(target.entityId, action)
            for (player in context.environment.playersIn(context.world())) {
                player.connection.sendPlayPacket(AnimatePacket.ID) { packet.write(it) }
            }
        }
        context.success(
            if (targets.size == 1) {
                Component.translatable("commands.swing.success.single", Component.text(targets.single().name))
            } else {
                Component.translatable("commands.swing.success.multiple", Component.text(targets.size.toString()))
            },
            broadcastToAdmins = true,
        )
        return targets.size
    }
}

/**
 * parity: `AttributeCommand`, base values and modifiers.
 *
 * A modifier is applied in vanilla's own order (`add_value`, then `add_multiplied_base` against the
 * base, then `add_multiplied_total` against the running total), so `get` reports the same number a
 * real server would.
 */
public class AttributeCommand : Command(name = "attribute", permissionLevel = PermissionLevel.GAMEMASTERS) {
    override fun build(root: CommandNodeBuilder) {
        root.argument("target", EntitiesArgument(single = true)) {
            argument("attribute", RegistryArgument.resource("minecraft:attribute")) {
                literal("get") {
                    executes { context -> get(context, 1.0, withModifiers = true) }
                    argument("scale", DoubleArgument(min = 0.0)) {
                        executes { context -> get(context, context.get("scale"), withModifiers = true) }
                    }
                }
                literal("base") {
                    literal("get") {
                        executes { context -> get(context, 1.0, withModifiers = false) }
                        argument("scale", DoubleArgument(min = 0.0)) {
                            executes { context -> get(context, context.get("scale"), withModifiers = false) }
                        }
                    }
                    literal("set") {
                        argument("value", DoubleArgument()) { executes { context -> set(context) } }
                    }
                    literal("reset") { executes { context -> reset(context) } }
                }
                literal("modifier") {
                    literal("add") {
                        argument("id", ResourceLocationArgument) {
                            argument("value", DoubleArgument()) {
                                for (operation in AttributeOperation.entries) {
                                    literal(operation.serializedName) {
                                        executes { context -> addModifier(context, operation) }
                                    }
                                }
                            }
                        }
                    }
                    literal("remove") {
                        argument("id", ResourceLocationArgument) {
                            suggestsFromServer { context -> context.matching(modifierIds(context)) }
                            executes { context -> removeModifier(context) }
                        }
                    }
                    literal("value") {
                        literal("get") {
                            argument("id", ResourceLocationArgument) {
                                suggestsFromServer { context -> context.matching(modifierIds(context)) }
                                executes { context -> modifierValue(context, 1.0) }
                                argument("scale", DoubleArgument(min = 0.0)) {
                                    executes { context -> modifierValue(context, context.get("scale")) }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    /** The modifiers the named entity already carries for the named attribute, and nothing else. */
    private fun modifierIds(context: SuggestionContext): Set<String> {
        val target = context.argument<List<CommandTarget>>("target")?.singleOrNull()
        val entity = (target as? CommandTarget.Entity)?.entity ?: return emptySet()
        val attribute = context.argument<Identifier>("attribute")?.toString() ?: return emptySet()
        return entity.attributeModifiers[attribute]?.keys.orEmpty()
    }

    private fun entityOf(context: CommandContext): ServerEntity {
        val target = context.get<List<CommandTarget>>("target").single()
        return (target as? CommandTarget.Entity)?.entity
            ?: throw CommandSyntaxException(
                Component.text(
                    "A player's attributes are not modelled yet, so this only works on a summoned entity.",
                ),
            )
    }

    private fun attributeId(context: CommandContext): String {
        val id = context.get<Identifier>("attribute").toString()
        if (Attribute.defaultFor(id) == null) {
            throw CommandSyntaxException(
                Component.translatable("attribute.unknown", Component.text(id)),
            )
        }
        return id
    }

    private fun value(
        entity: ServerEntity,
        id: String,
    ): Double = entity.attribute(id)

    private fun get(
        context: CommandContext,
        scale: Double,
        withModifiers: Boolean,
    ): Int {
        val entity = entityOf(context)
        val id = attributeId(context)
        val value = (if (withModifiers) entity.attributeWithModifiers(id) else value(entity, id)) * scale
        val key =
            if (withModifiers) {
                "commands.attribute.value.get.success"
            } else {
                "commands.attribute.base_value.get.success"
            }
        context.success(
            Component.translatable(
                key,
                Component.text(id),
                Component.text(entity.type.path),
                Component.text(value.toString()),
            ),
        )
        return value.toInt()
    }

    private fun set(context: CommandContext): Int {
        val entity = entityOf(context)
        val id = attributeId(context)
        val value = context.get<Double>("value")
        entity.attributeValues[id] = value
        if (id == Attribute.MAX_HEALTH) entity.health = minOf(entity.health, value)
        context.success(
            Component.translatable(
                "commands.attribute.base_value.set.success",
                Component.text(id),
                Component.text(entity.type.path),
                Component.text(value.toString()),
            ),
            broadcastToAdmins = true,
        )
        return value.toInt()
    }

    private fun addModifier(
        context: CommandContext,
        operation: AttributeOperation,
    ): Int {
        val entity = entityOf(context)
        val attribute = attributeId(context)
        val id = context.get<Identifier>("id").toString()
        val modifiers = entity.attributeModifiers.getOrPut(attribute) { mutableMapOf() }
        if (id in modifiers) {
            throw CommandSyntaxException(
                Component.translatable(
                    "commands.attribute.failed.modifier_already_present",
                    Component.text(id),
                    Component.text(attribute),
                    Component.text(entity.type.path),
                ),
            )
        }
        modifiers[id] = AttributeModifier(id, context.get("value"), operation)
        context.success(
            Component.translatable(
                "commands.attribute.modifier.add.success",
                Component.text(id),
                Component.text(attribute),
                Component.text(entity.type.path),
            ),
            broadcastToAdmins = true,
        )
        return 1
    }

    private fun removeModifier(context: CommandContext): Int {
        val entity = entityOf(context)
        val attribute = attributeId(context)
        val id = context.get<Identifier>("id").toString()
        if (entity.attributeModifiers[attribute]?.remove(id) == null) {
            throw CommandSyntaxException(
                Component.translatable(
                    "commands.attribute.failed.no_modifier",
                    Component.text(attribute),
                    Component.text(entity.type.path),
                    Component.text(id),
                ),
            )
        }
        context.success(
            Component.translatable(
                "commands.attribute.modifier.remove.success",
                Component.text(id),
                Component.text(attribute),
                Component.text(entity.type.path),
            ),
            broadcastToAdmins = true,
        )
        return 1
    }

    private fun modifierValue(
        context: CommandContext,
        scale: Double,
    ): Int {
        val entity = entityOf(context)
        val attribute = attributeId(context)
        val id = context.get<Identifier>("id").toString()
        val modifier =
            entity.attributeModifiers[attribute]?.get(id)
                ?: throw CommandSyntaxException(
                    Component.translatable(
                        "commands.attribute.failed.no_modifier",
                        Component.text(attribute),
                        Component.text(entity.type.path),
                        Component.text(id),
                    ),
                )
        val value = modifier.amount * scale
        context.success(
            Component.translatable(
                "commands.attribute.modifier.value.get.success",
                Component.text(id),
                Component.text(attribute),
                Component.text(entity.type.path),
                Component.text(value.toString()),
            ),
        )
        return value.toInt()
    }

    private fun reset(context: CommandContext): Int {
        val entity = entityOf(context)
        val id = attributeId(context)
        entity.attributeValues.remove(id)
        val value = value(entity, id)
        context.success(
            Component.translatable(
                "commands.attribute.base_value.reset.success",
                Component.text(id),
                Component.text(entity.type.path),
                Component.text(value.toString()),
            ),
            broadcastToAdmins = true,
        )
        return value.toInt()
    }
}
