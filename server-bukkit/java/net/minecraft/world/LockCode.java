// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world;

import com.mojang.serialization.Codec;
import java.util.Optional;
import net.minecraft.advancements.predicates.DataComponentMatchers;
import net.minecraft.advancements.predicates.ItemPredicate;
import net.minecraft.advancements.predicates.MinMaxBounds;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;

public record LockCode(ItemPredicate predicate) {
    public static final LockCode NO_LOCK = new LockCode(new ItemPredicate(Optional.empty(), MinMaxBounds.Ints.ANY, DataComponentMatchers.ANY));
    public static final Codec<LockCode> CODEC = ItemPredicate.CODEC.xmap(LockCode::new, LockCode::predicate);
    public static final String TAG_LOCK = "lock";

    public boolean unlocksWith(ItemStack itemStack) {
        return this == NO_LOCK || predicate.test(itemStack);
    }

    public void addToTag(ValueOutput parent) {
        if (this != NO_LOCK) {
            parent.store(TAG_LOCK, CODEC, this);
        }
    }

    public boolean canUnlock(Player player) {
        return player.isSpectator() || unlocksWith(player.getMainHandItem());
    }

    public static LockCode fromTag(ValueInput parent) {
        return parent.read(TAG_LOCK, CODEC).orElse(NO_LOCK);
    }
}
