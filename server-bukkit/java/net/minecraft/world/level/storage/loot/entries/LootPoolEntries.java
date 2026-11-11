// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.entries;
import net.minecraft.core.registries.BuiltInRegistries;
import com.mojang.serialization.Codec;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.Registry;
public class LootPoolEntries {
    public static final Codec<LootPoolEntryContainer> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.storage.loot.entries.LootPoolEntries.class, "CODEC");
    public static MapCodec<? extends LootPoolEntryContainer> bootstrap(Registry<MapCodec<? extends LootPoolEntryContainer>> registry) {
        Registry.register(registry, "empty", EmptyLootItem.MAP_CODEC);
        Registry.register(registry, "item", LootItem.MAP_CODEC);
        Registry.register(registry, "loot_table", NestedLootTable.MAP_CODEC);
        Registry.register(registry, "dynamic", DynamicLoot.MAP_CODEC);
        Registry.register(registry, "tag", TagEntry.MAP_CODEC);
        Registry.register(registry, "slots", SlotLoot.MAP_CODEC);
        Registry.register(registry, "alternatives", AlternativesEntry.MAP_CODEC);
        Registry.register(registry, "sequence", SequentialEntry.MAP_CODEC);
        return Registry.register(registry, "group", EntryGroup.MAP_CODEC);
    }
}
