package net.minecraft.world.item;

import net.minecraft.resources.Identifier;
import net.minecraft.world.InteractionHand;
import net.minecraft.world.InteractionResult;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.level.ItemLike;
import net.minecraft.world.level.Level;
import org.basaltmc.bukkit.Unsupported;

public class Item implements ItemLike {
    private final Identifier key;

    public Item(Identifier key) {
        this.key = key;
    }

    public Identifier itemKey() {
        return key;
    }

    public String getDescriptionId() {
        return "item." + key.getNamespace() + "." + key.getPath().replace('/', '.');
    }

    public InteractionResult use(Level level, Player player, InteractionHand hand) {
        throw Unsupported.hit("net.minecraft.world.item.Item.use");
    }

    @Override
    public String toString() {
        return key.toString();
    }
}
