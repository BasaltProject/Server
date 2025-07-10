package org.basaltmc.server.plugins

/**
 * The things only one plugin can be drawing at a time.
 *
 * The sidebar, the tab list header and footer, a boss bar, the MOTD, the chat format, the name over
 * a head. In Bukkit the last one to write wins, there is no way to find out who that was, and the
 * result is the classic two plugins fighting over the scoreboard.
 */
public enum class PluginSurface(
    public val surfaceName: String,
) {
    SIDEBAR("sidebar"),
    TAB_LIST("tab_list"),
    BOSS_BAR("boss_bar"),
    CHAT_FORMAT("chat_format"),
    NAME_TAG("name_tag"),
    ;

    public companion object {
        public fun of(name: String): PluginSurface? = entries.firstOrNull { it.surfaceName == name }
    }
}

/**
 * One plugin's claim on one surface.
 *
 * [priority] is asked for and never defaulted: a hidden default is how two plugins end up tied at
 * zero and the winner becomes whoever the file system listed first.
 */
public class PluginSurfaceClaim(
    public val pluginId: String,
    public val surface: PluginSurface,
    public val priority: Int,
    /** Whether it wants the surface for this player at all. Null means always. */
    public val appliesTo: ((PluginPlayerRef) -> Boolean)?,
    /** What to draw. Null means the claim is up but has nothing to say, which counts as not applying. */
    public val render: (PluginPlayerRef) -> PluginSurfaceContent?,
) {
    /** Set when the owner dies or oversteps: it keeps its claim but stops being asked. */
    public var yielded: Boolean = false

    public fun wants(player: PluginPlayerRef): Boolean = !yielded && (appliesTo?.invoke(player) ?: true)
}

/** A title and some lines, which is what every one of these surfaces comes down to. */
public data class PluginSurfaceContent(
    val title: String = "",
    val lines: List<String> = emptyList(),
)

/**
 * Who is drawing what, and who is next in line.
 *
 * The rule the config files already use: highest priority among the claims that apply, declaration
 * order breaking a tie. Losing is said out loud rather than silently, the loser keeps its claim and
 * takes the surface back when the one above lets go, and a holder that falls over lets go.
 */
public class PluginSurfaceRegistry(
    private val warn: (String) -> Unit = {},
) {
    private val claims = mutableListOf<PluginSurfaceClaim>()

    public fun claim(claim: PluginSurfaceClaim): PluginHandle {
        val rivals = claims.filter { it.surface == claim.surface && it.pluginId != claim.pluginId }
        claims += claim
        for (rival in rivals) {
            if (rival.priority == claim.priority) {
                warn(
                    "${claim.pluginId} and ${rival.pluginId} both claim the ${claim.surface.surfaceName} at " +
                        "priority ${claim.priority}; ${rival.pluginId} keeps it because it asked first. " +
                        "A tie means one of them was missing a priority.",
                )
            } else {
                val winner = if (claim.priority > rival.priority) claim else rival
                val loser = if (winner === claim) rival else claim
                warn(
                    "${winner.pluginId} takes the ${claim.surface.surfaceName} from ${loser.pluginId} " +
                        "(${winner.priority} over ${loser.priority}); ${loser.pluginId} gets it back if " +
                        "${winner.pluginId} lets go.",
                )
            }
        }
        return PluginHandle { claims.remove(claim) }
    }

    /** Everything registered, holder first, for `/plugins surfaces`. */
    public fun all(): List<PluginSurfaceClaim> =
        claims.sortedWith(compareByDescending<PluginSurfaceClaim> { it.priority })

    public fun claimsOn(surface: PluginSurface): List<PluginSurfaceClaim> = claims.filter { it.surface == surface }

    /**
     * Whether anybody at all is claiming [surface].
     *
     * The cheap question, asked every tick from a connection thread: it looks at nothing but the
     * list, and in particular never runs a plugin's `when`, which is what [holder] does.
     */
    public fun anyClaimOn(surface: PluginSurface): Boolean = claims.any { it.surface == surface && !it.yielded }

    /** Who draws [surface] for [player] right now, or null if nobody wants it. */
    public fun holder(
        surface: PluginSurface,
        player: PluginPlayerRef,
    ): PluginSurfaceClaim? =
        claims
            .filter { it.surface == surface && it.wants(player) }
            // maxByOrNull keeps the first of equals, which is declaration order, which is the
            // tie-break the config files already use.
            .maxByOrNull { it.priority }

    /** What to draw, skipping past a claim that applies but has nothing to say. */
    public fun content(
        surface: PluginSurface,
        player: PluginPlayerRef,
    ): Pair<PluginSurfaceClaim, PluginSurfaceContent>? {
        val candidates =
            claims
                .filter { it.surface == surface && it.wants(player) }
                .sortedWith(compareByDescending { it.priority })
        for (claim in candidates) {
            val content = claim.render(player) ?: continue
            return claim to content
        }
        return null
    }

    /** The holder fell over: it keeps its claim and stops being drawn until it is loaded again. */
    public fun yield(
        pluginId: String,
        why: String,
    ) {
        var any = false
        for (claim in claims.filter { it.pluginId == pluginId && !it.yielded }) {
            claim.yielded = true
            any = true
        }
        if (any) {
            warn("$pluginId let go of what it was drawing: $why. Whoever was below it takes over.")
        }
    }
}
