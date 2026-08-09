package org.basaltmc.server.config

import org.basaltmc.common.logging.Logger

internal const val GROUPS_DEFAULT_TOML = """# Permission groups.
#
# This file says what a group IS. Who belongs to it is player state, so it lives in
# players.db and is set with `op`, `deop` or `group`, never by editing this file.
#
# A permission is a dotted name. A trailing `*` matches everything below it, and a
# name prefixed with `-` denies instead of granting. The most specific rule wins, so
# a denial of `basalt.command.stop` beats a grant of `basalt.command.*` regardless of
# the order they are written in.
#
# A permission nobody grants is denied. There is no hidden ranking behind these
# groups: what a group can do is exactly what is written here.

[groups.default]
# The group everyone starts in. Exactly one group must be marked as the default.
default = true
permissions = [
    "basalt.command.help",
    "basalt.command.list",
    "basalt.command.me",
]

[groups.operator]
# What `op` assigns. Exactly one group must be marked as the operator group.
operator = true
permissions = ["*"]

# Groups can inherit from another, and add or take away from what they got:
# [groups.moderator]
# inherits = "default"
# permissions = ["basalt.command.kick", "basalt.command.teleport", "-basalt.command.seed"]
"""

/**
 * One permission rule: a dotted name, possibly ending in `*`, granting or denying.
 *
 * Specificity is the number of segments before any wildcard, so `basalt.command.stop` (3) beats
 * `basalt.command.*` (2) beats `basalt.*` (1). That ordering is what lets a group be written as
 * "everything except this one thing" without depending on the order of the list.
 */
public data class PermissionRule(
    public val pattern: String,
    public val granted: Boolean,
) {
    private val prefix: String = pattern.removeSuffix("*").removeSuffix(".")
    private val isWildcard: Boolean = pattern.endsWith("*")

    public val specificity: Int = if (prefix.isEmpty()) 0 else prefix.count { it == '.' } + 1

    public fun matches(node: String): Boolean =
        if (isWildcard) prefix.isEmpty() || node == prefix || node.startsWith("$prefix.") else node == pattern

    public companion object {
        public fun parse(raw: String): PermissionRule {
            val denied = raw.startsWith("-")
            return PermissionRule(raw.removePrefix("-").trim(), granted = !denied)
        }
    }
}

/** A named set of permissions. */
public data class PermissionGroup(
    public val name: String,
    public val rules: List<PermissionRule>,
    public val inherits: String?,
    public val isDefault: Boolean,
    public val isOperator: Boolean,
)

/**
 * `config/groups.toml`, resolved: every group with its inheritance already flattened.
 *
 * Membership is deliberately not here. A group definition is something the operator writes; which
 * group a player is in is something the server writes, so it belongs in `players.db`. Keeping the
 * two apart is what removed the old `[server] operators` list, which the server had to rewrite by
 * hand every time somebody ran `op`.
 */
public class GroupsConfig(
    private val groups: Map<String, PermissionGroup>,
    public val defaultGroup: PermissionGroup,
    public val operatorGroup: PermissionGroup,
) {
    public val names: Set<String> get() = groups.keys

    public operator fun get(name: String?): PermissionGroup = groups[name?.lowercase()] ?: defaultGroup

    /**
     * Whether [groupName] grants [node].
     *
     * Rules from the group itself win over inherited ones at equal specificity, so a group can
     * override what it inherits without having to be more specific than its parent. Nothing
     * matching at all is a denial: what a group can do is exactly what its file says, and a
     * permission nobody granted has not been granted.
     */
    public fun resolve(
        groupName: String?,
        node: String,
    ): Boolean {
        var best: PermissionRule? = null
        var bestDepth = Int.MAX_VALUE
        var depth = 0
        var current: PermissionGroup? = this[groupName]
        val seen = mutableSetOf<String>()

        while (current != null && seen.add(current.name)) {
            for (rule in current.rules) {
                if (!rule.matches(node)) continue
                val better =
                    best == null ||
                        rule.specificity > best.specificity ||
                        (rule.specificity == best.specificity && depth < bestDepth)
                if (better) {
                    best = rule
                    bestDepth = depth
                }
            }
            current = current.inherits?.let { groups[it] }
            depth++
        }
        return best?.granted ?: false
    }

    public companion object {
        public const val FILE_NAME: String = "groups.toml"

        public fun load(
            directory: ConfigDirectory = ConfigDirectory(),
            logger: Logger = Logger("Config"),
        ): GroupsConfig {
            val root = directory.load(FILE_NAME, GROUPS_DEFAULT_TOML)
            val declared =
                root.tableOrNull("groups")
                    ?: throw TomlFormatException("${directory.pathOf(FILE_NAME)} declares no [groups] table.")

            val groups =
                declared.keys().mapNotNull { name ->
                    val table = declared.tableOrNull(name) ?: return@mapNotNull null
                    PermissionGroup(
                        name = name.lowercase(),
                        rules = table.stringListOrNull("permissions").orEmpty().map(PermissionRule::parse),
                        inherits = table.stringOrNull("inherits")?.lowercase(),
                        isDefault = table.boolOrNull("default") ?: false,
                        isOperator = table.boolOrNull("operator") ?: false,
                    )
                }

            val path = directory.pathOf(FILE_NAME)
            if (groups.isEmpty()) throw TomlFormatException("$path declares no groups.")
            val byName = groups.associateBy { it.name }

            for (group in groups) {
                val parent = group.inherits ?: continue
                if (parent !in byName) {
                    throw TomlFormatException("Group '${group.name}' inherits from '$parent', which does not exist.")
                }
                // A cycle would otherwise be an infinite walk the first time anyone is checked.
                var seen = mutableSetOf(group.name)
                var current = byName[parent]
                while (current != null) {
                    if (!seen.add(current.name)) {
                        throw TomlFormatException("Group inheritance in $path forms a cycle through '${current.name}'.")
                    }
                    current = current.inherits?.let { byName[it] }
                }
            }

            val defaults = groups.filter { it.isDefault }
            if (defaults.size != 1) {
                throw TomlFormatException("$path must mark exactly one group as `default`, found ${defaults.size}.")
            }
            val operators = groups.filter { it.isOperator }
            if (operators.size != 1) {
                throw TomlFormatException("$path must mark exactly one group as `operator`, found ${operators.size}.")
            }
            logger.debug { "Loaded ${groups.size} permission group(s) from $path." }
            return GroupsConfig(byName, defaults.single(), operators.single())
        }
    }
}
