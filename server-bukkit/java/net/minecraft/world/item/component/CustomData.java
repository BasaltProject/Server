// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.component;

import com.mojang.serialization.Codec;
import io.netty.buffer.ByteBuf;
import java.util.function.Consumer;
import net.minecraft.core.component.DataComponentType;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.nbt.NbtUtils;
import net.minecraft.network.codec.ByteBufCodecs;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.world.item.ItemStack;

public final class CustomData {
    public static final CustomData EMPTY = new CustomData(new CompoundTag());
    public static final Codec<CompoundTag> COMPOUND_TAG_CODEC = CompoundTag.CODEC;
    public static final Codec<CustomData> CODEC = COMPOUND_TAG_CODEC.xmap(CustomData::new, data -> data.tag);
    public static final StreamCodec<ByteBuf, CustomData> STREAM_CODEC = ByteBufCodecs.COMPOUND_TAG.map(CustomData::new, data -> data.tag);

    private final CompoundTag tag;

    private CustomData(CompoundTag tag) {
        this.tag = tag;
    }

    public static CustomData of(CompoundTag tag) {
        return new CustomData(tag.copy());
    }

    public boolean matchedBy(CompoundTag expectedTag) {
        return NbtUtils.compareNbt(expectedTag, tag, true);
    }

    public static void update(DataComponentType<CustomData> component, ItemStack itemStack, Consumer<CompoundTag> consumer) {
        CustomData current = itemStack.getOrDefault(component, EMPTY);
        CustomData updated = current.update(consumer);
        if (updated.tag.isEmpty()) {
            itemStack.remove(component);
        } else {
            itemStack.set(component, updated);
        }
    }

    public static void set(DataComponentType<CustomData> component, ItemStack itemStack, CompoundTag tag) {
        if (tag.isEmpty()) {
            itemStack.remove(component);
        } else {
            itemStack.set(component, of(tag));
        }
    }

    public CustomData update(Consumer<CompoundTag> consumer) {
        CompoundTag copy = tag.copy();
        consumer.accept(copy);
        return new CustomData(copy);
    }

    public boolean isEmpty() {
        return tag.isEmpty();
    }

    public CompoundTag copyTag() {
        return tag.copy();
    }

    @Override
    public boolean equals(Object obj) {
        return obj instanceof CustomData other && tag.equals(other.tag);
    }

    @Override
    public int hashCode() {
        return tag.hashCode();
    }

    @Override
    public String toString() {
        return tag.toString();
    }
}
