// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.gametest.framework;

import net.minecraft.network.chat.Component;

public class UnknownGameTestException extends GameTestException {
    private final Throwable reason;

    public UnknownGameTestException(Throwable reason) {
        super(reason.getMessage());
        this.reason = reason;
    }

    @Override
    public Component getDescription() {
        return Component.translatable("test.error.unknown", reason.getMessage());
    }
}
