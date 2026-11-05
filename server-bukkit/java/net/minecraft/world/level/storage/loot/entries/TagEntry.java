// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.entries;

import com.mojang.serialization.MapCodec;
import java.util.Optional;
import java.util.function.Consumer;
import net.minecraft.core.Holder;
import net.minecraft.core.HolderSet;
import net.minecraft.world.item.Item;
import net.minecraft.world.level.storage.loot.functions.LootItemFunction;
import net.minecraft.world.level.storage.loot.predicates.LootItemCondition;

public class TagEntry extends ExpandableContainerBase {
    public static final MapCodec<TagEntry> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.storage.loot.entries.TagEntry.class, "MAP_CODEC");
    private final HolderSet<Item> tag;

    private TagEntry(HolderSet<Item> tag, boolean expand, int weight, int quality, Optional<Holder<LootItemCondition>> condition, Optional<Holder<LootItemFunction>> modifier) {
        super(expand, weight, quality, condition, modifier);
        this.tag = tag;
    }
    protected void writeEntry(StringBuilder out) {
        out.append("\"type\":\"minecraft:tag\",\"name\":");
        org.basaltmc.bukkit.loot.LootJson.string(out, tag.unwrapKey().map(key -> key.location().toString()).orElse(""));
        out.append(",\"expand\":").append(expand);
        out.append(",\"weight\":").append(weight).append(",\"quality\":").append(quality);
    }
    public MapCodec<TagEntry> codec() {
        return MAP_CODEC;
    }
    protected boolean addExpandedEntries(Consumer<LootPoolEntry> output) {
        return false;
    }
    protected boolean addUnexpandedEntry(Consumer<LootPoolEntry> output) {
        return false;
    }
    public static UniformContainerBase.Builder<?> tagContents(HolderSet<Item> tag) {
        return simpleBuilder((weight, quality, condition, modifier) -> new TagEntry(tag, false, weight, quality, condition, modifier));
    }
    public static UniformContainerBase.Builder<?> expandTag(HolderSet<Item> tag) {
        return simpleBuilder((weight, quality, condition, modifier) -> new TagEntry(tag, true, weight, quality, condition, modifier));
    }
}
