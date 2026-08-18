package org.basaltmc.server.runtime.world

import org.basaltmc.wire.protocol.play.SoundSource

/**
 * What a block asks the clients to do besides changing state.
 *
 * parity: `Level.playSound`, `Level.blockEvent` and `Level.levelEvent`. A block reacting on its own
 * has no player to answer to, so without this its note or its ring had nowhere to go.
 */
public sealed interface BlockEffect {
    /** parity: `Level.playSound`, at the middle of the block. */
    public class Sound(
        public val name: String,
        public val source: SoundSource,
        public val volume: Float,
        public val pitch: Float,
    ) : BlockEffect

    /** parity: `Level.blockEvent`, whose two bytes mean whatever the block says they mean. */
    public class Event(
        public val b0: Int,
        public val b1: Int,
    ) : BlockEffect

    /** parity: `Level.levelEvent`, the numbered one-off effects (`LevelEvent`). */
    public class Effect(
        public val type: Int,
        public val data: Int,
    ) : BlockEffect {
        public companion object {
            /** parity: `LevelEvent.SOUND_DISPENSER_FAIL`, the click of a dispenser with nothing in it. */
            public const val DISPENSER_FAIL: Int = 1001
        }
    }
}
