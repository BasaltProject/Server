// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.gametest.framework;

import net.minecraft.core.BlockPos;
import net.minecraft.network.chat.Component;

public class GameTestAssertPosException extends GameTestAssertException {
    private final BlockPos absolutePos;
    private final BlockPos relativePos;

    public GameTestAssertPosException(Component baseMessage, BlockPos absolutePos, BlockPos relativePos, int tick) {
        super(baseMessage, tick);
        this.absolutePos = absolutePos;
        this.relativePos = relativePos;
    }

    @Override
    public Component getDescription() {
        return Component.translatable("test.error.position", message, absolutePos.getX(), absolutePos.getY(), absolutePos.getZ(),
            relativePos.getX(), relativePos.getY(), relativePos.getZ(), tick);
    }

    public Component getMessageToShowAtBlock() {
        return message;
    }

    public BlockPos getRelativePos() {
        return relativePos;
    }

    public BlockPos getAbsolutePos() {
        return absolutePos;
    }
}
