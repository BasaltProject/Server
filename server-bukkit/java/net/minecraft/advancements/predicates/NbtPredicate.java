// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.advancements.predicates;

import com.mojang.serialization.Codec;
import io.netty.buffer.ByteBuf;
import net.minecraft.core.component.DataComponentGetter;
import net.minecraft.core.component.DataComponents;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.nbt.NbtOps;
import net.minecraft.nbt.NbtUtils;
import net.minecraft.nbt.Tag;
import net.minecraft.nbt.TagParser;
import net.minecraft.network.codec.ByteBufCodecs;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.component.CustomData;

public record NbtPredicate(CompoundTag tag) {
    public static final Codec<NbtPredicate> CODEC = TagParser.LENIENT_CODEC.xmap(NbtPredicate::new, NbtPredicate::tag);
    public static final StreamCodec<ByteBuf, NbtPredicate> STREAM_CODEC = ByteBufCodecs.COMPOUND_TAG.map(NbtPredicate::new, NbtPredicate::tag);
    public static final String SELECTED_ITEM_TAG = "SelectedItem";

    public boolean matches(DataComponentGetter components) {
        CustomData data = components.get(DataComponents.CUSTOM_DATA);
        return data != null && data.matchedBy(tag);
    }

    public boolean matches(Entity entity) {
        return matches(getEntityTagToCompare(entity));
    }

    public boolean matches(Tag other) {
        return other != null && NbtUtils.compareNbt(tag, other, true);
    }

    public static CompoundTag getEntityTagToCompare(Entity entity) {
        CompoundTag compare = entity.nbt();
        if (compare == null) {
            return new CompoundTag();
        }
        compare.remove("id");
        if (entity instanceof Player player) {
            ItemStack held = player.getInventory().getSelectedItem();
            if (!held.isEmpty()) {
                ItemStack.CODEC.encodeStart(NbtOps.INSTANCE, held).result().ifPresent(tag -> compare.put(SELECTED_ITEM_TAG, tag));
            }
        }
        return compare;
    }
}
