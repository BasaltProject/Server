package org.basaltmc.server.runtime.config

import org.basaltmc.server.runtime.ui.UiConfigs
import org.basaltmc.wire.common.logging.Logger
import kotlin.concurrent.AtomicReference

/**
 * The configuration that is read on every use, as one immutable set.
 *
 * One snapshot swapped atomically rather than five fields swapped one at a time: a reload that
 * replaced groups and then commands would, for the instant between them, be running with the new
 * permissions and the old commands. Everything reads through [current], so a reader either sees the
 * whole old set or the whole new one.
 */
public data class ConfigSnapshot(
    public val groups: GroupsConfig,
    public val serverList: ServerListConfig,
    public val commands: CommandsConfig,
    public val animations: AnimationsConfig,
    public val ui: UiConfigs,
)

/**
 * What `/reload` replaces.
 *
 * The line between what reloads and what does not is not a judgement call, it is a property of the
 * code: a file whose values are read every time they are used can be swapped under a running
 * server, and a file whose values were read once at startup and turned into a socket, a thread pool
 * or a loaded world cannot. Reload does the first and refuses the second by name, which is the
 * difference between this and the reload command everyone learned to distrust.
 */
public class LiveConfigs(
    initial: ConfigSnapshot,
) {
    private val snapshot = AtomicReference(initial)

    public val current: ConfigSnapshot get() = snapshot.value

    public fun replace(next: ConfigSnapshot) {
        snapshot.value = next
    }

    public companion object {
        /** The files a reload re-reads. `server.toml` and `worlds.toml` are deliberately not among them. */
        public val RELOADABLE_FILES: List<String> =
            listOf(
                GroupsConfig.FILE_NAME,
                ServerListConfig.FILE_NAME,
                CommandsConfig.FILE_NAME,
                AnimationsConfig.FILE_NAME,
                TabListConfig.FILE_NAME,
                ScoreboardConfig.FILE_NAME,
                BossBarConfig.FILE_NAME,
            )

        /**
         * Reads all of them, or none.
         *
         * A file that does not parse throws, and the caller keeps what it already had: a reload that
         * applied the three files it managed to read and gave up on the fourth would leave a server
         * in a state no file describes.
         */
        public fun read(
            directory: ConfigDirectory = ConfigDirectory(),
            logger: Logger = Logger("Config"),
        ): ConfigSnapshot =
            ConfigSnapshot(
                groups = GroupsConfig.load(directory, logger),
                serverList = ServerListConfig.load(directory, logger = logger),
                commands = CommandsConfig.load(directory, logger),
                animations = AnimationsConfig.load(directory, logger),
                ui =
                    UiConfigs(
                        tabList = TabListConfig.load(directory, logger),
                        scoreboard = ScoreboardConfig.load(directory, logger),
                        bossBar = BossBarConfig.load(directory, logger),
                    ),
            )
    }
}
