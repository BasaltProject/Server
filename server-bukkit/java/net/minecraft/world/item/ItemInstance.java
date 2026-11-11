// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item;

import net.minecraft.core.TypedInstance;
import net.minecraft.core.component.DataComponentGetter;
public interface ItemInstance extends TypedInstance<Item>, DataComponentGetter {
    String FIELD_ID = "id";
    String FIELD_COUNT = "count";
    String FIELD_COMPONENTS = "components";
    int count();
    default int getMaxStackSize() {
        return typeHolder().value().getDefaultMaxStackSize();
    }
}
