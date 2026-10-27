package io.papermc.paper.adventure;

public final class PaperAdventure {
    private PaperAdventure() {}
    public static net.minecraft.network.chat.Component asVanilla(net.kyori.adventure.text.Component component) {
        return new AdventureComponent(java.util.Objects.requireNonNull(component));
    }
    public static net.kyori.adventure.text.Component asAdventure(net.minecraft.network.chat.Component component) {
        return component.asAdventure();
    }
}
