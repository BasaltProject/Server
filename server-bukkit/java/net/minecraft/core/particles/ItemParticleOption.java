// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.core.particles;

import com.mojang.serialization.MapCodec;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.world.item.Item;
import net.minecraft.world.item.ItemStackTemplate;

public class ItemParticleOption implements ParticleOptions {
    private final ParticleType<ItemParticleOption> type;
    private final ItemStackTemplate itemStack;

    public static MapCodec<ItemParticleOption> codec(ParticleType<ItemParticleOption> type) {
        return org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(ItemParticleOption.class, "CODEC");
    }

    public static StreamCodec<? super RegistryFriendlyByteBuf, ItemParticleOption> streamCodec(ParticleType<ItemParticleOption> type) {
        return ItemStackTemplate.STREAM_CODEC.map(item -> new ItemParticleOption(type, item), ItemParticleOption::getItem);
    }

    public ItemParticleOption(ParticleType<ItemParticleOption> type, Item item) {
        this(type, new ItemStackTemplate(item));
    }

    public ItemParticleOption(ParticleType<ItemParticleOption> type, ItemStackTemplate itemStack) {
        this.type = type;
        this.itemStack = itemStack;
    }

    public ParticleType<ItemParticleOption> getType() {
        return type;
    }

    public ItemStackTemplate getItem() {
        return itemStack;
    }
}
