package io.papermc.paper.adventure;

public final class AdventureComponent implements net.minecraft.network.chat.Component {
    private final net.kyori.adventure.text.Component adventure;
    public AdventureComponent(net.kyori.adventure.text.Component adventure) {
        this.adventure = java.util.Objects.requireNonNull(adventure);
    }
    @Override
    public net.kyori.adventure.text.Component asAdventure() {
        return adventure;
    }
    @Override
    public String getString() {
        return org.basaltmc.bukkit.Text.plain(adventure);
    }

    private net.minecraft.network.chat.Component converted() {
        return org.basaltmc.bukkit.facade.ChatJson.fromAdventure(adventure);
    }

    @Override
    public net.minecraft.network.chat.Style getStyle() {
        return converted().getStyle();
    }

    @Override
    public net.minecraft.network.chat.ComponentContents getContents() {
        return converted().getContents();
    }

    @Override
    public java.util.List<net.minecraft.network.chat.Component> getSiblings() {
        return converted().getSiblings();
    }
}
