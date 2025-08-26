package org.basaltmc.server.runtime.net

import org.basaltmc.wire.auth.forwarding.ForwardedLogin
import org.basaltmc.wire.auth.forwarding.ForwardingMode
import org.basaltmc.wire.auth.forwarding.ForwardingResult
import org.basaltmc.wire.auth.forwarding.LegacyForwarding
import org.basaltmc.wire.auth.forwarding.ProxyForwarding
import org.basaltmc.wire.auth.forwarding.VelocityForwarding
import org.basaltmc.wire.common.logging.Logger
import kotlin.random.Random

/** One connection's side of proxy forwarding, kept beside the session: it all happens before there is a player. */
internal class ProxyLogin(
    private val forwarding: ProxyForwarding,
    private val logger: Logger,
) {
    /** Who the proxy says is connecting, once its data has been read and checked. */
    var login: ForwardedLogin? = null
        private set

    /** The query this connection is waiting on, or [NO_QUERY] when it is not waiting on one. */
    var transactionId: Int = NO_QUERY
        private set

    val enabled: Boolean get() = forwarding.enabled

    /** Whether the data rides on the handshake host, which is what the BungeeCord family does. */
    val readsHandshake: Boolean get() = forwarding.mode.isLegacy

    val asksTheProxy: Boolean get() = forwarding.mode == ForwardingMode.VELOCITY

    val channel: String get() = VelocityForwarding.CHANNEL

    /** Reads the handshake host, but the result is only acted on at login, where a refusal can still be a screen. */
    fun readHandshake(host: String): ForwardingResult = LegacyForwarding.decode(host, forwarding)

    /** Starts the Velocity query, returning the transaction id to send it under. */
    fun beginQuery(): Int {
        transactionId = Random.nextInt()
        return transactionId
    }

    /** What goes in the query body: the newest forwarding version this server can read. */
    fun query(): ByteArray = VelocityForwarding.request()

    fun readAnswer(
        answeredId: Int,
        payload: ByteArray?,
    ): ForwardingResult {
        if (answeredId != transactionId) return ForwardingResult.Rejected("the login query answer names another query")
        transactionId = NO_QUERY
        // A client that does not know the channel answers with nothing, which is every real one:
        // somebody came to this port directly instead of through the proxy.
        val data = payload ?: return ForwardingResult.Rejected("the connection did not answer the forwarding query")
        return VelocityForwarding.decode(data, forwarding)
    }

    /** Returns the message to disconnect with, or null to carry on. The reason goes to the log, never to the screen. */
    fun accept(
        result: ForwardingResult,
        name: String?,
        address: String?,
    ): String? {
        when (result) {
            is ForwardingResult.Accepted -> {
                login = result.login
                return null
            }
            is ForwardingResult.Rejected -> {
                logger.warn {
                    "Refused ${name ?: "a connection"} from ${address ?: "an unknown address"}: " +
                        "${result.reason}."
                }
                return REFUSED_MESSAGE
            }
        }
    }

    companion object {
        const val NO_QUERY: Int = -1

        /** Deliberately says nothing about which check failed. */
        private const val REFUSED_MESSAGE =
            "<red>This server only accepts players coming through its proxy.</red>\n" +
                "<gray>If that is you, its forwarding is not set up on both ends."
    }
}
