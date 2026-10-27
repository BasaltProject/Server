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
}
