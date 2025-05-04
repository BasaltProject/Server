package org.basaltmc.protocol.login

import org.basaltmc.io.ByteBuffer
import org.basaltmc.protocol.readBoolean
import org.basaltmc.protocol.readProtocolString
import org.basaltmc.protocol.readUuid
import org.basaltmc.protocol.readVarInt
import org.basaltmc.protocol.writeBoolean
import org.basaltmc.protocol.writeProtocolString
import org.basaltmc.protocol.writeUuid
import org.basaltmc.protocol.writeVarInt
import org.basaltmc.text.Component
import org.basaltmc.text.ComponentJson
import kotlin.uuid.ExperimentalUuidApi
import kotlin.uuid.Uuid

/**
 * Serverbound, id 0x00: the first packet of login. Mirrors `ServerboundHelloPacket`. This pass
 * only implements the offline-mode path: no encryption request/response is sent, so this goes
 * straight to [LoginCompressionPacket]/[LoginFinishedPacket]. Crypto is deferred alongside
 * `basalt-auth`'s online-mode support.
 */
@OptIn(ExperimentalUuidApi::class)
public data class LoginStartPacket(
    val name: String,
    val profileId: Uuid,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeProtocolString(name, MAX_NAME_LENGTH)
        buffer.writeUuid(profileId)
    }

    public companion object {
        public const val ID: Int = 0
        private const val MAX_NAME_LENGTH = 16

        public fun read(buffer: ByteBuffer): LoginStartPacket {
            val name = buffer.readProtocolString(MAX_NAME_LENGTH)
            val profileId = buffer.readUuid()
            return LoginStartPacket(name, profileId)
        }
    }
}

/** Clientbound, id 0x03: negotiates the compression threshold. Mirrors `ClientboundLoginCompressionPacket`. */
public data class LoginCompressionPacket(
    val threshold: Int,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeVarInt(threshold)
    }

    public companion object {
        public const val ID: Int = 3

        public fun read(buffer: ByteBuffer): LoginCompressionPacket = LoginCompressionPacket(buffer.readVarInt())
    }
}

public data class GameProfileProperty(
    val name: String,
    val value: String,
    val signature: String? = null,
)

@OptIn(ExperimentalUuidApi::class)
public data class GameProfile(
    val id: Uuid,
    val name: String,
    val properties: List<GameProfileProperty> = emptyList(),
)

/**
 * Clientbound, id 0x02: login success. Mirrors `ClientboundLoginFinishedPacket`. Notably carries
 * a `sessionId` field alongside the [GameProfile] in this version — confirmed against the
 * decompiled 26.2 jar, not present in older protocol documentation.
 */
@OptIn(ExperimentalUuidApi::class)
public data class LoginFinishedPacket(
    val gameProfile: GameProfile,
    val sessionId: Uuid,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeUuid(gameProfile.id)
        buffer.writeProtocolString(gameProfile.name, 16)
        buffer.writeVarInt(gameProfile.properties.size)
        for (property in gameProfile.properties) {
            buffer.writeProtocolString(property.name)
            buffer.writeProtocolString(property.value)
            buffer.writeBoolean(property.signature != null)
            property.signature?.let { buffer.writeProtocolString(it) }
        }
        buffer.writeUuid(sessionId)
    }

    public companion object {
        public const val ID: Int = 2

        public fun read(buffer: ByteBuffer): LoginFinishedPacket {
            val id = buffer.readUuid()
            val name = buffer.readProtocolString(16)
            val propertyCount = buffer.readVarInt()
            val properties =
                List(propertyCount) {
                    val propName = buffer.readProtocolString()
                    val propValue = buffer.readProtocolString()
                    val signature = if (buffer.readBoolean()) buffer.readProtocolString() else null
                    GameProfileProperty(propName, propValue, signature)
                }
            val sessionId = buffer.readUuid()
            return LoginFinishedPacket(GameProfile(id, name, properties), sessionId)
        }
    }
}

/** Serverbound, id 0x03: acknowledges [LoginFinishedPacket], moving the connection to CONFIGURATION. */
public object LoginAcknowledgedPacket {
    public const val ID: Int = 3
}

/** Clientbound, id 0x00: rejects the login attempt with a reason. Mirrors `ClientboundLoginDisconnectPacket`. */
public data class LoginDisconnectPacket(
    val reason: Component,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeProtocolString(ComponentJson.serialize(reason).toString())
    }

    public companion object {
        public const val ID: Int = 0
    }
}
