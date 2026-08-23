package org.basaltmc.server.runtime.command.commands

import org.basaltmc.server.runtime.BuildInfo
import org.basaltmc.server.runtime.command.Command
import org.basaltmc.server.runtime.command.CommandNodeBuilder
import org.basaltmc.server.runtime.command.PermissionLevel
import org.basaltmc.wire.multiversion.ProtocolVersions
import org.basaltmc.wire.text.Component
import org.basaltmc.wire.text.TextColor

/**
 * parity: `VersionCommand` in shape only. Vanilla prints its data pack version and feature flags,
 * which do not exist here; this prints what this build actually is, which is the useful answer.
 */
public class VersionCommand(
    /** Every protocol this server lets in right now, the native one included. */
    private val accepted: () -> Set<Int> = { setOf(BuildInfo.PROTOCOL_VERSION) },
) : Command(name = "version", permissionLevel = PermissionLevel.GAMEMASTERS) {
    override fun build(root: CommandNodeBuilder) {
        root.executes { context ->
            context.success(
                Component.text(
                    "Basalt ${BuildInfo.VERSION} for Minecraft ${BuildInfo.MINECRAFT_VERSION} " +
                        "(protocol ${BuildInfo.PROTOCOL_VERSION})",
                ),
            )
            context.success(Component.text("  Clients accepted: ${describe(accepted())}", TextColor.GRAY))
            1
        }
    }

    /** Ranges rather than a list of fifty numbers: what an operator wants to read is the span. */
    private fun describe(protocols: Set<Int>): String {
        if (protocols.isEmpty()) return "none"
        val known = ProtocolVersions.known.filter { it.number in protocols }
        if (known.isEmpty()) return protocols.sorted().joinToString(", ")
        val oldest = known.first()
        val newest = known.last()
        val span = if (oldest == newest) oldest.label else "${oldest.label} to ${newest.label}"
        return "$span (${known.size} protocol version" + (if (known.size == 1) ")" else "s)")
    }
}
