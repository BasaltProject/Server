// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item;

import net.minecraft.core.TypedInstance;
import net.minecraft.core.component.DataComponentGetter;
public interface ItemInstance extends TypedInstance<Item>, DataComponentGetter {
    String FIELD_ID = null;
    String FIELD_COUNT = null;
    String FIELD_COMPONENTS = null;
    default int count() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.item.ItemInstance#count");
    }
    default int getMaxStackSize() {
        return typeHolder().value().getDefaultMaxStackSize();
    }
}
