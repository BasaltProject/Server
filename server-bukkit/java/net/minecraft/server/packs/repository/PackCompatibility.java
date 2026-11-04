// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.packs.repository;

import net.minecraft.network.chat.Component;

public enum PackCompatibility {
    TOO_OLD("old"), TOO_NEW("new"), UNKNOWN("unknown"), COMPATIBLE("compatible");

    public static final int UNKNOWN_VERSION = Integer.MAX_VALUE;
    private final Component description;
    private final Component confirmation;

    PackCompatibility(String type) {
        this.description = Component.translatable("pack.incompatible." + type).withStyle(net.minecraft.ChatFormatting.GRAY);
        this.confirmation = Component.translatable("pack.incompatible.confirm." + type);
    }

    public boolean isCompatible() {
        return this == COMPATIBLE;
    }

    public Component getDescription() {
        return description;
    }

    public Component getConfirmation() {
        return confirmation;
    }
}
