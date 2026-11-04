// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.component;

import com.mojang.serialization.Codec;
import com.mojang.serialization.DataResult;
import io.netty.buffer.ByteBuf;
import java.util.function.Consumer;
import net.minecraft.core.component.DataComponentGetter;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.nbt.NbtOps;
import net.minecraft.nbt.Tag;
import net.minecraft.network.chat.Component;
import net.minecraft.network.codec.ByteBufCodecs;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.world.item.Item;
import net.minecraft.world.item.TooltipFlag;

public final class TypedEntityData<IdType> implements TooltipProvider {
    private static final String kTypeKey = "id";

    private final IdType type;
    private final CompoundTag data;

    private TypedEntityData(IdType type, CompoundTag data) {
        this.type = type;
        this.data = data;
    }

    public static <T> Codec<TypedEntityData<T>> codec(Codec<T> typeCodec) {
        return CompoundTag.CODEC.flatXmap(tag -> {
            Tag id = tag.get(kTypeKey);
            if (id == null) {
                return DataResult.error(() -> "Missing id");
            }
            CompoundTag rest = tag.copy();
            rest.remove(kTypeKey);
            return typeCodec.parse(NbtOps.INSTANCE, id).map(parsed -> new TypedEntityData<>(parsed, rest));
        }, value -> typeCodec.encodeStart(NbtOps.INSTANCE, value.type).map(id -> {
            CompoundTag out = value.data.copy();
            out.put(kTypeKey, id);
            return out;
        }));
    }

    public static <B extends ByteBuf, T> StreamCodec<B, TypedEntityData<T>> streamCodec(StreamCodec<B, T> typeCodec) {
        return StreamCodec.composite(typeCodec, TypedEntityData::type, ByteBufCodecs.COMPOUND_TAG, TypedEntityData::getUnsafe, TypedEntityData::new);
    }

    public static <T> TypedEntityData<T> of(T type, CompoundTag data) {
        return new TypedEntityData<>(type, data.copy());
    }

    public IdType type() {
        return type;
    }

    public boolean contains(String name) {
        return data.contains(name);
    }

    @Override
    public boolean equals(Object obj) {
        return obj instanceof TypedEntityData<?> other && type.equals(other.type) && data.equals(other.data);
    }

    @Override
    public int hashCode() {
        return 31 * type.hashCode() + data.hashCode();
    }

    @Override
    public String toString() {
        return type + " " + data;
    }

    public CompoundTag getUnsafe() {
        return data;
    }

    public CompoundTag copyTagWithoutId() {
        return data.copy();
    }

    @Override
    public void addToTooltip(Item.TooltipContext context, Consumer<Component> consumer, TooltipFlag flag, DataComponentGetter components) {
    }

    public boolean loadInto(net.minecraft.world.level.block.entity.BlockEntity entity, net.minecraft.core.HolderLookup.Provider registries) {
        CompoundTag current = entity.saveWithoutMetadata(registries);
        CompoundTag merged = current.copy().merge(data);
        if (current.equals(merged)) {
            return false;
        }
        entity.load(merged);
        entity.setChanged();
        return true;
    }
}
