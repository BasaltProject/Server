// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level;

import net.minecraft.world.item.Item;

public interface ItemLike {    default Item asItem() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.ItemLike#asItem");
    }
}
