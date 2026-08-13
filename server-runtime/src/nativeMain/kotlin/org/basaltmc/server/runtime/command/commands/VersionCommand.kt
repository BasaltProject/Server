package org.basaltmc.server.runtime.command.commands

import org.basaltmc.server.runtime.BuildInfo
import org.basaltmc.server.runtime.command.Command
import org.basaltmc.server.runtime.command.CommandNodeBuilder
import org.basaltmc.server.runtime.command.PermissionLevel
import org.basaltmc.wire.text.Component

/**
 * parity: `VersionCommand` in shape only. Vanilla prints its data pack version and feature flags,
 * which do not exist here; this prints what this build actually is, which is the useful answer.
 */
public class VersionCommand : Command(name = "version", permissionLevel = PermissionLevel.GAMEMASTERS) {
    override fun build(root: CommandNodeBuilder) {
        root.executes { context ->
            context.success(
                Component.text(
                    "Basalt for Minecraft ${BuildInfo.MINECRAFT_VERSION} (protocol ${BuildInfo.PROTOCOL_VERSION})",
                ),
            )
            1
        }
    }
}
