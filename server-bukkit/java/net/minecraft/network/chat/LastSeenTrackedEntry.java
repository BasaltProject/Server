// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.chat;
public record LastSeenTrackedEntry(MessageSignature signature, boolean pending) {
    public LastSeenTrackedEntry acknowledge() {
        return pending ? new LastSeenTrackedEntry(signature, false) : this;
    }
}
