package net.minecraft.network.chat;

import org.basaltmc.bukkit.Unsupported;
public interface Component extends FormattedText {
    static MutableComponent literal(String text) {
        throw Unsupported.hit("net.minecraft.network.chat.Component.literal");
    }
}
