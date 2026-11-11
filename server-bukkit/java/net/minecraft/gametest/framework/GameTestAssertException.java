// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.gametest.framework;

import net.minecraft.network.chat.Component;

public class GameTestAssertException extends GameTestException {
    protected final Component message;
    protected final int tick;

    public GameTestAssertException(Component message, int tick) {
        super(message.getString());
        this.message = message;
        this.tick = tick;
    }

    @Override
    public Component getDescription() {
        return Component.translatable("test.error.tick", message, tick);
    }

    @Override
    public String getMessage() {
        return getDescription().getString();
    }
}
