package org.basaltmc.auth

import kotlin.uuid.ExperimentalUuidApi
import kotlin.uuid.Uuid

@OptIn(ExperimentalUuidApi::class)
public data class SessionProfile(
    val id: Uuid,
    val name: String,
    val properties: List<SessionProfileProperty> = emptyList(),
)

public data class SessionProfileProperty(
    val name: String,
    val value: String,
    val signature: String? = null,
)

/**
 * Verifies that a client who completed the RSA/AES key exchange actually authenticated with
 * Mojang, by checking their claimed username and the session hash against Mojang's session
 * server.
 *
 * // parity: `com.mojang.authlib.minecraft.MinecraftSessionService.hasJoinedServer`, called from
 * `ServerLoginPacketListenerImpl.handleKey` as `GET
 * https://sessionserver.mojang.com/session/minecraft/hasJoined?username={username}&serverId={serverHash}`
 * (with an `&ip={clientAddress}` query parameter too, if `prevent-proxy-connections` is enabled),
 * returning the authenticated profile (UUID, name, and any signed properties such as skin/cape
 * textures) on success, or an absent/failed response if the session is invalid.
 *
 * No implementation ships in this pass: it needs an HTTPS client, which is a separate, deliberately
 * undecided architecture question, shared with the future asset downloader. The
 * RSA/AES key exchange and the resulting encrypted connection do not depend on this decision:
 * everything up to and including installing the cipher works with any [SessionService], including
 * a fake one in tests (see `ServerIntegrationTest`).
 */
public fun interface SessionService {
    public fun hasJoined(
        username: String,
        serverHash: String,
        clientAddress: String?,
    ): SessionProfile?
}
