package net.minecraft.world.item;

import net.minecraft.util.StringRepresentable;
import org.basaltmc.bukkit.Unsupported;
public enum DyeColor implements StringRepresentable {
    WHITE, ORANGE, MAGENTA, LIGHT_BLUE, YELLOW, LIME, PINK, GRAY, LIGHT_GRAY, CYAN, PURPLE, BLUE, BROWN, GREEN, RED, BLACK;
    public int getId() {
        throw Unsupported.hit("net.minecraft.world.item.DyeColor.getId");
    }
}
