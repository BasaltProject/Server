// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.structure.templatesystem.rule.blockentity;
import com.mojang.serialization.MapCodec;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.resources.ResourceKey;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.storage.loot.LootTable;
public class AppendLoot implements RuleBlockEntityModifier {
    private final ResourceKey<LootTable> lootTable;
    public static final MapCodec<AppendLoot> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.structure.templatesystem.rule.blockentity.AppendLoot.class, "CODEC");
    public AppendLoot(ResourceKey<LootTable> lootTable) {
        this.lootTable = lootTable;
    }
    public CompoundTag apply(RandomSource random, CompoundTag existingTag) {
        CompoundTag result = existingTag == null ? new CompoundTag() : existingTag.copy();
        result.store("LootTable", LootTable.KEY_CODEC, lootTable);
        result.putLong("LootTableSeed", random.nextLong());
        return result;
    }
    public RuleBlockEntityModifierType<?> getType() {
        return RuleBlockEntityModifierType.APPEND_LOOT;
    }
}
