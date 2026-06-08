package net.minecraft.world;

import org.basaltmc.bukkit.Unsupported;
public interface InteractionResult {
    public static InteractionResult.Success SUCCESS = null;
    public default boolean consumesAction() {
        throw Unsupported.hit("net.minecraft.world.InteractionResult.consumesAction");
    }

    public record Success(Object swingSource, Object itemContext) implements InteractionResult {
    }
}
