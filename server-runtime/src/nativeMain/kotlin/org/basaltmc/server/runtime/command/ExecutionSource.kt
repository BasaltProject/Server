package org.basaltmc.server.runtime.command

import org.basaltmc.server.runtime.player.PlayerSession
import org.basaltmc.server.runtime.world.LoadedWorld
import org.basaltmc.wire.text.Component

/**
 * A command source with pieces swapped out, which is all `/execute` does.
 *
 * parity: `CommandSourceStack.withEntity`/`withPosition`/`withLevel`. Everything else is delegated
 * to whoever started the chain, so feedback still reaches them and permissions are still theirs:
 * `execute as <player>` runs *as* that player but not *with their permissions*, exactly as vanilla.
 */
public class ExecutionSource(
    private val base: CommandSource,
    override val player: PlayerSession? = base.player,
    override val position: Triple<Double, Double, Double> = base.position,
    /** The world this link of the chain acts in, or null to use whatever the base source would. */
    public val world: LoadedWorld? = (base as? ExecutionSource)?.world,
    public val rotation: Pair<Float, Float> = (base as? ExecutionSource)?.rotation ?: (0f to 0f),
    /** parity: `anchored`, which decides whether a position means the feet or the eyes. */
    public val anchorEyes: Boolean = (base as? ExecutionSource)?.anchorEyes ?: false,
) : CommandSource {
    override val name: String get() = player?.name ?: base.name
    override val displayName: Component get() = player?.let { Component.text(it.name) } ?: base.displayName
    override val permissionLevel: Int get() = base.permissionLevel

    override fun hasPermission(node: String?): Boolean = base.hasPermission(node)

    override fun sendSystemMessage(message: Component) {
        base.sendSystemMessage(message)
    }

    /** The source that started the chain, which is who feedback belongs to. */
    public val root: CommandSource get() = (base as? ExecutionSource)?.root ?: base
}

/** The world a source acts in: what `/execute in` chose, else the runner's own. */
public fun CommandSource.worldIn(environment: CommandEnvironment): LoadedWorld? {
    (this as? ExecutionSource)?.world?.let { return it }
    val name = player?.worldName
    return environment.worlds[name]
        ?: environment.worlds[environment.defaultWorldName]
        ?: environment.worlds.values.firstOrNull()
}
