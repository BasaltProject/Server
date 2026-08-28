package org.basaltmc.server.runtime.config

import org.basaltmc.wire.auth.forwarding.ForwardingMode
import org.basaltmc.wire.auth.forwarding.ProxyForwarding
import org.basaltmc.wire.common.logging.Logger
import org.basaltmc.wire.common.toml.TomlFormatException
import org.basaltmc.wire.common.toml.TomlValue

/** `[network.forwarding]` in `config/server.toml`: which proxy, if any, speaks for the players. */
public object ForwardingConfig {
    /** Reads `[network.forwarding]`, or [ProxyForwarding.DISABLED] when nothing is configured. */
    public fun fromToml(
        root: TomlValue.Table,
        path: String,
    ): ProxyForwarding {
        val table = root.tableOrNull("network")?.tableOrNull("forwarding") ?: return ProxyForwarding.DISABLED
        val written = table.stringOrNull("mode") ?: ForwardingMode.NONE.configName
        val mode =
            ForwardingMode.parse(written)
                ?: throw TomlFormatException(
                    "$path: forwarding mode must be one of " +
                        ForwardingMode.entries.joinToString(", ") { it.configName } +
                        ", not \"$written\"",
                )
        if (mode == ForwardingMode.NONE) return ProxyForwarding.DISABLED

        val forwarding =
            ProxyForwarding(
                mode = mode,
                secret = table.stringOrNull("secret").orEmpty(),
                tokens =
                    table
                        .stringListOrNull("tokens")
                        .orEmpty()
                        .filter { it.isNotEmpty() }
                        .toSet(),
                propertyName = table.stringOrNull("property-name") ?: ProxyForwarding.SAFE_NET_PROPERTY,
            )
        // A mode without its secret refuses every login anyway, and finding that out one player at
        // a time is worse than being told here.
        forwarding.misconfiguration()?.let { throw TomlFormatException("$path: $it") }
        return forwarding
    }

    /** Every mode turns off Mojang authentication here, so what that costs is said out loud. */
    public fun announce(
        forwarding: ProxyForwarding,
        onlineMode: Boolean,
        port: Int,
        logger: Logger,
    ) {
        if (!forwarding.enabled) return
        logger.info { "Player data is forwarded by a ${forwarding.mode.configName} proxy." }
        if (onlineMode) {
            logger.warn {
                "online-mode is on and a proxy is forwarding: the proxy authenticates, so this " +
                    "server will not. Set online-mode = false to stop this warning."
            }
        }
        if (!forwarding.authenticated) {
            logger.warn {
                "${forwarding.mode.configName} forwarding carries no secret: anybody who can reach " +
                    "port $port can claim to be any player. Firewall it, or use bungeeguard, " +
                    "safenet or velocity instead."
            }
        }
    }
}
