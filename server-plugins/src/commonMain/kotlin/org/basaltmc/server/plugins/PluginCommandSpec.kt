package org.basaltmc.server.plugins

/** What a plugin asked for in one argument of a command. */
public data class PluginCommandArgument(
    val name: String,
    val type: PluginArgumentType,
    val optional: Boolean,
    val min: Long? = null,
    val max: Long? = null,
)

/**
 * The argument types a plugin can ask for today.
 *
 * The list is short on purpose: these are the ones that need nothing from the world. The rest of
 * vanilla's types (players, positions, items, blocks) arrive with the object model, and asking for
 * one before then fails at load with a message that says so rather than at the first use.
 */
public enum class PluginArgumentType(
    public val spelling: String,
) {
    WORD("word"),
    TEXT("text"),
    INT("int"),
    FLOAT("float"),
    BOOL("bool"),
    ;

    public companion object {
        public fun of(spelling: String): PluginArgumentType? = entries.firstOrNull { it.spelling == spelling }
    }
}

/** One command of a plugin, as the loader understood it. */
public data class PluginCommandSpec(
    /** `arena create` is two: the command and one literal below it. */
    val literals: List<String>,
    val arguments: List<PluginCommandArgument>,
    val permission: String?,
) {
    public val name: String get() = literals.first()

    /** What the plugin wrote, put back together for logs and `/plugins`. */
    public val usage: String
        get() = (literals + arguments.map { spell(it) }).joinToString(" ")

    private fun spell(argument: PluginCommandArgument): String {
        val range = if (argument.min != null || argument.max != null) " ${argument.min}..${argument.max}" else ""
        val body = "${argument.name}: ${argument.type.spelling}$range"
        return if (argument.optional) "[$body]" else "<$body>"
    }

    public companion object {
        /**
         * Reads `arena create <name: word> [size: int 8..64]`.
         *
         * One string rather than a builder because the shape of a command is the one thing every
         * plugin gets right by copying, and a tree built call by call is the part of Bukkit nobody
         * enjoys.
         */
        public fun parse(
            spec: String,
            permission: String? = null,
        ): PluginCommandSpec {
            val tokens = tokenize(spec)
            if (tokens.isEmpty()) throw ManifestException("a command with no name")
            val literals = mutableListOf<String>()
            val arguments = mutableListOf<PluginCommandArgument>()
            for (token in tokens) {
                when {
                    token.startsWith("<") || token.startsWith("[") -> arguments += argumentOf(token, spec)
                    arguments.isNotEmpty() ->
                        throw ManifestException("'$spec' puts the word '$token' after an argument")

                    else -> literals += token
                }
            }
            if (literals.isEmpty()) throw ManifestException("'$spec' starts with an argument instead of a name")
            var seenOptional = false
            for (argument in arguments) {
                if (seenOptional && !argument.optional) {
                    throw ManifestException("'$spec' has the required '${argument.name}' after an optional one")
                }
                seenOptional = seenOptional || argument.optional
            }
            return PluginCommandSpec(literals, arguments, permission)
        }

        private fun argumentOf(
            token: String,
            spec: String,
        ): PluginCommandArgument {
            val optional = token.startsWith("[")
            val closing = if (optional) ']' else '>'
            if (!token.endsWith(closing)) throw ManifestException("'$spec' has an argument that never closes")
            val body = token.substring(1, token.length - 1)
            val name = body.substringBefore(':').trim()
            if (name.isEmpty()) throw ManifestException("'$spec' has an argument with no name")
            val rest =
                body
                    .substringAfter(':', "word")
                    .trim()
                    .split(' ')
                    .filter { it.isNotEmpty() }
            val typeName = rest.firstOrNull() ?: "word"
            val type =
                PluginArgumentType.of(typeName)
                    ?: throw ManifestException(
                        "'$spec' asks for the argument type '$typeName', which this server does not have yet",
                    )
            var min: Long? = null
            var max: Long? = null
            val range = rest.getOrNull(1)
            if (range != null && range.contains("..")) {
                min = range.substringBefore("..").toLongOrNull()
                max = range.substringAfter("..").toLongOrNull()
            }
            return PluginCommandArgument(name, type, optional, min, max)
        }

        /** Splits on spaces, except inside an argument: `<name: word>` is one token, spaces and all. */
        private fun tokenize(spec: String): List<String> {
            val out = mutableListOf<String>()
            val current = StringBuilder()
            var closing: Char? = null
            for (c in spec.trim()) {
                when {
                    closing != null -> {
                        current.append(c)
                        if (c == closing) {
                            out += current.toString()
                            current.clear()
                            closing = null
                        }
                    }

                    c == '<' || c == '[' -> {
                        if (current.isNotEmpty()) {
                            out += current.toString()
                            current.clear()
                        }
                        current.append(c)
                        closing = if (c == '<') '>' else ']'
                    }

                    c == ' ' -> {
                        if (current.isNotEmpty()) {
                            out += current.toString()
                            current.clear()
                        }
                    }

                    else -> current.append(c)
                }
            }
            if (current.isNotEmpty()) out += current.toString()
            return out
        }
    }
}
