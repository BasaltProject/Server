// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item;

import java.util.*;
import java.util.function.*;
import java.util.stream.*;
import net.minecraft.core.*;
import net.minecraft.core.component.*;
import net.minecraft.network.chat.*;
import net.minecraft.server.level.*;
import net.minecraft.sounds.*;
import net.minecraft.world.*;
import net.minecraft.world.entity.*;
import net.minecraft.world.entity.player.*;
import net.minecraft.world.item.context.*;
import net.minecraft.world.level.*;
import net.minecraft.world.level.block.*;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.gameevent.GameEvent;
import net.minecraft.world.phys.*;
import net.minecraft.world.item.component.*;
import net.minecraft.world.entity.projectile.*;
import net.minecraft.world.entity.projectile.throwableitemprojectile.*;
import net.minecraft.world.entity.projectile.hurtingprojectile.*;
import net.minecraft.world.entity.projectile.arrow.*;
import net.minecraft.world.level.block.state.properties.BlockStateProperties;
import net.minecraft.advancements.triggers.CriteriaTriggers;
import net.minecraft.stats.Stats;
import net.minecraft.util.Mth;
import net.minecraft.util.RandomSource;
import net.minecraft.world.item.enchantment.EnchantmentHelper;
import net.minecraft.world.level.block.entity.SignBlockEntity;
import net.minecraft.world.level.block.entity.SignTextSlot;
import net.minecraft.tags.*;

import com.mojang.serialization.Codec;
import net.minecraft.ChatFormatting;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.chat.ComponentUtils;
import net.minecraft.network.chat.Style;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.world.level.block.entity.JukeboxBlockEntity;
public record JukeboxPlayable(Holder<JukeboxSong> song) implements TooltipProvider {
    public static final Codec<JukeboxPlayable> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.item.JukeboxPlayable.class, "CODEC");
    public static final StreamCodec<RegistryFriendlyByteBuf, JukeboxPlayable> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.item.JukeboxPlayable.class, "STREAM_CODEC");

    @Override
    public void addToTooltip(Item.TooltipContext context, Consumer<Component> consumer, TooltipFlag flag, DataComponentGetter components) {
        consumer.accept(ComponentUtils.mergeStyles(song.value().description(), Style.EMPTY.withColor(ChatFormatting.GRAY)));
    }

    public static InteractionResult tryInsertIntoJukebox(Level level, BlockPos pos, ItemStack toInsert, Player player) {
        if (toInsert.get(DataComponents.JUKEBOX_PLAYABLE) == null) {
            return InteractionResult.TRY_WITH_EMPTY_HAND;
        }
        BlockState state = level.getBlockState(pos);
        if (!state.is(Blocks.JUKEBOX) || state.getValue(JukeboxBlock.HAS_RECORD)) {
            return InteractionResult.TRY_WITH_EMPTY_HAND;
        }
        if (!level.isClientSide()) {
            ItemStack inserted = toInsert.consumeAndReturn(1, player);
            if (level.getBlockEntity(pos) instanceof JukeboxBlockEntity jukebox) {
                jukebox.load(jukeboxTag(jukebox, inserted));
                level.gameEvent(GameEvent.BLOCK_CHANGE, pos, GameEvent.Context.of(player, state));
            }
            player.awardStat(Stats.PLAY_RECORD);
        }
        return InteractionResult.SUCCESS;
    }

    private static net.minecraft.nbt.CompoundTag jukeboxTag(JukeboxBlockEntity jukebox, ItemStack record) {
        net.minecraft.nbt.CompoundTag tag = jukebox.nbt();
        tag.put("RecordItem", ItemStack.CODEC.encodeStart(net.minecraft.nbt.NbtOps.INSTANCE, record).getOrThrow());
        return tag;
    }
}
