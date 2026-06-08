package net.minecraft.network.chat;

import org.basaltmc.bukkit.Unsupported;
public class MutableComponent implements Component {
    protected MutableComponent() {
    }
    public MutableComponent append(Component component) {
        throw Unsupported.hit("net.minecraft.network.chat.MutableComponent.append");
    }
}
