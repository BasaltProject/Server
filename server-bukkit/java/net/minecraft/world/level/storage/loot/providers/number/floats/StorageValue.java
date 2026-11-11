// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.providers.number.floats;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.Holder;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.ValidationContext;
import net.minecraft.world.level.storage.loot.providers.number.StoredNumberAccess;
public record StorageValue(StoredNumberAccess access, Holder<ContextFloatProvider> fallback) implements ContextFloatProvider {
    public static final MapCodec<StorageValue> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.storage.loot.providers.number.floats.StorageValue.class, "MAP_CODEC");
    public float getFloatUnsafe(LootContext context) {
        Number stored = access.getNumericTag(context);
        return stored != null ? stored.floatValue() : fallback.value().getFloatUnsafe(context);

    }
    public void validate(ValidationContext context) {
    }
    public MapCodec<StorageValue> codec() {
        return (com.mojang.serialization.MapCodec) MAP_CODEC;
    }
}
