package org.basaltmc.server.runtime.command.commands

import org.basaltmc.server.runtime.command.Command
import org.basaltmc.server.runtime.command.CommandNodeBuilder
import org.basaltmc.server.runtime.command.PermissionLevel
import org.basaltmc.wire.text.Component

/** parity: `SeedCommand`, which on a dedicated server needs game-master level. */
public class SeedCommand : Command(name = "seed", permissionLevel = PermissionLevel.GAMEMASTERS) {
    override fun build(root: CommandNodeBuilder) {
        root.executes { context ->
            val seed = context.environment.worldSeed
            context.success(Component.translatable("commands.seed.success", Component.text(seed.toString())))
            seed.toInt()
        }
    }
}
