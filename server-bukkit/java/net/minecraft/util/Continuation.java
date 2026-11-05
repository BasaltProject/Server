// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util;

public enum Continuation {
    CONTINUE, ABORT;

    public boolean shouldAbort() {
        return this == ABORT;
    }

    public static Continuation abortIf(boolean abortIf) {
        return abortIf ? ABORT : CONTINUE;
    }

    public static Continuation continueIf(boolean continueIf) {
        return continueIf ? CONTINUE : ABORT;
    }
}
