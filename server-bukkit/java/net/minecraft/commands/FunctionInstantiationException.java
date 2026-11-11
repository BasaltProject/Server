// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.commands;
import net.minecraft.network.chat.Component;
public class FunctionInstantiationException extends Exception {
    private final Component detail;
    public FunctionInstantiationException(Component messageComponent) {
        super(messageComponent.getString());
        this.detail = messageComponent;
    }
    public Component messageComponent() {
        return detail;
    }
}
