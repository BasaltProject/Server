// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.entries;

import com.mojang.serialization.MapCodec;
import java.util.Optional;
import java.util.function.Consumer;
import net.minecraft.core.Holder;
import net.minecraft.resources.Identifier;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.functions.LootItemFunction;
import net.minecraft.world.level.storage.loot.predicates.LootItemCondition;

public class DynamicLoot extends SingleEntryContainerBase {
    public static final MapCodec<DynamicLoot> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.storage.loot.entries.DynamicLoot.class, "MAP_CODEC");
    private final Identifier name;

    private DynamicLoot(Identifier name, int weight, int quality, Optional<Holder<LootItemCondition>> condition, Optional<Holder<LootItemFunction>> modifier) {
        super(weight, quality, condition, modifier);
        this.name = name;
    }
    protected void writeEntry(StringBuilder out) {
        out.append("\"type\":\"minecraft:dynamic\",\"name\":");
        org.basaltmc.bukkit.loot.LootJson.string(out, name.toString());
        out.append(",\"weight\":").append(weight).append(",\"quality\":").append(quality);
    }
    public MapCodec<DynamicLoot> codec() {
        return MAP_CODEC;
    }
    public void createItemStack(Consumer<ItemStack> output, LootContext context) {
        context.addDynamicDrops(name, output);
    }
    public static UniformContainerBase.Builder<?> dynamicEntry(Identifier name) {
        return simpleBuilder((weight, quality, condition, modifier) -> new DynamicLoot(name, weight, quality, condition, modifier));
    }
}
