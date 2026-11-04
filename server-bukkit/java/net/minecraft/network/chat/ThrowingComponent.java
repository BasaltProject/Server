// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.chat;
public class ThrowingComponent extends Exception {
    private final Component component;
    public ThrowingComponent(Component component) {
        super(component.getString());
        this.component = component;
    }
    public ThrowingComponent(Component component, Throwable cause) {
        super(component.getString(), cause);
        this.component = component;
    }
    public Component getComponent() {
        return component;
    }
}
